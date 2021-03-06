/*
 * Copyright (c) 2009  Arthur Huillet
 * Copyright (c) 2006-2008  Pekka Enberg
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

#include "jit/compilation-unit.h"
#include "jit/basic-block.h"
#include "jit/lir-printer.h"
#include "jit/emit-code.h"
#include "jit/statement.h"
#include "jit/compiler.h"

#include "lib/list.h"
#include "lib/buffer.h"
#include "vm/method.h"
#include "lib/string.h"

#include "arch/instruction.h"
#include "arch/memory.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

static inline int print_imm(struct string *str, struct operand *op)
{
	return str_append(str, "$0x%lx", op->imm);
}

static inline int print_reg(struct string *str, struct operand *op)
{
	struct live_interval *interval = op->reg.interval;

	if (interval->fixed_reg)
		return str_append(str, "r%lu=%s", interval->var_info->vreg,
				  reg_name(interval->reg));
	else
		return str_append(str, "r%lu", interval->var_info->vreg);
}

static inline int print_membase(struct string *str, struct operand *op)
{
	return str_append(str, "$0x%lx(r%lu)", op->disp, op->base_reg.interval->var_info->vreg);
}

static inline int print_memlocal(struct string *str, struct operand *op)
{
	return str_append(str, "@%ld(bp)", op->slot->index);
}

static inline int print_memindex(struct string *str, struct operand *op)
{
	return str_append(str, "(r%lu, r%lu, %d)", op->base_reg.interval->var_info->vreg, op->index_reg.interval->var_info->vreg, op->shift);
}

static inline int print_rel(struct string *str, struct operand *op)
{
	return str_append(str, "$0x%lx", op->rel);
}

static inline int print_branch(struct string *str, struct operand *op)
{
	return str_append(str, "bb 0x%lx", op->branch_target);
}

static int print_imm_reg(struct string *str, struct insn *insn)
{
	print_imm(str, &insn->src);
	str_append(str, ", ");
	return print_reg(str, &insn->dest);
}

static int print_imm_membase(struct string *str, struct insn *insn)
{
	print_imm(str, &insn->src);
	str_append(str, ", ");
	return print_membase(str, &insn->dest);
}

static int print_membase_reg(struct string *str, struct insn *insn)
{
	print_membase(str, &insn->src);
	str_append(str, ", ");
	return print_reg(str, &insn->dest);
}

static int print_memdisp_reg(struct string *str, struct insn *insn)
{
	str_append(str, "(");
	print_imm(str, &insn->src);
	str_append(str, "), ");
	return print_reg(str, &insn->dest);
}

static int print_reg_memdisp(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", (");
	print_imm(str, &insn->dest);
	return str_append(str, ")");
}

static int print_tlmemdisp_reg(struct string *str, struct insn *insn)
{
	str_append(str, "gs:(");
	print_imm(str, &insn->src);
	str_append(str, "), ");
	return print_reg(str, &insn->dest);
}

static int print_reg_membase(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", ");
	return print_membase(str, &insn->dest);
}

static int print_memlocal_reg(struct string *str, struct insn *insn)
{
	print_memlocal(str, &insn->src);
	str_append(str, ", ");
	return print_reg(str, &insn->dest);
}

static int print_memindex_reg(struct string *str, struct insn *insn)
{
	print_memindex(str, &insn->src);
	str_append(str, ", ");
	return print_reg(str, &insn->dest);
}

static int print_reg_memlocal(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", ");
	return print_memlocal(str, &insn->dest);
}

static int print_reg_memindex(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", ");
	return print_memindex(str, &insn->dest);
}

static int print_reg_reg(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", ");
	return print_reg(str, &insn->dest);
}

#define print_func_name(str) str_append(str, "%-20s ", __func__ + 6)

static int print_adc_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_adc_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_adc_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_add_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_add_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_add_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_fadd_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_fsub_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_fmul_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_fdiv_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_fld_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase(str, &insn->operand);
}

static int print_fstp_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase(str, &insn->operand);
}

static int print_and_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_and_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_call_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	str_append(str, "(");
	print_reg(str, &insn->operand);
	return str_append(str, ")");
}

static int print_call_rel(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_rel(str, &insn->operand);
}

static int print_cltd_reg_reg(struct string *str, struct insn *insn)	/* CDQ in Intel manuals*/
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_cmp_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_cmp_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_cmp_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_div_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_div_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_mov_membase_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_mov_xmm_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_membase(str, insn);
}

static int print_conv_fpu_to_gpr(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_conv_gpr_to_fpu(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_je_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jge_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jg_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jle_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jl_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jmp_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jne_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_mov_imm_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_membase(str, insn);
}

static int print_mov_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_mov_memlocal_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal_reg(str, insn);
}

static int print_mov_memlocal_freg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal_reg(str, insn);
}

static int print_mov_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_mov_memdisp_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memdisp_reg(str, insn);
}

static int print_mov_reg_memdisp(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memdisp(str, insn);
}

static int print_mov_tlmemdisp_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_tlmemdisp_reg(str, insn);
}

static int print_mov_memindex_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memindex_reg(str, insn);
}

static int print_mov_reg_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_membase(str, insn);
}

static int print_mov_reg_memindex(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memindex(str, insn);
}

static int print_mov_reg_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memlocal(str, insn);
}

static int print_mov_freg_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memlocal(str, insn);
}

static int print_mov_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_movsx_8_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	print_reg_reg(str, insn);
	return str_append(str, "(8bit->32bit)");
}

static int print_movsx_16_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	print_reg_reg(str, insn);
	return str_append(str, "(16bit->32bit)");
}

static int print_mul_membase_eax(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_mul_reg_eax(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_mul_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_neg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg(str, &insn->operand);
}

static int print_or_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_or_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_push_imm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm(str, &insn->operand);
}

static int print_push_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg(str, &insn->operand);
}

static int print_pop_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal(str, &insn->operand);
}

static int print_pop_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg(str, &insn->operand);
}

static int print_ret(struct string *str, struct insn *insn)
{
	return print_func_name(str);
}

static int print_sar_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_sar_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_sbb_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_sbb_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_sbb_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_shl_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_shr_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_sub_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_sub_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_sub_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_test_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_xor_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_xor_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_xor_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_xor_xmm_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

typedef int (*print_insn_fn) (struct string *str, struct insn *insn);

static print_insn_fn insn_printers[] = {
	[INSN_ADC_IMM_REG] = print_adc_imm_reg,
	[INSN_ADC_MEMBASE_REG] = print_adc_membase_reg,
	[INSN_ADC_REG_REG] = print_adc_reg_reg,
	[INSN_ADD_IMM_REG] = print_add_imm_reg,
	[INSN_ADD_MEMBASE_REG] = print_add_membase_reg,
	[INSN_ADD_REG_REG] = print_add_reg_reg,
	[INSN_AND_MEMBASE_REG] = print_and_membase_reg,
	[INSN_AND_REG_REG] = print_and_reg_reg,
	[INSN_CALL_REG] = print_call_reg,
	[INSN_CALL_REL] = print_call_rel,
	[INSN_CLTD_REG_REG] = print_cltd_reg_reg,	/* CDQ in Intel manuals*/
	[INSN_CMP_IMM_REG] = print_cmp_imm_reg,
	[INSN_CMP_MEMBASE_REG] = print_cmp_membase_reg,
	[INSN_CMP_REG_REG] = print_cmp_reg_reg,
	[INSN_DIV_MEMBASE_REG] = print_div_membase_reg,
	[INSN_DIV_REG_REG] = print_div_reg_reg,
	[INSN_FADD_REG_REG] = print_fadd_reg_reg,
	[INSN_FSUB_REG_REG] = print_fsub_reg_reg,
	[INSN_FMUL_REG_REG] = print_fmul_reg_reg,
	[INSN_FDIV_REG_REG] = print_fdiv_reg_reg,
	[INSN_FLD_MEMBASE] = print_fld_membase,
	[INSN_FSTP_MEMBASE] = print_fstp_membase,
	[INSN_MOV_MEMBASE_XMM] = print_mov_membase_xmm,
	[INSN_MOV_XMM_MEMBASE] = print_mov_xmm_membase,
	[INSN_CONV_FPU_TO_GPR] = print_conv_fpu_to_gpr,
	[INSN_CONV_GPR_TO_FPU] = print_conv_gpr_to_fpu,
	[INSN_JE_BRANCH] = print_je_branch,
	[INSN_JGE_BRANCH] = print_jge_branch,
	[INSN_JG_BRANCH] = print_jg_branch,
	[INSN_JLE_BRANCH] = print_jle_branch,
	[INSN_JL_BRANCH] = print_jl_branch,
	[INSN_JMP_BRANCH] = print_jmp_branch,
	[INSN_JNE_BRANCH] = print_jne_branch,
	[INSN_MOV_IMM_MEMBASE] = print_mov_imm_membase,
	[INSN_MOV_IMM_REG] = print_mov_imm_reg,
	[INSN_MOV_MEMLOCAL_REG] = print_mov_memlocal_reg,
	[INSN_MOV_MEMLOCAL_FREG] = print_mov_memlocal_freg,
	[INSN_MOV_MEMBASE_REG] = print_mov_membase_reg,
	[INSN_MOV_MEMDISP_REG] = print_mov_memdisp_reg,
	[INSN_MOV_REG_MEMDISP] = print_mov_reg_memdisp,
	[INSN_MOV_THREAD_LOCAL_MEMDISP_REG] = print_mov_tlmemdisp_reg,
	[INSN_MOV_MEMINDEX_REG] = print_mov_memindex_reg,
	[INSN_MOV_REG_MEMBASE] = print_mov_reg_membase,
	[INSN_MOV_REG_MEMINDEX] = print_mov_reg_memindex,
	[INSN_MOV_REG_MEMLOCAL] = print_mov_reg_memlocal,
	[INSN_MOV_FREG_MEMLOCAL] = print_mov_freg_memlocal,
	[INSN_MOV_REG_REG] = print_mov_reg_reg,
	[INSN_MOVSX_8_REG_REG] = print_movsx_8_reg_reg,
	[INSN_MOVSX_16_REG_REG] = print_movsx_16_reg_reg,
	[INSN_MUL_MEMBASE_EAX] = print_mul_membase_eax,
	[INSN_MUL_REG_EAX] = print_mul_reg_eax,
	[INSN_MUL_REG_REG] = print_mul_reg_reg,
	[INSN_NEG_REG] = print_neg_reg,
	[INSN_OR_MEMBASE_REG] = print_or_membase_reg,
	[INSN_OR_REG_REG] = print_or_reg_reg,
	[INSN_PUSH_IMM] = print_push_imm,
	[INSN_PUSH_REG] = print_push_reg,
	[INSN_POP_MEMLOCAL] = print_pop_memlocal,
	[INSN_POP_REG] = print_pop_reg,
	[INSN_RET] = print_ret,
	[INSN_SAR_IMM_REG] = print_sar_imm_reg,
	[INSN_SAR_REG_REG] = print_sar_reg_reg,
	[INSN_SBB_IMM_REG] = print_sbb_imm_reg,
	[INSN_SBB_MEMBASE_REG] = print_sbb_membase_reg,
	[INSN_SBB_REG_REG] = print_sbb_reg_reg,
	[INSN_SHL_REG_REG] = print_shl_reg_reg,
	[INSN_SHR_REG_REG] = print_shr_reg_reg,
	[INSN_SUB_IMM_REG] = print_sub_imm_reg,
	[INSN_SUB_MEMBASE_REG] = print_sub_membase_reg,
	[INSN_SUB_REG_REG] = print_sub_reg_reg,
	[INSN_TEST_MEMBASE_REG] = print_test_membase_reg,
	[INSN_XOR_MEMBASE_REG] = print_xor_membase_reg,
	[INSN_XOR_IMM_REG] = print_xor_imm_reg,
	[INSN_XOR_REG_REG] = print_xor_reg_reg,
	[INSN_XOR_XMM_REG_REG] = print_xor_xmm_reg_reg,
};

int lir_print(struct insn *insn, struct string *str)
{
	print_insn_fn print = insn_printers[insn->type];

	return print(str, insn);
}
