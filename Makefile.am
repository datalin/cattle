include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =
EXTRA_DIST = $(BUILT_SOURCES)
BUILT_SOURCES =
DISTCLEANFILES =
CLEANFILES =

SUBDIRS += build-aux
SUBDIRS += src
SUBDIRS += info
SUBDIRS += test

DISTCLEANFILES += version.mk
EXTRA_DIST += version.mk.in
BUILT_SOURCES += .version

doc_DATA = README.md
EXTRA_DIST += $(doc_DATA)

if USE_ASM_CORUS
DISTCHECK_CONFIGURE_FLAGS = --with-asm-backed-coroutines
endif  USE_ASM_CORUS

.version:
	$(AM_V_GEN) echo "v$(VERSION)" > $@

## make sure .version is read-only in the dist
dist-hook:
	chmod ugo-w $(distdir)/.version

## Makefile.am ends here
