/*
 * Copyright (C) 2006  Pekka Enberg
 *
 * This file is released under the GPL version 2. Please refer to the file
 * LICENSE for details.
 */

#include <expression.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

struct expression *alloc_expression(enum expression_type type,
				    enum jvm_type jvm_type)
{
	struct expression *expr = malloc(sizeof *expr);
	if (expr) {
		memset(expr, 0, sizeof *expr);
		expr->type = type;
		expr->jvm_type = jvm_type;
		expr->refcount = 1;
	}
	return expr;
}

void free_expression(struct expression *expr)
{
	if (!expr)
		return;

	switch (expr->type) {
	case EXPR_VALUE:
	case EXPR_FVALUE:
	case EXPR_LOCAL:
	case EXPR_TEMPORARY:
		/* nothing to do */
		break;
	case EXPR_ARRAY_DEREF:
		if (expr->arrayref)
			expr_put(expr->arrayref);
		if (expr->array_index)
			expr_put(expr->array_index);
		break;
	case EXPR_BINOP:
		if (expr->binary_left)
			expr_put(expr->binary_left);
		if (expr->binary_right)
			expr_put(expr->binary_right);
		break;
	case EXPR_UNARY_OP:
		if (expr->unary_expression)
			expr_put(expr->unary_expression);
		break;
	case EXPR_CONVERSION:
		if (expr->from_expression)
			expr_put(expr->from_expression);
		break;
	};
	free(expr);
}

void expr_get(struct expression *expr)
{
	expr->refcount++;
}

void expr_put(struct expression *expr)
{
	expr->refcount--;
	if (expr->refcount == 0)
		free_expression(expr);
}

struct expression *value_expr(enum jvm_type jvm_type, unsigned long long value)
{
	struct expression *expr = alloc_expression(EXPR_VALUE, jvm_type);
	if (expr)
		expr->value = value;

	return expr;
}

struct expression *fvalue_expr(enum jvm_type jvm_type, double fvalue)
{
	struct expression *expr = alloc_expression(EXPR_FVALUE, jvm_type);
	if (expr)
		expr->fvalue = fvalue;

	return expr;
}

struct expression *local_expr(enum jvm_type jvm_type, unsigned long local_index)
{
	struct expression *expr = alloc_expression(EXPR_LOCAL, jvm_type);
	if (expr)
		expr->local_index = local_index;

	return expr;
}

struct expression *temporary_expr(enum jvm_type jvm_type,
				  unsigned long temporary)
{
	struct expression *expr = alloc_expression(EXPR_TEMPORARY, jvm_type);
	if (expr)
		expr->temporary = temporary;

	return expr;
}

struct expression *array_deref_expr(enum jvm_type jvm_type,
				    struct expression *arrayref,
				    struct expression *array_index)
{
	struct expression *expr = alloc_expression(EXPR_ARRAY_DEREF, jvm_type);
	if (expr) {
		expr->arrayref = arrayref;
		expr->array_index = array_index;
	}
	return expr;
}

struct expression *binop_expr(enum jvm_type jvm_type,
			      enum binary_operator binary_operator,
			      struct expression *binary_left, struct expression *binary_right)
{
	struct expression *expr = alloc_expression(EXPR_BINOP, jvm_type);
	if (expr) {
		expr->binary_operator = binary_operator;
		expr->binary_left = binary_left;
		expr->binary_right = binary_right;
	}
	return expr;
}

struct expression *unary_op_expr(enum jvm_type jvm_type,
				 enum unary_operator unary_operator,
				 struct expression *unary_expression)
{
	struct expression *expr = alloc_expression(EXPR_UNARY_OP, jvm_type);
	if (expr) {
		expr->unary_operator = unary_operator;
		expr->unary_expression = unary_expression;
	}
	return expr;
}

struct expression *conversion_expr(enum jvm_type jvm_type,
				   struct expression *from_expression)
{
	struct expression *expr = alloc_expression(EXPR_CONVERSION, jvm_type);
	if (expr)
		expr->from_expression = from_expression;
	return expr;
}