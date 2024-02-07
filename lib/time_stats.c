// SPDX-License-Identifier: GPL-2.0

#include <linux/eytzinger.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/time.h>
#include <linux/time_stats.h>
#include <linux/spinlock.h>

static const struct time_unit time_units[] = {
	{ "ns",		1		 },
	{ "us",		NSEC_PER_USEC	 },
	{ "ms",		NSEC_PER_MSEC	 },
	{ "s",		NSEC_PER_SEC	 },
	{ "m",          (u64) NSEC_PER_SEC * 60},
	{ "h",          (u64) NSEC_PER_SEC * 3600},
	{ "eon",        U64_MAX          },
};

const struct time_unit *pick_time_units(u64 ns)
{
	const struct time_unit *u;

	for (u = time_units;
	     u + 1 < time_units + ARRAY_SIZE(time_units) &&
	     ns >= u[1].nsecs << 1;
	     u++)
		;

	return u;
}
EXPORT_SYMBOL_GPL(pick_time_units);

static void quantiles_update(struct quantiles *q, u64 v)
{
	unsigned i = 0;

	while (i < ARRAY_SIZE(q->entries)) {
		struct quantile_entry *e = q->entries + i;

		if (unlikely(!e->step)) {
			e->m = v;
			e->step = max_t(unsigned, v / 2, 1024);
		} else if (e->m > v) {
			e->m = e->m >= e->step
				? e->m - e->step
				: 0;
		} else if (e->m < v) {
			e->m = e->m + e->step > e->m
				? e->m + e->step
				: U32_MAX;
		}

		if ((e->m > v ? e->m - v : v - e->m) < e->step)
			e->step = max_t(unsigned, e->step / 2, 1);

		if (v >= e->m)
			break;

		i = eytzinger0_child(i, v > e->m);
	}
}

static inline void time_stats_update_one(struct time_stats *stats,
					      u64 start, u64 end)
{
	u64 duration, freq;

	if (time_after64(end, start)) {
		duration = end - start;
		mean_and_variance_update(&stats->duration_stats, duration);
		mean_and_variance_weighted_update(&stats->duration_stats_weighted, duration);
		stats->max_duration = max(stats->max_duration, duration);
		stats->min_duration = min(stats->min_duration, duration);
		stats->total_duration += duration;

		if (stats->quantiles_enabled)
			quantiles_update(&stats->quantiles, duration);
	}

	if (stats->last_event && time_after64(end, stats->last_event)) {
		freq = end - stats->last_event;
		mean_and_variance_update(&stats->freq_stats, freq);
		mean_and_variance_weighted_update(&stats->freq_stats_weighted, freq);
		stats->max_freq = max(stats->max_freq, freq);
		stats->min_freq = min(stats->min_freq, freq);
	}

	stats->last_event = end;
}

void __time_stats_clear_buffer(struct time_stats *stats,
			       struct time_stat_buffer *b)
{
	for (struct time_stat_buffer_entry *i = b->entries;
	     i < b->entries + ARRAY_SIZE(b->entries);
	     i++)
		time_stats_update_one(stats, i->start, i->end);
	b->nr = 0;
}
EXPORT_SYMBOL_GPL(__time_stats_clear_buffer);

static noinline void time_stats_clear_buffer(struct time_stats *stats,
					     struct time_stat_buffer *b)
{
	unsigned long flags;

	spin_lock_irqsave(&stats->lock, flags);
	__time_stats_clear_buffer(stats, b);
	spin_unlock_irqrestore(&stats->lock, flags);
}

void __time_stats_update(struct time_stats *stats, u64 start, u64 end)
{
	unsigned long flags;

	WARN_ONCE(!stats->duration_stats_weighted.weight ||
		  !stats->freq_stats_weighted.weight,
		  "uninitialized time_stats");

	if (!stats->buffer) {
		spin_lock_irqsave(&stats->lock, flags);
		time_stats_update_one(stats, start, end);

		if (mean_and_variance_weighted_get_mean(stats->freq_stats_weighted) < 32 &&
		    stats->duration_stats.n > 1024)
			stats->buffer =
				alloc_percpu_gfp(struct time_stat_buffer,
						 GFP_ATOMIC);
		spin_unlock_irqrestore(&stats->lock, flags);
	} else {
		struct time_stat_buffer *b;

		preempt_disable();
		b = this_cpu_ptr(stats->buffer);

		BUG_ON(b->nr >= ARRAY_SIZE(b->entries));
		b->entries[b->nr++] = (struct time_stat_buffer_entry) {
			.start = start,
			.end = end
		};

		if (unlikely(b->nr == ARRAY_SIZE(b->entries)))
			time_stats_clear_buffer(stats, b);
		preempt_enable();
	}
}
EXPORT_SYMBOL_GPL(__time_stats_update);

#include <linux/seq_buf.h>

static void seq_buf_time_units_aligned(struct seq_buf *out, u64 ns)
{
	const struct time_unit *u = pick_time_units(ns);

	seq_buf_printf(out, "%8llu %s", div64_u64(ns, u->nsecs), u->name);
}

void time_stats_to_seq_buf(struct seq_buf *out, struct time_stats *stats)
{
	s64 f_mean = 0, d_mean = 0;
	u64 f_stddev = 0, d_stddev = 0;

	if (stats->buffer) {
		int cpu;

		spin_lock_irq(&stats->lock);
		for_each_possible_cpu(cpu)
			__time_stats_clear_buffer(stats, per_cpu_ptr(stats->buffer, cpu));
		spin_unlock_irq(&stats->lock);
	}

	/*
	 * avoid divide by zero
	 */
	if (stats->freq_stats.n) {
		f_mean = mean_and_variance_get_mean(stats->freq_stats);
		f_stddev = mean_and_variance_get_stddev(stats->freq_stats);
		d_mean = mean_and_variance_get_mean(stats->duration_stats);
		d_stddev = mean_and_variance_get_stddev(stats->duration_stats);
	}

	seq_buf_printf(out, "count: %llu\n", stats->duration_stats.n);

	seq_buf_printf(out, "                       since mount        recent\n");

	seq_buf_printf(out, "duration of events\n");

	seq_buf_printf(out, "  min:                     ");
	seq_buf_time_units_aligned(out, stats->min_duration);
	seq_buf_printf(out, "\n");

	seq_buf_printf(out, "  max:                     ");
	seq_buf_time_units_aligned(out, stats->max_duration);
	seq_buf_printf(out, "\n");

	seq_buf_printf(out, "  total:                   ");
	seq_buf_time_units_aligned(out, stats->total_duration);
	seq_buf_printf(out, "\n");

	seq_buf_printf(out, "  mean:                    ");
	seq_buf_time_units_aligned(out, d_mean);
	seq_buf_time_units_aligned(out, mean_and_variance_weighted_get_mean(stats->duration_stats_weighted));
	seq_buf_printf(out, "\n");

	seq_buf_printf(out, "  stddev:                  ");
	seq_buf_time_units_aligned(out, d_stddev);
	seq_buf_time_units_aligned(out, mean_and_variance_weighted_get_stddev(stats->duration_stats_weighted));
	seq_buf_printf(out, "\n");

	seq_buf_printf(out, "time between events\n");

	seq_buf_printf(out, "  min:                     ");
	seq_buf_time_units_aligned(out, stats->min_freq);
	seq_buf_printf(out, "\n");

	seq_buf_printf(out, "  max:                     ");
	seq_buf_time_units_aligned(out, stats->max_freq);
	seq_buf_printf(out, "\n");

	seq_buf_printf(out, "  mean:                    ");
	seq_buf_time_units_aligned(out, f_mean);
	seq_buf_time_units_aligned(out, mean_and_variance_weighted_get_mean(stats->freq_stats_weighted));
	seq_buf_printf(out, "\n");

	seq_buf_printf(out, "  stddev:                  ");
	seq_buf_time_units_aligned(out, f_stddev);
	seq_buf_time_units_aligned(out, mean_and_variance_weighted_get_stddev(stats->freq_stats_weighted));
	seq_buf_printf(out, "\n");

	if (stats->quantiles_enabled) {
		int i = eytzinger0_first(NR_QUANTILES);
		const struct time_unit *u =
			pick_time_units(stats->quantiles.entries[i].m);
		u64 last_q = 0;

		seq_buf_printf(out, "quantiles (%s):\t", u->name);
		eytzinger0_for_each(i, NR_QUANTILES) {
			bool is_last = eytzinger0_next(i, NR_QUANTILES) == -1;

			u64 q = max(stats->quantiles.entries[i].m, last_q);
			seq_buf_printf(out, "%llu ", div_u64(q, u->nsecs));
			if (is_last)
				seq_buf_printf(out, "\n");
			last_q = q;
		}
	}
}
EXPORT_SYMBOL_GPL(time_stats_to_seq_buf);

void time_stats_exit(struct time_stats *stats)
{
	free_percpu(stats->buffer);
}
EXPORT_SYMBOL_GPL(time_stats_exit);

void time_stats_init(struct time_stats *stats)
{
	memset(stats, 0, sizeof(*stats));
	stats->duration_stats_weighted.weight = 8;
	stats->freq_stats_weighted.weight = 8;
	stats->min_duration = U64_MAX;
	stats->min_freq = U64_MAX;
	spin_lock_init(&stats->lock);
}
EXPORT_SYMBOL_GPL(time_stats_init);

MODULE_AUTHOR("Kent Overstreet");
MODULE_LICENSE("GPL");
