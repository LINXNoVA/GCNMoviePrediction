/* markdown.c - generic markdown parser */

/*
 * Copyright (c) 2009, Natacha Porté
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

#include "markdown.h"
#include "stack.h"

#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#if defined(_WIN32)
#define strncasecmp	_strnicmp
#endif

#define REF_TABLE_SIZE 8

#define BUFFER_BLOCK 0
#define BUFFER_SPAN 1

#define MKD_LI_END 8	/* internal list flag */

#define gperf_case_strncmp(s1, s2, n) strncasecmp(s1, s2, n)
#define GPERF_DOWNCASE 1
#define GPERF_CASE_STRNCMP 1
#include "html_blocks.h"

/***************
 * LOCAL TYPES *
 ***************/

/* link_ref: reference to a link */
struct link_ref {
	unsigned int id;

	struct buf *link;
	struct buf *title;

	struct link_ref *next;
};

/* char_trigger: function pointer to render active chars */
/*   returns the number of chars taken care of */
/*   data is the pointer of the beginning of the span */
/*   offset is the number of valid chars before data */
struct sd_markdown;
typedef size_t
(*char_trigger)(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);

static size_t char_emphasis(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);
static size_t char_linebreak(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);
static size_t char_codespan(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);
static size_t char_escape(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);
static size_t char_entity(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);
static size_t char_langle_tag(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);
static size_t char_autolink_url(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);
static size_t char_autolink_email(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);
static size_t char_autolink_www(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);
static size_t char_link(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);
static size_t char_superscript(struct buf *ob, struct sd_markdown *rndr, uint8_t *data, size_t offset, size_t size);

enum markdown_char_t {
	MD_CHAR_NONE = 0,
	MD_CHAR_EMPHASIS,
	MD_CHAR_CODESPAN,
	MD_CHAR_LINEBREAK,
	MD_CHAR_LINK,
	MD_CHAR_LANGLE,
	MD_CHAR_ESCAPE,
	MD_CHAR_ENTITITY,
	MD_CHAR_AUTOLINK_URL,
	MD_CHAR_AUTOLINK_EMAIL,
	MD_CHAR_AUTOLINK_WWW,
	MD_CHAR_SUPERSCRIPT,
};

static char_trigger markdown_char_ptrs[] = {
	NULL,
	&char_emphasis,
	&char_codespan,
	&char_linebreak,
	&char_link,
	&char_langle_tag,
	&char_escape,
	&char_entity,
	&char_autolink_url,
	&char_autolink_email,
	&char_autolink_www,
	&char_superscript,
};

/* render • structure containing one particular render */
struct sd_markdown {
	struct sd_callbacks	cb;
	void *opaque;

	struct link_ref *refs[REF_TABLE_SIZE];
	uint8_t active_char[256];
	struct stack work_bufs[2];
	unsigned int ext_flags;
	size_t max_nesting;
	int in_link_body;
};

/***************************
 * HELPER FUNCTIONS *
 ***************************/

static inline struct buf *
rndr_newbuf(struct sd_markdown *rndr, int type)
{
	static const size_t buf_size[2] = {256, 64};
	struct buf *work = NULL;
	struct stack *pool = &rndr->work_bufs[type];

	if (pool->size < pool->asize &&
		pool->item[pool->size] != NULL) {
		work = pool->item[pool->size++];
		work->size = 0;
	} else {
		work = bufnew(buf_size[type]);
		stack_push(pool, work);
	}

	return work;
}

static inline void
rndr_popbuf(struct sd_markdown *rndr, int type)
{
	rndr->work_bufs[type].size--;
}

static void
unscape_text(struct buf *ob, struct buf *src)
{
	size_t i = 0, org;
	while (i < src->size) {
		org = i;
		while (i < src->size && src->data[i] != '\\')
			i++;

		if (i > org)
			bufput(ob, src->data + org, i - org);

		if (i + 1 >= src->size)
			break;

		bufputc(ob, src->data[i + 1]);
		i += 2;
	}
}

static unsigned int
hash_link_ref(const uint8_t *link_ref, size_t length)
{
	size_t i;
	unsigned int hash = 0;

	for (i = 0; i < length; ++i)
		hash = tolower(link_ref[i]) + (hash << 6) + (hash << 16) - hash;

	return hash;
}
