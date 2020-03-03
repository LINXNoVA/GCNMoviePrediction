
﻿Sundown
=======

`Sundown` is a Markdown parser based on the original code of the
[Upskirt library](http://fossil.instinctive.eu/libupskirt/index) by Natacha Porté.

Features
--------

*	**Fully standards compliant**

	`Sundown` passes out of the box the official Markdown v1.0.0 and v1.0.3
	test suites, and has been extensively tested with additional corner cases
	to make sure its output is as sane as possible at all times.

*	**Massive extension support**

	`Sundown` has optional support for several (unofficial) Markdown extensions,
	such as non-strict emphasis, fenced code blocks, tables, autolinks,
	strikethrough and more.

*	**UTF-8 aware**

	`Sundown` is fully UTF-8 aware, both when parsing the source document and when
	generating the resulting (X)HTML code.

*	**Tested & Ready to be used on production**

	`Sundown` has been extensively security audited, and includes protection against
	all possible DOS attacks (stack overflows, out of memory situations, malformed
	Markdown syntax...) and against client attacks through malicious embedded HTML.

	We've worked very hard to make `Sundown` never crash or run out of memory
	under *any* input. `Sundown` renders all the Markdown content in GitHub and so
	far hasn't crashed a single time.

*	**Customizable renderers**

	`Sundown` is not stuck with XHTML output: the Markdown parser of the library
	is decoupled from the renderer, so it's trivial to extend the library with