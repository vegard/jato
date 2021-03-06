#ifndef __JIT_SYSTEM_H
#define __JIT_SYSTEM_H

#include <stdint.h>
#include <stddef.h>

#define BITS_PER_LONG (sizeof(unsigned long) * 8)

/* Macros stolen shamelessly from Linux kernel. */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ALIGN(x,a) (((x)+(a)-1)&~((a)-1))

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#define min(x,y) ({ \
	typeof(x) _x = (x);     \
	typeof(y) _y = (y);     \
	(void) (&_x == &_y);    \
	_x < _y ? _x : _y; })

#define max(x,y) ({ \
	typeof(x) _x = (x);     \
	typeof(y) _y = (y);     \
	(void) (&_x == &_y);    \
	_x > _y ? _x : _y; })

static inline float uint32_to_float(uint32_t value)
{
	union {
		uint32_t	val;
		float		fv;
	} a;
	a.val = value;
	return a.fv;
}

static inline int float_to_uint32(float value)
{
	union {
		uint32_t	iv;
		float		val;
	} a;
	a.val = value;
	return a.iv;
}

#endif
