// SPDX-License-Identifier: GPL-2.0
/*
 * Common time service routines for parisc machines.
 * based on arch/loongarch/kernel/time.c
 *
 * Copyright (C) 2024 Helge Deller <deller@gmx.de>
 */
#include <linux/clockchips.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/sched_clock.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <asm/processor.h>

static u64 cr16_hz;			/* frequency of CPU cr16 register */
static unsigned long clocktick;

int time_keeper_id __read_mostly;	/* CPU used for timekeeping. */

static DEFINE_PER_CPU(struct clock_event_device, parisc_clockevent_device);

#define MONARCH_CLOCKTICK_FACTOR	10
static bool clock_use_monarch_cr16;
static unsigned long monarch_cr16_ticks;

/*
 * We keep time on PA-RISC Linux by using the Interval Timer which is
 * a pair of registers; one is read-only and one is write-only; both
 * accessed through CR16.  The read-only register is 32 or 64 bits wide,
 * and increments by 1 every CPU clock tick.  The architecture only
 * guarantees us a rate between 0.5 and 2, but all implementations use a
 * rate of 1.  The write-only register is 32-bits wide.  When the lowest
 * 32 bits of the read-only register compare equal to the write-only
 * register, it raises a maskable external interrupt.  Each processor has
 * an Interval Timer of its own and they are not synchronized.
 *
 * We want to generate an interrupt every 1/HZ seconds.  So we program
 * CR16 to interrupt every @clocktick cycles.  The it_value in cpu_data
 * is programmed with the intended time of the next tick.  We can be
 * held off for an arbitrarily long period of time by interrupts being
 * disabled, so we may miss one or more ticks.
 *
 * Note that on SMP machines the CR16 clocks are not synchronized between
 * the CPUs. So, on such machines CR16 can not be used as high-res clock
 * input for the monotonic clock_gettime64() syscall, which is why
 * clock_gettime64() delivers poor resolution on SMP when configured with
 * HZ=100 or HZ=250.  To work around this issue, let the CR16 timer
 * interrupt trigger MONARCH_CLOCKTICK_FACTOR times more often, and
 * increase the a cr16 counter everytime when interrupted. This counter is
 * then used instead of the local CPU cr16 counter to get higher resolution
 * than just using the jiffie-based timer.
 */
irqreturn_t __irq_entry timer_interrupt(int irq, void *dev_id)
{
	unsigned long now;
	unsigned long next_tick;
	unsigned long ticks_elapsed = 0;
	unsigned int cpu = smp_processor_id();
	struct cpuinfo_parisc *cpuinfo = &per_cpu(cpu_data, cpu);
	bool increase_monarch_tickrate = false;
	static int monarch_tickrate_counter;

static int parisc_timer_next_event(unsigned long delta, struct clock_event_device *evt)
{
	mtctl(mfctl(16) + delta, 16);

	/* let main clock tick faster if necessary */
	if (IS_ENABLED(CONFIG_SMP) &&
	    (IS_ENABLED(CONFIG_HZ_100) || IS_ENABLED(CONFIG_HZ_250)) &&
	    clock_use_monarch_cr16 && (cpu == time_keeper_id)) {
		increase_monarch_tickrate = true;
		cpt /= MONARCH_CLOCKTICK_FACTOR;
		monarch_tickrate_counter++;
	}

	/* Initialize next_tick to the old expected tick time. */
	next_tick = cpuinfo->it_value;

irqreturn_t timer_interrupt(int irq, void *data)
{
	int cpu = smp_processor_id();
	struct clock_event_device *cd;

	cd = &per_cpu(parisc_clockevent_device, cpu);
	if (clockevent_state_periodic(cd))
		parisc_timer_next_event(clocktick, cd);

	/* Go do system house keeping. */
	if (IS_ENABLED(CONFIG_SMP) && (cpu != time_keeper_id))
		ticks_elapsed = 0;
	else
		monarch_cr16_ticks++;
	if (!increase_monarch_tickrate ||
		monarch_tickrate_counter == MONARCH_CLOCKTICK_FACTOR) {
		legacy_timer_tick(ticks_elapsed);
		if (increase_monarch_tickrate)
			monarch_tickrate_counter = 0;
	}

	/* Skip clockticks on purpose if we know we would miss those.
	 * The new CR16 must be "later" than current CR16 otherwise
	 * itimer would not fire until CR16 wrapped - e.g 4 seconds
	 * later on a 1Ghz processor. We'll account for the missed
	 * ticks on the next timer interrupt.
	 * We want IT to fire modulo clocktick even if we miss/skip some.
	 * But those interrupts don't in fact get delivered that regularly.
	 *
	 * "next_tick - now" will always give the difference regardless
	 * if one or the other wrapped. If "now" is "bigger" we'll end up
	 * with a very large unsigned number.
	 */
	now = mfctl(16);
	while (next_tick - now > cpt)
		next_tick += cpt;

	/* Program the IT when to deliver the next interrupt.
	 * Only bottom 32-bits of next_tick are writable in CR16!
	 * Timer interrupt will be delivered at least a few hundred cycles
	 * after the IT fires, so if we are too close (<= 8000 cycles) to the
	 * next cycle, simply skip it.
	 */
	if (next_tick - now <= 8000)
		next_tick += cpt;
	mtctl(next_tick, 16);

	return IRQ_HANDLED;
}

static int parisc_set_state_oneshot(struct clock_event_device *evt)
{
	return parisc_timer_next_event(clocktick, evt);
}

static int parisc_set_state_periodic(struct clock_event_device *evt)
{
	return parisc_timer_next_event(clocktick, evt);
}

static int parisc_set_state_shutdown(struct clock_event_device *evt)
{
	return 0;
}

static u64 notrace read_cr16_sched_clock(void)
{
	return get_cycles();
}

static u64 notrace read_cr16(struct clocksource *cs)
{
	return get_cycles();
}

static void cr16_cs_mark_unstable(struct clocksource *cs)
{
	static bool cr16_unstable = false;

	if (cr16_unstable)
		return;

	cr16_unstable = true;
	pr_info("Marking cr16 unstable due to clocksource watchdog\n");
}

static struct clocksource clocksource_cr16 = {
	.name		= "cr16",
	.rating		= 300,
	.read		= read_cr16,
	.mask		= CLOCKSOURCE_MASK(BITS_PER_LONG),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS |
				CLOCK_SOURCE_VALID_FOR_HRES |
				CLOCK_SOURCE_MUST_VERIFY |
				CLOCK_SOURCE_VERIFY_PERCPU,
	.mark_unstable	= cr16_cs_mark_unstable,
};


void parisc_clockevent_init(void)
{
	unsigned int cpu = smp_processor_id();
	unsigned long min_delta = 0x600;	/* XXX */
	/* writing cr16 timeout is limited to 32bit even on 64-bit kernel: */
	unsigned long max_delta = UINT_MAX - min_delta;
	struct clock_event_device *cd;

	/*
	 * The cr16 interval timers are not synchronized across CPUs
	 * on older 64-bit SMP machines.
	 */
	if (cpu > 0 && !running_on_qemu && !parisc_requires_coherency())
		clocksource_mark_unstable(&clocksource_cr16);

	cd = &per_cpu(parisc_clockevent_device, cpu);
	cd->name = "cr16_clockevent";
	cd->features = CLOCK_EVT_FEAT_ONESHOT | CLOCK_EVT_FEAT_PERIODIC |
			CLOCK_EVT_FEAT_PERCPU;

	cd->irq = TIMER_IRQ;
	cd->rating = 320;
	cd->cpumask = cpumask_of(cpu);
	cd->set_state_oneshot = parisc_set_state_oneshot;
	cd->set_state_oneshot_stopped = parisc_set_state_shutdown;
	cd->set_state_periodic = parisc_set_state_periodic;
	cd->set_state_shutdown = parisc_set_state_shutdown;
	cd->set_next_event = parisc_timer_next_event;
	cd->event_handler = parisc_event_handler;

	clockevents_config_and_register(cd, cr16_hz, min_delta, max_delta);
}

unsigned long notrace profile_pc(struct pt_regs *regs)
{
	unsigned long pc = instruction_pointer(regs);

	if (regs->gr[0] & PSW_N)
		pc -= 4;

#ifdef CONFIG_SMP
	if (in_lock_functions(pc))
		pc = regs->gr[2];
#endif

	return pc;
}
EXPORT_SYMBOL(profile_pc);


/* clock source code */

static u64 notrace read_cr16(struct clocksource *cs)
{
	if (clock_use_monarch_cr16)
		return monarch_cr16_ticks;
	else
		return get_cycles();
}

static struct clocksource clocksource_cr16 = {
	.name			= "cr16",
	.rating			= 300,
	.read			= read_cr16,
	.mask			= CLOCKSOURCE_MASK(BITS_PER_LONG),
	.flags			= CLOCK_SOURCE_IS_CONTINUOUS,
};

void start_cpu_itimer(void)
{
	unsigned int cpu = smp_processor_id();
	unsigned long next_tick = mfctl(16) + clocktick;

	mtctl(next_tick, 16);		/* kick off Interval Timer (CR16) */

	per_cpu(cpu_data, cpu).it_value = next_tick;
}

#if IS_ENABLED(CONFIG_RTC_DRV_GENERIC)
static int rtc_generic_get_time(struct device *dev, struct rtc_time *tm)
{
	struct pdc_tod tod_data;

	memset(tm, 0, sizeof(*tm));
	if (pdc_tod_read(&tod_data) < 0)
		return -EOPNOTSUPP;

	/* we treat tod_sec as unsigned, so this can work until year 2106 */
	rtc_time64_to_tm(tod_data.tod_sec, tm);
	return 0;
}

static int rtc_generic_set_time(struct device *dev, struct rtc_time *tm)
{
	time64_t secs = rtc_tm_to_time64(tm);
	int ret;

	/* hppa has Y2K38 problem: pdc_tod_set() takes an u32 value! */
	ret = pdc_tod_set(secs, 0);
	if (ret != 0) {
		pr_warn("pdc_tod_set(%lld) returned error %d\n", secs, ret);
		if (ret == PDC_INVALID_ARG)
			return -EINVAL;
		return -EOPNOTSUPP;
	}

	return 0;
}

static const struct rtc_class_ops rtc_generic_ops = {
	.read_time = rtc_generic_get_time,
	.set_time = rtc_generic_set_time,
};

static int __init rtc_init(void)
{
	struct platform_device *pdev;

	pdev = platform_device_register_data(NULL, "rtc-generic", -1,
					     &rtc_generic_ops,
					     sizeof(rtc_generic_ops));

	return PTR_ERR_OR_ZERO(pdev);
}
device_initcall(rtc_init);
#endif

void read_persistent_clock64(struct timespec64 *ts)
{
	static struct pdc_tod tod_data;
	if (pdc_tod_read(&tod_data) == 0) {
		ts->tv_sec = tod_data.tod_sec;
		ts->tv_nsec = tod_data.tod_usec * 1000;
	} else {
		printk(KERN_ERR "Error reading tod clock\n");
	        ts->tv_sec = 0;
		ts->tv_nsec = 0;
	}
}

/*
 * timer interrupt and sched_clock() initialization
 */

void __init time_init(void)
{
	cr16_hz = 100 * PAGE0->mem_10msec;  /* Hz */
	clocktick = cr16_hz / HZ;

	printk("Cr16 XXXXXXXXXXX   %llu HZ,   clocktick %lu\n", cr16_hz, clocktick);

	/* register as sched_clock source */
	sched_clock_register(read_cr16_sched_clock, BITS_PER_LONG, cr16_hz);

static int __init init_cr16_clocksource(void)
{
	unsigned int cr16_hz = 100 * PAGE0->mem_10msec;

	/*
	 * The cr16 interval timers are not synchronized across CPUs.
	 */
	if (num_online_cpus() > 1 && !running_on_qemu) {
		clock_use_monarch_cr16 = true;
		clocksource_cr16.name = "cr16_monarch";
		cr16_hz = HZ;
		/* keep CONFIG_HZ, but let timer fire more often */
		if (IS_ENABLED(CONFIG_HZ_100) || IS_ENABLED(CONFIG_HZ_250))
			cr16_hz *= MONARCH_CLOCKTICK_FACTOR;
	}

	/* register at clocksource framework */
	clocksource_register_hz(&clocksource_cr16, cr16_hz);

	return 0;
}
