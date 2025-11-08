#pragma once
#include <linux/types.h>
#include <linux/atomic.h>

#define CMPE283_DUMP_INTERVAL 10000

extern atomic64_t cmpe283_exit_count;

void cmpe283_count_exit(u32 reason);
void cmpe283_maybe_dump(void);
