#include <stdbool.h>
#include <stdlib.h>

#include <vm/object.h>
#include <vm/stdlib.h>

struct vm_object *vm_object_alloc(struct vm_class *class)
{
	struct vm_object *res;

	res = zalloc(sizeof(*res) + class->object_size);
	if (res) {
		res->class = class;
	}

	return res;
}

struct vm_object *vm_object_alloc_native_array(int type, int count)
{
	struct vm_object *res;

	/* XXX: Use the right size... */
	res = zalloc(sizeof(*res) + 8 * count);
	if (res) {
		res->array_length = count;
	}

	return res;
}

struct vm_object *vm_object_alloc_multi_array(struct vm_class *class,
	int nr_dimensions, int **counts)
{
	NOT_IMPLEMENTED;
	return NULL;
}

struct vm_object *vm_object_alloc_array(struct vm_class *class, int count)
{
	NOT_IMPLEMENTED;
	return NULL;
}

void vm_object_lock(struct vm_object *obj)
{
	NOT_IMPLEMENTED;
}

void vm_object_unlock(struct vm_object *obj)
{
	NOT_IMPLEMENTED;
}

#include <vm/classloader.h>
#include <vm/utf8.h>

struct vm_object *
vm_object_alloc_string(const uint8_t bytes[], unsigned int length)
{
	struct vm_object *array = utf8_to_char_array(bytes, length);
	if (!array) {
		NOT_IMPLEMENTED;
		return NULL;
	}

	/* XXX: A lot of this can move out to its own initialisation function
	 * that only needs to run once. */
	struct vm_class *string_class = classloader_load("java/lang/String");
	if (!string_class) {
		NOT_IMPLEMENTED;
		return NULL;
	}

	struct vm_field *offset
		= vm_class_get_field(string_class, "offset", "I");
	if (!offset) {
		NOT_IMPLEMENTED;
		return NULL;
	}

	struct vm_field *count
		= vm_class_get_field(string_class, "count", "I");
	if (!count) {
		NOT_IMPLEMENTED;
		return NULL;
	}

	struct vm_field *value
		= vm_class_get_field(string_class, "value", "[C");
	if (!value) {
		NOT_IMPLEMENTED;
		return NULL;
	}

	struct vm_object *string = vm_object_alloc(string_class);
	if (!string) {
		NOT_IMPLEMENTED;
		return NULL;
	}

	*(int32_t *) &string->fields[offset->offset] = 0;
	*(int32_t *) &string->fields[count->offset] = array->array_length;
	*(void **) &string->fields[value->offset] = array;

	return string;
}

bool vm_object_is_instance_of(struct vm_object *obj, struct vm_object *class)
{
	if (!obj)
		return false;

	NOT_IMPLEMENTED;
	return false;
}

void vm_object_check_null(struct vm_object *obj)
{
	NOT_IMPLEMENTED;
}

void vm_object_check_array(struct vm_object *obj, unsigned int index)
{
	NOT_IMPLEMENTED;
}

void vm_object_check_cast(struct vm_object *obj, struct vm_object *class)
{
	NOT_IMPLEMENTED;
}
