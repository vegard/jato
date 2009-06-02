/*
 * Copyright (C) 2006  Pekka Enberg
 *
 * This file is released under the GPL version 2. Please refer to the file
 * LICENSE for details.
 */

#include <jit/expression.h>
#include <jit/bc-offset-mapping.h>

#include <vm/vm.h>
#include <vm/method.h>
#include <vm/object.h>

#include <stdlib.h>
#include <string.h>
#include <glib.h>

/* How many child expressions are used by each type of expression. */
int expr_nr_kids(struct expression *expr)
{
	switch (expr_type(expr)) {
	case EXPR_ARRAY_DEREF:
	case EXPR_BINOP:
	case EXPR_ARGS_LIST:
		return 2;
	case EXPR_UNARY_OP:
	case EXPR_CONVERSION:
	case EXPR_INSTANCE_FIELD:
	case EXPR_INVOKE:
	case EXPR_INVOKEVIRTUAL:
	case EXPR_ARG:
	case EXPR_NEWARRAY:
	case EXPR_ANEWARRAY:
	case EXPR_MULTIANEWARRAY:
	case EXPR_ARRAYLENGTH:
	case EXPR_INSTANCEOF:
	case EXPR_NULL_CHECK:
		return 1;
	case EXPR_VALUE:
	case EXPR_FVALUE:
	case EXPR_LOCAL:
	case EXPR_TEMPORARY:
	case EXPR_CLASS_FIELD:
	case EXPR_NO_ARGS:
	case EXPR_NEW:
	case EXPR_EXCEPTION_REF:
		return 0;
	default:
		assert(!"Invalid expression type");
	}
}

struct expression *alloc_expression(enum expression_type type,
				    enum vm_type vm_type)
{
	struct expression *expr = malloc(sizeof *expr);
	if (expr) {
		memset(expr, 0, sizeof *expr);
		expr->node.op = type << EXPR_TYPE_SHIFT;
		expr->vm_type = vm_type;
		expr->refcount = 1;
		expr->bytecode_offset = BC_OFFSET_UNKNOWN;
	}
	return expr;
}

void free_expression(struct expression *expr)
{
	int i;

	if (!expr)
		return;

	for (i = 0; i < expr_nr_kids(expr); i++)
		if (expr->node.kids[i])
			expr_put(to_expr(expr->node.kids[i]));

	free(expr);
}

struct expression *expr_get(struct expression *expr)
{
	assert(expr->refcount > 0);

	expr->refcount++;

	return expr;
}

void expr_put(struct expression *expr)
{
	assert(expr->refcount > 0);

	expr->refcount--;
	if (expr->refcount == 0)
		free_expression(expr);
}

struct expression *value_expr(enum vm_type vm_type, unsigned long long value)
{
	struct expression *expr = alloc_expression(EXPR_VALUE, vm_type);
	if (expr)
		expr->value = value;

	return expr;
}

struct expression *fvalue_expr(enum vm_type vm_type, double fvalue)
{
	struct expression *expr = alloc_expression(EXPR_FVALUE, vm_type);
	if (expr)
		expr->fvalue = fvalue;

	return expr;
}

struct expression *local_expr(enum vm_type vm_type, unsigned long local_index)
{
	struct expression *expr = alloc_expression(EXPR_LOCAL, vm_type);
	if (expr)
		expr->local_index = local_index;

	return expr;
}

struct expression *temporary_expr(enum vm_type vm_type, struct var_info *tmp_high, struct var_info *tmp_low)
{
	struct expression *expr = alloc_expression(EXPR_TEMPORARY, vm_type);
	if (expr) {
		expr->tmp_high = tmp_high;
		expr->tmp_low = tmp_low;
	}

	return expr;
}

struct expression *array_deref_expr(enum vm_type vm_type,
				    struct expression *arrayref,
				    struct expression *array_index)
{
	struct expression *expr = alloc_expression(EXPR_ARRAY_DEREF, vm_type);
	if (expr) {
		expr->arrayref = &arrayref->node;
		expr->array_index = &array_index->node;
	}
	return expr;
}

struct expression *binop_expr(enum vm_type vm_type,
			      enum binary_operator binary_operator,
			      struct expression *binary_left, struct expression *binary_right)
{
	struct expression *expr = alloc_expression(EXPR_BINOP, vm_type);
	if (expr) {
		expr->node.op |= binary_operator << OP_SHIFT;
		expr->binary_left = &binary_left->node;
		expr->binary_right = &binary_right->node;
	}
	return expr;
}

struct expression *unary_op_expr(enum vm_type vm_type,
				 enum unary_operator unary_operator,
				 struct expression *unary_expression)
{
	struct expression *expr = alloc_expression(EXPR_UNARY_OP, vm_type);
	if (expr) {
		expr->node.op |= unary_operator << OP_SHIFT;
		expr->unary_expression = &unary_expression->node;
	}
	return expr;
}

struct expression *conversion_expr(enum vm_type vm_type,
				   struct expression *from_expression)
{
	struct expression *expr = alloc_expression(EXPR_CONVERSION, vm_type);
	if (expr)
		expr->from_expression = &from_expression->node;
	return expr;
}

struct expression *class_field_expr(enum vm_type vm_type, struct vm_field *class_field)
{
	struct expression *expr = alloc_expression(EXPR_CLASS_FIELD, vm_type);
	if (expr)
		expr->class_field = class_field;
	return expr;
}

struct expression *instance_field_expr(enum vm_type vm_type,
				       struct vm_field *instance_field,
				       struct expression *objectref_expression)
{
	struct expression *expr = alloc_expression(EXPR_INSTANCE_FIELD, vm_type);
	if (expr) {
		expr->objectref_expression = &objectref_expression->node;
		expr->instance_field = instance_field;
	}
	return expr;
}

struct expression *__invoke_expr(enum expression_type expr_type, enum vm_type vm_type, struct vm_method *target_method)
{
	struct expression *expr = alloc_expression(expr_type, vm_type);

	if (expr)
		expr->target_method = target_method;

	return expr;
}

struct expression *invokevirtual_expr(struct vm_method *target)
{
	enum vm_type return_type;

	return_type = method_return_type(target);
	return __invoke_expr(EXPR_INVOKEVIRTUAL, return_type, target);
}

struct expression *invoke_expr(struct vm_method *target)
{
	enum vm_type return_type;

	return_type = method_return_type(target);
	return  __invoke_expr(EXPR_INVOKE, return_type, target);
}

struct expression *args_list_expr(struct expression *args_left,
				  struct expression *args_right)
{
	struct expression *expr = alloc_expression(EXPR_ARGS_LIST, J_VOID);
	if (expr) {
		expr->args_left = &args_left->node;
		expr->args_right = &args_right->node;
	}
	return expr;
}

struct expression *arg_expr(struct expression *arg_expression)
{
	struct expression *expr = alloc_expression(EXPR_ARG, J_VOID);
	if (expr)
		expr->arg_expression = &arg_expression->node;
	return expr;
}

struct expression *no_args_expr(void)
{
	return alloc_expression(EXPR_NO_ARGS, J_VOID);
}

struct expression *new_expr(struct vm_class *class)
{
	struct expression *expr = alloc_expression(EXPR_NEW, J_REFERENCE);
	if (expr)
		expr->class = class;
	return expr;
}

struct expression *newarray_expr(unsigned long type, struct expression *size)
{
	struct expression *expr = alloc_expression(EXPR_NEWARRAY, J_REFERENCE);

	if (expr) {
		expr->array_type = type;
		expr->array_size = &size->node;
	}

	return expr;
}

struct expression *anewarray_expr(struct vm_object *class, struct expression *size)
{
	struct expression *expr = alloc_expression(EXPR_ANEWARRAY, J_REFERENCE);

	if (expr) {
		expr->anewarray_ref_type = class;
		expr->anewarray_size = &size->node;
	}
	return expr;
}

struct expression *multianewarray_expr(struct vm_object *class)
{
	struct expression *expr = alloc_expression(EXPR_MULTIANEWARRAY, J_REFERENCE);

	if (expr)
		expr->multianewarray_ref_type = class;
	return expr;
}

struct expression *arraylength_expr(struct expression *arrayref)
{
	struct expression *expr = alloc_expression(EXPR_ARRAYLENGTH, J_REFERENCE);

	if (expr)
		expr->arraylength_ref = &arrayref->node;
	return expr;
}

struct expression *instanceof_expr(struct expression *objectref, struct vm_object *class)
{
	struct expression *expr = alloc_expression(EXPR_INSTANCEOF, J_REFERENCE);

	if (expr) {
		expr->instanceof_class = class;
		expr->instanceof_ref = &objectref->node;
	}
	return expr;
}

unsigned long nr_args(struct expression *args_list)
{
	struct expression *left, *right;

	if (expr_type(args_list) == EXPR_NO_ARGS)
		return 0;

	if (expr_type(args_list) == EXPR_ARG)
		return 1;

	left  = to_expr(args_list->args_left);
	right = to_expr(args_list->args_right);

	return nr_args(left) + nr_args(right);
}

struct expression *exception_ref_expr(void)
{
	return alloc_expression(EXPR_EXCEPTION_REF, J_REFERENCE);
}

struct expression *null_check_expr(struct expression *ref)
{
	struct expression *expr;

	expr = alloc_expression(EXPR_NULL_CHECK, J_REFERENCE);
	if (!expr)
		return NULL;

	assert(ref->vm_type == J_REFERENCE);

	expr->bytecode_offset = ref->bytecode_offset;
	expr->null_check_ref = &ref->node;

	return expr;
}
