/*
 * cafebabe - the class loader library in C
 * Copyright (C) 2008  Vegard Nossum <vegardno@ifi.uio.no>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cafebabe/access.h"
#include "cafebabe/attribute_array.h"
#include "cafebabe/attribute_info.h"
#include "cafebabe/class.h"
#include "cafebabe/code_attribute.h"
#include "cafebabe/constant_pool.h"
#include "cafebabe/error.h"
#include "cafebabe/field_info.h"
#include "cafebabe/method_info.h"
#include "cafebabe/stream.h"

int
main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s CLASS\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	const char *classname = argv[1];
	char *filename;
	if (asprintf(&filename, "%s.class", classname) == -1) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		goto out;
	}

	struct cafebabe_stream stream;
	if (cafebabe_stream_open(&stream, filename)) {
		fprintf(stderr, "error: %s: %s\n", filename,
			cafebabe_stream_error(&stream));
		goto out_filename;
	}

	struct cafebabe_class class;
	if (cafebabe_class_init(&class, &stream)) {
		fprintf(stderr, "error: %s:%d/%d: %s\n", filename,
			stream.virtual_i, stream.virtual_n,
			cafebabe_stream_error(&stream));
		goto out_stream;
	}

	unsigned int main_method_index;
	if (cafebabe_class_get_method(&class,
		"main", "([Ljava/lang/String;)V", &main_method_index))
	{
		fprintf(stderr, "error: %s: no main method\n", classname);
		goto out_class;
	}

	struct cafebabe_method_info *main_method
		= &class.methods[main_method_index];

	unsigned int main_code_index = 0;
	if (cafebabe_attribute_array_get(&main_method->attributes,
		"Code", &class, &main_code_index))
	{
		fprintf(stderr, "error: %s: no 'Class' attribute for main "
			"method\n", classname);
		goto out_class;
	}

	struct cafebabe_attribute_info *main_code_attribute_info
		= &main_method->attributes.array[main_code_index];

	struct cafebabe_stream main_code_attribute_stream;
	cafebabe_stream_open_buffer(&main_code_attribute_stream,
		main_code_attribute_info->info,
		main_code_attribute_info->attribute_length);

	struct cafebabe_code_attribute main_code_attribute;
	if (cafebabe_code_attribute_init(&main_code_attribute,
		&main_code_attribute_stream))
	{
		fprintf(stderr, "error: %s:%s:%d/%d: %s\n",
			filename,
			"main",
			main_code_attribute_stream.virtual_i,
			main_code_attribute_stream.virtual_n,
			cafebabe_stream_error(&main_code_attribute_stream));
		exit(1);
	}

	cafebabe_stream_close_buffer(&main_code_attribute_stream);

	printf("main:\n");
	printf("\tmax_stack = %d\n", main_code_attribute.max_stack);
	printf("\tmax_locals = %d\n", main_code_attribute.max_locals);
	printf("\tcode_length = %d\n", main_code_attribute.code_length);

	cafebabe_code_attribute_deinit(&main_code_attribute);

	cafebabe_class_deinit(&class);
	cafebabe_stream_close(&stream);
	free(filename);

	return EXIT_SUCCESS;

out_class:
	cafebabe_class_deinit(&class);
out_stream:
	cafebabe_stream_close(&stream);
out_filename:
	free(filename);
out:
	return EXIT_FAILURE;
}
