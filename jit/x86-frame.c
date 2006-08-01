/*
 * Copyright (C) 2006  Pekka Enberg
 *
 * This file is released under the GPL version 2. Please refer to the file
 * LICENSE for details.
 *
 * The file contains functions for managing the native IA-32 function stack
 * frame.
 */

#include <jit/expression.h>
#include <jit/x86-frame.h>
#include <vm/vm.h>

#define LOCAL_START (2 * sizeof(unsigned long))

long frame_local_offset(struct methodblock *method, struct expression *local)
{
	unsigned long idx, nr_args;

	idx = local->local_index;
	nr_args = method->args_count;

	if (idx < nr_args)
		return (idx * sizeof(unsigned long)) + LOCAL_START;

	return 0 - ((idx - nr_args+1) * sizeof(unsigned long));
}