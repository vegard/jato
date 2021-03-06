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

#include <dlfcn.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jit/disassemble.h"

#include "vm/die.h"
#include "vm/jni.h"

#include "lib/string.h"

static char *vm_jni_get_mangled_name(const char *name)
{
	struct string *str;
	char *result;
	int err;
	int i;

	str = alloc_str();
	if (!str) {
		NOT_IMPLEMENTED;
		return NULL;
	}

	result = NULL;

	for (i = 0; name[i]; i++) {
		if (name[i] == '_')
			err = str_append(str, "_1");
		else if (name[i] == ';')
			err = str_append(str, "_2");
		else if (name[i] == '[')
			err = str_append(str, "_3");
		else if (name[i] == '/')
			err = str_append(str, "_");
		else if (name[i] & 0x80) {
			/* Unicode characters should be transformed to
			   "_0xxxx" */
			NOT_IMPLEMENTED;
		} else
			err = str_append(str, "%c", (char)name[i]);

		if (err)
			goto out;
	}

	result = strdup(str->value);

 out:
	free_str(str);

	return result;
}

static int vm_jni_lookup_object_handle(void *handle)
{
	for (int i = 0; i < vm_jni_nr_loaded_objects; i++)
		if (vm_jni_loaded_objects[i] == handle)
			return i;

	return -1;
}

static int vm_jni_add_object_handle(void *handle)
{
	void **new_table;
	int new_size;

	if (vm_jni_lookup_object_handle(handle) >= 0)
		return 0; /* handle already in table */

	new_size = sizeof(void *) * (vm_jni_nr_loaded_objects + 1);
	new_table = realloc(vm_jni_loaded_objects, new_size);
	if (!new_table)
		return -ENOMEM;

	vm_jni_loaded_objects = new_table;
	vm_jni_loaded_objects[vm_jni_nr_loaded_objects++] = handle;

	return 0;
}

int vm_jni_load_object(const char *name)
{
	char *classpath_install_dir;
	void *handle;

	classpath_install_dir = getenv("CLASSPATH_INSTALL_DIR");
	if (!classpath_install_dir) {
		warn("environment variable CLASSPATH_INSTALL_DIR not set");
		return -1;
	}

	char *so_name = NULL;

	if (asprintf(&so_name, "%s/lib/classpath/%s", classpath_install_dir, name) < 0)
		die("asprintf");

	handle = dlopen(so_name, RTLD_NOW);
	free(so_name);

	if (!handle) {
		fprintf(stderr, "%s: %s\n", __func__, dlerror());
		return -1;
	}

	if (vm_jni_add_object_handle(handle)) {
		dlclose(handle);
		return -ENOMEM;
	}

	return 0;
}

static void *vm_jni_lookup_symbol(const char *symbol_name)
{
	for (int i = 0; i < vm_jni_nr_loaded_objects; i++) {
		void *addr;

		addr = dlsym(vm_jni_loaded_objects[i], symbol_name);
		if (addr)
			return addr;
	}

	return NULL;
}

void *vm_jni_lookup_method(const char *class_name, const char *method_name,
			   const char *method_type)
{
	char *mangled_class_name;
	char *mangled_method_name;
	char *mangled_method_type;
	char *symbol_name;
	void *sym_addr;

	mangled_class_name = vm_jni_get_mangled_name(class_name);
	mangled_method_name = vm_jni_get_mangled_name(method_name);
	mangled_method_type = vm_jni_get_mangled_name(method_type);

	symbol_name = NULL;
	if (asprintf(&symbol_name, "Java_%s_%s__%s", mangled_class_name,
		 mangled_method_name, mangled_method_type) < 0)
		die("asprintf");

	sym_addr = vm_jni_lookup_symbol(symbol_name);
	if (sym_addr)
		goto out;

	if (asprintf(&symbol_name, "Java_%s_%s", mangled_class_name,
		 mangled_method_name) < 0)
		die("asprintf");

	sym_addr = vm_jni_lookup_symbol(symbol_name);

 out:
	free(mangled_method_name);
	free(mangled_class_name);
	free(mangled_method_type);
	free(symbol_name);

	return sym_addr;
}
