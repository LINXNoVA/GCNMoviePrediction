
/*
 * Copyright (c) 2011, Vicent Marti
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "buffer.h"
#include "html.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#if defined(_WIN32)
#define snprintf	_snprintf		
#endif

struct smartypants_data {
	int in_squote;
	int in_dquote;
};

static size_t smartypants_cb__ltag(struct buf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__dquote(struct buf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__amp(struct buf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__period(struct buf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__number(struct buf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__dash(struct buf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__parens(struct buf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__squote(struct buf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__backtick(struct buf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);
static size_t smartypants_cb__escape(struct buf *ob, struct smartypants_data *smrt, uint8_t previous_char, const uint8_t *text, size_t size);

static size_t (*smartypants_cb_ptrs[])
	(struct buf *, struct smartypants_data *, uint8_t, const uint8_t *, size_t) =
{
	NULL,					/* 0 */
	smartypants_cb__dash,	/* 1 */
	smartypants_cb__parens,	/* 2 */
	smartypants_cb__squote, /* 3 */
	smartypants_cb__dquote, /* 4 */
	smartypants_cb__amp,	/* 5 */
	smartypants_cb__period,	/* 6 */
	smartypants_cb__number,	/* 7 */
	smartypants_cb__ltag,	/* 8 */
	smartypants_cb__backtick, /* 9 */
	smartypants_cb__escape, /* 10 */
};

static const uint8_t smartypants_cb_chars[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 4, 0, 0, 0, 5, 3, 2, 0, 0, 0, 0, 1, 6, 0,
	0, 7, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0,
	9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static inline int
word_boundary(uint8_t c)
{
	return c == 0 || isspace(c) || ispunct(c);
}

static int
smartypants_quotes(struct buf *ob, uint8_t previous_char, uint8_t next_char, uint8_t quote, int *is_open)
{
	char ent[8];

	if (*is_open && !word_boundary(next_char))
		return 0;

	if (!(*is_open) && !word_boundary(previous_char))
		return 0;

	snprintf(ent, sizeof(ent), "&%c%cquo;", (*is_open) ? 'r' : 'l', quote);
	*is_open = !(*is_open);
	bufputs(ob, ent);
	return 1;
}
