#include <string.h>

#include <cafebabe/attribute_array.h>
#include <cafebabe/attribute_info.h>
#include <cafebabe/class.h>
#include <cafebabe/code_attribute.h>
#include <cafebabe/constant_pool.h>
#include <cafebabe/method_info.h>
#include <cafebabe/stream.h>

#include <vm/class.h>
#include <vm/method.h>

int vm_method_init(struct vm_method *vmm,
	struct vm_class *vmc, unsigned int method_index)
{
	const struct cafebabe_class *class = vmc->class;
	const struct cafebabe_method_info *method
		= &class->methods[method_index];

	vmm->class = vmc;
	vmm->method_index = method_index;
	vmm->method = method;

	const struct cafebabe_constant_info_utf8 *name;
	if (cafebabe_class_constant_get_utf8(class, method->name_index,
		&name))
	{
		NOT_IMPLEMENTED;
		return -1;
	}

	vmm->name = strndup((char *) name->bytes, name->length);
	if (!vmm->name) {
		NOT_IMPLEMENTED;
		return -1;
	}

	const struct cafebabe_constant_info_utf8 *type;
	if (cafebabe_class_constant_get_utf8(class, method->descriptor_index,
		&type))
	{
		NOT_IMPLEMENTED;
		return -1;
	}

	vmm->type = strndup((char *) type->bytes, type->length);
	if (!vmm->type) {
		NOT_IMPLEMENTED;
		return -1;
	}

	/*
	 * XXX: Jam VM legacy? It seems that JamVM counts the number of
	 * _32-bit_ arguments. This probably needs some fixing. What do we do
	 * on x86_64?
	 */
	vmm->args_count = count_arguments(vmm->type);
	if (vmm->args_count < 0) {
		NOT_IMPLEMENTED;
		return -1;
	}

	if (!vm_method_is_static(vmm))
		++vmm->args_count;

	/*
	 * Note: We can return here because the rest of the function deals
	 * with loading the Code attribute (which native methods don't have).
	 */
	if (vm_method_is_native(vmm) || vm_method_is_abstract(vmm)) {
		/* Hm, we're now modifying a cafebabe structure. */
		vmm->code_attribute.max_stack = 0;
		vmm->code_attribute.max_locals = vmm->args_count;
		return 0;
	}

	unsigned int code_index = 0;
	if (cafebabe_attribute_array_get(&method->attributes,
		"Code", class, &code_index))
	{
		NOT_IMPLEMENTED;
		return -1;
	}

	{
		/* There must be only one "Code" attribute for the method! */
		unsigned int code_index2 = code_index + 1;
		if (!cafebabe_attribute_array_get(&method->attributes,
			"Code", class, &code_index2))
		{
			NOT_IMPLEMENTED;
			return -1;
		}
	}

	const struct cafebabe_attribute_info *attribute
		= &method->attributes.array[code_index];

	struct cafebabe_stream stream;
	cafebabe_stream_open_buffer(&stream,
		attribute->info, attribute->attribute_length);

	if (cafebabe_code_attribute_init(&vmm->code_attribute, &stream)) {
		NOT_IMPLEMENTED;
		return -1;
	}

	cafebabe_stream_close_buffer(&stream);

	return 0;
}

int vm_method_prepare_jit(struct vm_method *vmm)
{
	vmm->compilation_unit = compilation_unit_alloc(vmm);
	if (!vmm->compilation_unit) {
		NOT_IMPLEMENTED;
		return -1;
	}

	vmm->trampoline = build_jit_trampoline(vmm->compilation_unit);
	if (!vmm->trampoline) {
		NOT_IMPLEMENTED;
		return -1;
	}

	return 0;
}
