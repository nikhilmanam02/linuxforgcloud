#include <linux/atomic.h>
#include <linux/printk.h>
#include "exit_stats.h"

#define MAX_VMX_EXIT 512

atomic64_t cmpe283_exit_count = ATOMIC_INIT(0);
static atomic64_t reason_cnt[MAX_VMX_EXIT];

void cmpe283_count_exit(u32 reason)
{
        if (reason < MAX_VMX_EXIT)
                atomic64_inc(&reason_cnt[reason]);
}

static void cmpe283_dump_now(void)
{
        long long total = atomic64_read(&cmpe283_exit_count);

        pr_info("CMPE283: Exit dump (total=%lld)\n", total);
        for (int i = 0; i < MAX_VMX_EXIT; i++) {
                unsigned long long c =
                        (unsigned long long)atomic64_read(&reason_cnt[i]);
                if (c)
                        pr_info("CMPE283: reason %d -> %llu\n", i, c);
        }
}

void cmpe283_maybe_dump(void)
{
        long long n = atomic64_inc_return(&cmpe283_exit_count);
        if (n % CMPE283_DUMP_INTERVAL == 0)
                cmpe283_dump_now();
}
