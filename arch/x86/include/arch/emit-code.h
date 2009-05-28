#ifndef __X86_EMIT_CODE_H
#define __X86_EMIT_CODE_H

struct jit_trampoline;
struct compilation_unit;
struct basic_block;
struct buffer;

void emit_prolog(struct buffer *, unsigned long);
void emit_epilog(struct buffer *);
void emit_body(struct basic_block *, struct buffer *);
void emit_trampoline(struct compilation_unit *, void *, struct jit_trampoline*);
void emit_unwind(struct buffer *);
void emit_lock(struct buffer *, struct object *);
void emit_lock_this(struct buffer *);
void emit_unlock(struct buffer *, struct object *);
void emit_unlock_this(struct buffer *);

#endif /* __X86_EMIT_CODE */
