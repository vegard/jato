/*
 * Copyright (c) 2009 Tomasz Grabiec
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

#include <stdio.h>

#include "jit/bc-offset-mapping.h"
#include "jit/statement.h"
#include "jit/expression.h"

#include "arch/instruction.h"

#include "lib/buffer.h"

/**
 * tree_patch_bc_offset - sets bytecode_offset field of a tree node
 *                        unless it is already set.
 * @node: a tree node
 * @offset: bytecode offset to set.
 */
void tree_patch_bc_offset(struct tree_node *node, unsigned long bc_offset)
{
	int i;

	if (node->bytecode_offset != BC_OFFSET_UNKNOWN)
		return;

	node->bytecode_offset = bc_offset;

	for (i = 0; i < node_nr_kids(node); i++)
		tree_patch_bc_offset(node->kids[i], bc_offset);
}

/**
 * native_ptr_to_bytecode_offset - translates native instruction pointer
 *                                 to bytecode offset from which given
 *                                 instruction originates.
 * @cu: compilation unit of method containing @native_ptr.
 * @native_ptr: native instruction pointer to be translated.
 */
unsigned long native_ptr_to_bytecode_offset(struct compilation_unit *cu,
					    unsigned char *native_ptr)
{
	unsigned long method_addr = (unsigned long)buffer_ptr(cu->objcode);
	unsigned long offset;
	struct basic_block *bb;
	struct insn *insn;
	struct insn *prev_insn = NULL;

	if ((unsigned long)native_ptr < method_addr)
		return BC_OFFSET_UNKNOWN;

	offset = (unsigned long)native_ptr - method_addr;

	for_each_basic_block(bb, &cu->bb_list) {
		for_each_insn(insn, &bb->insn_list) {
			if (insn->mach_offset == offset)
				return insn->bytecode_offset;

			if (insn->mach_offset > offset) {
				if (prev_insn &&
				    prev_insn->mach_offset <= offset)
					return prev_insn->bytecode_offset;
				break;
			}

			prev_insn = insn;
		}
	}

	return BC_OFFSET_UNKNOWN;
}

void print_bytecode_offset(unsigned long bytecode_offset, struct string *str)
{
	if (bytecode_offset == BC_OFFSET_UNKNOWN)
		str_append(str, "?");
	else {
		static char buf[32];
		sprintf(buf, "%ld", bytecode_offset);
		str_append(str, buf);
	}
}

bool all_insn_have_bytecode_offset(struct compilation_unit *cu)
{
	struct basic_block *bb;
	struct insn *insn;

	for_each_basic_block(bb, &cu->bb_list) {
		for_each_insn(insn, &bb->insn_list) {
			if (insn->bytecode_offset == BC_OFFSET_UNKNOWN)
				return false;
		}
	}

	return true;
}

int bytecode_offset_to_line_no(struct vm_method *mb, unsigned long bc_offset)
{
	struct cafebabe_line_number_table_entry *table;
	int length;
	int i;

	table = mb->line_number_table_attribute.line_number_table;
	length = mb->line_number_table_attribute.line_number_table_length;

	if(bc_offset == BC_OFFSET_UNKNOWN || length == 0)
		return -1;

	i = length - 1;
	while (i && bc_offset < table[i].start_pc)
		i--;

	return table[i].line_number;
}
