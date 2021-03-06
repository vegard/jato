#ifndef JATO_EMIT_CODE_H
#define JATO_EMIT_CODE_H

struct compilation_unit;
struct jit_trampoline;
struct basic_block;
struct buffer;
struct insn;
struct vm_object;
struct vm_jni_env;

enum emitter_type {
	NO_OPERANDS = 1,
	SINGLE_OPERAND,
	TWO_OPERANDS,
	BRANCH,
};

struct emitter {
	void *emit_fn;
	enum emitter_type type;
};

extern struct emitter emitters[];

#define DECL_EMITTER(_insn_type, _fn, _emitter_type) \
	[_insn_type] = { .emit_fn = _fn, .type = _emitter_type }

extern void emit_prolog(struct buffer *, unsigned long);
extern void emit_trace_invoke(struct buffer *, struct compilation_unit *);
extern void emit_epilog(struct buffer *);
extern void emit_trampoline(struct compilation_unit *, void *, struct jit_trampoline *);
extern void emit_unwind(struct buffer *);
extern void emit_lock(struct buffer *, struct vm_object *);
extern void emit_lock_this(struct buffer *);
extern void emit_unlock(struct buffer *, struct vm_object *);
extern void emit_unlock_this(struct buffer *);
extern void emit_body(struct basic_block *, struct buffer *);
extern void backpatch_branch_target(struct buffer *buf, struct insn *insn,
				    unsigned long target_offset);
extern void emit_jni_trampoline(struct buffer *buf, struct vm_jni_env *jni_env,
				void *target);

#endif /* JATO_EMIT_CODE_H */
