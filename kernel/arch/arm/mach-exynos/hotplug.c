/* linux arch/arm/mach-exynos/hotplug.c
 *
 *  Cloned from linux/arch/arm/mach-realview/hotplug.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/completion.h>
#include <linux/io.h>

#include <asm/cacheflush.h>

#include <plat/cpu.h>
#include <mach/regs-pmu.h>

extern volatile int pen_release;

static inline void cpu_enter_lowpower_a9(void)
{
	unsigned int v;

	flush_cache_all();
	asm volatile(
	"	mcr	p15, 0, %1, c7, c5, 0\n"
	"	mcr	p15, 0, %1, c7, c10, 4\n"
	/*
	 * Turn off coherency
	 */
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	bic	%0, %0, %3\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	"	mrc	p15, 0, %0, c1, c0, 0\n"
	"	bic	%0, %0, %2\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	  : "=&r" (v)
	  : "r" (0), "Ir" (CR_C), "Ir" (0x40)
	  : "cc");
}

static inline void cpu_enter_lowpower_a15(void)
{
	unsigned int v;

	asm volatile(
	"       mrc     p15, 0, %0, c1, c0, 0\n"
	"       bic     %0, %0, %1\n"
	"       mcr     p15, 0, %0, c1, c0, 0\n"
	  : "=&r" (v)
	  : "Ir" (CR_C)
	  : "cc");

	flush_cache_all();

	asm volatile(
	/*
	* Turn off coherency
	*/
	"       mrc     p15, 0, %0, c1, c0, 1\n"
	"       bic     %0, %0, %1\n"
	"       mcr     p15, 0, %0, c1, c0, 1\n"
	: "=&r" (v)
	: "Ir" (0x40)
	: "cc");

	isb();
	dsb();
}

static inline void cpu_leave_lowpower(unsigned int cpu)
{
	unsigned int v;

	asm volatile(
	"mrc	p15, 0, %0, c1, c0, 0\n"
	"	orr	%0, %0, %1\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	orr	%0, %0, %2\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	  : "=&r" (v)
	  : "Ir" (CR_C), "Ir" (0x40)
	  : "cc");

/*
 * leehc add
	On Exynos4 USE_DELAYED_RESET_ASSERTION must be set in
	ARM_COREx_OPTION register during CPU power down. This is the proper way
	of powering down CPU.
	
	Additionally without this the CPU clock down won't work after powering
	down some other CPU and online CPUs will work at full frequency chosen
	by CPUfreq governor.

	if ((cpu >= 1) && (cpu < NR_CPUS)) {
		unsigned int tmp;
		tmp = __raw_readl(S5P_ARM_CORE_CONFIGURATION(cpu));
		tmp &= ~(S5P_USE_DELAYED_RESET_ASSERTION);
		__raw_writel(tmp, S5P_ARM_CORE_CONFIGURATION(cpu));
	}
	*/
}

static inline void platform_do_lowpower(unsigned int cpu, int *spurious)
{
	for (;;) {
		/* make cpu1 to be turned off at next WFI command */
		if ((cpu >= 1) && (cpu < NR_CPUS)) {
			__raw_writel(0, S5P_ARM_CORE_CONFIGURATION(cpu));
			/*
			 * leehc add
				On Exynos4 USE_DELAYED_RESET_ASSERTION must be set in
				ARM_COREx_OPTION register during CPU power down. This is the proper way
				of powering down CPU.
				
				Additionally without this the CPU clock down won't work after powering
				down some other CPU and online CPUs will work at full frequency chosen
				by CPUfreq governor.
			*/

			/*
				unsigned int tmp;						
				tmp = __raw_readl(S5P_ARM_CORE_CONFIGURATION(cpu));
				tmp |= S5P_USE_DELAYED_RESET_ASSERTION;
				__raw_writel(tmp, S5P_ARM_CORE_CONFIGURATION(cpu));
				*/
			
		}
		 
		/*	
		 * here's the WFI
		 */
		asm(".word	0xe320f003\n"
		    :
		    :
		    : "memory", "cc");

		if (pen_release == cpu) {
			/*
			 * OK, proper wakeup, we're done
			 */
			break;
		}

		/*
		 * Getting here, means that we have come out of WFI without
		 * having been woken up - this shouldn't happen
		 *
		 * Just note it happening - when we're woken, we can report
		 * its occurrence.
		 */
		(*spurious)++;
	}
}

int platform_cpu_kill(unsigned int cpu)
{
	return 1;
}

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void platform_cpu_die(unsigned int cpu)
{
	int spurious = 0;

	/*
	 * we're ready for shutdown now, so do it
	 */
	if (soc_is_exynos5250())
		cpu_enter_lowpower_a15();
	else
		cpu_enter_lowpower_a9();
	platform_do_lowpower(cpu, &spurious);

	/*
	 * bring this CPU back into the world of cache
	 * coherency, and then restore interrupts
	 */
	 //leehc
	 
	/*
	 * leehc add
		On Exynos4 USE_DELAYED_RESET_ASSERTION must be set in
		ARM_COREx_OPTION register during CPU power down. This is the proper way
		of powering down CPU.
		
		Additionally without this the CPU clock down won't work after powering
		down some other CPU and online CPUs will work at full frequency chosen
		by CPUfreq governor.
	*/
	cpu_leave_lowpower(cpu);

	if (spurious)
		pr_warn("CPU%u: %u spurious wakeup calls\n", cpu, spurious);
}

int platform_cpu_disable(unsigned int cpu)
{
	/*
	 * we don't allow CPU 0 to be shutdown (it is still too special
	 * e.g. clock tick interrupts)
	 */
	return cpu == 0 ? -EPERM : 0;
}
