#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "houdini.h"

#define ESCAPE_GROW_FACTOR(x) (((x) * 12) / 10) /* this is very scientific, yes */

/**
 * According to the OWASP rules:
 *
 * & --> &amp;
 * < --> &lt;
 * > --> &gt;
 * " --> &quot;
 * ' --> &#x27;     &apos; is not recommended
 * / --> &#x2F;     forward slash is included as it helps end an HTML entity
 *
 */
static const char HTML_ESCAPE_TABLE[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 4, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 6, 0, 
	0, 0, 0, 0, 0, 0,