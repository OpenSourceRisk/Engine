SUBDIRS = orea test m4 doc

ACLOCAL_AMFLAGS = -I m4

dist_data_DATA = config

m4datadir = $(datadir)/aclocal

EXTRA_DIST = \
	autogen.sh \
	OREAnalytics.sln \
	OREAnalytics.vcxproj \
	OREAnalytics.vcxproj.filters

.PHONY: docs docs-man docs-online docs-dist docs-clean
BASEPATH = @abs_top_srcdir@
docs:
	BASEPATH=$(BASEPATH) $(MAKE) -C doc docs-all
docs-html:
	BASEPATH=$(BASEPATH) $(MAKE) -C doc docs-html
docs-man:
	BASEPATH=$(BASEPATH) $(MAKE) -C doc docs-man
docs-dist:
	BASEPATH=$(BASEPATH) $(MAKE) -C doc docs-dist
docs-clean:
	$(MAKE) -C doc docs-clean

dist-hook:
	mkdir -p $(distdir)/build
