/*** caev-rdr.c -- reader for caev message strings
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
#include <string.h>
#include <ctype.h>
#include "caev-supp.h"
#include "caev-rdr.h"
#include "ctl-dfp754.h"
#include "intern.h"
#include "nifty.h"

/* gperf gen'd goodness */
#if defined __INTEL_COMPILER
# pragma warning (disable:869)
#endif	/* __INTEL_COMPILER */
#include "caev-rdr-gp.c"
#include "caev-fld-gp.c"
#if defined __INTEL_COMPILER
# pragma warning (default:869)
#endif	/* __INTEL_COMPILER */


static ctl_fld_val_t
snarf_fv(ctl_fld_key_t fc, const char *s)
{
	ctl_fld_val_t res = {};

	switch (ctl_fld_type(fc)) {
	case CTL_FLD_TYPE_ADMIN:
		;
		break;

	case CTL_FLD_TYPE_DATE:
		;
		break;

	case CTL_FLD_TYPE_RATIO: {
		char *pp;
		signed int p;
		unsigned int q;

		p = strtol(s, &pp, 10);
		if (*pp != ':' && *pp != '/') {
			break;
		} else if (!(q = strtoul(pp + 1U, &pp, 10))) {
			break;
		}
		/* otherwise ass */
		res.ratio = (ctl_ratio_t){p, q};
		break;
	}
	case CTL_FLD_TYPE_PRICE: {
		char *pp;
		_Decimal32 p;

		p = strtod32(s, &pp);
		res.price = (ctl_price_t)p;
		break;
	}
	case CTL_FLD_TYPE_PERIO:
		;
		break;

	case CTL_FLD_TYPE_CUSTM: {
		char *pp;
		_Decimal32 v;
		signed int p;
		unsigned int q;

		v = strtod32(s, &pp);
		if (*pp != '+' && *pp != '-') {
			break;
		}
		p = strtol(pp, &pp, 10);
		if (pp[0] != '<' || pp[1] != '-') {
			break;
		}
		q = strtoul(pp + 2U, &pp, 10),
		/* otherwise ass */
		res.custm = (ctl_custm_t){.r = (ctl_ratio_t){p, q}, .a = v};
		break;
	}
	default:
		return res;
	}
	return res;
}

static ctl_kvv_t
make_kvv(const struct ctl_kv_s *f, size_t n)
{
	ctl_kvv_t res = malloc(sizeof(*res) + n * sizeof(*res->kvv));

	memcpy(res->kvv, f, (res->nkvv = n) * sizeof(*res->kvv));
	return res;
}


ctl_caev_t
ctl_caev_rdr(echs_instant_t t, const char *s)
{
	static struct ctl_fld_s *flds;
	static size_t nflds;
	ctl_caev_code_t ccod;
	ctl_caev_t res = ctl_zero_caev();
	size_t fldi;

	if (*s == '.') {
		s++;
	}
	with (ctl_fld_rdr_t f) {
		if ((f = __ctl_fldify(s, 4U)) == NULL) {
			/* not a caev message */
			goto out;
		} else if ((ctl_fld_admin_t)f->fc != CTL_FLD_CAEV) {
			/* a field but not a caev message */
			goto out;
		}
		/* fast forward s then */
		s += 4U/*CAEV*/ + 1U/*=*/;
	}
	/* otherwise try and read the code */
	switch (*s) {
	case '"':
	case '\'':
		s++;
		break;
	default:
		/* no quotes :/ wish me luck */
		break;
	}
	with (ctl_caev_rdr_t m = __ctl_caev_codify(s, 4U)) {
		if (LIKELY(m == NULL)) {
			/* not a caev message */
			goto out;
		}
		/* otherwise assign the caev code */
		ccod = m->code;
		/* and fast forward s */
		s += 4U;
	}

#define CHECK_FLDS							\
	if (UNLIKELY(fldi >= nflds)) {					\
		const size_t nu = nflds + 64U;				\
		flds = realloc(flds, nu * sizeof(*flds));		\
		memset(flds + nflds, 0, (nu - nflds) * sizeof(*flds));	\
		nflds = nu;						\
	}

	/* reset field counter */
	fldi = 0U;
	/* add the instant passed onto us as ex-date */
	CHECK_FLDS;
	flds[fldi++] = MAKE_CTL_FLD(admin, CTL_FLD_CAEV, ccod);
	flds[fldi++] = MAKE_CTL_FLD(date, CTL_FLD_XDTE, t);

	/* now look for .XXXX= or .XXXX/ or XXXX= or XXXX/ */
	for (const char *sp = s; (sp = strchr(sp, ' ')) != NULL; sp++) {
		sp++;
		if (*sp == '{') {
			/* overread braces */
			sp++;
		}
		if (*sp == '.') {
			/* overread . */
			sp++;
		}
		switch (sp[4U]) {
			ctl_fld_rdr_t f;
			ctl_fld_unk_t fc;
			ctl_fld_val_t fv;
		case '/':
		case '=':
			/* ah, could be a field */
			if ((f = __ctl_fldify(sp, 4U)) == NULL) {
				break;
			}
			/* otherwise we've got the code */
			fc = (ctl_fld_unk_t)f->fc;
			with (const char *vp = sp += 4U) {
				if (*vp++ != '=') {
					if ((vp = strchr(vp, '=')) == NULL) {
						continue;
					}
					vp++;
				}
				if (*vp == '"' || *vp == '\'') {
					/* dequote */
					vp++;
				}
				fv = snarf_fv(fc, vp);
			}

			/* bang to array */
			if (fldi >= nflds) {
				const size_t nu = nflds + 64U;
				flds = realloc(flds, nu * sizeof(*flds));
				memset(flds + nflds, 0, 64U * sizeof(*flds));
				nflds = nu;
			}
			/* actually add the field now */
			flds[fldi++] = (ctl_fld_t){{fc}, fv};
			break;
		default:
			break;
		}
	}
	/* just let the actual mt564 support figure everything out */
	res = make_caev(flds, fldi);
out:
	return res;
}

__attribute__((pure, const)) ctl_caev_code_t
ctl_kvv_get_caev_code(ctl_kvv_t fldv)
{
	if (UNLIKELY(fldv->nkvv == 0U)) {
		goto out;
	}
	with (ctl_fld_rdr_t f) {
		/* assume first field is the caev indicator */
		const char *k0 = obint_name(fldv->kvv[0].key);

		if ((f = __ctl_fldify(k0, 4U)) == NULL) {
			/* not a caev message */
			goto out;
		} else if ((ctl_fld_admin_t)f->fc != CTL_FLD_CAEV) {
			/* a field but not a caev message */
			goto out;
		}
	}
	/* otherwise try and read the code */
	with (ctl_caev_rdr_t m) {
		const char *v0 = obint_name(fldv->kvv[0].val);

		if (UNLIKELY((m = __ctl_caev_codify(v0, 4U)) == NULL)) {
			/* not a caev message */
			goto out;
		}
		/* otherwise return the caev code */
		return m->code;
	}
out:
	return CTL_CAEV_UNK;
}

ctl_caev_t
ctl_kvv_get_caev(ctl_kvv_t fldv)
{
	static struct ctl_fld_s *flds;
	static size_t nflds;
	ctl_caev_code_t ccod;
	ctl_caev_t res = ctl_zero_caev();
	size_t fldi;

	if (UNLIKELY((ccod = ctl_kvv_get_caev_code(fldv)) == CTL_CAEV_UNK)) {
		/* not a caev message, at least not one we support */
		goto out;
	}

	/* reset field counter */
	fldi = 0U;
	/* add the instant passed onto us as ex-date */
	CHECK_FLDS;
	flds[fldi++] = MAKE_CTL_FLD(admin, CTL_FLD_CAEV, ccod);
	/* go through all them fields then */
	for (size_t i = 1U; i < fldv->nkvv; i++) {
		const char *k = obint_name(fldv->kvv[i].key);
		const char *v = obint_name(fldv->kvv[i].val);
		ctl_fld_rdr_t f;
		ctl_fld_unk_t fc;
		ctl_fld_val_t fv;

		if ((f = __ctl_fldify(k, 4U)) == NULL) {
			/* not a field then aye */
			continue;
		}
		/* otherwise we've got the code */
		fc = (ctl_fld_unk_t)f->fc;
		fv = snarf_fv(fc, v);

		/* bang to array */
		CHECK_FLDS;
		/* actually add the field now */
		flds[fldi++] = (ctl_fld_t){{fc}, fv};
	}
	/* just let the actual mt564 support figure everything out */
	res = make_caev(flds, fldi);
out:
	return res;
}

ctl_kvv_t
ctl_kv_rdr(const char *s)
{
	static struct ctl_kv_s *flds;
	static size_t nflds;
	const char *cp;
	size_t fldi = 0U;

	do {
		char ep = ' ';

		/* fast forward to a field */
		for (; *s && !isalpha(*s); s++);
		if (!*s) {
			break;
		}

		/* fast forward to the assignment */
		for (cp = s; *s != '='; s++);

		/* use CHECK_FLDS from above */
		CHECK_FLDS;

		/* get interned field */
		flds[fldi].key = intern(cp, s++ - cp);

		/* otherwise try and read the code */
		switch (*s) {
		case '"':
		case '\'':
			ep = *s;
			s++;
			break;
		default:
			/* no quotes :/ wish me luck */
			break;
		}
		for (cp = s; *s >= ' ' && *s != ep; s++);
		/* get interned value */
		flds[fldi++].val = intern(cp, s - cp);
	} while (1);
	return make_kvv(flds, fldi);
}

void
free_kvv(ctl_kvv_t f)
{
	free(f);
	return;
}

/* caev-rdr.c ends here */
