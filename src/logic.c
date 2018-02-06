#include "logic.h"

void dnf_compute_ors(const node_t* node, node_vec_t* ors) {
    if (node->tag == NODE_OR) {
        dnf_compute_ors(node->ops[0], ors);
        dnf_compute_ors(node->ops[1], ors);
    } else {
        node_vec_push(ors, node);
    }
}

static const node_t* dnf_convert_and(mod_t* mod, const node_t* left, const node_t* right, const node_t* res) {
    if (left->tag == NODE_OR) {
        return dnf_convert_and(mod, left->ops[1], right,
            dnf_convert_and(mod, left->ops[0], right, res));
    } else if (right->tag == NODE_OR) {
        return dnf_convert_and(mod, left, right->ops[1],
            dnf_convert_and(mod, left, right->ops[0], res));
    } else {
        return node_or(mod, res, node_and(mod, left, right, res->dbg), res->dbg);
    }
}

const node_t* dnf_convert(mod_t* mod, const node_t* node) {
    if (node->tag == NODE_OR) {
        return node_or(mod, dnf_convert(mod, node->ops[0]), dnf_convert(mod, node->ops[1]), node->dbg);
    } else if (node->tag == NODE_AND) {
        return dnf_convert_and(mod, dnf_convert(mod, node->ops[0]), dnf_convert(mod, node->ops[1]), node_i1(mod, false));
    } else if (node->tag == NODE_XOR) {
        if (node_is_not(node)) {
            const node_t* op = node->ops[1];
            if (op->tag == NODE_AND) {
                return dnf_convert(mod,
                    node_or(mod,
                        node_not(mod, op->ops[0], op->dbg),
                        node_not(mod, op->ops[1], op->dbg),
                        NULL)
                );
            } else if (op->tag == NODE_OR) {
                return dnf_convert(mod,
                    node_and(mod,
                        node_not(mod, op->ops[0], op->dbg),
                        node_not(mod, op->ops[1], op->dbg),
                        NULL)
                );
            } else {
                return node;
            }
        }
        // a ^ b => (a & ~b) | (~a & b)
        return dnf_convert(mod,
            node_or(mod,
                node_and(mod, node->ops[0], node_not(mod, node->ops[1], node->dbg), node->dbg),
                node_and(mod, node_not(mod, node->ops[0], node->dbg), node->ops[1], node->dbg),
                node->dbg)
            );
    } else if (node->tag == NODE_CMPEQ && node->ops[0]->type->tag == TYPE_I1) {
        // a == b => (a & b) | (~a & ~b)
        return dnf_convert(mod,
            node_or(mod,
                node_and(mod, node->ops[0], node->ops[1], node->dbg),
                node_and(mod,
                    node_not(mod, node->ops[0], node->dbg),
                    node_not(mod, node->ops[1], node->dbg),
                    node->dbg),
                node->dbg)
            );
    } else {
        return node;
    }
}

void cnf_compute_ands(const node_t* node, node_vec_t* ands) {
    if (node->tag == NODE_AND) {
        cnf_compute_ands(node->ops[0], ands);
        cnf_compute_ands(node->ops[1], ands);
    } else {
        node_vec_push(ands, node);
    }
}

static const node_t* cnf_convert_or(mod_t* mod, const node_t* left, const node_t* right, const node_t* res) {
    if (left->tag == NODE_AND) {
        return cnf_convert_or(mod, left->ops[1], right,
            cnf_convert_or(mod, left->ops[0], right, res));
    } else if (right->tag == NODE_AND) {
        return cnf_convert_or(mod, left, right->ops[1],
            cnf_convert_or(mod, left, right->ops[0], res));
    } else {
        return node_and(mod, res, node_or(mod, left, right, res->dbg), res->dbg);
    }
}

const node_t* cnf_convert(mod_t* mod, const node_t* node) {
    if (node->tag == NODE_AND) {
        return node_and(mod, cnf_convert(mod, node->ops[0]), cnf_convert(mod, node->ops[1]), node->dbg);
    } else if (node->tag == NODE_OR) {
        return cnf_convert_or(mod, cnf_convert(mod, node->ops[0]), cnf_convert(mod, node->ops[1]), node_i1(mod, true));
    } else if (node->tag == NODE_XOR) {
        if (node_is_not(node)) {
            const node_t* op = node->ops[1];
            if (op->tag == NODE_AND) {
                return dnf_convert(mod,
                    node_or(mod,
                        node_not(mod, op->ops[0], op->dbg),
                        node_not(mod, op->ops[1], op->dbg),
                        NULL)
                );
            } else if (op->tag == NODE_OR) {
                return dnf_convert(mod,
                    node_and(mod,
                        node_not(mod, op->ops[0], op->dbg),
                        node_not(mod, op->ops[1], op->dbg),
                        NULL)
                );
            } else {
                return node;
            }
        }
        // a ^ b => (~a | ~b) & (a | b)
        return cnf_convert(mod,
            node_and(mod,
                node_or(mod,
                    node_not(mod, node->ops[0], node->dbg),
                    node_not(mod, node->ops[1], node->dbg),
                    node->dbg),
                node_or(mod, node->ops[0], node->ops[1], node->dbg),
                node->dbg)
            );
    } else if (node->tag == NODE_CMPEQ && node->ops[0]->type->tag == TYPE_I1) {
        // a == b => (a | ~b) & (b | ~a)
        return cnf_convert(mod,
            node_and(mod,
                node_or(mod, node->ops[0], node_not(mod, node->ops[1], node->dbg), node->dbg),
                node_or(mod, node_not(mod, node->ops[0], node->dbg), node->ops[1], node->dbg),
                node->dbg)
            );
    } else {
        return node;
    }
}
