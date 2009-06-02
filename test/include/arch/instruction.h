#ifndef __ARCH_INSTRUCTION_H
#define __ARCH_INSTRUCTION_H

#include <arch/registers.h>
#include <jit/basic-block.h>
#include <jit/use-position.h>
#include <jit/vars.h>

#include <stdbool.h>

struct stack_slot;

/*
 * This is MMIX.  See the following URL for details:
 *
 *   http://www-cs-staff.stanford.edu/~uno/mmop.html
 */

enum operand_type {
	OPERAND_NONE,
	OPERAND_BRANCH,
	OPERAND_IMM,
	OPERAND_LOCAL,
	OPERAND_REG,
};

struct operand {
	enum operand_type	type;
	union {
		struct use_position	reg;
		unsigned long		imm;
		struct stack_slot	*slot; /* frame pointer + displacement */
		struct basic_block	*branch_target;
	};
};

enum insn_type {
	INSN_ADD,
	INSN_JMP,
	INSN_SETL,
	INSN_LD_LOCAL,	/* X = destination register, Y = source stack slot */
	INSN_ST_LOCAL,	/* X = source reg, Y = destination stack slot */
};

struct insn {
	enum insn_type type;
        union {
		struct operand operands[3];
                struct {
                        struct operand x, y, z;
                };
                struct operand operand;
        };
        struct list_head insn_list_node;
	/* Position of this instruction in LIR.  */
	unsigned long		lir_pos;
	/* Offset in machine code.  */
	unsigned long mach_offset;
	unsigned long bytecode_offset;
};

static inline unsigned long lir_position(struct use_position *reg)
{
	return reg->insn->lir_pos;
}

struct insn *arithmetic_insn(enum insn_type, struct var_info *, struct var_info *, struct var_info *);
struct insn *imm_insn(enum insn_type, unsigned long, struct var_info *);
struct insn *branch_insn(enum insn_type, struct basic_block *);
struct insn *st_insn(enum insn_type, struct var_info *, struct stack_slot *);
struct insn *ld_insn(enum insn_type, struct stack_slot *, struct var_info *);

/*
 * These functions are used by generic code to insert spill/reload
 * instructions.
 */

static inline struct insn *
spill_insn(struct var_info *var, struct stack_slot *slot)
{
	return st_insn(INSN_ST_LOCAL, var, slot);
}

static inline struct insn *
reload_insn(struct stack_slot *slot, struct var_info *var)
{
	return ld_insn(INSN_LD_LOCAL, slot, var);
}

static inline struct insn *
exception_spill_insn(struct stack_slot *slot)
{
	return NULL;
}

struct insn *alloc_insn(enum insn_type);
void free_insn(struct insn *);

bool insn_defs(struct insn *, struct var_info *);
bool insn_uses(struct insn *, struct var_info *);

#define for_each_insn(insn, insn_list) list_for_each_entry(insn, insn_list, insn_list_node)

#endif /* __ARCH_INSTRUCTION_H */
