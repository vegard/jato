#ifndef __CLASS_H
#define __CLASS_H

#include <vm/field.h>
#include <vm/method.h>
#include <vm/types.h>
#include <vm/vm.h>

#include <jit/vtable.h>

struct vm_object;

enum vm_class_state {
	VM_CLASS_LOADED,
	VM_CLASS_LINKED,
	VM_CLASS_INITIALIZED,
};

struct vm_class {
	const struct cafebabe_class *class;

	enum vm_class_state state;

	char *name;

	struct vm_class *super;
	struct vm_field *fields;
	struct vm_method *methods;

	unsigned int object_size;
	unsigned int static_size;

	unsigned int vtable_size;
	struct vtable vtable;

	/* The java.lang.Class object representing this class */
	struct vm_object *object;

	/* This is an array of all the values of the static members of this
	 * class. */
	void *static_values;

	/* This points to either:
	 *  1) static_guard_page: in case the class wasn't initialized yet
	 *  2) static_values: (above) */
	void **static_values_base;
};

int vm_class_link(struct vm_class *vmc, const struct cafebabe_class *class);
int vm_class_init(struct vm_class *vmc);

static inline int vm_class_ensure_init(struct vm_class *vmc)
{
	if (vmc->state == VM_CLASS_INITIALIZED)
		return 0;

	return vm_class_init(vmc);
}

static inline bool vm_class_is_interface(const struct vm_class *vmc)
{
	return vmc->class->access_flags & CAFEBABE_CLASS_ACC_INTERFACE;
}

static inline bool vm_class_is_array_class(const struct vm_class *vmc)
{
	return vmc->name && vmc->name[0] == '[';
}

struct vm_class *vm_class_resolve_class(const struct vm_class *vmc, uint16_t i);

struct vm_field *vm_class_get_field(const struct vm_class *vmc,
	const char *name, const char *type);
struct vm_field *vm_class_get_field_recursive(const struct vm_class *vmc,
	const char *name, const char *type);

int vm_class_resolve_field(const struct vm_class *vmc, uint16_t i,
	struct vm_class **r_vmc, char **r_name, char **r_type);
struct vm_field *vm_class_resolve_field_recursive(const struct vm_class *vmc,
	uint16_t i);

struct vm_method *vm_class_get_method(const struct vm_class *vmc,
	const char *name, const char *type);
struct vm_method *vm_class_get_method_recursive(const struct vm_class *vmc,
	const char *name, const char *type);

int vm_class_resolve_method(const struct vm_class *vmc, uint16_t i,
	struct vm_class **r_vmc, char **r_name, char **r_type);
struct vm_method *vm_class_resolve_method_recursive(const struct vm_class *vmc,
	uint16_t i);

bool vm_class_is_assignable_from(const struct vm_class *vmc, const struct vm_class *from);

#endif /* __CLASS_H */
