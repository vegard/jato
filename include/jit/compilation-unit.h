#ifndef __JIT_COMPILATION_UNIT_H
#define __JIT_COMPILATION_UNIT_H

#include <jit/basic-block.h>

#include <vm/list.h>
#include <vm/stack.h>

#include <arch/stack-frame.h>
#include <arch/instruction.h>

#include <stdbool.h>
#include <pthread.h>

struct buffer;

struct compilation_unit {
	struct methodblock *method;
	struct list_head bb_list;
	struct basic_block *exit_bb;
	struct var_info *var_infos;
	unsigned long nr_vregs;
	struct buffer *objcode;
	bool is_compiled;
	pthread_mutex_t mutex;

	/* The mimic stack is used to simulate JVM operand stack at
	   compile-time.  See Section 2.2 ("Lazy code selection") of the paper
	   "Fast, Effective Code Generation in a Just-In-Time Java Compiler" by
	   Adl-Tabatabai et al (1998) for more in-depth explanation.  */
	struct stack *mimic_stack;

	/* The frame pointer for this method.  */
	struct var_info *frame_ptr;

	/* The stack pointer for this method.  */
	struct var_info *stack_ptr;

	/* The stack frame contains information of stack slots for stack-based
	   arguments, local variables, and spill/reload storage.  */
	struct stack_frame *stack_frame;
};

struct compilation_unit *alloc_compilation_unit(struct methodblock *);
int init_stack_slots(struct compilation_unit *cu);
void free_compilation_unit(struct compilation_unit *);
struct var_info *get_var(struct compilation_unit *);
struct var_info *get_fixed_var(struct compilation_unit *, enum machine_reg);
struct basic_block *find_bb(struct compilation_unit *, unsigned long);
unsigned long nr_bblocks(struct compilation_unit *);
void compute_insn_positions(struct compilation_unit *);

#define for_each_variable(var, var_list) for (var = var_list; var != NULL; var = var->next)

#endif
