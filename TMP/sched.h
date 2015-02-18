#ifndef __SCHED_H__
#define __SCHED_H__

#define DEFAULTSCHED	0
#define LINUXSCHED      1               /* Scheduler policy-linux type  */
#define MULTIQSCHED     2               /* Scheduler policy-MultiQ      */

int schedPolicy;

void setschedclass(int sched_class);
int getschedclass();

#endif
