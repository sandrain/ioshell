#ifndef __RDTSC_H_DEFINED__
#define __RDTSC_H_DEFINED__


#if defined(__i386__)

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#elif defined(__x86_64__)

typedef unsigned long long int unsigned long long;

static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#elif defined(__powerpc__)

typedef unsigned long long int unsigned long long;

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int result=0;
  unsigned long int upper, lower,tmp;
  __asm__ volatile(
                "0:                  \n"
                "\tmftbu   %0           \n"
                "\tmftb    %1           \n"
                "\tmftbu   %2           \n"
                "\tcmpw    %2,%0        \n"
                "\tbne     0b         \n"
                : "=r"(upper),"=r"(lower),"=r"(tmp)
                );
  result = upper;
  result = result<<32;
  result = result|lower;

  return(result);
}

#else

#error "No tick counter is available!"

#endif


/*  $RCSfile:  $   $Author: kazutomo $
 *  $Revision: 1.6 $  $Date: 2005/04/13 18:49:58 $
 */
/*
 * Pentium class cpu has an instruction to read the current time-stamp counter variable ,which is a
 * 64-bit variable, into registers (edx:eax). TSC(time stamp counter) is incremented every cpu tick
 * (1/CPU_HZ). For example, at 1GHz cpu, TSC is incremented by 10^9 per second. It allows to
 * measure time activety in an accurate fashion.
 *
 * PowerPC provides similar capability but PowerPC time counter is increments at either equal the
 * processor core clock rate or as driven by a separate timer clock input. PowerPC time counter is
 * also a 64-bit.
 *
 * The example..
 */
#if 0

#include <stdio.h>
#include "rdtsc.h"

int main(int argc, char* argv[])
{
	unsigned long long a,b;

	a = rdtsc();
	b = rdtsc();

	printf("%llu\n", b-a);
	return 0;
}

#endif

#endif /* __RDTSC_H_DEFINED__ */

