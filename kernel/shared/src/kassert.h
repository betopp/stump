//kassert.h
//Kernel assertion macro
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef KASSERT_H
#define KASSERT_H

//Runaround for making a string out of __LINE__ 
#define KASSERT_STR1(x) #x
#define KASSERT_STR2(x) KASSERT_STR1(x)

//Macro for panicking if a condition is false.
#define KASSERT(cond) do { if(!(cond)) { kassert_failed("Failed(" __FILE__ ":" KASSERT_STR2(__LINE__) "):" #cond ); } } while(0)

//Handler for failed assertions
extern void kassert_failed(const char *str);

#endif //KASSERT_H
