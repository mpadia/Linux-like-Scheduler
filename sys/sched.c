#include <kernel.h>
#include <proc.h>
#include <sched.h>


void setschedclass(int sched_class) {
  schedPolicy = sched_class;
  preempt = proctab[currpid].pprio;
}

int getschedclass() {
  return schedPolicy;
}
