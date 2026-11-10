#include <stdint.h>
#include <string.h>

#include "ready.h"
#include "unity.h"

static kestrel_tcb_t task_a;
static kestrel_tcb_t task_b;
static kestrel_tcb_t task_c;
static kestrel_tcb_t task_d;

static void reset_task(kestrel_tcb_t *tcb, uint8_t priority)
{
    memset(tcb, 0, sizeof(*tcb));
    tcb->priority = priority;
}

void setUp(void)
{
    reset_task(&task_a, 0);
    reset_task(&task_b, 0);
    reset_task(&task_c, 0);
    reset_task(&task_d, 0);
    ready_init();
}

void tearDown(void)
{
}

static void insert_abc(uint8_t priority)
{
    task_a.priority = priority;
    task_b.priority = priority;
    task_c.priority = priority;
    ready_insert(&task_a);
    ready_insert(&task_b);
    ready_insert(&task_c);
}

void test_empty_pick_returns_null(void)
{
    TEST_ASSERT_EQUAL_HEX32(0U, ready_get_mask());
    TEST_ASSERT_NULL(ready_pick());
}

void test_insert_sets_priority_bit(void)
{
    task_a.priority = 5;
    ready_insert(&task_a);

    TEST_ASSERT_EQUAL_HEX32(1U << 5, ready_get_mask());
}

void test_insert_priority_31_sets_top_bit(void)
{
    task_a.priority = 31;
    ready_insert(&task_a);

    TEST_ASSERT_EQUAL_HEX32(0x80000000U, ready_get_mask());
    TEST_ASSERT_EQUAL_PTR(&task_a, ready_pick());
}

void test_pick_returns_highest_priority(void)
{
    task_a.priority = 1;
    task_b.priority = 7;
    task_c.priority = 3;
    ready_insert(&task_a);
    ready_insert(&task_b);
    ready_insert(&task_c);

    TEST_ASSERT_EQUAL_PTR(&task_b, ready_pick());
    TEST_ASSERT_EQUAL_PTR(&task_b, ready_pick());
}

void test_pick_does_not_remove_task(void)
{
    task_a.priority = 4;
    ready_insert(&task_a);

    (void)ready_pick();

    TEST_ASSERT_EQUAL_HEX32(1U << 4, ready_get_mask());
    TEST_ASSERT_EQUAL_PTR(&task_a, ready_pick());
}

void test_single_insert_ring_points_to_itself(void)
{
    task_a.priority = 2;
    ready_insert(&task_a);

    TEST_ASSERT_EQUAL_PTR(&task_a, task_a.next);
    TEST_ASSERT_EQUAL_PTR(&task_a, task_a.prev);
}

void test_insert_three_links_ring(void)
{
    insert_abc(2);

    TEST_ASSERT_EQUAL_PTR(&task_b, task_a.next);
    TEST_ASSERT_EQUAL_PTR(&task_c, task_b.next);
    TEST_ASSERT_EQUAL_PTR(&task_a, task_c.next);

    TEST_ASSERT_EQUAL_PTR(&task_c, task_a.prev);
    TEST_ASSERT_EQUAL_PTR(&task_a, task_b.prev);
    TEST_ASSERT_EQUAL_PTR(&task_b, task_c.prev);
}

void test_round_robin_same_priority(void)
{
    insert_abc(2);

    TEST_ASSERT_EQUAL_PTR(&task_a, ready_pick());
    TEST_ASSERT_EQUAL_PTR(&task_b, ready_pick());
    TEST_ASSERT_EQUAL_PTR(&task_c, ready_pick());
    TEST_ASSERT_EQUAL_PTR(&task_a, ready_pick());
}

void test_different_priorities_separate_rings(void)
{
    task_a.priority = 1;
    task_b.priority = 2;
    ready_insert(&task_a);
    ready_insert(&task_b);

    TEST_ASSERT_EQUAL_PTR(&task_a, task_a.next);
    TEST_ASSERT_EQUAL_PTR(&task_b, task_b.next);
    TEST_ASSERT_EQUAL_HEX32((1U << 1) | (1U << 2), ready_get_mask());
}

void test_remove_only_task_clears_bit(void)
{
    task_a.priority = 6;
    ready_insert(&task_a);

    ready_remove(&task_a);

    TEST_ASSERT_EQUAL_HEX32(0U, ready_get_mask());
    TEST_ASSERT_NULL(ready_pick());
}

void test_remove_only_task_keeps_other_bits(void)
{
    task_a.priority = 1;
    task_b.priority = 3;
    ready_insert(&task_a);
    ready_insert(&task_b);

    ready_remove(&task_b);

    TEST_ASSERT_EQUAL_HEX32(1U << 1, ready_get_mask());
    TEST_ASSERT_EQUAL_PTR(&task_a, ready_pick());
}

void test_remove_head_moves_head(void)
{
    insert_abc(2);

    ready_remove(&task_a);

    TEST_ASSERT_EQUAL_PTR(&task_b, ready_pick());
    TEST_ASSERT_EQUAL_PTR(&task_c, ready_pick());
    TEST_ASSERT_EQUAL_PTR(&task_b, ready_pick());
}

void test_remove_middle_relinks(void)
{
    insert_abc(2);

    ready_remove(&task_b);

    TEST_ASSERT_EQUAL_PTR(&task_c, task_a.next);
    TEST_ASSERT_EQUAL_PTR(&task_a, task_c.next);
    TEST_ASSERT_EQUAL_PTR(&task_c, task_a.prev);
    TEST_ASSERT_EQUAL_PTR(&task_a, task_c.prev);
}

void test_remove_clears_task_links(void)
{
    insert_abc(2);

    ready_remove(&task_b);

    TEST_ASSERT_NULL(task_b.next);
    TEST_ASSERT_NULL(task_b.prev);
}

void test_remove_then_reinsert(void)
{
    insert_abc(2);

    ready_remove(&task_a);
    ready_insert(&task_a);

    TEST_ASSERT_EQUAL_PTR(&task_b, ready_pick());
    TEST_ASSERT_EQUAL_PTR(&task_c, ready_pick());
    TEST_ASSERT_EQUAL_PTR(&task_a, ready_pick());
    TEST_ASSERT_EQUAL_PTR(&task_b, ready_pick());
}

void test_starvation_regression(void)
{
    kestrel_tcb_t *bg0 = &task_a;
    kestrel_tcb_t *bg1 = &task_b;
    kestrel_tcb_t *crit = &task_c;

    bg0->priority = 1;
    bg1->priority = 1;
    crit->priority = 3;
    ready_insert(bg0);
    ready_insert(bg1);

    for (int i = 0; i < 10; i++) {
        ready_insert(crit);
        TEST_ASSERT_EQUAL_PTR(crit, ready_pick());
        ready_remove(crit);

        kestrel_tcb_t *expected = (i % 2 == 0) ? bg0 : bg1;
        TEST_ASSERT_EQUAL_PTR(expected, ready_pick());
    }
}

void test_init_resets_state(void)
{
    task_a.priority = 1;
    task_d.priority = 9;
    ready_insert(&task_a);
    ready_insert(&task_d);

    ready_init();

    TEST_ASSERT_EQUAL_HEX32(0U, ready_get_mask());
    TEST_ASSERT_NULL(ready_pick());
}

static kestrel_tcb_t ns_tcb[4];

static void ns_add(uint32_t i, uint8_t priority)
{
    memset(&ns_tcb[i], 0, sizeof(ns_tcb[i]));
    ns_tcb[i].priority = priority;
    ns_tcb[i].state = KESTREL_TASK_READY;
    ready_insert(&ns_tcb[i]);
}

static void test_need_switch_empty_mask(void)
{
    ready_init();
    TEST_ASSERT_FALSE(ready_need_switch(3, false));
    TEST_ASSERT_FALSE(ready_need_switch(0, true));
}

static void test_need_switch_idle_with_ready_task(void)
{
    ready_init();
    ns_add(0, 1);
    TEST_ASSERT_TRUE(ready_need_switch(0, true));
}

static void test_need_switch_higher_priority_woke(void)
{
    ready_init();
    ns_add(0, 1);
    ns_add(1, 3);
    TEST_ASSERT_TRUE(ready_need_switch(1, false));
}

static void test_need_switch_alone_on_priority(void)
{
    ready_init();
    ns_add(0, 3);
    ns_add(1, 1);
    TEST_ASSERT_FALSE(ready_need_switch(3, false));
}

static void test_need_switch_round_robin(void)
{
    ready_init();
    ns_add(0, 3);
    ns_add(1, 3);
    TEST_ASSERT_TRUE(ready_need_switch(3, false));
}

static void test_need_switch_only_lower_ready(void)
{
    ready_init();
    ns_add(0, 1);
    TEST_ASSERT_FALSE(ready_need_switch(3, false));
}

static void test_need_switch_after_remove_back_to_alone(void)
{
    ready_init();
    ns_add(0, 3);
    ns_add(1, 3);
    ready_remove(&ns_tcb[1]);
    TEST_ASSERT_FALSE(ready_need_switch(3, false));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_empty_pick_returns_null);
    RUN_TEST(test_insert_sets_priority_bit);
    RUN_TEST(test_insert_priority_31_sets_top_bit);
    RUN_TEST(test_pick_returns_highest_priority);
    RUN_TEST(test_pick_does_not_remove_task);

    RUN_TEST(test_single_insert_ring_points_to_itself);
    RUN_TEST(test_insert_three_links_ring);
    RUN_TEST(test_round_robin_same_priority);
    RUN_TEST(test_different_priorities_separate_rings);

    RUN_TEST(test_remove_only_task_clears_bit);
    RUN_TEST(test_remove_only_task_keeps_other_bits);
    RUN_TEST(test_remove_head_moves_head);
    RUN_TEST(test_remove_middle_relinks);
    RUN_TEST(test_remove_clears_task_links);
    RUN_TEST(test_remove_then_reinsert);

    RUN_TEST(test_starvation_regression);
    RUN_TEST(test_init_resets_state);

    RUN_TEST(test_need_switch_empty_mask);
    RUN_TEST(test_need_switch_idle_with_ready_task);
    RUN_TEST(test_need_switch_higher_priority_woke);
    RUN_TEST(test_need_switch_alone_on_priority);
    RUN_TEST(test_need_switch_round_robin);
    RUN_TEST(test_need_switch_only_lower_ready);
    RUN_TEST(test_need_switch_after_remove_back_to_alone);
    return UNITY_END();
}
