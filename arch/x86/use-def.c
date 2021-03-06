/*
 * Copyright (C) 2007  Pekka Enberg
 *
 * This file is released under the GPL version 2. Please refer to the file
 * LICENSE for details.
 */

#include "jit/compilation-unit.h"
#include "arch/instruction.h"

enum {
	DEF_DST		= 1,
	DEF_SRC		= 2,
	DEF_NONE	= 4,
	DEF_xAX		= 8,
	DEF_xCX		= 16,
	DEF_xDX		= 32,
	USE_DST		= 64,
	USE_IDX_DST	= 128,	/* destination operand is memindex */
	USE_IDX_SRC	= 256,	/* source operand is memindex */
	USE_NONE	= 512,
	USE_SRC		= 1024,
	USE_FP		= 2048,	/* frame pointer */

#ifdef CONFIG_X86_32
	DEF_EAX		= DEF_xAX,
	DEF_ECX		= DEF_xCX,
	DEF_EDX		= DEF_xDX,
#else
	DEF_RAX		= DEF_xAX,
	DEF_RCX		= DEF_xCX,
	DEF_RDX		= DEF_xDX,
#endif
};

struct insn_info {
	unsigned long flags;
};

#define DECLARE_INFO(_type, _flags) [_type] = { .flags = _flags }

static struct insn_info insn_infos[] = {
	DECLARE_INFO(INSN_CALL_REL, USE_NONE | DEF_xAX | DEF_xCX | DEF_xDX),
	DECLARE_INFO(INSN_JE_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JGE_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JG_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JLE_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JL_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JMP_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JNE_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_RET, USE_NONE | DEF_NONE),

#ifdef CONFIG_X86_32
	DECLARE_INFO(INSN_ADC_IMM_REG, DEF_DST),
	DECLARE_INFO(INSN_ADC_MEMBASE_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_ADC_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_ADD_IMM_REG, DEF_DST),
	DECLARE_INFO(INSN_ADD_MEMBASE_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_ADD_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_AND_MEMBASE_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_AND_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_CALL_REG, USE_SRC | DEF_EAX | DEF_ECX | DEF_EDX),
	DECLARE_INFO(INSN_CLTD_REG_REG, USE_SRC | DEF_SRC | DEF_DST),
	DECLARE_INFO(INSN_CMP_IMM_REG, USE_DST),
	DECLARE_INFO(INSN_CMP_MEMBASE_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_DIV_MEMBASE_REG, USE_SRC | DEF_DST | DEF_EAX | DEF_EDX),
	DECLARE_INFO(INSN_DIV_REG_REG, USE_SRC | DEF_DST | DEF_EAX | DEF_EDX),
	DECLARE_INFO(INSN_FADD_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_FSUB_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_FMUL_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_FDIV_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_FLD_MEMBASE, USE_SRC),
	DECLARE_INFO(INSN_FSTP_MEMBASE, USE_SRC),
	DECLARE_INFO(INSN_CONV_GPR_TO_FPU, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_CONV_FPU_TO_GPR, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_MOV_MEMBASE_XMM, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_MOV_XMM_MEMBASE, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_JE_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JGE_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JG_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JLE_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JL_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JMP_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_JNE_BRANCH, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_MOV_IMM_MEMBASE, USE_DST),
	DECLARE_INFO(INSN_MOV_IMM_REG, DEF_DST),
	DECLARE_INFO(INSN_MOV_MEMLOCAL_REG, USE_FP | DEF_DST),
	DECLARE_INFO(INSN_MOV_MEMLOCAL_FREG, USE_FP | DEF_DST),
	DECLARE_INFO(INSN_MOV_MEMBASE_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_MOV_MEMDISP_REG, USE_NONE | DEF_DST),
	DECLARE_INFO(INSN_MOV_REG_MEMDISP, USE_NONE | DEF_SRC),
	DECLARE_INFO(INSN_MOV_THREAD_LOCAL_MEMDISP_REG, USE_NONE | DEF_DST),
	DECLARE_INFO(INSN_MOV_MEMINDEX_REG, USE_SRC | USE_IDX_SRC | DEF_DST),
	DECLARE_INFO(INSN_MOV_REG_MEMINDEX, USE_SRC | USE_DST | USE_IDX_DST | DEF_NONE),
	DECLARE_INFO(INSN_MOV_REG_MEMLOCAL, USE_SRC),
	DECLARE_INFO(INSN_MOV_FREG_MEMLOCAL, USE_SRC),
	DECLARE_INFO(INSN_MOV_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_MOVSX_8_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_MOVSX_16_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_MUL_MEMBASE_EAX, USE_SRC | DEF_DST | DEF_EDX | DEF_EAX),
	DECLARE_INFO(INSN_MUL_REG_EAX, USE_SRC | DEF_DST | DEF_EDX | DEF_EAX),
	DECLARE_INFO(INSN_MUL_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_NEG_REG, USE_SRC | DEF_SRC),
	DECLARE_INFO(INSN_OR_MEMBASE_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_OR_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_PUSH_IMM, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN_PUSH_REG, USE_SRC | DEF_NONE),
	DECLARE_INFO(INSN_POP_REG, USE_NONE | DEF_SRC),
	DECLARE_INFO(INSN_SAR_IMM_REG, DEF_DST),
	DECLARE_INFO(INSN_SAR_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_SBB_IMM_REG, USE_NONE | DEF_DST),
	DECLARE_INFO(INSN_SBB_MEMBASE_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_SBB_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_SHL_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_SHR_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_SUB_IMM_REG, USE_NONE | DEF_DST),
	DECLARE_INFO(INSN_SUB_MEMBASE_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_SUB_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_TEST_MEMBASE_REG, USE_SRC | USE_DST | DEF_NONE),
	DECLARE_INFO(INSN_XOR_MEMBASE_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_XOR_IMM_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_XOR_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN_XOR_XMM_REG_REG, USE_SRC | DEF_DST),
#else /* CONFIG_X86_32 */
	DECLARE_INFO(INSN32_ADD_IMM_REG, DEF_DST),
	DECLARE_INFO(INSN32_MOV_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN32_SUB_IMM_REG, USE_NONE | DEF_DST),

	DECLARE_INFO(INSN64_ADD_IMM_REG, DEF_DST),
	DECLARE_INFO(INSN64_MOV_REG_REG, USE_SRC | DEF_DST),
	DECLARE_INFO(INSN64_PUSH_IMM, USE_NONE | DEF_NONE),
	DECLARE_INFO(INSN64_PUSH_REG, USE_SRC | DEF_NONE),
	DECLARE_INFO(INSN64_POP_REG, USE_NONE | DEF_SRC),
	DECLARE_INFO(INSN64_SUB_IMM_REG, USE_NONE | DEF_DST),
#endif
};

static inline struct insn_info *get_info(struct insn *insn)
{
	return insn_infos + insn->type;
}

struct mach_reg_def {
	enum machine_reg reg;
	int def;
};

static struct mach_reg_def checkregs[] = {
	{ REG_xAX, DEF_xAX },
	{ REG_xCX, DEF_xCX },
	{ REG_xDX, DEF_xDX },
};


bool insn_defs(struct insn *insn, struct var_info *var)
{
	struct insn_info *info;
	unsigned long vreg;
	unsigned int i;

	info = get_info(insn);
	vreg = var->vreg;

	if (info->flags & DEF_SRC) {
		if (is_vreg(&insn->src.reg, vreg))
			return true;
	}

	if (info->flags & DEF_DST) {
		if (is_vreg(&insn->dest.reg, vreg))
			return true;
	}

	for (i = 0; i < ARRAY_SIZE(checkregs); i++) {
		if (info->flags & checkregs[i].def &&
				var->interval->reg == checkregs[i].reg)
			return true;
	}

	return false;
}

bool insn_uses(struct insn *insn, struct var_info *var)
{
	struct insn_info *info;
	unsigned long vreg;

	info = get_info(insn);
	vreg = var->vreg;

	if (info->flags & USE_SRC) {
		if (is_vreg(&insn->src.reg, vreg))
			return true;
	}

	if (info->flags & USE_DST) {
		if (is_vreg(&insn->dest.reg, vreg))
			return true;
	}

	if (info->flags & USE_IDX_SRC) {
		if (is_vreg(&insn->src.index_reg, vreg))
			return true;
	}

	if (info->flags & USE_IDX_DST) {
		if (is_vreg(&insn->dest.index_reg, vreg))
			return true;
	}

	return false;
}
