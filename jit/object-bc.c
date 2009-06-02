/*
 * Copyright (C) 2005-2006  Pekka Enberg
 *
 * This file is released under the GPL version 2. Please refer to the file
 * LICENSE for details.
 *
 * The file contains functions for converting Java bytecode arithmetic
 * instructions to immediate representation of the JIT compiler.
 */

#include <jit/bytecode-converters.h>
#include <jit/compiler.h>
#include <jit/statement.h>
#include <jit/args.h>

#include <vm/bytecode.h>
#include <vm/bytecodes.h>
#include <vm/field.h>
#include <vm/object.h>
#include <vm/stack.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

static char *class_name_to_array_name(const char *class_name)
{
	char *array_name = malloc(strlen(class_name) + 4);

	if (class_name[0] == '[') {
		strcpy(array_name, "[");
		strcat(array_name, class_name);
	} else {
		strcpy(array_name, "[L");
		strcat(array_name, class_name);
		strcat(array_name, ";");
	}

	return array_name;
}

static struct vm_object *class_to_array_class(struct vm_object *class)
{
	struct vm_object *array_class;
	const char *class_name;
	char *array_class_name;

	NOT_IMPLEMENTED;
	array_class = NULL;

#if 0
	class_name = CLASS_CB(class)->name;
	array_class_name = class_name_to_array_name(class_name);

	array_class = findArrayClassFromClass(array_class_name, class);

	free(array_class_name);
#endif

	return array_class;
}

static struct vm_field *lookup_field(struct parse_context *ctx)
{
	unsigned short index;

	index = bytecode_read_u16(ctx->buffer);

	return vm_class_resolve_field_recursive(ctx->cu->method->class, index);
}

int convert_getstatic(struct parse_context *ctx)
{
	struct expression *value;
	struct vm_field *fb;

	fb = lookup_field(ctx);
	if (!fb)
		return -EINVAL;

	value = class_field_expr(vm_field_type(fb), fb);
	if (!value)
		return -ENOMEM;

	convert_expression(ctx, value);
	return 0;
}

int convert_putstatic(struct parse_context *ctx)
{
	struct vm_field *fb;
	struct statement *store_stmt;
	struct expression *dest, *src;

	fb = lookup_field(ctx);
	if (!fb)
		return -EINVAL;

	src = stack_pop(ctx->bb->mimic_stack);
	dest = class_field_expr(vm_field_type(fb), fb);
	if (!dest)
		return -ENOMEM;
	
	store_stmt = alloc_statement(STMT_STORE);
	if (!store_stmt) {
		expr_put(dest);
		return -ENOMEM;
	}
	store_stmt->store_dest = &dest->node;
	store_stmt->store_src = &src->node;
	convert_statement(ctx, store_stmt);
	
	return 0;
}

int convert_getfield(struct parse_context *ctx)
{
	struct expression *objectref;
	struct expression *value;
	struct vm_field *fb;

	fb = lookup_field(ctx);
	if (!fb)
		return -EINVAL;

	objectref = stack_pop(ctx->bb->mimic_stack);

	value = instance_field_expr(vm_field_type(fb), fb, objectref);
	if (!value)
		return -ENOMEM;

	convert_expression(ctx, value);
	return 0;
}

int convert_putfield(struct parse_context *ctx)
{
	struct expression *dest, *src;
	struct statement *store_stmt;
	struct expression *objectref;
	struct vm_field *fb;

	fb = lookup_field(ctx);
	if (!fb)
		return -EINVAL;

	src = stack_pop(ctx->bb->mimic_stack);
	objectref = stack_pop(ctx->bb->mimic_stack);
	dest = instance_field_expr(vm_field_type(fb), fb, objectref);
	if (!dest)
		return -ENOMEM;
	
	store_stmt = alloc_statement(STMT_STORE);
	if (!store_stmt) {
		expr_put(dest);
		return -ENOMEM;
	}
	store_stmt->store_dest = &dest->node;
	store_stmt->store_src = &src->node;
	convert_statement(ctx, store_stmt);
	
	return 0;
}

int convert_array_load(struct parse_context *ctx, enum vm_type type)
{
	struct expression *index, *arrayref, *arrayref_nullcheck;
	struct expression *src_expr, *dest_expr;
	struct statement *store_stmt, *arraycheck;
	struct var_info *temporary;

	index = stack_pop(ctx->bb->mimic_stack);
	arrayref = stack_pop(ctx->bb->mimic_stack);

	store_stmt = alloc_statement(STMT_STORE);
	if (!store_stmt)
		goto failed;

	arrayref_nullcheck = null_check_expr(arrayref);
	if (!arrayref_nullcheck)
		goto failed_arrayref_nullcheck;

	src_expr = array_deref_expr(type, arrayref_nullcheck, index);

	temporary = get_var(ctx->cu);
	dest_expr = temporary_expr(type, NULL, temporary);
	
	store_stmt->store_src = &src_expr->node;
	store_stmt->store_dest = &dest_expr->node;

	expr_get(dest_expr);
	convert_expression(ctx, dest_expr);

	arraycheck = alloc_statement(STMT_ARRAY_CHECK);
	if (!arraycheck)
		goto failed_arraycheck;

	expr_get(src_expr);
	arraycheck->expression = &src_expr->node;

	convert_statement(ctx, arraycheck);
	convert_statement(ctx, store_stmt);

	return 0;

      failed_arraycheck:
      failed_arrayref_nullcheck:
	free_statement(store_stmt);
      failed:
	return -ENOMEM;
}

int convert_iaload(struct parse_context *ctx)
{
	return convert_array_load(ctx, J_INT);
}

int convert_laload(struct parse_context *ctx)
{
	return convert_array_load(ctx, J_LONG);
}

int convert_faload(struct parse_context *ctx)
{
	return convert_array_load(ctx, J_FLOAT);
}

int convert_daload(struct parse_context *ctx)
{
	return convert_array_load(ctx, J_DOUBLE);
}

int convert_aaload(struct parse_context *ctx)
{
	return convert_array_load(ctx, J_REFERENCE);
}

int convert_baload(struct parse_context *ctx)
{
	return convert_array_load(ctx, J_BYTE);
}

int convert_caload(struct parse_context *ctx)
{
	return convert_array_load(ctx, J_CHAR);
}

int convert_saload(struct parse_context *ctx)
{
	return convert_array_load(ctx, J_SHORT);
}

static int convert_array_store(struct parse_context *ctx, enum vm_type type)
{
	struct expression *value, *index, *arrayref, *arrayref_nullcheck;
	struct statement *store_stmt, *arraycheck;
	struct expression *src_expr, *dest_expr;

	value = stack_pop(ctx->bb->mimic_stack);
	index = stack_pop(ctx->bb->mimic_stack);
	arrayref = stack_pop(ctx->bb->mimic_stack);

	store_stmt = alloc_statement(STMT_STORE);
	if (!store_stmt)
		goto failed;

	arrayref_nullcheck = null_check_expr(arrayref);
	if (!arrayref_nullcheck)
		goto failed_arrayref_nullcheck;

	dest_expr = array_deref_expr(type, arrayref_nullcheck, index);
	src_expr = value;

	store_stmt->store_dest = &dest_expr->node;
	store_stmt->store_src = &src_expr->node;

	arraycheck = alloc_statement(STMT_ARRAY_CHECK);
	if (!arraycheck)
		goto failed_arraycheck;

	expr_get(dest_expr);
	arraycheck->expression = &dest_expr->node;

	convert_statement(ctx, arraycheck);
	convert_statement(ctx, store_stmt);

	return 0;

      failed_arraycheck:
      failed_arrayref_nullcheck:
	free_statement(store_stmt);
      failed:
	return -ENOMEM;
}

int convert_iastore(struct parse_context *ctx)
{
	return convert_array_store(ctx, J_INT);
}

int convert_lastore(struct parse_context *ctx)
{
	return convert_array_store(ctx, J_LONG);
}

int convert_fastore(struct parse_context *ctx)
{
	return convert_array_store(ctx, J_FLOAT);
}

int convert_dastore(struct parse_context *ctx)
{
	return convert_array_store(ctx, J_DOUBLE);
}

int convert_aastore(struct parse_context *ctx)
{
	return convert_array_store(ctx, J_REFERENCE);
}

int convert_bastore(struct parse_context *ctx)
{
	return convert_array_store(ctx, J_BYTE);
}

int convert_castore(struct parse_context *ctx)
{
	return convert_array_store(ctx, J_CHAR);
}

int convert_sastore(struct parse_context *ctx)
{
	return convert_array_store(ctx, J_SHORT);
}

int convert_new(struct parse_context *ctx)
{
	struct expression *expr;
	unsigned long type_idx;
	struct vm_class *class;

	type_idx = bytecode_read_u16(ctx->buffer);
	class = vm_class_resolve_class(ctx->cu->method->class, type_idx);
	if (!class)
		return -EINVAL;

	expr = new_expr(class);
	if (!expr)
		return -ENOMEM;

	convert_expression(ctx, expr);

	return 0;
}

int convert_newarray(struct parse_context *ctx)
{
	struct expression *size, *arrayref;
	unsigned long type;

	size = stack_pop(ctx->bb->mimic_stack);
	type = bytecode_read_u8(ctx->buffer);

	arrayref = newarray_expr(type, size);
	if (!arrayref)
		return -ENOMEM;

	convert_expression(ctx, arrayref);

	return 0;
}

int convert_anewarray(struct parse_context *ctx)
{
	struct expression *size,*arrayref;
	unsigned long type_idx;
	struct vm_object *class, *arrayclass;

	size = stack_pop(ctx->bb->mimic_stack);
	type_idx = bytecode_read_u16(ctx->buffer);

	class = vm_class_resolve_class(ctx->cu->method->class, type_idx);
	if (!class)
		return -EINVAL;

	arrayclass = class_to_array_class(class);
	if (!arrayclass)
		return -EINVAL;

	arrayref = anewarray_expr(arrayclass,size);
	if (!arrayref)
		return -ENOMEM;

	convert_expression(ctx, arrayref);

	return 0;
}

int convert_multianewarray(struct parse_context *ctx)
{
	struct expression *arrayref;
	struct expression *args_list;
	unsigned long type_idx;
	unsigned char dimension;
	struct vm_object *class;

	NOT_IMPLEMENTED;

	type_idx = bytecode_read_u16(ctx->buffer);
	dimension = bytecode_read_u8(ctx->buffer);
	//class = resolveClass(ctx->cu->method->class, type_idx, FALSE);
	class = NULL;
	if (!class)
		return -ENOMEM;

	arrayref = multianewarray_expr(class);
	if (!arrayref)
		return -ENOMEM;

	args_list = convert_args(ctx->bb->mimic_stack, dimension);
	arrayref->multianewarray_dimensions = &args_list->node;
	if (!arrayref->multianewarray_dimensions)
		return -ENOMEM;

	convert_expression(ctx, arrayref);

	return 0;
}

int convert_arraylength(struct parse_context *ctx)
{
	struct expression *arrayref, *arraylength_exp;

	arrayref = stack_pop(ctx->bb->mimic_stack);

	arraylength_exp = arraylength_expr(arrayref);
	if (!arraylength_exp)
		return -ENOMEM;

	convert_expression(ctx, arraylength_exp);

	return 0;
}

int convert_instanceof(struct parse_context *ctx)
{
	struct expression *objectref, *expr;
	struct vm_object *class;
	unsigned long type_idx;

	NOT_IMPLEMENTED;

	objectref = stack_pop(ctx->bb->mimic_stack);

	type_idx = bytecode_read_u16(ctx->buffer);
	//class = resolveClass(ctx->cu->method->class, type_idx, FALSE);
	class = NULL;
	if (!class)
		return -EINVAL;

	expr = instanceof_expr(objectref, class);
	if (!expr)
		return -ENOMEM;

	convert_expression(ctx, expr);

	return 0;
}

int convert_checkcast(struct parse_context *ctx)
{
	struct expression *object_ref;
	struct vm_class *class;
	struct statement *checkcast_stmt;
	unsigned long type_idx;

	object_ref = stack_pop(ctx->bb->mimic_stack);

	type_idx = bytecode_read_u16(ctx->buffer);
	class = vm_class_resolve_class(ctx->cu->method->class, type_idx);
	if (!class)
		return -ENOMEM;

	checkcast_stmt = alloc_statement(STMT_CHECKCAST);
	if (!checkcast_stmt)
		return -ENOMEM;

	checkcast_stmt->checkcast_class = class;
	checkcast_stmt->checkcast_ref = &object_ref->node;

	expr_get(object_ref);
	convert_statement(ctx, checkcast_stmt);
	convert_expression(ctx, object_ref);

	return 0;
}

int convert_monitor_enter(struct parse_context *ctx)
{
	struct expression *nullcheck;
	struct expression *exp;
	struct statement *stmt;

	exp = stack_pop(ctx->bb->mimic_stack);

	nullcheck = null_check_expr(exp);
	if (!nullcheck)
		return -ENOMEM;

	stmt = alloc_statement(STMT_MONITOR_ENTER);
	if (!stmt)
		return -ENOMEM;

	expr_get(exp);
	stmt->expression = &nullcheck->node;
	convert_statement(ctx, stmt);

	return 0;
}

int convert_monitor_exit(struct parse_context *ctx)
{
	struct expression *nullcheck;
	struct expression *exp;
	struct statement *stmt;

	exp = stack_pop(ctx->bb->mimic_stack);

	nullcheck = null_check_expr(exp);
	if (!nullcheck)
		return -ENOMEM;

	stmt = alloc_statement(STMT_MONITOR_EXIT);
	if (!stmt)
		return -ENOMEM;

	expr_get(exp);
	stmt->expression = &nullcheck->node;
	convert_statement(ctx, stmt);

	return 0;
}
