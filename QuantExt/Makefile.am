SUBDIRS = qle test m4 doc

ACLOCAL_AMFLAGS = -I m4 

dist_data_DATA = config

m4datadir = $(datadir)/aclocal

EXTRA_DIST = \
	autogen.sh \
	QuantExt_vc11.sln \
	QuantExt_vc11.vcxproj \
	QuantExt_vc11.vcxproj.filters


.PHONY: docs docs-man docs-online docs-dist docs-clean
BASEPATH = @abs_top_srcdir@
docs:
	BASEPATH=$(BASEPATH) $(MAKE) -C doc docs-all
html-local:
	BASEPATH=$(BASEPATH) $(MAKE) -C doc docs-html
pdf-local:
	BASEPATH=$(BASEPATH) $(MAKE) -C doc docs-pdf
ps-local:
	BASEPATH=$(BASEPATH) $(MAKE) -C doc docs-ps
docs-man:
	BASEPATH=$(BASEPATH) $(MAKE) -C doc docs-man
docs-online:
	BASEPATH=$(BASEPATH) $(MAKE) -C doc docs-online
docs-dist:
	BASEPATH=$(BASEPATH) $(MAKE) -C doc docs-dist
docs-clean:
	$(MAKE) -C doc docs-clean

dist-hook:
	mkdir -p $(distdir)/build
