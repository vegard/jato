#ifndef __JIT_COMPILER_H
#define __JIT_COMPILER_H

#include <jit/compilation-unit.h>
#include <jit/basic-block.h>
#include <vm/buffer.h>
#include <vm/stack.h>
#include <vm/vm.h>

#include <pthread.h>
#include <stdbool.h>

struct compilation_unit;
struct expression;
struct statement;
struct buffer;

struct fixup_site {
	/* Compilation unit to which relcall_insn belongs */
	struct compilation_unit *cu;
	/* We need this, because we don't have native pointer at
	   instruction selection */
	struct insn *relcall_insn;
	struct list_head fixup_list_node;
};

struct jit_trampoline {
	struct buffer *objcode;
	struct list_head fixup_site_list;
	/* This mutex is protecting operations on fixup_site_list */
	pthread_mutex_t mutex;
};

struct parse_context {
	struct compilation_unit *cu;
	struct basic_block *bb;

	struct bytecode_buffer *buffer;
	unsigned char *code;
	unsigned long offset;
	unsigned long code_size;
	unsigned char opc;
};

void convert_expression(struct parse_context *ctx, struct expression *expr);
void convert_statement(struct parse_context *ctx, struct statement *stmt);

int compile(struct compilation_unit *);
int analyze_control_flow(struct compilation_unit *);
int convert_to_ir(struct compilation_unit *);
int analyze_liveness(struct compilation_unit *);
int select_instructions(struct compilation_unit *cu);
int allocate_registers(struct compilation_unit *cu);
int insert_spill_reload_insns(struct compilation_unit *cu);
int emit_machine_code(struct compilation_unit *);
void *jit_magic_trampoline(struct compilation_unit *);

struct jit_trampoline *alloc_jit_trampoline(void);
struct jit_trampoline *build_jit_trampoline(struct compilation_unit *);
void free_jit_trampoline(struct jit_trampoline *);

struct fixup_site *alloc_fixup_site(void);
void free_fixup_site(struct fixup_site *);
void trampoline_add_fixup_site(struct jit_trampoline *, struct fixup_site *);
unsigned char *fixup_site_addr(struct fixup_site *);

bool is_jit_method(unsigned long eip);

void fixup_direct_calls(struct jit_trampoline *trampoline, unsigned long target);

extern bool opt_trace_method;
extern bool opt_trace_cfg;
extern bool opt_trace_tree_ir;
extern bool opt_trace_lir;
extern bool opt_trace_liveness;
extern bool opt_trace_regalloc;
extern bool opt_trace_machine_code;
extern bool opt_trace_magic_trampoline;
extern bool opt_trace_bytecode_offset;

void trace_magic_trampoline(struct compilation_unit *);
void trace_method(struct compilation_unit *);
void trace_cfg(struct compilation_unit *);
void trace_tree_ir(struct compilation_unit *);
void trace_lir(struct compilation_unit *);
void trace_liveness(struct compilation_unit *);
void trace_regalloc(struct compilation_unit *);
void trace_machine_code(struct compilation_unit *);

#endif
