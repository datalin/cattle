/*** cattle.c -- tool to apply corporate actions
 *
 * Copyright (C) 2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of cattle.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "cattle.h"
#include "caev.h"
#include "nifty.h"
#include "dt-strpf.h"
#include "wheap.h"

struct ctl_ctx_s {
	ctl_wheap_t q;
	ctl_caev_t sum;
};


static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


/* callbacks */
static inline void
ctl_add_caev(struct ctl_ctx_s ctx[static 1], echs_instant_t inst, uintptr_t msg)
{
	ctl_wheap_add_deferred(ctx->q, inst, msg);
	return;
}

static void
ctl_parse_caev(struct ctl_ctx_s ctx[static 1], echs_instant_t t, const char *s)
{
	ctl_add_caev(ctx, t, (uintptr_t)strdup(s));
	return;
}


/* public api, might go to libcattle one day */
ctl_ctx_t
ctl_open_caev_file(const char *fn)
{
/* wants a const char *fn */
	char *line = NULL;
	size_t llen = 0UL;
	ssize_t nrd;
	struct ctl_ctx_s *ctx; 
	FILE *f;

	if (UNLIKELY((f = fopen(fn, "r")) == NULL)) {
		goto nul;
	} else if (UNLIKELY((ctx = calloc(1, sizeof(*ctx))) == NULL)) {
		goto nul;
	} else if (UNLIKELY((ctx->q = make_ctl_wheap()) == NULL)) {
		goto nul;
	}

	while ((nrd = getline(&line, &llen, f)) > 0) {
		char *p;
		echs_instant_t t;

		if (*line == '#') {
			continue;
		} else if ((p = strchr(line, '\t')) == NULL) {
			break;
		} else if (__inst_0_p(t = dt_strp(line))) {
			break;
		}
		/* insert */
		ctl_parse_caev(ctx, t, p + 1U);
	}
	/* now sort the guy */
	ctl_wheap_fix_deferred(ctx->q);
nul:
	if (f != NULL) {
		fclose(f);
	}
	if (line != NULL) {
		free(line);
	}
	return ctx;
}

void
ctl_close_caev_file(ctl_ctx_t x)
{
	if (LIKELY(x->q != NULL)) {
		free_ctl_wheap(x->q);
	}
	free(x);
	return;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "cattle.xh"
#include "cattle.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct gengetopt_args_info argi[1];
	ctl_ctx_t ctx;
	int res = 0;

	if (cmdline_parser(argc, argv, argi)) {
		goto out;
	}

	with (const char *fn = argi->inputs[0]) {
		if (UNLIKELY((ctx = ctl_open_caev_file(fn)) == NULL)) {
			error("cannot open file `%s'", fn);
			goto out;
		}
	}

	for (echs_instant_t t; !__inst_0_p(t = ctl_wheap_top_rank(ctx->q));) {
		uintptr_t x = ctl_wheap_pop(ctx->q);
		char buf[256U];
		char *bp = buf;

		bp += dt_strf(buf, sizeof(buf), t);
		*bp++ = '\t';
		with (char *s = (void*)x) {
			strcpy(bp, s);
			free(s);
		}
		puts(buf);
	}

	/* and out again */
	ctl_close_caev_file(ctx);

out:
	/* just to make sure */
	fflush(stdout);
	cmdline_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* cattle.c ends here */
