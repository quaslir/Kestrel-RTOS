#include <stdint.h>
#include <string.h>

#include "unity.h"
#include "wait_list.h"

static kestrel_wait_list_t list;
static kestrel_wait_list_t other_list;

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
    wait_list_init(&list);
    wait_list_init(&other_list);
}

void tearDown(void)
{
}

static void insert_abc_same_priority(uint8_t priority)
{
    task_a.priority = priority;
    task_b.priority = priority;
    task_c.priority = priority;
    wait_list_insert(&list, &task_a);
    wait_list_insert(&list, &task_b);
    wait_list_insert(&list, &task_c);
}

static void assert_links_cleared(const kestrel_tcb_t *tcb)
{
    TEST_ASSERT_NULL(tcb->next);
    TEST_ASSERT_NULL(tcb->prev);
    TEST_ASSERT_NULL(tcb->waiting_on);
}

/* ---------- пустой список ---------- */

void test_init_makes_empty_list(void)
{
    TEST_ASSERT_TRUE(wait_list_is_empty(&list));
    TEST_ASSERT_NULL(list.head);
}

void test_pop_empty_returns_null(void)
{
    TEST_ASSERT_NULL(wait_list_pop(&list));
    TEST_ASSERT_TRUE(wait_list_is_empty(&list));
}

/* ---------- вставка ---------- */

void test_insert_single_becomes_head(void)
{
    task_a.priority = 2;
    wait_list_insert(&list, &task_a);

    TEST_ASSERT_FALSE(wait_list_is_empty(&list));
    TEST_ASSERT_EQUAL_PTR(&task_a, list.head);
    TEST_ASSERT_NULL(task_a.prev);
    TEST_ASSERT_NULL(task_a.next);
}

void test_insert_sets_waiting_on(void)
{
    task_a.priority = 1;
    task_b.priority = 1;
    wait_list_insert(&list, &task_a);
    wait_list_insert(&other_list, &task_b);

    TEST_ASSERT_EQUAL_PTR(&list, task_a.waiting_on);
    TEST_ASSERT_EQUAL_PTR(&other_list, task_b.waiting_on);
}

void test_insert_more_important_becomes_head(void)
{
    task_a.priority = 1;
    task_b.priority = 3;
    wait_list_insert(&list, &task_a);
    wait_list_insert(&list, &task_b);

    TEST_ASSERT_EQUAL_PTR(&task_b, list.head);
    TEST_ASSERT_NULL(task_b.prev);
    TEST_ASSERT_EQUAL_PTR(&task_a, task_b.next);
    TEST_ASSERT_EQUAL_PTR(&task_b, task_a.prev);
    TEST_ASSERT_NULL(task_a.next);
}

void test_insert_into_middle_links_both_sides(void)
{
    task_a.priority = 3;
    task_b.priority = 1;
    task_c.priority = 2;
    wait_list_insert(&list, &task_a);
    wait_list_insert(&list, &task_b);
    wait_list_insert(&list, &task_c);

    TEST_ASSERT_EQUAL_PTR(&task_a, list.head);
    TEST_ASSERT_EQUAL_PTR(&task_c, task_a.next);
    TEST_ASSERT_EQUAL_PTR(&task_b, task_c.next);
    TEST_ASSERT_NULL(task_b.next);

    TEST_ASSERT_NULL(task_a.prev);
    TEST_ASSERT_EQUAL_PTR(&task_a, task_c.prev);
    TEST_ASSERT_EQUAL_PTR(&task_c, task_b.prev);
}

void test_insert_less_important_goes_to_tail(void)
{
    task_a.priority = 3;
    task_b.priority = 1;
    wait_list_insert(&list, &task_a);
    wait_list_insert(&list, &task_b);

    TEST_ASSERT_EQUAL_PTR(&task_a, list.head);
    TEST_ASSERT_EQUAL_PTR(&task_b, task_a.next);
    TEST_ASSERT_EQUAL_PTR(&task_a, task_b.prev);
    TEST_ASSERT_NULL(task_b.next);
}

/* ---------- порядок pop ---------- */

void test_pop_order_by_priority(void)
{
    task_a.priority = 1;
    task_b.priority = 3;
    task_c.priority = 2;
    wait_list_insert(&list, &task_a);
    wait_list_insert(&list, &task_b);
    wait_list_insert(&list, &task_c);

    TEST_ASSERT_EQUAL_PTR(&task_b, wait_list_pop(&list));
    TEST_ASSERT_EQUAL_PTR(&task_c, wait_list_pop(&list));
    TEST_ASSERT_EQUAL_PTR(&task_a, wait_list_pop(&list));
    TEST_ASSERT_NULL(wait_list_pop(&list));
}

void test_pop_fifo_among_equal_priority(void)
{
    insert_abc_same_priority(1);

    TEST_ASSERT_EQUAL_PTR(&task_a, wait_list_pop(&list));
    TEST_ASSERT_EQUAL_PTR(&task_b, wait_list_pop(&list));
    TEST_ASSERT_EQUAL_PTR(&task_c, wait_list_pop(&list));
}

void test_pop_mixed_priorities_and_fifo(void)
{
    kestrel_tcb_t *crit = &task_a;
    kestrel_tcb_t *bg0 = &task_b;
    kestrel_tcb_t *led = &task_c;
    kestrel_tcb_t *bg1 = &task_d;

    crit->priority = 3;
    bg0->priority = 1;
    led->priority = 2;
    bg1->priority = 1;

    wait_list_insert(&list, crit);
    wait_list_insert(&list, bg0);
    wait_list_insert(&list, led);
    wait_list_insert(&list, bg1);

    TEST_ASSERT_EQUAL_PTR(crit, wait_list_pop(&list));
    TEST_ASSERT_EQUAL_PTR(led, wait_list_pop(&list));
    TEST_ASSERT_EQUAL_PTR(bg0, wait_list_pop(&list));
    TEST_ASSERT_EQUAL_PTR(bg1, wait_list_pop(&list));
    TEST_ASSERT_TRUE(wait_list_is_empty(&list));
}

void test_pop_fixes_new_head_prev(void)
{
    insert_abc_same_priority(1);

    (void)wait_list_pop(&list);

    TEST_ASSERT_EQUAL_PTR(&task_b, list.head);
    TEST_ASSERT_NULL(task_b.prev);
}

void test_pop_clears_links(void)
{
    insert_abc_same_priority(1);

    kestrel_tcb_t *popped = wait_list_pop(&list);

    assert_links_cleared(popped);
}

/* ---------- удаление ---------- */

void test_remove_only_task_empties_list(void)
{
    task_a.priority = 1;
    wait_list_insert(&list, &task_a);

    wait_list_remove(&list, &task_a);

    TEST_ASSERT_TRUE(wait_list_is_empty(&list));
    assert_links_cleared(&task_a);
}

void test_remove_head(void)
{
    insert_abc_same_priority(1);

    wait_list_remove(&list, &task_a);

    TEST_ASSERT_EQUAL_PTR(&task_b, list.head);
    TEST_ASSERT_NULL(task_b.prev);
    TEST_ASSERT_EQUAL_PTR(&task_c, task_b.next);
    assert_links_cleared(&task_a);
}

void test_remove_middle(void)
{
    insert_abc_same_priority(1);

    wait_list_remove(&list, &task_b);

    TEST_ASSERT_EQUAL_PTR(&task_a, list.head);
    TEST_ASSERT_EQUAL_PTR(&task_c, task_a.next);
    TEST_ASSERT_EQUAL_PTR(&task_a, task_c.prev);
    assert_links_cleared(&task_b);
}

void test_remove_tail(void)
{
    insert_abc_same_priority(1);

    wait_list_remove(&list, &task_c);

    TEST_ASSERT_EQUAL_PTR(&task_b, task_a.next);
    TEST_ASSERT_NULL(task_b.next);
    assert_links_cleared(&task_c);
}

void test_remove_then_reinsert_goes_after_equals(void)
{
    insert_abc_same_priority(1);

    wait_list_remove(&list, &task_a);
    wait_list_insert(&list, &task_a);

    TEST_ASSERT_EQUAL_PTR(&task_b, wait_list_pop(&list));
    TEST_ASSERT_EQUAL_PTR(&task_c, wait_list_pop(&list));
    TEST_ASSERT_EQUAL_PTR(&task_a, wait_list_pop(&list));
}

void test_lists_are_independent(void)
{
    task_a.priority = 2;
    task_b.priority = 2;
    wait_list_insert(&list, &task_a);
    wait_list_insert(&other_list, &task_b);

    wait_list_remove(&list, &task_a);

    TEST_ASSERT_TRUE(wait_list_is_empty(&list));
    TEST_ASSERT_EQUAL_PTR(&task_b, other_list.head);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_makes_empty_list);
    RUN_TEST(test_pop_empty_returns_null);

    RUN_TEST(test_insert_single_becomes_head);
    RUN_TEST(test_insert_sets_waiting_on);
    RUN_TEST(test_insert_more_important_becomes_head);
    RUN_TEST(test_insert_into_middle_links_both_sides);
    RUN_TEST(test_insert_less_important_goes_to_tail);

    RUN_TEST(test_pop_order_by_priority);
    RUN_TEST(test_pop_fifo_among_equal_priority);
    RUN_TEST(test_pop_mixed_priorities_and_fifo);
    RUN_TEST(test_pop_fixes_new_head_prev);
    RUN_TEST(test_pop_clears_links);

    RUN_TEST(test_remove_only_task_empties_list);
    RUN_TEST(test_remove_head);
    RUN_TEST(test_remove_middle);
    RUN_TEST(test_remove_tail);
    RUN_TEST(test_remove_then_reinsert_goes_after_equals);
    RUN_TEST(test_lists_are_independent);

    return UNITY_END();
}
