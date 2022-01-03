//m_time.h
//Timing functions, per-machine
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef M_TIME_H
#define M_TIME_H

//Returns a count of CPU cycles elapsed since boot on the calling CPU, or a similar timer.
int64_t m_time_tsc(void);

#endif //M_TIME_H
