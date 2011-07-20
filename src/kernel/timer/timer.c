/**
 * @file
 * @brief Multi-threads Timer handling.
 *
 * @date 09.02.09
 * @author Andrey Baboshin
 * @author Ilia Vaprol
 */

#include <mem/misc/pool.h>
#include <hal/clock.h>
#include <lib/list.h>
#include <embox/unit.h>
#include <kernel/timer.h>
#include <kernel/thread/event.h>
#include <kernel/thread/sched.h>
#include <kernel/clock_source.h>

EMBOX_UNIT_INIT(timer_init);

const uint32_t clock_source = 1000;

static volatile uint32_t cnt_sys_time; /**< quantity ms after start system */

POOL_DEF(timer_pool, sys_tmr_t, TIMER_POOL_SZ);
static struct list_head *sys_timers_list;

uint32_t cnt_system_time(void) {
	return cnt_sys_time;
}

int init_timer(sys_tmr_t **ptimer, uint32_t ticks,
		TIMER_FUNC handle, void *param) {
	sys_tmr_t *new_timer = *ptimer;

	if (!handle || !new_timer) {
		return 1;
	}
	new_timer->is_preallocated = false;
	new_timer->cnt    = new_timer->load = ticks;
	new_timer->handle = handle;
	new_timer->param  = param;
	list_add_tail((struct list_head *) new_timer, sys_timers_list);
	if (ptimer) {
		*ptimer = new_timer;
	}
	return 0;
}

int set_timer(sys_tmr_t **ptimer, uint32_t ticks,
		TIMER_FUNC handle, void *param) {
	sys_tmr_t *new_timer;

	if (!handle || !(new_timer = (sys_tmr_t*) pool_alloc(&timer_pool))) {
		if (ptimer) {
			*ptimer = NULL;
		}
		return 1;
	}
	new_timer->is_preallocated = true;
	new_timer->cnt    = new_timer->load = ticks;
	new_timer->handle = handle;
	new_timer->param  = param;
	list_add_tail((struct list_head *) new_timer, sys_timers_list);
	if (ptimer) {
		*ptimer = new_timer;
	}
	return 0;
}

int close_timer(sys_tmr_t **ptimer) {
	if (ptimer) {
		list_del((struct list_head *) *ptimer);
		if ((*ptimer)->is_preallocated) {
			pool_free(&timer_pool, *ptimer);
		}
		*ptimer = NULL;
	}
	return 0;
}

/**
 * For each timer in the timers array do the following: if the timer is enable
 * and the counter of this timer is the zero then its initial value is assigned
 * to the counter and the function is executed.
 */
static void inc_sys_timers(void) {
	struct list_head *tmp, *tmp2;
	sys_tmr_t *tmr;

	list_for_each_safe(tmp, tmp2, sys_timers_list) {
		tmr = (sys_tmr_t*) tmp;
		if (0 == tmr->cnt--) {
			tmr->handle(tmr, tmr->param);
			tmr->cnt = tmr->load;
		}
	}
}

/**
 * Handling of the clock tick.
 */
void clock_tick_handler(int irq_num, void *dev_id) {
	cnt_sys_time++;
	inc_sys_timers();
}

/**
 * Initialization of the timer subsystem.
 *
 * @return 0 if success
 */
int timer_init(void) {
	cnt_sys_time = 0;
	clock_init();
	clock_setup(clock_source_get_HZ());
	sys_timers_list = clock_source_get_timers_list();
	return 0;
}
