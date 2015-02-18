/* resched.c  -  resched */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>
#include <sched.h>



unsigned long currSP;	/* REAL sp of current process */
extern int ctxsw(int, int, int, int);
/*-----------------------------------------------------------------------
 * resched  --  reschedule processor to highest priority ready process
 *
 * Notes:	Upon entry, currpid gives current process id.
 *		Proctab[currpid].pstate gives correct NEXT state for
 *			current process if other than PRREADY.
 *------------------------------------------------------------------------
 */

static int random = 1;
int flagRealProc = 0;
int epoch = 0;

int calcGoodness(struct pentry  *ptr) {
  /*if(ptr == &proctab[0])
    return -1;*/
  if(ptr->counter == 0) 
    return 0;
  
  if(ptr->ncreate == 0) {
    if(ptr->realproc == 0)
      return ptr->counter + ptr->pprio;
    else
      return ptr->counter;
  }

  return 0;
}

void calcEpoch() {
  int pid;

  epoch = 0;
  for (pid = 1; pid < NPROC; pid++) {

    if(proctab[pid].pstate == PRFREE)
      continue;

    // If in last epoch the process did not run, then make counter = 0
    if((proctab[pid].ncreate == 0) && (proctab[pid].ocounter == proctab[pid].counter)) {
      proctab[pid].counter = 0;
    }
    // If process was created in last epoch, then update the flag to consider that process to get scheduled now
    // If process priority was changed in last epoch, then bring new  priority in effect now
    if((proctab[pid].chgprio ==  1) || (proctab[pid].ncreate == 1)) {
      proctab[pid].pprio = proctab[pid].nprio;
      proctab[pid].chgprio = 0;
      proctab[pid].ncreate = 0;
    }
 
    if((proctab[pid].realproc == 1) && (flagRealProc == 1)) {
      proctab[pid].counter = REALQUANTUM;
      proctab[pid].ocounter = proctab[pid].counter;
      epoch = epoch + proctab[pid].counter;
    }
    if((proctab[pid].realproc == 0) && (flagRealProc == 0)) {
      proctab[pid].counter = proctab[pid].pprio + (proctab[pid].counter>>1);
      proctab[pid].ocounter = proctab[pid].counter;
      epoch = epoch + proctab[pid].counter;
    }
  }
}

int resched()
{

	register struct	pentry	*optr;	/* pointer to old process entry */
	register struct	pentry	*nptr;	/* pointer to new process entry */
	int scheduler = getschedclass();
	STATWORD ps;
	disable(ps);
        if(scheduler == DEFAULTSCHED) {
	  flagRealProc = 0;
          /* no switch needed if current process priority higher than next*/
  
          if ( ( (optr= &proctab[currpid])->pstate == PRCURR) &&
             (lastkey(rdytail)<optr->pprio)) {
                  return(OK);
          }

          /* force context switch */
  
          if (optr->pstate == PRCURR) {
            optr->pstate = PRREADY;
            insert(currpid,rdyhead,optr->pprio);
          }

          /* remove highest priority process at end of ready list */

          nptr = &proctab[ (currpid = getlast(rdytail)) ];
          nptr->pstate = PRCURR;          /* mark it currently running    */
#ifdef  RTCLOCK
          preempt = QUANTUM;              /* reset preemption counter     */
#endif

          ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);

          /* The OLD process returns here when resumed. */
	  restore(ps);
          return OK;

        }

    optr = &proctab[currpid];
    int rdyProc = q[rdyhead].qnext;
    int procTosched = NPROC;
    int maxGoodness = 0;
    int tmpGoodness = 0;
    int flagCTXSW = 0;
    int sum = 0;
    int pid;
    int countRdyProc = 0;


    if(scheduler == LINUXSCHED) {
      flagRealProc = 0;
      if (optr->counter)
        optr->counter = preempt;
      if (optr->counter < 0)
        optr->counter = 0;

      if (optr->pstate == PRCURR) {
        optr->pstate = PRREADY;
        insert(currpid,rdyhead,optr->pprio);
      }

      do {
	countRdyProc = 1;
        procTosched = rdyProc = q[rdyhead].qnext;
        maxGoodness = calcGoodness(&proctab[rdyProc]);

        rdyProc = q[rdyProc].qnext;

        while((rdyProc < NPROC) || (rdyProc != rdytail)) {
	  if(rdyProc == 0) {
	    rdyProc = q[rdyProc].qnext;
	    continue;
	  }
	  countRdyProc++;
          tmpGoodness = calcGoodness(&proctab[rdyProc]);
          if(tmpGoodness >= maxGoodness) {
            maxGoodness = tmpGoodness;
            procTosched = rdyProc;
          }
          rdyProc = q[rdyProc].qnext;
        } 

        if(maxGoodness > 0) 	flagCTXSW = 1;
        if(maxGoodness == 0) {
	  if(countRdyProc == 1) {
	    flagCTXSW = 1;
	  }
	  else {
            calcEpoch();
            continue;
	  }
        }
      } while (!flagCTXSW);

      dequeue(procTosched);
      nptr = &proctab[ (currpid = procTosched) ];
      nptr->pstate = PRCURR;                        /* mark it currently running    */
#ifdef  RTCLOCK
      preempt = nptr->counter;              /* reset preemption counter     */
#endif
      ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);

    }

    if(scheduler == MULTIQSCHED) {
      if (optr->counter)
        optr->counter = preempt;
      if (optr->counter < 0)
        optr->counter = 0;

      if (optr->pstate == PRCURR) {
        optr->pstate = PRREADY;
        if(optr->realproc == 1)
          insert(currpid,rdyheadreal,REALQUANTUM);
        else
          insert(currpid,rdyhead,optr->pprio);
      }

      while (1) {
        if(flagRealProc == 1) {
	  if(isempty(rdyheadreal)) {
	    flagRealProc = 0;
	    calcEpoch();
	  }
	  else {
            sum = 0;
            for (pid = 1; pid < NPROC; pid++) {
              if(proctab[pid].realproc == 1)
              sum = sum + proctab[pid].counter;
            }
           if(sum > 0) { 
             nptr = &proctab[ (currpid = getlast(rdytailreal)) ];
             nptr->pstate = PRCURR;          /* mark it currently running    */
#ifdef  RTCLOCK
             preempt = REALQUANTUM;
#endif
              ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	      restore(ps);
              return OK;
            }
            else {
              srand(random++);
              if(isempty(rdyhead) && nonempty(rdyheadreal))	flagRealProc = 1;
              else if(isempty(rdyheadreal) && nonempty(rdyhead))	flagRealProc = 0;
	      else {
                if(rand()%100 < 30)	 flagRealProc = 0;
                else	flagRealProc = 1;
              }

              calcEpoch();
            }
	  }
        }
        else {
          if(isempty(rdyhead)) {
	    flagRealProc = 1;
	    calcEpoch();
	  }
          else {
	  countRdyProc = 1;
          procTosched = rdyProc = q[rdyhead].qnext;
          maxGoodness = calcGoodness(&proctab[rdyProc]);
          rdyProc = q[rdyProc].qnext;

          while((rdyProc < NPROC) || (rdyProc != rdytail)) {
            if(rdyProc == 0) {
              rdyProc = q[rdyProc].qnext;
              continue;
            }
	    countRdyProc++;
            tmpGoodness = calcGoodness(&proctab[rdyProc]);
            if(tmpGoodness >= maxGoodness) {
              maxGoodness = tmpGoodness;
              procTosched = rdyProc;
            }
            rdyProc = q[rdyProc].qnext;
          }

	  if(countRdyProc == 1 && isempty(rdyheadreal)) {
            dequeue(procTosched);
            nptr = &proctab[ (currpid = procTosched) ];
            nptr->pstate = PRCURR;                        /* mark it currently running    */
#ifdef  RTCLOCK
            preempt = nptr->counter;              /* reset preemption counter     */
#endif
            ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
            restore(ps);
            return OK;	    
	  }
          if(maxGoodness > 0) {
            dequeue(procTosched); 
            nptr = &proctab[ (currpid = procTosched) ];
            nptr->pstate = PRCURR;                        /* mark it currently running    */
#ifdef  RTCLOCK
            preempt = nptr->counter;              /* reset preemption counter     */
#endif
            ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
            restore(ps);
            return OK;
          }
          else {
            srand(random++);
            if(isempty(rdyhead) && nonempty(rdyheadreal))       flagRealProc = 1;
            else if(isempty(rdyheadreal) && nonempty(rdyhead))  flagRealProc = 0;
            else {
              if(rand()%100 < 30)        flagRealProc = 0;
              else      flagRealProc = 1;
            }
            calcEpoch();
          }
	  }
        }
      }
    }

  restore(ps);
  /* The OLD process returns here when resumed. */
  return OK;
}
