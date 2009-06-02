/*
 * Compiles bytecode methods to machine code.
 *
 * Copyright (C) 2005-2007  Pekka Enberg
 *
 * This file is released under the GPL version 2. Please refer to the file
 * LICENSE for details.
 */

#include <jit/compilation-unit.h>
#include <jit/compiler.h>
#include <jit/statement.h>
#include <jit/bc-offset-mapping.h>
#include <jit/exception.h>

#include <vm/class.h>
#include <vm/method.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void compile_error(struct compilation_unit *cu, int err)
{
	struct vm_method *method = cu->method;
	struct vm_class *class = method->class;

	die("%s: Failed to compile method `%s' in class `%s', error: %i\n",
	       __func__, method->name, class->name, err);
}

int compile(struct compilation_unit *cu)
{
	int err;

	if (opt_trace_method)
		trace_method(cu);

	err = analyze_control_flow(cu);
	if (err)
		goto out;

	err = convert_to_ir(cu);
	if (err)
		goto out;

	err = sort_basic_blocks(cu);
	if (err)
		goto out;

	if (opt_trace_cfg)
		trace_cfg(cu);

	if (opt_trace_tree_ir)
		trace_tree_ir(cu);

	err = insert_exception_spill_insns(cu);
	if (err)
		goto out;

	err = select_instructions(cu);
	if (err)
		goto out;

	compute_insn_positions(cu);

	if (opt_trace_lir)
		trace_lir(cu);

	err = analyze_liveness(cu);
	if (err)
		goto out;

	if (opt_trace_liveness)
		trace_liveness(cu);

	err = allocate_registers(cu);
	if (err)
		goto out;

	if (opt_trace_regalloc)
		trace_regalloc(cu);

	err = insert_spill_reload_insns(cu);
	if (err)
		goto out;

	assert(all_insn_have_bytecode_offset(cu));

	err = emit_machine_code(cu);
	if (err)
		goto out;

	if (opt_trace_machine_code)
		trace_machine_code(cu);

	cu->is_compiled = true;

  out:
	if (err)
		compile_error(cu, err);

	return err;
}
