#include "sched.h"

typedef struct sched_elem_s sched_elem_t;

struct sched_elem_s {
    const node_t* node;
    bool insert;
};

VEC(sched_stack, sched_elem_t)

void schedule_node(const node_t* node, node_vec_t* sched) {
    node_set_t done = node_set_create(64);
    sched_stack_t stack = sched_stack_create(64);
    sched_stack_push(&stack, (sched_elem_t) {
        .node = node,
        .insert = false
    });
    while (stack.nelems > 0) {
        const sched_elem_t elem = sched_stack_pop(&stack);
        const node_t* node = elem.node;
        if (elem.insert) {
            node_vec_push(sched, node);
        } else if (node->tag != NODE_PARAM &&
                   node->tag != NODE_FN &&
                   node->tag != NODE_LITERAL &&
                   node_set_insert(&done, node)) {
            sched_stack_push(&stack, (sched_elem_t) {
                .node = node,
                .insert = true
            });
            for (size_t i = 0; i < node->nops; ++i) {
                const node_t* op = node->ops[i];
                if (!node_set_lookup(&done, op)) {
                    sched_stack_push(&stack, (sched_elem_t) {
                        .node = op,
                        .insert = false
                    });
                }
            }
        }
    }
    node_set_destroy(&done);
    sched_stack_destroy(&stack);
}
