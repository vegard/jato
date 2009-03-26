#ifndef __PPC_INSTRUCTION_H
#define __PPC_INSTRUCTION_H

#include <stdbool.h>
#include <vm/list.h>
#include <arch/registers.h>
#include <jit/use-position.h>

struct var_info;

struct insn {
	unsigned long lir_pos;
        struct list_head insn_list_node;
};

#define for_each_insn(insn, insn_list) list_for_each_entry(insn, insn_list, insn_list_node)

void free_insn(struct insn *);

static inline unsigned long lir_position(struct use_position *reg)
{
	assert(!"oops");
}

static inline bool insn_defs(struct insn *insn, struct var_info *var)
{
	return false;
}

static inline bool insn_uses(struct insn *insn, struct var_info *var)
{
	return false;
}

static inline const char *reg_name(enum machine_reg reg)
{
	return "<unknown>";
}

/*
 * These functions are used by generic code to insert spill/reload
 * instructions.
 */

static inline struct insn *
spill_insn(struct var_info *var, struct stack_slot *slot)
{
	return NULL;
}

static inline struct insn *
reload_insn(struct stack_slot *slot, struct var_info *var)
{
	return NULL;
}

#endif /* __PPC_INSTRUCTION_H */