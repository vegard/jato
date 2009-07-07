/*
 * x86-32/x86-64 code emitter.
 *
 * Copyright (C) 2006  Pekka Enberg
 *
 * This file is released under the GPL version 2. Please refer to the file
 * LICENSE for details.
 */

#include "cafebabe/method_info.h"

#include "jit/basic-block.h"
#include "jit/statement.h"
#include "jit/compilation-unit.h"
#include "jit/compiler.h"
#include "jit/exception.h"
#include "jit/stack-slot.h"
#include "jit/emit-code.h"
#include "jit/text.h"

#include "lib/list.h"
#include "lib/buffer.h"

#include "vm/method.h"
#include "vm/object.h"

#include "arch/instruction.h"
#include "arch/memory.h"
#include "arch/stack-frame.h"
#include "arch/thread.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Aliases and prototypes to make common emitters work as expected. */
#ifdef CONFIG_X86_64
# define __emit_add_imm_reg		__emit64_add_imm_reg
# define __emit_pop_reg			__emit64_pop_reg
# define __emit_push_imm		__emit64_push_imm
# define __emit_push_membase		__emit64_push_membase
# define __emit_push_reg		__emit64_push_reg
# define __emit_mov_reg_reg		__emit64_mov_reg_reg
#endif

static unsigned char __encode_reg(enum machine_reg reg);
static void __emit_add_imm_reg(struct buffer *buf,
			       long imm,
			       enum machine_reg reg);
static void __emit_pop_reg(struct buffer *buf, enum machine_reg reg);
static void __emit_push_imm(struct buffer *buf, long imm);
static void __emit_push_membase(struct buffer *buf,
				enum machine_reg src_reg,
				unsigned long disp);
static void __emit_push_reg(struct buffer *buf, enum machine_reg reg);
static void __emit_mov_reg_reg(struct buffer *buf,
			       enum machine_reg src,
			       enum machine_reg dst);
static void emit_indirect_jump_reg(struct buffer *buf, enum machine_reg reg);
static void emit_exception_test(struct buffer *buf, enum machine_reg reg);
static void emit_restore_regs(struct buffer *buf);

/************************
 * Common code emitters *
 ************************/

#define PREFIX_SIZE 1
#define BRANCH_INSN_SIZE 5
#define BRANCH_TARGET_OFFSET 1

#define CALL_INSN_SIZE 5

#define PTR_SIZE	sizeof(long)

#define GENERIC_X86_EMITTERS \
	DECL_EMITTER(INSN_CALL_REL, emit_call, SINGLE_OPERAND),		\
	DECL_EMITTER(INSN_JE_BRANCH, emit_je_branch, BRANCH),		\
	DECL_EMITTER(INSN_JNE_BRANCH, emit_jne_branch, BRANCH),		\
	DECL_EMITTER(INSN_JGE_BRANCH, emit_jge_branch, BRANCH),		\
	DECL_EMITTER(INSN_JG_BRANCH, emit_jg_branch, BRANCH),		\
	DECL_EMITTER(INSN_JLE_BRANCH, emit_jle_branch, BRANCH),		\
	DECL_EMITTER(INSN_JL_BRANCH, emit_jl_branch, BRANCH),		\
	DECL_EMITTER(INSN_JMP_BRANCH, emit_jmp_branch, BRANCH),		\
	DECL_EMITTER(INSN_RET, emit_ret, NO_OPERANDS)

static unsigned char encode_reg(struct use_position *reg)
{
	return __encode_reg(mach_reg(reg));
}

static inline bool is_imm_8(long imm)
{
	return (imm >= -128) && (imm <= 127);
}

/**
 *	encode_modrm:	Encode a ModR/M byte of an IA-32 instruction.
 *	@mod: The mod field of the byte.
 *	@reg_opcode: The reg/opcode field of the byte.
 *	@rm: The r/m field of the byte.
 */
static inline unsigned char encode_modrm(unsigned char mod,
					 unsigned char reg_opcode,
					 unsigned char rm)
{
	return ((mod & 0x3) << 6) | ((reg_opcode & 0x7) << 3) | (rm & 0x7);
}

static inline unsigned char encode_sib(unsigned char scale,
				       unsigned char index, unsigned char base)
{
	return ((scale & 0x3) << 6) | ((index & 0x7) << 3) | (base & 0x7);
}

static inline void emit(struct buffer *buf, unsigned char c)
{
	int err;

	err = append_buffer(buf, c);
	assert(!err);
}

static void write_imm32(struct buffer *buf, unsigned long offset, long imm32)
{
	unsigned char *buffer;
	union {
		int val;
		unsigned char b[4];
	} imm_buf;

	buffer = buf->buf;
	imm_buf.val = imm32;

	buffer[offset] = imm_buf.b[0];
	buffer[offset + 1] = imm_buf.b[1];
	buffer[offset + 2] = imm_buf.b[2];
	buffer[offset + 3] = imm_buf.b[3];
}

static void emit_imm32(struct buffer *buf, int imm)
{
	union {
		int val;
		unsigned char b[4];
	} imm_buf;

	imm_buf.val = imm;
	emit(buf, imm_buf.b[0]);
	emit(buf, imm_buf.b[1]);
	emit(buf, imm_buf.b[2]);
	emit(buf, imm_buf.b[3]);
}

static void emit_imm(struct buffer *buf, long imm)
{
	if (is_imm_8(imm))
		emit(buf, imm);
	else
		emit_imm32(buf, imm);
}

static void __emit_call(struct buffer *buf, void *call_target)
{
	int disp = call_target - buffer_current(buf) - CALL_INSN_SIZE;

	emit(buf, 0xe8);
	emit_imm32(buf, disp);
}

static void emit_call(struct buffer *buf, struct operand *operand)
{
	__emit_call(buf, (void *)operand->rel);
}

static void emit_ret(struct buffer *buf)
{
	emit(buf, 0xc3);
}

static void emit_leave(struct buffer *buf)
{
	emit(buf, 0xc9);
}

static void emit_branch_rel(struct buffer *buf, unsigned char prefix,
			    unsigned char opc, long rel32)
{
	if (prefix)
		emit(buf, prefix);
	emit(buf, opc);
	emit_imm32(buf, rel32);
}

static long branch_rel_addr(struct insn *insn, unsigned long target_offset)
{
	long ret;

	ret = target_offset - insn->mach_offset - BRANCH_INSN_SIZE;
	if (insn->escaped)
		ret -= PREFIX_SIZE;

	return ret;
}

static void __emit_branch(struct buffer *buf, unsigned char prefix,
			  unsigned char opc, struct insn *insn)
{
	struct basic_block *target_bb;
	long addr = 0;

	if (prefix)
		insn->escaped = true;

	target_bb = insn->operand.branch_target;

	if (target_bb->is_emitted) {
		struct insn *target_insn =
		    list_first_entry(&target_bb->insn_list, struct insn,
			       insn_list_node);

		addr = branch_rel_addr(insn, target_insn->mach_offset);
	} else
		list_add(&insn->branch_list_node, &target_bb->backpatch_insns);

	emit_branch_rel(buf, prefix, opc, addr);
}

static void emit_je_branch(struct buffer *buf, struct insn *insn)
{
	__emit_branch(buf, 0x0f, 0x84, insn);
}

static void emit_jne_branch(struct buffer *buf, struct insn *insn)
{
	__emit_branch(buf, 0x0f, 0x85, insn);
}

static void emit_jge_branch(struct buffer *buf, struct insn *insn)
{
	__emit_branch(buf, 0x0f, 0x8d, insn);
}

static void emit_jg_branch(struct buffer *buf, struct insn *insn)
{
	__emit_branch(buf, 0x0f, 0x8f, insn);
}

static void emit_jle_branch(struct buffer *buf, struct insn *insn)
{
	__emit_branch(buf, 0x0f, 0x8e, insn);
}

static void emit_jl_branch(struct buffer *buf, struct insn *insn)
{
	__emit_branch(buf, 0x0f, 0x8c, insn);
}

static void emit_jmp_branch(struct buffer *buf, struct insn *insn)
{
	__emit_branch(buf, 0x00, 0xe9, insn);
}

void emit_lock(struct buffer *buf, struct vm_object *obj)
{
	__emit_push_imm(buf, (unsigned long)obj);
	__emit_call(buf, vm_object_lock);
	__emit_add_imm_reg(buf, PTR_SIZE, REG_xSP);

	__emit_push_reg(buf, REG_xAX);
	emit_exception_test(buf, REG_xAX);
	__emit_pop_reg(buf, REG_xAX);
}

void emit_unlock(struct buffer *buf, struct vm_object *obj)
{
	/* Save caller-saved registers which contain method's return value */
	__emit_push_reg(buf, REG_xAX);
	__emit_push_reg(buf, REG_xDX);

	__emit_push_imm(buf, (unsigned long)obj);
	__emit_call(buf, vm_object_unlock);
	__emit_add_imm_reg(buf, PTR_SIZE, REG_xSP);

	emit_exception_test(buf, REG_xAX);

	__emit_pop_reg(buf, REG_xDX);
	__emit_pop_reg(buf, REG_xAX);
}

void emit_lock_this(struct buffer *buf)
{
	unsigned long this_arg_offset;

	this_arg_offset = offsetof(struct jit_stack_frame, args);

	__emit_push_membase(buf, REG_xBP, this_arg_offset);
	__emit_call(buf, vm_object_lock);
	__emit_add_imm_reg(buf, PTR_SIZE, REG_xSP);

	__emit_push_reg(buf, REG_xAX);
	emit_exception_test(buf, REG_xAX);
	__emit_pop_reg(buf, REG_xAX);
}

void emit_unlock_this(struct buffer *buf)
{
	unsigned long this_arg_offset;

	this_arg_offset = offsetof(struct jit_stack_frame, args);

	/* Save caller-saved registers which contain method's return value */
	__emit_push_reg(buf, REG_xAX);
	__emit_push_reg(buf, REG_xDX);

	__emit_push_membase(buf, REG_xBP, this_arg_offset);
	__emit_call(buf, vm_object_unlock);
	__emit_add_imm_reg(buf, PTR_SIZE, REG_xSP);

	emit_exception_test(buf, REG_xAX);

	__emit_pop_reg(buf, REG_xDX);
	__emit_pop_reg(buf, REG_xAX);
}

void backpatch_branch_target(struct buffer *buf,
			     struct insn *insn,
			     unsigned long target_offset)
{
	unsigned long backpatch_offset;
	long relative_addr;

	backpatch_offset = insn->mach_offset + BRANCH_TARGET_OFFSET;
	if (insn->escaped)
		backpatch_offset += PREFIX_SIZE;

	relative_addr = branch_rel_addr(insn, target_offset);

	write_imm32(buf, backpatch_offset, relative_addr);
}

void emit_epilog(struct buffer *buf)
{
	emit_leave(buf);
	emit_restore_regs(buf);
	emit_ret(buf);
}

static void __emit_jmp(struct buffer *buf, unsigned long addr)
{
	unsigned long current = (unsigned long)buffer_current(buf);
	emit(buf, 0xE9);
	emit_imm32(buf, addr - current - BRANCH_INSN_SIZE);
}

void emit_unwind(struct buffer *buf)
{
	emit_leave(buf);
	emit_restore_regs(buf);
	__emit_jmp(buf, (unsigned long)&unwind);
}

void emit_trace_invoke(struct buffer *buf, struct compilation_unit *cu)
{
	__emit_push_imm(buf, (unsigned long) cu);
	__emit_call(buf, &trace_invoke);
	__emit_add_imm_reg(buf, PTR_SIZE, REG_xSP);
}

/*
 * This fixes relative calls generated by EXPR_INVOKE.
 *
 * Please note, that this code does not care about icache flushing in
 * MP environment. This may lead to a GPF when one CPU modifies code
 * already prefetched by another CPU on some bogus Intel CPUs (see
 * section 7.1.3 of "Intel 64 and IA-32 Architectures Software
 * Developers Manual Volume 3A"). It is required for other CPUs to
 * execute a serializing instruction (to flush instruction cache)
 * between modification and execution of new instruction. To achieve
 * this, we could suspend all threads before patching, and force them
 * to execute flush_icache() on resume.
 */
void fixup_direct_calls(struct jit_trampoline *t, unsigned long target)
{
	struct fixup_site *this, *next;

	pthread_mutex_lock(&t->mutex);

	list_for_each_entry_safe(this, next, &t->fixup_site_list,
				 fixup_list_node) {
		unsigned char *site_addr;
		uint32_t new_target;

		site_addr = fixup_site_addr(this);
		new_target = target - ((unsigned long) site_addr + CALL_INSN_SIZE);
		cpu_write_u32(site_addr+1, new_target);

		list_del(&this->fixup_list_node);
		free_fixup_site(this);
	}

	pthread_mutex_unlock(&t->mutex);
}

/*
 * This function replaces pointers in vtable so that they point
 * directly to compiled code instead of trampoline code.
 */
static void fixup_vtable(struct compilation_unit *cu,
	struct vm_object *objref, void *target)
{
	struct vm_class *vmc = objref->class;

	vmc->vtable.native_ptr[cu->method->virtual_index] = target;
}

void fixup_static(struct vm_class *vmc)
{
	struct static_fixup_site *this, *next;

	list_for_each_entry_safe(this, next,
		&vmc->static_fixup_site_list, vmc_node)
	{
		struct vm_field *vmf = this->vmf;
		void *site_addr = buffer_ptr(this->cu->objcode)
			+ this->insn->mach_offset;
		void *new_target = vmc->static_values + vmf->offset;

		cpu_write_u32(site_addr + 2, (unsigned long) new_target);

		list_del(&this->vmc_node);
		list_del(&this->cu_node);
		free(this);
	}
}

int fixup_static_at(unsigned long addr)
{
	struct compilation_unit *cu;
	struct static_fixup_site *this;

	cu = jit_lookup_cu(addr);
	assert(cu);

	list_for_each_entry(this, &cu->static_fixup_site_list, cu_node)
	{
		void *site_addr = buffer_ptr(cu->objcode)
			+ this->insn->mach_offset;

		if ((unsigned long) site_addr == addr) {
			int ret = vm_class_ensure_init(this->vmf->class);
			if (ret)
				return ret;

			fixup_static(this->vmf->class);
			return 0;
		}
	}

	return 0;
}

#ifdef CONFIG_X86_32

/************************
 * x86-32 code emitters *
 ************************/

/*
 *	__encode_reg:	Encode register to be used in IA-32 instruction.
 *	@reg: Register to encode.
 *
 *	Returns register in r/m or reg/opcode field format of the ModR/M byte.
 */
static unsigned char __encode_reg(enum machine_reg reg)
{
	unsigned char ret = 0;

	switch (reg) {
	case REG_EAX:
	case REG_XMM0:
		ret = 0x00;
		break;
	case REG_EBX:
	case REG_XMM3:
		ret = 0x03;
		break;
	case REG_ECX:
	case REG_XMM1:
		ret = 0x01;
		break;
	case REG_EDX:
	case REG_XMM2:
		ret = 0x02;
		break;
	case REG_ESI:
	case REG_XMM6:
		ret = 0x06;
		break;
	case REG_EDI:
	case REG_XMM7:
		ret = 0x07;
		break;
	case REG_ESP:
	case REG_XMM4:
		ret = 0x04;
		break;
	case REG_EBP:
	case REG_XMM5:
		ret = 0x05;
		break;
	case REG_UNASSIGNED:
		assert(!"unassigned register in code emission");
		break;
	}
	return ret;
}

static void
__emit_reg_reg(struct buffer *buf, unsigned char opc,
	       enum machine_reg direct_reg, enum machine_reg rm_reg)
{
	unsigned char mod_rm;

	mod_rm = encode_modrm(0x03, __encode_reg(direct_reg), __encode_reg(rm_reg));

	emit(buf, opc);
	emit(buf, mod_rm);
}

static void
emit_reg_reg(struct buffer *buf, unsigned char opc,	
	     struct operand *direct, struct operand *rm)
{
	enum machine_reg direct_reg, rm_reg;

	direct_reg = mach_reg(&direct->reg);
	rm_reg = mach_reg(&rm->reg);

	__emit_reg_reg(buf, opc, direct_reg, rm_reg);
}

static void
__emit_memdisp(struct buffer *buf, unsigned char opc, unsigned long disp,
	       unsigned char reg_opcode)
{
	unsigned char mod_rm;

	mod_rm = encode_modrm(0, reg_opcode, 5);

	emit(buf, opc);
	emit(buf, mod_rm);
	emit_imm32(buf, disp);
}

static void
__emit_memdisp_reg(struct buffer *buf, unsigned char opc, unsigned long disp,
		   enum machine_reg reg)
{
	__emit_memdisp(buf, opc, disp, __encode_reg(reg));
}

static void
__emit_reg_memdisp(struct buffer *buf, unsigned char opc, enum machine_reg reg,
		   unsigned long disp)
{
	__emit_memdisp(buf, opc, disp, __encode_reg(reg));
}

static void
__emit_membase(struct buffer *buf, unsigned char opc,
	       enum machine_reg base_reg, unsigned long disp,
	       unsigned char reg_opcode)
{
	unsigned char mod, rm, mod_rm;
	int needs_sib;

	needs_sib = (base_reg == REG_ESP);

	emit(buf, opc);

	if (needs_sib)
		rm = 0x04;
	else
		rm = __encode_reg(base_reg);

	if (disp == 0)
		mod = 0x00;
	else if (is_imm_8(disp))
		mod = 0x01;
	else
		mod = 0x02;

	mod_rm = encode_modrm(mod, reg_opcode, rm);
	emit(buf, mod_rm);

	if (needs_sib)
		emit(buf, encode_sib(0x00, 0x04, __encode_reg(base_reg)));

	if (disp)
		emit_imm(buf, disp);
}

static void
__emit_membase_reg(struct buffer *buf, unsigned char opc,
		   enum machine_reg base_reg, unsigned long disp,
		   enum machine_reg dest_reg)
{
	__emit_membase(buf, opc, base_reg, disp, __encode_reg(dest_reg));
}

static void 
emit_membase_reg(struct buffer *buf, unsigned char opc, struct operand *src,
		 struct operand *dest)
{
	enum machine_reg base_reg, dest_reg;
	unsigned long disp;

	base_reg = mach_reg(&src->base_reg);
	disp = src->disp;
	dest_reg = mach_reg(&dest->reg);

	__emit_membase_reg(buf, opc, base_reg, disp, dest_reg);
}

static void __emit_push_reg(struct buffer *buf, enum machine_reg reg)
{
	emit(buf, 0x50 + __encode_reg(reg));
}

static void __emit_push_membase(struct buffer *buf, enum machine_reg src_reg,
				unsigned long disp)
{
	__emit_membase(buf, 0xff, src_reg, disp, 6);
}

static void __emit_pop_reg(struct buffer *buf, enum machine_reg reg)
{
	emit(buf, 0x58 + __encode_reg(reg));
}

static void emit_push_reg(struct buffer *buf, struct operand *operand)
{
	__emit_push_reg(buf, mach_reg(&operand->reg));
}

static void __emit_mov_reg_reg(struct buffer *buf, enum machine_reg src_reg,
			       enum machine_reg dest_reg)
{
	__emit_reg_reg(buf, 0x89, src_reg, dest_reg);
}

static void emit_mov_reg_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	__emit_mov_reg_reg(buf, mach_reg(&src->reg), mach_reg(&dest->reg));
}

static void emit_movsx_8_reg_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	emit(buf, 0x0f);
	__emit_reg_reg(buf, 0xbe, mach_reg(&dest->reg), mach_reg(&src->reg));
}

static void emit_movsx_16_reg_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	emit(buf, 0x0f);
	__emit_reg_reg(buf, 0xbf, mach_reg(&dest->reg), mach_reg(&src->reg));
}

static void
emit_mov_memlocal_reg(struct buffer *buf, struct operand *src, struct operand *dest)
{
	enum machine_reg dest_reg;
	unsigned long disp;

	dest_reg = mach_reg(&dest->reg);
	disp = slot_offset(src->slot);

	__emit_membase_reg(buf, 0x8b, REG_EBP, disp, dest_reg);
}

static void
emit_mov_memlocal_freg(struct buffer *buf, struct operand *src, struct operand *dest)
{
	enum machine_reg dest_reg;
	unsigned long disp;

	dest_reg = mach_reg(&dest->reg);
	disp = slot_offset(src->slot);

	emit(buf, 0xf3);
	emit(buf, 0x0f);
	__emit_membase_reg(buf, 0x10, REG_EBP, disp, dest_reg);
}

static void emit_mov_membase_reg(struct buffer *buf,
				 struct operand *src, struct operand *dest)
{
	emit_membase_reg(buf, 0x8b, src, dest);
}

static void emit_mov_thread_local_memdisp_reg(struct buffer *buf,
					      struct operand *src,
					      struct operand *dest)
{
	emit(buf, 0x65); /* GS segment override prefix */
	__emit_memdisp_reg(buf, 0x8b, src->imm, mach_reg(&dest->reg));
}

static void emit_mov_memdisp_reg(struct buffer *buf,
				 struct operand *src,
				 struct operand *dest)
{
	__emit_memdisp_reg(buf, 0x8b, src->imm, mach_reg(&dest->reg));
}

static void emit_mov_reg_memdisp(struct buffer *buf,
				 struct operand *src,
				 struct operand *dest)
{
	__emit_reg_memdisp(buf, 0x89, mach_reg(&src->reg), dest->imm);
}

static void emit_mov_memindex_reg(struct buffer *buf,
				  struct operand *src, struct operand *dest)
{
	emit(buf, 0x8b);
	emit(buf, encode_modrm(0x00, encode_reg(&dest->reg), 0x04));
	emit(buf, encode_sib(src->shift, encode_reg(&src->index_reg), encode_reg(&src->base_reg)));
}

static void __emit_mov_imm_reg(struct buffer *buf, long imm, enum machine_reg reg)
{
	emit(buf, 0xb8 + __encode_reg(reg));
	emit_imm32(buf, imm);
}

static void emit_mov_imm_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	__emit_mov_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void emit_mov_imm_membase(struct buffer *buf, struct operand *src,
				 struct operand *dest)
{
	__emit_membase(buf, 0xc7, mach_reg(&dest->base_reg), dest->disp, 0);

	emit_imm32(buf, src->imm);
}

static void
emit_mov_reg_membase(struct buffer *buf, struct operand *src, struct operand *dest)
{
	__emit_membase(buf, 0x89, mach_reg(&dest->base_reg), dest->disp, encode_reg(&src->reg));
}

static void emit_mov_reg_memlocal(struct buffer *buf, struct operand *src,
				  struct operand *dest)
{
	unsigned long disp;
	int mod;

	disp = slot_offset(dest->slot);

	if (is_imm_8(disp))
		mod = 0x01;
	else
		mod = 0x02;

	emit(buf, 0x89);
	emit(buf, encode_modrm(mod, encode_reg(&src->reg),
			       __encode_reg(REG_EBP)));

	emit_imm(buf, disp);
}

static void emit_mov_freg_memlocal(struct buffer *buf, struct operand *src,
				struct operand *dest)
{
	unsigned long disp;
	int mod;

	disp = slot_offset(dest->slot);

	if (is_imm_8(disp))
		mod = 0x01;
	else
		mod = 0x02;

	emit(buf, 0xf3);
	emit(buf, 0x0f);
	emit(buf, 0x11);
	emit(buf, encode_modrm(mod, encode_reg(&src->reg),
			       __encode_reg(REG_EBP)));

	emit_imm(buf, disp);
}

static void emit_mov_reg_memindex(struct buffer *buf, struct operand *src,
				  struct operand *dest)
{
	emit(buf, 0x89);
	emit(buf, encode_modrm(0x00, encode_reg(&src->reg), 0x04));
	emit(buf, encode_sib(dest->shift, encode_reg(&dest->index_reg), encode_reg(&dest->base_reg)));
}

static void emit_alu_imm_reg(struct buffer *buf, unsigned char opc_ext,
			     long imm, enum machine_reg reg)
{
	int opc;

	if (is_imm_8(imm))
		opc = 0x83;
	else
		opc = 0x81;

	emit(buf, opc);
	emit(buf, encode_modrm(0x3, opc_ext, __encode_reg(reg)));
	emit_imm(buf, imm);
}

static void __emit_sub_imm_reg(struct buffer *buf, unsigned long imm,
			       enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 0x05, imm, reg);
}

static void emit_sub_imm_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	__emit_sub_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void emit_sub_reg_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	emit_reg_reg(buf, 0x29, src, dest);
}

static void __emit_sbb_imm_reg(struct buffer *buf, unsigned long imm,
			       enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 0x03, imm, reg);
}

static void emit_sbb_imm_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	__emit_sbb_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void emit_sbb_reg_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	emit_reg_reg(buf, 0x19, src, dest);
}

void emit_prolog(struct buffer *buf, unsigned long nr_locals)
{
	/* Unconditionally push callee-saved registers */
	__emit_push_reg(buf, REG_EDI);
	__emit_push_reg(buf, REG_ESI);
	__emit_push_reg(buf, REG_EBX);

	__emit_push_reg(buf, REG_EBP);
	__emit_mov_reg_reg(buf, REG_ESP, REG_EBP);

	if (nr_locals)
		__emit_sub_imm_reg(buf, nr_locals * sizeof(unsigned long), REG_ESP);
}

static void emit_pop_memlocal(struct buffer *buf, struct operand *operand)
{
	unsigned long disp = slot_offset(operand->slot);

	__emit_membase(buf, 0x8f, REG_EBP, disp, 0);
}

static void emit_pop_reg(struct buffer *buf, struct operand *operand)
{
	__emit_pop_reg(buf, mach_reg(&operand->reg));
}

static void __emit_push_imm(struct buffer *buf, long imm)
{
	unsigned char opc;

	if (is_imm_8(imm))
		opc = 0x6a;
	else
		opc = 0x68;

	emit(buf, opc);
	emit_imm(buf, imm);
}

static void emit_push_imm(struct buffer *buf, struct operand *operand)
{
	__emit_push_imm(buf, operand->imm);
}

static void emit_restore_regs(struct buffer *buf)
{
	__emit_pop_reg(buf, REG_EBX);
	__emit_pop_reg(buf, REG_ESI);
	__emit_pop_reg(buf, REG_EDI);
}

static void emit_adc_reg_reg(struct buffer *buf,
                             struct operand *src, struct operand *dest)
{
	emit_reg_reg(buf, 0x13, dest, src);
}

static void emit_adc_membase_reg(struct buffer *buf,
				 struct operand *src, struct operand *dest)
{
	emit_membase_reg(buf, 0x13, src, dest);
}

static void emit_add_reg_reg(struct buffer *buf,
			     struct operand *src, struct operand *dest)
{
	emit_reg_reg(buf, 0x03, dest, src);
}

static void emit_fadd_reg_reg(struct buffer *buf,
			     struct operand *src, struct operand *dest)
{
	emit(buf, 0xf3);
	emit(buf, 0x0f);
	emit_reg_reg(buf, 0x58, dest, src);
}

static void emit_fsub_reg_reg(struct buffer *buf,
			     struct operand *src, struct operand *dest)
{
	emit(buf, 0xf3);
	emit(buf, 0x0f);
	emit_reg_reg(buf, 0x5c, dest, src);
}
static void emit_fmul_reg_reg(struct buffer *buf,
			     struct operand *src, struct operand *dest)
{
	emit(buf, 0xf3);
	emit(buf, 0x0f);
	emit_reg_reg(buf, 0x59, dest, src);
}
static void emit_fdiv_reg_reg(struct buffer *buf,
			     struct operand *src, struct operand *dest)
{
	emit(buf, 0xf3);
	emit(buf, 0x0f);
	emit_reg_reg(buf, 0x5e, dest, src);
}

static void emit_fld_membase(struct buffer *buf, struct operand *src)
{
	__emit_membase(buf, 0xd9, mach_reg(&src->base_reg), src->disp, 0);
}

static void emit_fstp_membase(struct buffer *buf, struct operand *dest)
{
	__emit_membase(buf, 0xd9, mach_reg(&dest->base_reg), dest->disp, 3);
}

static void emit_add_membase_reg(struct buffer *buf,
				 struct operand *src, struct operand *dest)
{
	emit_membase_reg(buf, 0x03, src, dest);
}

static void emit_and_reg_reg(struct buffer *buf,
			     struct operand *src, struct operand *dest)
{
	emit_reg_reg(buf, 0x23, dest, src);
}

static void emit_and_membase_reg(struct buffer *buf,
				 struct operand *src, struct operand *dest)
{
	emit_membase_reg(buf, 0x23, src, dest);
}

static void emit_sbb_membase_reg(struct buffer *buf,
				 struct operand *src, struct operand *dest)
{
	emit_membase_reg(buf, 0x1b, src, dest);
}

static void emit_sub_membase_reg(struct buffer *buf,
				 struct operand *src, struct operand *dest)
{
	emit_membase_reg(buf, 0x2b, src, dest);
}

static void __emit_div_mul_membase_eax(struct buffer *buf,
				       struct operand *src,
				       struct operand *dest,
				       unsigned char opc_ext)
{
	long disp;
	int mod;

	assert(mach_reg(&dest->reg) == REG_EAX);

	disp = src->disp;

	if (is_imm_8(disp))
		mod = 0x01;
	else
		mod = 0x02;

	emit(buf, 0xf7);
	emit(buf, encode_modrm(mod, opc_ext, encode_reg(&src->base_reg)));
	emit_imm(buf, disp);
}

static void __emit_div_mul_reg_eax(struct buffer *buf,
				       struct operand *src,
				       struct operand *dest,
				       unsigned char opc_ext)
{
	assert(mach_reg(&dest->reg) == REG_EAX);

	emit(buf, 0xf7);
	emit(buf, encode_modrm(0x03, opc_ext, encode_reg(&src->base_reg)));
}

static void emit_mul_membase_eax(struct buffer *buf,
				 struct operand *src, struct operand *dest)
{
	__emit_div_mul_membase_eax(buf, src, dest, 0x04);
}

static void emit_mul_reg_eax(struct buffer *buf, 
			     struct operand *src, struct operand *dest)
{
    	__emit_div_mul_reg_eax(buf, src, dest, 0x04);
}

static void emit_mul_reg_reg(struct buffer *buf,
			     struct operand *src, struct operand *dest)
{
    	emit(buf, 0x0f);
	__emit_reg_reg(buf, 0xaf, mach_reg(&dest->reg), mach_reg(&src->reg));
}

static void emit_neg_reg(struct buffer *buf, struct operand *operand)
{
	emit(buf, 0xf7);
	emit(buf, encode_modrm(0x3, 0x3, encode_reg(&operand->reg)));
}

static void emit_cltd_reg_reg(struct buffer *buf, struct operand *src, struct operand *dest)
{
	assert(mach_reg(&src->reg) == REG_EAX);
	assert(mach_reg(&dest->reg) == REG_EDX);

	emit(buf, 0x99);
}

static void emit_div_membase_reg(struct buffer *buf, struct operand *src,
				 struct operand *dest)
{
	__emit_div_mul_membase_eax(buf, src, dest, 0x07);
}

static void emit_div_reg_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	__emit_div_mul_reg_eax(buf, src, dest, 0x07);
}

static void __emit_shift_reg_reg(struct buffer *buf,
				 struct operand *src,
				 struct operand *dest, unsigned char opc_ext)
{
	assert(mach_reg(&src->reg) == REG_ECX);

	emit(buf, 0xd3);
	emit(buf, encode_modrm(0x03, opc_ext, encode_reg(&dest->reg)));
}

static void emit_shl_reg_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	__emit_shift_reg_reg(buf, src, dest, 0x04);
}

static void emit_sar_imm_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	emit(buf, 0xc1);
	emit(buf, encode_modrm(0x03, 0x07, encode_reg(&dest->reg)));
	emit(buf, src->imm);
}

static void emit_sar_reg_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	__emit_shift_reg_reg(buf, src, dest, 0x07);
}

static void emit_shr_reg_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	__emit_shift_reg_reg(buf, src, dest, 0x05);
}

static void emit_or_membase_reg(struct buffer *buf,
				struct operand *src, struct operand *dest)
{
	emit_membase_reg(buf, 0x0b, src, dest);
}

static void emit_or_reg_reg(struct buffer *buf, struct operand *src,
			    struct operand *dest)
{
	emit_reg_reg(buf, 0x0b, dest, src);
}

static void emit_xor_reg_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	emit_reg_reg(buf, 0x33, dest, src);
}

static void emit_xor_xmm_reg_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	emit(buf, 0x0f);
	emit_reg_reg(buf, 0x57, dest, src);
}

static void __emit_add_imm_reg(struct buffer *buf, long imm, enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 0x00, imm, reg);
}

static void emit_add_imm_reg(struct buffer *buf,
			     struct operand *src, struct operand *dest)
{
	__emit_add_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void __emit_adc_imm_reg(struct buffer *buf, long imm, enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 0x02, imm, reg);
}

static void emit_adc_imm_reg(struct buffer *buf,
			     struct operand *src, struct operand *dest)
{
	__emit_adc_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void __emit_cmp_imm_reg(struct buffer *buf, long imm, enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 0x07, imm, reg);
}

static void emit_cmp_imm_reg(struct buffer *buf, struct operand *src,
			     struct operand *dest)
{
	__emit_cmp_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void emit_cmp_membase_reg(struct buffer *buf, struct operand *src, struct operand *dest)
{
	emit_membase_reg(buf, 0x3b, src, dest);
}

static void emit_cmp_reg_reg(struct buffer *buf, struct operand *src, struct operand *dest)
{
	emit_reg_reg(buf, 0x39, src, dest);
}

static void emit_indirect_jump_reg(struct buffer *buf, enum machine_reg reg)
{
	emit(buf, 0xff);
	emit(buf, encode_modrm(0x3, 0x04, __encode_reg(reg)));
}

static void emit_really_indirect_jump_reg(struct buffer *buf, enum machine_reg reg)
{
	emit(buf, 0xff);
	emit(buf, encode_modrm(0x0, 0x04, __encode_reg(reg)));
}

static void emit_indirect_call(struct buffer *buf, struct operand *operand)
{
	emit(buf, 0xff);
	emit(buf, encode_modrm(0x0, 0x2, encode_reg(&operand->reg)));
}

static void emit_xor_membase_reg(struct buffer *buf,
				 struct operand *src, struct operand *dest)
{
	emit_membase_reg(buf, 0x33, src, dest);
}

static void __emit_xor_imm_reg(struct buffer *buf, long imm, enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 0x06, imm, reg);
}

static void emit_xor_imm_reg(struct buffer *buf, struct operand * src,
			     struct operand *dest)
{
	__emit_xor_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void __emit_test_membase_reg(struct buffer *buf, enum machine_reg src,
				    unsigned long disp, enum machine_reg dest)
{
	__emit_membase_reg(buf, 0x85, src, disp, dest);
}

static void emit_test_membase_reg(struct buffer *buf, struct operand *src,
				  struct operand *dest)
{
	emit_membase_reg(buf, 0x85, src, dest);
}

/* Emits exception test using given register. */
static void emit_exception_test(struct buffer *buf, enum machine_reg reg)
{
	/* mov gs:(0xXXX), %reg */
	emit(buf, 0x65);
	__emit_memdisp_reg(buf, 0x8b,
		get_thread_local_offset(&exception_guard), reg);

	/* test (%reg), %reg */
	__emit_test_membase_reg(buf, reg, 0, reg);
}

static void emit_conv_gpr_to_fpu(struct buffer *buf, struct operand *src,
				struct operand *dest)
{
	emit(buf, 0xf3);
	emit(buf, 0x0f);
	emit_reg_reg(buf, 0x2a, dest, src);
}

static void emit_conv_fpu_to_gpr(struct buffer *buf, struct operand *src,
				struct operand *dest)
{
	emit(buf, 0xf3);
	emit(buf, 0x0f);
	emit_reg_reg(buf, 0x2d, dest, src);
}

static void emit_mov_membase_xmm(struct buffer *buf, struct operand *src,
				struct operand *dest)
{
	emit(buf, 0xf3);
	emit(buf, 0x0f);
	emit_membase_reg(buf, 0x10, src, dest);
}

static void emit_mov_xmm_membase(struct buffer *buf, struct operand *src,
				struct operand *dest)
{
	emit(buf, 0xf3);
	emit(buf, 0x0f);
	emit_membase_reg(buf, 0x11, dest, src);
}

struct emitter emitters[] = {
	GENERIC_X86_EMITTERS,
	DECL_EMITTER(INSN_ADC_IMM_REG, emit_adc_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_ADC_REG_REG, emit_adc_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_ADC_MEMBASE_REG, emit_adc_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_ADD_IMM_REG, emit_add_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_ADD_MEMBASE_REG, emit_add_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_ADD_REG_REG, emit_add_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_AND_MEMBASE_REG, emit_and_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_AND_REG_REG, emit_and_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_CALL_REG, emit_indirect_call, SINGLE_OPERAND),
	DECL_EMITTER(INSN_CLTD_REG_REG, emit_cltd_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_CMP_IMM_REG, emit_cmp_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_CMP_MEMBASE_REG, emit_cmp_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_CMP_REG_REG, emit_cmp_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_DIV_MEMBASE_REG, emit_div_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_DIV_REG_REG, emit_div_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_FADD_REG_REG, emit_fadd_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_FSUB_REG_REG, emit_fsub_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_FMUL_REG_REG, emit_fmul_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_FDIV_REG_REG, emit_fdiv_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_FLD_MEMBASE, emit_fld_membase, TWO_OPERANDS),
	DECL_EMITTER(INSN_FSTP_MEMBASE, emit_fstp_membase, TWO_OPERANDS),
	DECL_EMITTER(INSN_CONV_GPR_TO_FPU, emit_conv_gpr_to_fpu, TWO_OPERANDS),
	DECL_EMITTER(INSN_CONV_FPU_TO_GPR, emit_conv_fpu_to_gpr, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_MEMBASE_XMM, emit_mov_membase_xmm, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_XMM_MEMBASE, emit_mov_xmm_membase, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_IMM_MEMBASE, emit_mov_imm_membase, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_IMM_REG, emit_mov_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_MEMLOCAL_REG, emit_mov_memlocal_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_MEMLOCAL_FREG, emit_mov_memlocal_freg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_MEMBASE_REG, emit_mov_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_MEMDISP_REG, emit_mov_memdisp_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_REG_MEMDISP, emit_mov_reg_memdisp, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_THREAD_LOCAL_MEMDISP_REG, emit_mov_thread_local_memdisp_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_MEMINDEX_REG, emit_mov_memindex_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_REG_MEMBASE, emit_mov_reg_membase, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_REG_MEMINDEX, emit_mov_reg_memindex, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_REG_MEMLOCAL, emit_mov_reg_memlocal, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_FREG_MEMLOCAL, emit_mov_freg_memlocal, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOV_REG_REG, emit_mov_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOVSX_8_REG_REG, emit_movsx_8_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MOVSX_16_REG_REG, emit_movsx_16_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_MUL_MEMBASE_EAX, emit_mul_membase_eax, TWO_OPERANDS),
	DECL_EMITTER(INSN_MUL_REG_EAX, emit_mul_reg_eax, TWO_OPERANDS),
	DECL_EMITTER(INSN_MUL_REG_REG, emit_mul_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_NEG_REG, emit_neg_reg, SINGLE_OPERAND),
	DECL_EMITTER(INSN_OR_MEMBASE_REG, emit_or_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_OR_REG_REG, emit_or_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_PUSH_IMM, emit_push_imm, SINGLE_OPERAND),
	DECL_EMITTER(INSN_PUSH_REG, emit_push_reg, SINGLE_OPERAND),
	DECL_EMITTER(INSN_POP_MEMLOCAL, emit_pop_memlocal, SINGLE_OPERAND),
	DECL_EMITTER(INSN_POP_REG, emit_pop_reg, SINGLE_OPERAND),
	DECL_EMITTER(INSN_SAR_IMM_REG, emit_sar_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SAR_REG_REG, emit_sar_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SBB_IMM_REG, emit_sbb_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SBB_MEMBASE_REG, emit_sbb_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SBB_REG_REG, emit_sbb_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SHL_REG_REG, emit_shl_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SHR_REG_REG, emit_shr_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SUB_IMM_REG, emit_sub_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SUB_MEMBASE_REG, emit_sub_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_SUB_REG_REG, emit_sub_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_TEST_MEMBASE_REG, emit_test_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_XOR_MEMBASE_REG, emit_xor_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_XOR_IMM_REG, emit_xor_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_XOR_REG_REG, emit_xor_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN_XOR_XMM_REG_REG, emit_xor_xmm_reg_reg, TWO_OPERANDS),
};

void emit_trampoline(struct compilation_unit *cu,
		     void *call_target,
		     struct jit_trampoline *trampoline)
{
	struct buffer *buf = trampoline->objcode;

	jit_text_lock();

	buf->buf = jit_text_ptr();

	/* This is for __builtin_return_address() to work and to access
	   call arguments in correct manner. */
	__emit_push_reg(buf, REG_EBP);
	__emit_mov_reg_reg(buf, REG_ESP, REG_EBP);

	__emit_push_imm(buf, (unsigned long)cu);
	__emit_call(buf, call_target);
	__emit_add_imm_reg(buf, 0x04, REG_ESP);

	/*
	 * Test for exeption occurance.
	 * We do this by polling a dedicated thread-specific pointer,
	 * which triggers SIGSEGV when exception is set.
	 *
	 * mov gs:(0xXXX), %ecx
	 * test (%ecx), %ecx
	 */
	emit(buf, 0x65);
	__emit_memdisp_reg(buf, 0x8b,
			   get_thread_local_offset(&trampoline_exception_guard),
			   REG_ECX);
	__emit_test_membase_reg(buf, REG_ECX, 0, REG_ECX);

	__emit_push_reg(buf, REG_EAX);

	if (method_is_virtual(cu->method)) {
		__emit_push_membase(buf, REG_EBP, 0x08);
		__emit_push_imm(buf, (unsigned long)cu);
		__emit_call(buf, fixup_vtable);
		__emit_add_imm_reg(buf, 0x08, REG_ESP);
	}

	__emit_pop_reg(buf, REG_EAX);

	__emit_pop_reg(buf, REG_EBP);
	emit_indirect_jump_reg(buf, REG_EAX);

	jit_text_reserve(buffer_offset(buf));
	jit_text_unlock();
}

/* Note: a < b, always */
static void emit_itable_bsearch(struct buffer *buf,
	struct itable_entry **table, unsigned int a, unsigned int b)
{
	uint8_t *jb_addr = NULL;
	uint8_t *ja_addr = NULL;
	unsigned int m;

	/* Find middle (safe from overflows) */
	m = a + (b - a) / 2;

	/* No point in emitting the "cmp" if we're not going to test
	 * anything */
	if (b - a >= 1)
		__emit_cmp_imm_reg(buf, (long) table[m]->i_method, REG_EAX);

	if (m - a > 0) {
		/* open-coded "jb" */
		emit(buf, 0x0f);
		emit(buf, 0x82);

		/* placeholder address */
		jb_addr = buffer_current(buf);
		emit_imm32(buf, 0);
	}

	if (b - m > 0) {
		/* open-coded "ja" */
		emit(buf, 0x0f);
		emit(buf, 0x87);

		/* placeholder address */
		ja_addr = buffer_current(buf);
		emit_imm32(buf, 0);
	}

	__emit_add_imm_reg(buf, 4 * table[m]->c_method->virtual_index, REG_ECX);
	emit_really_indirect_jump_reg(buf, REG_ECX);

	/* This emits the code for checking the interval [a, m> */
	if (jb_addr) {
		long cur = (long) (buffer_current(buf) - (void *) jb_addr) - 4;
		jb_addr[3] = cur >> 24;
		jb_addr[2] = cur >> 16;
		jb_addr[1] = cur >> 8;
		jb_addr[0] = cur;

		emit_itable_bsearch(buf, table, a, m - 1);
	}

	/* This emits the code for checking the interval <m, b] */
	if (ja_addr) {
		long cur = (long) (buffer_current(buf) - (void *) ja_addr) - 4;
		ja_addr[3] = cur >> 24;
		ja_addr[2] = cur >> 16;
		ja_addr[1] = cur >> 8;
		ja_addr[0] = cur;

		emit_itable_bsearch(buf, table, m + 1, b);
	}
}

/* Note: table is always sorted on entry->method address */
/* Note: nr_entries is always >= 2 */
void *emit_itable_resolver_stub(struct vm_class *vmc,
	struct itable_entry **table, unsigned int nr_entries)
{
	static struct buffer_operations exec_buf_ops = {
		.expand = NULL,
		.free   = NULL,
	};

	struct buffer *buf = __alloc_buffer(&exec_buf_ops);

	jit_text_lock();

	buf->buf = jit_text_ptr();

	/* Note: When the stub is called, %eax contains the signature hash that
	 * we look up in the stub. 0(%esp) contains the object reference. %ecx
	 * and %edx are available here because they are already saved by the
	 * caller (guaranteed by ABI). */

	/* Load the start of the vtable into %ecx. Later we just add the
	 * right offset to %ecx and jump to *(%ecx). */
	__emit_mov_imm_reg(buf, (long) vmc->vtable.native_ptr, REG_ECX);

	emit_itable_bsearch(buf, table, 0, nr_entries - 1);

	jit_text_reserve(buffer_offset(buf));
	jit_text_unlock();

	return buffer_ptr(buf);
}

#else /* CONFIG_X86_32 */

/************************
 * x86-64 code emitters *
 ************************/

#define	REX		0x40
#define REX_W		(REX | 8)	/* 64-bit operands */
#define REX_R		(REX | 4)	/* ModRM reg extension */
#define REX_X		(REX | 2)	/* SIB index extension */
#define REX_B		(REX | 1)	/* ModRM r/m extension */

/*
 *	__encode_reg:	Encode register to be used in x86-64 instruction.
 *	@reg: Register to encode.
 *
 *	Returns register in r/m or reg/opcode field format of the ModR/M byte.
 */
static unsigned char __encode_reg(enum machine_reg reg)
{
	unsigned char ret = 0;

	switch (reg) {
	case REG_RAX:
		ret = 0x00;
		break;
	case REG_RBX:
		ret = 0x03;
		break;
	case REG_RCX:
		ret = 0x01;
		break;
	case REG_RDX:
		ret = 0x02;
		break;
	case REG_RSI:
		ret = 0x06;
		break;
	case REG_RDI:
		ret = 0x07;
		break;
	case REG_RSP:
		ret = 0x04;
		break;
	case REG_RBP:
		ret = 0x05;
		break;
	case REG_R8:
		ret = 0x08;
		break;
	case REG_R9:
		ret = 0x09;
		break;
	case REG_R10:
		ret = 0x0A;
		break;
	case REG_R11:
		ret = 0x0B;
		break;
	case REG_R12:
		ret = 0x0C;
		break;
	case REG_R13:
		ret = 0x0D;
		break;
	case REG_R14:
		ret = 0x0E;
		break;
	case REG_R15:
		ret = 0x0F;
		break;
	case REG_UNASSIGNED:
		assert(!"unassigned register in code emission");
		break;
	}
	return ret;
}

static inline unsigned char reg_low(unsigned char reg)
{
	return (reg & 0x7);
}

static inline unsigned char reg_high(unsigned char reg)
{
	return (reg & 0x8);
}

static inline unsigned long rip_relative(struct buffer *buf,
					 unsigned long addr,
					 unsigned long insn_size)
{
	return addr - (unsigned long) buffer_current(buf) - insn_size;
}

static void __emit_reg(struct buffer *buf,
		       int rex_w,
		       unsigned char opc,
		       enum machine_reg reg)
{
	unsigned char __reg = __encode_reg(reg);
	unsigned char rex_pfx = 0;

	if (rex_w)
		rex_pfx |= REX_W;
	if (reg_high(__reg))
		rex_pfx |= REX_B;

	if (rex_pfx)
		emit(buf, rex_pfx);
	emit(buf, opc + reg_low(__reg));
}

static void __emit64_push_reg(struct buffer *buf, enum machine_reg reg)
{
	__emit_reg(buf, 0, 0x50, reg);
}

static void emit64_push_reg(struct buffer *buf, struct operand *operand)
{
	__emit64_push_reg(buf, mach_reg(&operand->reg));
}

static void __emit64_pop_reg(struct buffer *buf, enum machine_reg reg)
{
	__emit_reg(buf, 0, 0x58, reg);
}

static void emit64_pop_reg(struct buffer *buf, struct operand *operand)
{
	__emit64_pop_reg(buf, mach_reg(&operand->reg));
}

static void __emit_reg_reg(struct buffer *buf,
			   int rex_w,
			   unsigned char opc,
			   enum machine_reg direct_reg,
			   enum machine_reg rm_reg)
{
	unsigned char rex_pfx = 0, mod_rm;
	unsigned char direct, rm;

	direct = __encode_reg(direct_reg);
	rm = __encode_reg(rm_reg);

	if (rex_w)
		rex_pfx |= REX_W;
	if (reg_high(direct))
		rex_pfx |= REX_R;
	if (reg_high(rm))
		rex_pfx |= REX_B;

	mod_rm = encode_modrm(0x03, direct, rm);

	if (rex_pfx)
		emit(buf, rex_pfx);
	emit(buf, opc);
	emit(buf, mod_rm);
}

static void emit_reg_reg(struct buffer *buf,
			 int rex_w,
			 unsigned char opc,
			 struct operand *direct,
			 struct operand *rm)
{
	enum machine_reg direct_reg, rm_reg;

	direct_reg = mach_reg(&direct->reg);
	rm_reg = mach_reg(&rm->reg);

	__emit_reg_reg(buf, rex_w, opc, direct_reg, rm_reg);
}

static void __emit64_mov_reg_reg(struct buffer *buf,
				 enum machine_reg src,
				 enum machine_reg dst)
{
	__emit_reg_reg(buf, 1, 0x89, src, dst);
}

static void emit64_mov_reg_reg(struct buffer *buf,
			       struct operand *src,
			       struct operand *dest)
{
	__emit64_mov_reg_reg(buf, mach_reg(&src->reg), mach_reg(&dest->reg));
}

static void __emit32_mov_reg_reg(struct buffer *buf,
				 enum machine_reg src,
				 enum machine_reg dst)
{
	__emit_reg_reg(buf, 0, 0x89, src, dst);
}

static void emit32_mov_reg_reg(struct buffer *buf,
			       struct operand *src,
			       struct operand *dest)
{
	__emit32_mov_reg_reg(buf, mach_reg(&src->reg), mach_reg(&dest->reg));
}

static void emit_alu_imm_reg(struct buffer *buf,
			     int rex_w,
			     unsigned char opc_ext,
			     long imm,
			     enum machine_reg reg)
{
	unsigned char rex_pfx = 0, opc, __reg;

	__reg = __encode_reg(reg);

	if (rex_w)
		rex_pfx |= REX_W;
	if (reg_high(__reg))
		rex_pfx |= REX_B;

	if (is_imm_8(imm))
		opc = 0x83;
	else
		opc = 0x81;

	if (rex_pfx)
		emit(buf, rex_pfx);
	emit(buf, opc);
	emit(buf, encode_modrm(0x3, opc_ext, __reg));
	emit_imm(buf, imm);
}

static void __emit64_sub_imm_reg(struct buffer *buf,
				 unsigned long imm,
				 enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 1, 0x05, imm, reg);
}

static void emit64_sub_imm_reg(struct buffer *buf,
			       struct operand *src,
			       struct operand *dest)
{
	__emit64_sub_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void __emit32_sub_imm_reg(struct buffer *buf,
				 unsigned long imm,
				 enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 0, 0x05, imm, reg);
}

static void emit32_sub_imm_reg(struct buffer *buf,
			       struct operand *src,
			       struct operand *dest)
{
	__emit32_sub_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void __emit64_add_imm_reg(struct buffer *buf,
				 long imm,
				 enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 1, 0x00, imm, reg);
}

static void emit64_add_imm_reg(struct buffer *buf,
			       struct operand *src,
			       struct operand *dest)
{
	__emit64_add_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void __emit32_add_imm_reg(struct buffer *buf,
				 long imm,
				 enum machine_reg reg)
{
	emit_alu_imm_reg(buf, 0, 0x00, imm, reg);
}

static void emit32_add_imm_reg(struct buffer *buf,
			       struct operand *src,
			       struct operand *dest)
{
	__emit64_add_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void emit_imm64(struct buffer *buf, unsigned long imm)
{
	union {
		unsigned long val;
		unsigned char b[8];
	} imm_buf;

	imm_buf.val = imm;
	emit(buf, imm_buf.b[0]);
	emit(buf, imm_buf.b[1]);
	emit(buf, imm_buf.b[2]);
	emit(buf, imm_buf.b[3]);
	emit(buf, imm_buf.b[4]);
	emit(buf, imm_buf.b[5]);
	emit(buf, imm_buf.b[6]);
	emit(buf, imm_buf.b[7]);
}

static void emit64_imm(struct buffer *buf, long imm)
{
	if (is_imm_8(imm))
		emit(buf, imm);
	else
		emit_imm64(buf, imm);
}

static void __emit64_push_imm(struct buffer *buf, long imm)
{
	unsigned char opc;

	if (is_imm_8(imm))
		opc = 0x6a;
	else
		opc = 0x68;

	emit(buf, opc);
	emit_imm(buf, imm);
}

static void emit64_push_imm(struct buffer *buf, struct operand *operand)
{
	__emit64_push_imm(buf, operand->imm);
}

static void __emit_membase(struct buffer *buf,
			   int rex_w,
			   unsigned char opc,
			   enum machine_reg base_reg,
			   unsigned long disp,
			   unsigned char reg_opcode)
{
	unsigned char rex_pfx = 0, mod, rm, mod_rm;
	unsigned char __base_reg = __encode_reg(base_reg);
	int needs_sib;

	needs_sib = (base_reg == REG_RSP);

	if (needs_sib)
		rm = 0x04;
	else
		rm = __base_reg;

	if (disp == 0)
		mod = 0x00;
	else if (is_imm_8(disp))
		mod = 0x01;
	else
		mod = 0x02;

	if (rex_w)
		rex_pfx |= REX_W;
	if (reg_high(reg_opcode))
		rex_pfx |= REX_R;
	if (reg_high(__base_reg))
		rex_pfx |= REX_B;

	if (rex_pfx)
		emit(buf, rex_pfx);

	emit(buf, opc);

	mod_rm = encode_modrm(mod, reg_opcode, rm);
	emit(buf, mod_rm);

	if (needs_sib)
		emit(buf, encode_sib(0x00, 0x04, __base_reg));

	if (disp)
		emit_imm(buf, disp);
}

static void __emit_membase_reg(struct buffer *buf,
			       int rex_w,
			       unsigned char opc,
			       enum machine_reg base_reg,
			       unsigned long disp,
			       enum machine_reg dest_reg)
{
	__emit_membase(buf, rex_w, opc, base_reg, disp, __encode_reg(dest_reg));
}

static void emit_membase_reg(struct buffer *buf,
			     int rex_w,
			     unsigned char opc,
			     struct operand *src,
			     struct operand *dest)
{
	enum machine_reg base_reg, dest_reg;
	unsigned long disp;

	base_reg = mach_reg(&src->base_reg);
	disp = src->disp;
	dest_reg = mach_reg(&dest->reg);

	__emit_membase_reg(buf, rex_w, opc, base_reg, disp, dest_reg);
}

static void __emit64_push_membase(struct buffer *buf,
				  enum machine_reg src_reg,
				  unsigned long disp)
{
	__emit_membase(buf, 0, 0xff, src_reg, disp, 6);
}

static void emit_exception_test(struct buffer *buf, enum machine_reg reg)
{
	/* FIXME: implement this! */
}

static void __emit_memdisp(struct buffer *buf,
			   int rex_w,
			   unsigned char opc,
			   unsigned long disp,
			   unsigned char reg_opcode)
{
	unsigned char rex_pfx = 0, mod_rm;
	size_t insn_size = 6;

	if (rex_w)
		rex_pfx |= REX_W;
	if (reg_high(reg_opcode))
		rex_pfx |= REX_R;

	mod_rm = encode_modrm(0, reg_opcode, 5);

	if (rex_pfx) {
		emit(buf, rex_pfx);
		insn_size++;
	}
	emit(buf, opc);
	emit(buf, mod_rm);
	emit_imm32(buf, rip_relative(buf, disp, insn_size));
}

static void __emit_memdisp_reg(struct buffer *buf,
			       int rex_w,
			       unsigned char opc,
			       unsigned long disp,
			       enum machine_reg reg)
{
	__emit_memdisp(buf, rex_w, opc, disp, __encode_reg(reg));
}

static void __emit64_test_membase_reg(struct buffer *buf,
				      enum machine_reg src,
				      unsigned long disp,
				      enum machine_reg dest)
{
	__emit_membase_reg(buf, 1, 0x85, src, disp, dest);
}

static void __emit32_test_membase_reg(struct buffer *buf,
				      enum machine_reg src,
				      unsigned long disp,
				      enum machine_reg dest)
{
	__emit_membase_reg(buf, 0, 0x85, src, disp, dest);
}

static void emit64_test_membase_reg(struct buffer *buf,
				    struct operand *src,
				    struct operand *dest)
{
	emit_membase_reg(buf, 1, 0x85, src, dest);
}

static void emit32_test_membase_reg(struct buffer *buf,
				    struct operand *src,
				    struct operand *dest)
{
	emit_membase_reg(buf, 0, 0x85, src, dest);
}

static void emit_indirect_jump_reg(struct buffer *buf, enum machine_reg reg)
{
	unsigned char __reg = __encode_reg(reg);

	emit(buf, 0xff);
	if (reg_high(__reg))
		emit(buf, REX_B);
	emit(buf, encode_modrm(0x3, 0x04, __reg));
}

static void __emit64_mov_imm_reg(struct buffer *buf,
				 long imm,
				 enum machine_reg reg)
{
	__emit_reg(buf, 1, 0xb8, reg);
	emit_imm64(buf, imm);
}

static void emit64_mov_imm_reg(struct buffer *buf,
			       struct operand *src,
			       struct operand *dest)
{
	__emit64_mov_imm_reg(buf, src->imm, mach_reg(&dest->reg));
}

static void __emit64_mov_membase_reg(struct buffer *buf,
				     enum machine_reg base_reg,
				     unsigned long disp,
				     enum machine_reg dest_reg)
{
	__emit_membase_reg(buf, 1, 0x8b, base_reg, disp, dest_reg);
}

static void emit64_mov_membase_reg(struct buffer *buf,
				   struct operand *src,
				   struct operand *dest)
{
	emit_membase_reg(buf, 1, 0x8b, src, dest);
}

struct emitter emitters[] = {
	GENERIC_X86_EMITTERS,

	DECL_EMITTER(INSN64_ADD_IMM_REG, emit64_add_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN64_MOV_IMM_REG, emit64_mov_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN64_MOV_MEMBASE_REG, emit64_mov_membase_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN64_MOV_REG_REG, emit64_mov_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN64_PUSH_IMM, emit64_push_imm, SINGLE_OPERAND),
	DECL_EMITTER(INSN64_PUSH_REG, emit64_push_reg, SINGLE_OPERAND),
	DECL_EMITTER(INSN64_POP_REG, emit64_pop_reg, SINGLE_OPERAND),
	DECL_EMITTER(INSN64_SUB_IMM_REG, emit64_sub_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN64_TEST_MEMBASE_REG, emit64_test_membase_reg, TWO_OPERANDS),

	DECL_EMITTER(INSN32_ADD_IMM_REG, emit32_add_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN32_MOV_REG_REG, emit32_mov_reg_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN32_SUB_IMM_REG, emit32_sub_imm_reg, TWO_OPERANDS),
	DECL_EMITTER(INSN32_TEST_MEMBASE_REG, emit64_test_membase_reg, TWO_OPERANDS),
};

void emit_prolog(struct buffer *buf, unsigned long nr_locals)
{
	__emit_push_reg(buf, REG_RBX);
	__emit_push_reg(buf, REG_R12);
	__emit_push_reg(buf, REG_R13);
	__emit_push_reg(buf, REG_R14);
	__emit_push_reg(buf, REG_R15);

	/*
	 * The ABI requires us to clear DF, but we
	 * don't need to. Though keep this in mind:
	 * emit(buf, 0xFC);
	 */

	__emit_push_reg(buf, REG_RBP);
	__emit64_mov_reg_reg(buf, REG_RSP, REG_RBP);

	if (nr_locals)
		__emit64_sub_imm_reg(buf,
				     nr_locals * sizeof(unsigned long),
				     REG_RSP);
}

static void emit_restore_regs(struct buffer *buf)
{
	__emit_pop_reg(buf, REG_R15);
	__emit_pop_reg(buf, REG_R14);
	__emit_pop_reg(buf, REG_R13);
	__emit_pop_reg(buf, REG_R12);
	__emit_pop_reg(buf, REG_RBX);
}

void emit_trampoline(struct compilation_unit *cu,
		     void *call_target,
		     struct jit_trampoline *trampoline)
{
	struct buffer *buf = trampoline->objcode;

	jit_text_lock();

	buf->buf = jit_text_ptr();

	/* This is for __builtin_return_address() to work and to access
	   call arguments in correct manner. */
	__emit64_push_reg(buf, REG_RBP);
	__emit64_mov_reg_reg(buf, REG_RSP, REG_RBP);

	__emit64_mov_imm_reg(buf, (unsigned long) cu, REG_RDI);
	__emit_call(buf, call_target);

	/*
	 * Test for exception occurance.
	 * We do this by polling a dedicated thread-specific pointer,
	 * which triggers SIGSEGV when exception is set.
	 *
	 * mov fs:(0xXXX), %rcx
	 * test (%rcx), %rcx
	 */
	emit(buf, 0x64);
	__emit_memdisp_reg(buf, 1, 0x8b,
			   get_thread_local_offset(&trampoline_exception_guard),
			   REG_RCX);
	__emit64_test_membase_reg(buf, REG_RCX, 0, REG_RCX);

	__emit64_mov_reg_reg(buf, REG_RAX, REG_RDI);

	if (method_is_virtual(cu->method)) {
		__emit64_mov_membase_reg(buf, REG_RBP, 0x08, REG_RSI);
		__emit64_mov_imm_reg(buf, (unsigned long) cu, REG_RDX);
		__emit_call(buf, fixup_vtable);
	}

	__emit64_pop_reg(buf, REG_RBP);
	emit_indirect_jump_reg(buf, REG_RDX);

	jit_text_reserve(buffer_offset(buf));
	jit_text_unlock();
}

void *emit_itable_resolver_stub(struct vm_class *vmc,
				struct itable_entry **table,
				unsigned int nr_entries)
{
	return NULL;
}

#endif /* CONFIG_X86_32 */
