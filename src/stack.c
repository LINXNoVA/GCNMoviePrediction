#include "stack.h"
#include <string.h>

int
stack_grow(struct stack *st, size_t new_size)
{
	void **new_st;

	if (st->asize >= new_size)
		return 0;

	new_st = realloc(st->item, new_size * sizeof(void *));
	if (new_st == NULL)
		return -1;

	memset(new_st + st->asize, 0x0,
		(new_size - st->asize) * sizeof(void *));

	st->item = new_st;
	st->asize = new_size;

	if 