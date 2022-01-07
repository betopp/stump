//arm32.h
//ARM 32-bit helpers
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef ARM32_H
#define ARM32_H

//Invalidates TLB and virtually-tagged caches
extern void _arm_invl(void);

//Invalidates single page in TLB
extern void _arm_invlpg(uint32_t vaddr);

//Memory barrier - orders accesses before/after
extern void _arm_mbarrier(void);

#endif //ARM32_H


