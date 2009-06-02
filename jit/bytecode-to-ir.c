/*
 * Convert bytecode to register-based immediate representation.
 *
 * Copyright (c) 2005, 2006, 2009  Pekka Enberg
 * 
 * This file is released under the GPL version 2 with the following
 * clarification and special exception:
 *
 *     Linking this library statically or dynamically with other modules is
 *     making a combined work based on this library. Thus, the terms and
 *     conditions of the GNU General Public License cover the whole
 *     combination.
 *
 *     As a special exception, the copyright holders of this library give you
 *     permission to link this library with independent modules to produce an
 *     executable, regardless of the license terms of these independent
 *     modules, and to copy and distribute the resulting executable under terms
 *     of your choice, provided that you also meet, for each linked independent
 *     module, the terms and conditions of the license of that module. An
 *     independent module is a module which is not derived from or based on
 *     this library. If you modify this library, you may extend this exception
 *     to your version of the library, but you are not obligated to do so. If
 *     you do not wish to do so, delete this exception statement from your
 *     version.
 *
 * Please refer to the file LICENSE for details.
 */
#include <jit/bc-offset-mapping.h>
#include <jit/expression.h>
#include <jit/statement.h>
#include <jit/tree-node.h>
#include <jit/compiler.h>

#include <vm/bytecodes.h>
#include <vm/bytecode.h>
#include <vm/stack.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define DECLARE_CONVERTER(name) int name(struct parse_context *)

static int convert_nop(struct parse_context *ctx)
{
	/* Do nothing. */
	return 0;
}

DECLARE_CONVERTER(convert_aaload);
DECLARE_CONVERTER(convert_aastore);
DECLARE_CONVERTER(convert_aconst_null);
DECLARE_CONVERTER(convert_aload);
DECLARE_CONVERTER(convert_aload_n);
DECLARE_CONVERTER(convert_aload_n);
DECLARE_CONVERTER(convert_aload_n);
DECLARE_CONVERTER(convert_aload_n);
DECLARE_CONVERTER(convert_anewarray);
DECLARE_CONVERTER(convert_arraylength);
DECLARE_CONVERTER(convert_astore);
DECLARE_CONVERTER(convert_astore_n);
DECLARE_CONVERTER(convert_astore_n);
DECLARE_CONVERTER(convert_astore_n);
DECLARE_CONVERTER(convert_astore_n);
DECLARE_CONVERTER(convert_athrow);
DECLARE_CONVERTER(convert_baload);
DECLARE_CONVERTER(convert_bastore);
DECLARE_CONVERTER(convert_bipush);
DECLARE_CONVERTER(convert_caload);
DECLARE_CONVERTER(convert_castore);
DECLARE_CONVERTER(convert_checkcast);
DECLARE_CONVERTER(convert_d2f);
DECLARE_CONVERTER(convert_d2i);
DECLARE_CONVERTER(convert_d2l);
DECLARE_CONVERTER(convert_dadd);
DECLARE_CONVERTER(convert_daload);
DECLARE_CONVERTER(convert_dastore);
DECLARE_CONVERTER(convert_dconst);
DECLARE_CONVERTER(convert_dconst);
DECLARE_CONVERTER(convert_ddiv);
DECLARE_CONVERTER(convert_dload);
DECLARE_CONVERTER(convert_dload_n);
DECLARE_CONVERTER(convert_dload_n);
DECLARE_CONVERTER(convert_dload_n);
DECLARE_CONVERTER(convert_dload_n);
DECLARE_CONVERTER(convert_dmul);
DECLARE_CONVERTER(convert_dneg);
DECLARE_CONVERTER(convert_drem);
DECLARE_CONVERTER(convert_dstore);
DECLARE_CONVERTER(convert_dstore_n);
DECLARE_CONVERTER(convert_dstore_n);
DECLARE_CONVERTER(convert_dstore_n);
DECLARE_CONVERTER(convert_dstore_n);
DECLARE_CONVERTER(convert_dsub);
DECLARE_CONVERTER(convert_dup);
DECLARE_CONVERTER(convert_dup2);
DECLARE_CONVERTER(convert_dup2_x1);
DECLARE_CONVERTER(convert_dup2_x2);
DECLARE_CONVERTER(convert_dup_x1);
DECLARE_CONVERTER(convert_dup_x2);
DECLARE_CONVERTER(convert_f2d);
DECLARE_CONVERTER(convert_f2i);
DECLARE_CONVERTER(convert_f2l);
DECLARE_CONVERTER(convert_fadd);
DECLARE_CONVERTER(convert_faload);
DECLARE_CONVERTER(convert_fastore);
DECLARE_CONVERTER(convert_fconst);
DECLARE_CONVERTER(convert_fconst);
DECLARE_CONVERTER(convert_fconst);
DECLARE_CONVERTER(convert_fdiv);
DECLARE_CONVERTER(convert_fload);
DECLARE_CONVERTER(convert_fload_n);
DECLARE_CONVERTER(convert_fload_n);
DECLARE_CONVERTER(convert_fload_n);
DECLARE_CONVERTER(convert_fload_n);
DECLARE_CONVERTER(convert_fmul);
DECLARE_CONVERTER(convert_fneg);
DECLARE_CONVERTER(convert_frem);
DECLARE_CONVERTER(convert_fstore);
DECLARE_CONVERTER(convert_fstore_n);
DECLARE_CONVERTER(convert_fstore_n);
DECLARE_CONVERTER(convert_fstore_n);
DECLARE_CONVERTER(convert_fstore_n);
DECLARE_CONVERTER(convert_fsub);
DECLARE_CONVERTER(convert_getfield);
DECLARE_CONVERTER(convert_getstatic);
DECLARE_CONVERTER(convert_goto);
DECLARE_CONVERTER(convert_i2b);
DECLARE_CONVERTER(convert_i2c);
DECLARE_CONVERTER(convert_i2d);
DECLARE_CONVERTER(convert_i2f);
DECLARE_CONVERTER(convert_i2l);
DECLARE_CONVERTER(convert_i2s);
DECLARE_CONVERTER(convert_iadd);
DECLARE_CONVERTER(convert_iaload);
DECLARE_CONVERTER(convert_iand);
DECLARE_CONVERTER(convert_iastore);
DECLARE_CONVERTER(convert_iconst);
DECLARE_CONVERTER(convert_iconst);
DECLARE_CONVERTER(convert_iconst);
DECLARE_CONVERTER(convert_iconst);
DECLARE_CONVERTER(convert_iconst);
DECLARE_CONVERTER(convert_iconst);
DECLARE_CONVERTER(convert_iconst);
DECLARE_CONVERTER(convert_idiv);
DECLARE_CONVERTER(convert_if_acmpeq);
DECLARE_CONVERTER(convert_if_acmpne);
DECLARE_CONVERTER(convert_if_icmpeq);
DECLARE_CONVERTER(convert_if_icmpge);
DECLARE_CONVERTER(convert_if_icmpgt);
DECLARE_CONVERTER(convert_if_icmple);
DECLARE_CONVERTER(convert_if_icmplt);
DECLARE_CONVERTER(convert_if_icmpne);
DECLARE_CONVERTER(convert_ifeq);
DECLARE_CONVERTER(convert_ifge);
DECLARE_CONVERTER(convert_ifgt);
DECLARE_CONVERTER(convert_ifle);
DECLARE_CONVERTER(convert_iflt);
DECLARE_CONVERTER(convert_ifne);
DECLARE_CONVERTER(convert_ifnonnull);
DECLARE_CONVERTER(convert_ifnull);
DECLARE_CONVERTER(convert_iinc);
DECLARE_CONVERTER(convert_iload);
DECLARE_CONVERTER(convert_iload_n);
DECLARE_CONVERTER(convert_iload_n);
DECLARE_CONVERTER(convert_iload_n);
DECLARE_CONVERTER(convert_iload_n);
DECLARE_CONVERTER(convert_imul);
DECLARE_CONVERTER(convert_ineg);
DECLARE_CONVERTER(convert_instanceof);
DECLARE_CONVERTER(convert_invokespecial);
DECLARE_CONVERTER(convert_invokestatic);
DECLARE_CONVERTER(convert_invokevirtual);
DECLARE_CONVERTER(convert_ior);
DECLARE_CONVERTER(convert_irem);
DECLARE_CONVERTER(convert_ishl);
DECLARE_CONVERTER(convert_ishr);
DECLARE_CONVERTER(convert_istore);
DECLARE_CONVERTER(convert_istore_n);
DECLARE_CONVERTER(convert_istore_n);
DECLARE_CONVERTER(convert_istore_n);
DECLARE_CONVERTER(convert_istore_n);
DECLARE_CONVERTER(convert_isub);
DECLARE_CONVERTER(convert_iushr);
DECLARE_CONVERTER(convert_ixor);
DECLARE_CONVERTER(convert_l2d);
DECLARE_CONVERTER(convert_l2f);
DECLARE_CONVERTER(convert_l2i);
DECLARE_CONVERTER(convert_ladd);
DECLARE_CONVERTER(convert_laload);
DECLARE_CONVERTER(convert_land);
DECLARE_CONVERTER(convert_lastore);
DECLARE_CONVERTER(convert_lcmp);
DECLARE_CONVERTER(convert_lconst);
DECLARE_CONVERTER(convert_lconst);
DECLARE_CONVERTER(convert_ldc);
DECLARE_CONVERTER(convert_ldc2_w);
DECLARE_CONVERTER(convert_ldc_w);
DECLARE_CONVERTER(convert_ldiv);
DECLARE_CONVERTER(convert_lload);
DECLARE_CONVERTER(convert_lload_n);
DECLARE_CONVERTER(convert_lload_n);
DECLARE_CONVERTER(convert_lload_n);
DECLARE_CONVERTER(convert_lload_n);
DECLARE_CONVERTER(convert_lmul);
DECLARE_CONVERTER(convert_lneg);
DECLARE_CONVERTER(convert_lor);
DECLARE_CONVERTER(convert_lrem);
DECLARE_CONVERTER(convert_lshl);
DECLARE_CONVERTER(convert_lshr);
DECLARE_CONVERTER(convert_lstore);
DECLARE_CONVERTER(convert_lstore_n);
DECLARE_CONVERTER(convert_lstore_n);
DECLARE_CONVERTER(convert_lstore_n);
DECLARE_CONVERTER(convert_lstore_n);
DECLARE_CONVERTER(convert_lsub);
DECLARE_CONVERTER(convert_lushr);
DECLARE_CONVERTER(convert_lxor);
DECLARE_CONVERTER(convert_monitor_enter);
DECLARE_CONVERTER(convert_monitor_exit);
DECLARE_CONVERTER(convert_multianewarray);
DECLARE_CONVERTER(convert_new);
DECLARE_CONVERTER(convert_newarray);
DECLARE_CONVERTER(convert_non_void_return);
DECLARE_CONVERTER(convert_pop);
DECLARE_CONVERTER(convert_pop);
DECLARE_CONVERTER(convert_putfield);
DECLARE_CONVERTER(convert_putstatic);
DECLARE_CONVERTER(convert_saload);
DECLARE_CONVERTER(convert_sastore);
DECLARE_CONVERTER(convert_sipush);
DECLARE_CONVERTER(convert_swap);
DECLARE_CONVERTER(convert_void_return);
DECLARE_CONVERTER(convert_xcmpg);
DECLARE_CONVERTER(convert_xcmpg);
DECLARE_CONVERTER(convert_xcmpl);
DECLARE_CONVERTER(convert_xcmpl);

typedef int (*convert_fn_t) (struct parse_context *);

static convert_fn_t converters[] = {
	[OPC_AALOAD] 		= convert_aaload,
	[OPC_AASTORE] 		= convert_aastore,
	[OPC_ACONST_NULL] 	= convert_aconst_null,
	[OPC_ALOAD] 		= convert_aload,
	[OPC_ALOAD_0] 		= convert_aload_n,
	[OPC_ALOAD_1] 		= convert_aload_n,
	[OPC_ALOAD_2] 		= convert_aload_n,
	[OPC_ALOAD_3] 		= convert_aload_n,
	[OPC_ANEWARRAY] 	= convert_anewarray,
	[OPC_ARETURN] 		= convert_non_void_return,
	[OPC_ARRAYLENGTH] 	= convert_arraylength,
	[OPC_ASTORE] 		= convert_astore,
	[OPC_ASTORE_0] 		= convert_astore_n,
	[OPC_ASTORE_1] 		= convert_astore_n,
	[OPC_ASTORE_2] 		= convert_astore_n,
	[OPC_ASTORE_3] 		= convert_astore_n,
	[OPC_ATHROW] 		= convert_athrow,
	[OPC_BALOAD] 		= convert_baload,
	[OPC_BASTORE] 		= convert_bastore,
	[OPC_BIPUSH] 		= convert_bipush,
	[OPC_CALOAD] 		= convert_caload,
	[OPC_CASTORE] 		= convert_castore,
	[OPC_CHECKCAST] 	= convert_checkcast,
	[OPC_D2F] 		= convert_d2f,
	[OPC_D2I] 		= convert_d2i,
	[OPC_D2L] 		= convert_d2l,
	[OPC_DADD] 		= convert_dadd,
	[OPC_DALOAD] 		= convert_daload,
	[OPC_DASTORE] 		= convert_dastore,
	[OPC_DCMPG] 		= convert_xcmpg,
	[OPC_DCMPL] 		= convert_xcmpl,
	[OPC_DCONST_0] 		= convert_dconst,
	[OPC_DCONST_1] 		= convert_dconst,
	[OPC_DDIV] 		= convert_ddiv,
	[OPC_DLOAD] 		= convert_dload,
	[OPC_DLOAD_0] 		= convert_dload_n,
	[OPC_DLOAD_1] 		= convert_dload_n,
	[OPC_DLOAD_2] 		= convert_dload_n,
	[OPC_DLOAD_3] 		= convert_dload_n,
	[OPC_DMUL] 		= convert_dmul,
	[OPC_DNEG] 		= convert_dneg,
	[OPC_DREM] 		= convert_drem,
	[OPC_DRETURN] 		= convert_non_void_return,
	[OPC_DSTORE] 		= convert_dstore,
	[OPC_DSTORE_0] 		= convert_dstore_n,
	[OPC_DSTORE_1] 		= convert_dstore_n,
	[OPC_DSTORE_2] 		= convert_dstore_n,
	[OPC_DSTORE_3] 		= convert_dstore_n,
	[OPC_DSUB] 		= convert_dsub,
	[OPC_DUP2] 		= convert_dup2,
	[OPC_DUP2_X1] 		= convert_dup2_x1,
	[OPC_DUP2_X2] 		= convert_dup2_x2,
	[OPC_DUP] 		= convert_dup,
	[OPC_DUP_X1] 		= convert_dup_x1,
	[OPC_DUP_X2] 		= convert_dup_x2,
	[OPC_F2D] 		= convert_f2d,
	[OPC_F2I] 		= convert_f2i,
	[OPC_F2L] 		= convert_f2l,
	[OPC_FADD] 		= convert_fadd,
	[OPC_FALOAD] 		= convert_faload,
	[OPC_FASTORE] 		= convert_fastore,
	[OPC_FCMPG] 		= convert_xcmpg,
	[OPC_FCMPL] 		= convert_xcmpl,
	[OPC_FCONST_0] 		= convert_fconst,
	[OPC_FCONST_1] 		= convert_fconst,
	[OPC_FCONST_2] 		= convert_fconst,
	[OPC_FDIV] 		= convert_fdiv,
	[OPC_FLOAD] 		= convert_fload,
	[OPC_FLOAD_0] 		= convert_fload_n,
	[OPC_FLOAD_1] 		= convert_fload_n,
	[OPC_FLOAD_2] 		= convert_fload_n,
	[OPC_FLOAD_3] 		= convert_fload_n,
	[OPC_FMUL] 		= convert_fmul,
	[OPC_FNEG] 		= convert_fneg,
	[OPC_FREM] 		= convert_frem,
	[OPC_FRETURN] 		= convert_non_void_return,
	[OPC_FSTORE] 		= convert_fstore,
	[OPC_FSTORE_0] 		= convert_fstore_n,
	[OPC_FSTORE_1] 		= convert_fstore_n,
	[OPC_FSTORE_2] 		= convert_fstore_n,
	[OPC_FSTORE_3] 		= convert_fstore_n,
	[OPC_FSUB] 		= convert_fsub,
	[OPC_GETFIELD] 		= convert_getfield,
	[OPC_GETSTATIC] 	= convert_getstatic,
	[OPC_GOTO] 		= convert_goto,
	[OPC_I2B] 		= convert_i2b,
	[OPC_I2C] 		= convert_i2c,
	[OPC_I2D] 		= convert_i2d,
	[OPC_I2F] 		= convert_i2f,
	[OPC_I2L] 		= convert_i2l,
	[OPC_I2S] 		= convert_i2s,
	[OPC_IADD] 		= convert_iadd,
	[OPC_IALOAD] 		= convert_iaload,
	[OPC_IAND] 		= convert_iand,
	[OPC_IASTORE] 		= convert_iastore,
	[OPC_ICONST_0] 		= convert_iconst,
	[OPC_ICONST_1] 		= convert_iconst,
	[OPC_ICONST_2] 		= convert_iconst,
	[OPC_ICONST_3] 		= convert_iconst,
	[OPC_ICONST_4] 		= convert_iconst,
	[OPC_ICONST_5] 		= convert_iconst,
	[OPC_ICONST_M1] 	= convert_iconst,
	[OPC_IDIV] 		= convert_idiv,
	[OPC_IFEQ] 		= convert_ifeq,
	[OPC_IFGE] 		= convert_ifge,
	[OPC_IFGT] 		= convert_ifgt,
	[OPC_IFLE] 		= convert_ifle,
	[OPC_IFLT] 		= convert_iflt,
	[OPC_IFNE] 		= convert_ifne,
	[OPC_IFNONNULL] 	= convert_ifnonnull,
	[OPC_IFNULL] 		= convert_ifnull,
	[OPC_IF_ACMPEQ] 	= convert_if_acmpeq,
	[OPC_IF_ACMPNE] 	= convert_if_acmpne,
	[OPC_IF_ICMPEQ] 	= convert_if_icmpeq,
	[OPC_IF_ICMPGE] 	= convert_if_icmpge,
	[OPC_IF_ICMPGT] 	= convert_if_icmpgt,
	[OPC_IF_ICMPLE] 	= convert_if_icmple,
	[OPC_IF_ICMPLT] 	= convert_if_icmplt,
	[OPC_IF_ICMPNE] 	= convert_if_icmpne,
	[OPC_IINC] 		= convert_iinc,
	[OPC_ILOAD] 		= convert_iload,
	[OPC_ILOAD_0] 		= convert_iload_n,
	[OPC_ILOAD_1] 		= convert_iload_n,
	[OPC_ILOAD_2] 		= convert_iload_n,
	[OPC_ILOAD_3] 		= convert_iload_n,
	[OPC_IMUL] 		= convert_imul,
	[OPC_INEG] 		= convert_ineg,
	[OPC_INSTANCEOF] 	= convert_instanceof,
	[OPC_INVOKESPECIAL] 	= convert_invokespecial,
	[OPC_INVOKESTATIC] 	= convert_invokestatic,
	[OPC_INVOKEVIRTUAL] 	= convert_invokevirtual,
	[OPC_IOR] 		= convert_ior,
	[OPC_IREM] 		= convert_irem,
	[OPC_IRETURN] 		= convert_non_void_return,
	[OPC_ISHL] 		= convert_ishl,
	[OPC_ISHR] 		= convert_ishr,
	[OPC_ISTORE] 		= convert_istore,
	[OPC_ISTORE_0] 		= convert_istore_n,
	[OPC_ISTORE_1] 		= convert_istore_n,
	[OPC_ISTORE_2] 		= convert_istore_n,
	[OPC_ISTORE_3] 		= convert_istore_n,
	[OPC_ISUB] 		= convert_isub,
	[OPC_IUSHR] 		= convert_iushr,
	[OPC_IXOR] 		= convert_ixor,
	[OPC_L2D] 		= convert_l2d,
	[OPC_L2F] 		= convert_l2f,
	[OPC_L2I] 		= convert_l2i,
	[OPC_LADD] 		= convert_ladd,
	[OPC_LALOAD] 		= convert_laload,
	[OPC_LAND] 		= convert_land,
	[OPC_LASTORE] 		= convert_lastore,
	[OPC_LCMP] 		= convert_lcmp,
	[OPC_LCONST_0] 		= convert_lconst,
	[OPC_LCONST_1] 		= convert_lconst,
	[OPC_LDC2_W] 		= convert_ldc2_w,
	[OPC_LDC] 		= convert_ldc,
	[OPC_LDC_W] 		= convert_ldc_w,
	[OPC_LDIV] 		= convert_ldiv,
	[OPC_LLOAD] 		= convert_lload,
	[OPC_LLOAD_0] 		= convert_lload_n,
	[OPC_LLOAD_1] 		= convert_lload_n,
	[OPC_LLOAD_2] 		= convert_lload_n,
	[OPC_LLOAD_3] 		= convert_lload_n,
	[OPC_LMUL] 		= convert_lmul,
	[OPC_LNEG] 		= convert_lneg,
	[OPC_LOR] 		= convert_lor,
	[OPC_LREM] 		= convert_lrem,
	[OPC_LRETURN] 		= convert_non_void_return,
	[OPC_LSHL] 		= convert_lshl,
	[OPC_LSHR] 		= convert_lshr,
	[OPC_LSTORE] 		= convert_lstore,
	[OPC_LSTORE_0] 		= convert_lstore_n,
	[OPC_LSTORE_1] 		= convert_lstore_n,
	[OPC_LSTORE_2] 		= convert_lstore_n,
	[OPC_LSTORE_3] 		= convert_lstore_n,
	[OPC_LSUB] 		= convert_lsub,
	[OPC_LUSHR] 		= convert_lushr,
	[OPC_LXOR] 		= convert_lxor,
	[OPC_MONITORENTER] 	= convert_monitor_enter,
	[OPC_MONITOREXIT] 	= convert_monitor_exit,
	[OPC_MULTIANEWARRAY] 	= convert_multianewarray,
	[OPC_NEWARRAY] 		= convert_newarray,
	[OPC_NEW] 		= convert_new,
	[OPC_NOP] 		= convert_nop,
	[OPC_POP2] 		= convert_pop,
	[OPC_POP] 		= convert_pop,
	[OPC_PUTFIELD] 		= convert_putfield,
	[OPC_PUTSTATIC] 	= convert_putstatic,
	[OPC_RETURN] 		= convert_void_return,
	[OPC_SALOAD] 		= convert_saload,
	[OPC_SASTORE] 		= convert_sastore,
	[OPC_SIPUSH] 		= convert_sipush,
	[OPC_SWAP] 		= convert_swap,
};

void convert_expression(struct parse_context *ctx, struct expression *expr)
{
	expr->bytecode_offset = ctx->offset;
	stack_push(ctx->bb->mimic_stack, expr);
}

static void
do_convert_statement(struct basic_block *bb, struct statement *stmt, unsigned short bc_offset)
{
	/*
	 * Some expressions do not go through convert_expression()
	 * so we need to set their bytecode_offset here if it is not set.
	 */
	switch (stmt_type(stmt)) {
	case STMT_STORE:
		tree_patch_bc_offset(stmt->store_dest, bc_offset);
		tree_patch_bc_offset(stmt->store_src, bc_offset);
		break;
	case STMT_IF:
		tree_patch_bc_offset(stmt->if_conditional, bc_offset);
		break;
	case STMT_RETURN:
		tree_patch_bc_offset(stmt->return_value, bc_offset);
		break;
	case STMT_EXPRESSION:
	case STMT_ARRAY_CHECK:
		tree_patch_bc_offset(stmt->expression, bc_offset);
		break;
	case STMT_ATHROW:
		tree_patch_bc_offset(stmt->exception_ref, bc_offset);
		break;
	default: ;
	}

	stmt->bytecode_offset = bc_offset;
	bb_add_stmt(bb, stmt);
}

void convert_statement(struct parse_context *ctx, struct statement *stmt)
{
	do_convert_statement(ctx->bb, stmt, ctx->offset);
}

static int spill_expression(struct basic_block *bb,
			    struct stack *reload_stack,
			    struct expression *expr)
{
	struct compilation_unit *cu = bb->b_parent;
	struct var_info *tmp_low, *tmp_high;
	struct statement *store, *branch;
	struct expression *tmp;

	tmp_low = get_var(cu);
	if (expr->vm_type == J_LONG)
		tmp_high = get_var(cu);
	else
		tmp_high = NULL;

	tmp = temporary_expr(expr->vm_type, tmp_high, tmp_low);
	if (!tmp)
		return -ENOMEM;

	store = alloc_statement(STMT_STORE);
	if (!store)
		return -ENOMEM;

	store->store_dest = &tmp->node;
	store->store_src = &expr->node;

	if (bb->has_branch)
		branch = bb_remove_last_stmt(bb);
	else
		branch = NULL;

	/*
	 * Insert spill store to the current basic block.
	 */
	do_convert_statement(bb, store, bb->end);

	if (branch)
		bb_add_stmt(bb, branch);

	/*
	 * And add a reload expression that is put on the mimic stack of
	 * successor basic blocks.
	 */
	stack_push(reload_stack, tmp);

	return 0;
}

static struct stack *spill_mimic_stack(struct basic_block *bb)
{
	struct stack *reload_stack;

	reload_stack = alloc_stack();
	if (!reload_stack)
		return NULL;

	/*
	 * The reload stack contains elements in reverse order of the mimic
	 * stack.
	 */
	while (!stack_is_empty(bb->mimic_stack)) {
		struct expression *expr;
		int err;

		expr = stack_pop(bb->mimic_stack);

		err = spill_expression(bb, reload_stack, expr);
		if (err)
			goto error_oom;
	}
	return reload_stack;
error_oom:
	free_stack(reload_stack);
	return NULL;
}

static int do_convert_bb_to_ir(struct basic_block *bb)
{
	struct compilation_unit *cu = bb->b_parent;
	struct bytecode_buffer buffer = { };
	struct parse_context ctx = {
		.buffer = &buffer,
		.cu = cu,
		.bb = bb,
		.code = cu->method->jit_code,
	};
	int err = 0;

	buffer.buffer = cu->method->jit_code;
	buffer.pos = bb->start;

	if (bb->is_eh)
		stack_push(bb->mimic_stack, exception_ref_expr());

	while (buffer.pos < bb->end) {
		convert_fn_t convert;

		ctx.offset = ctx.buffer->pos;	/* this is fragile */
		ctx.opc = bytecode_read_u8(ctx.buffer);

		if (ctx.opc >= ARRAY_SIZE(converters)) {
			err = -EINVAL;
			break;
		}

		convert = converters[ctx.opc];
		if (!convert) {
			err = -EINVAL;
			break;
		}

		err = convert(&ctx);
		if (err)
			break;
	}
	return err;
}

static int convert_bb_to_ir(struct basic_block *bb)
{
	struct stack *reload_stack;
	unsigned int i;
	int err;

	if (bb->is_converted)
		return 0;

	err = do_convert_bb_to_ir(bb);
	if (err)
		return err;

	bb->is_converted = true;

	/*
	 * If there are no successors, avoid spilling mimic stack contents.
	 */
	if (bb->nr_successors == 0)
		goto out;

	/*
	 * The operand stack can be non-empty at the entry or exit of a basic
	 * block because of, for example, ternary operator. To guarantee that
	 * the mimic stack operands are the same at the merge points of all
	 * paths, we spill any remaining values at the end of a basic block in
	 * memory locations and initialize the mimic stack of any successor
	 * basic blocks to load those values from memory.
	 */
	reload_stack = spill_mimic_stack(bb);
	if (!reload_stack)
		return -ENOMEM;

	while (!stack_is_empty(reload_stack)) {
		struct expression *expr = stack_pop(reload_stack);

		for (i = 0; i < bb->nr_successors; i++) {
			struct basic_block *s = bb->successors[i];

			expr_get(expr);

			stack_push(s->mimic_stack, expr);
		}
		expr_put(expr);
	}
	free_stack(reload_stack);

	for (i = 0; i < bb->nr_successors; i++) {
		struct basic_block *s = bb->successors[i];

		err = convert_bb_to_ir(s);
		if (err)
			break;
	}
out:
	return err;
}

/**
 *	convert_to_ir - Convert bytecode to intermediate representation.
 *	@compilation_unit: compilation unit to convert.
 *
 *	This function converts bytecode in a compilation unit to intermediate
 *	representation of the JIT compiler.
 *
 *	Returns zero if conversion succeeded; otherwise returns a negative
 * 	integer.
 */
int convert_to_ir(struct compilation_unit *cu)
{
	struct basic_block *bb;
	int err;

	err = convert_bb_to_ir(cu->entry_bb);
	if (err)
		return err;

	/*
	 * A compilation unit can have exception handler basic blocks that are
	 * not reachable from the entry basic block. Therefore, make sure we've
	 * really converted all basic blocks.
	 */
	for_each_basic_block(bb, &cu->bb_list) {
		err = convert_bb_to_ir(bb);
		if (err)
			break;
	}
	return err;
}
