
EXTRA_DIST = \
             quantext.doxy \
             quantextextra.css \
             quantextfooter.html \
             quantextheader.html

DOXYGEN_CONFIG := quantext.doxy
DOXYGEN_CUSTOM := quantextextra.css quantextheader.html quantextfooter.html
DOXYGEN_INPUT := $(shell find ${top_srcdir} -name *.hpp) \
                 $(wildcard pages/*.docs)

BASEPATH = @abs_top_srcdir@

%.gz: %
	rm -f $@
	gzip --best $<
%.tar: %
	tar chf $@ $<
	rm $<

.PHONY: docs-all docs-html docs-man docs-clean
.PHONY: docs-dist docs-html-dist docs-man-dist

dist-hook:
#	mkdir -p $(distdir)/pages $(distdir)/Examples $(distdir)/images
#	mkdir -p $(distdir)/pages $(distdir)/images
#	cp -p $(srcdir)/pages/*.docs $(distdir)/pages
#	cp -p $(srcdir)/Examples/*.cpp $(distdir)/Examples
#	cp -p $(srcdir)/images/*.jpg \
#	      $(srcdir)/images/*.ico \
#	      $(distdir)/images

docs-all: docs-html docs-man
docs-dist: docs-html-dist docs-man-dist

docs-html: .time-stamp

docs-html-dist: docs-html
	ln -s html QuantExt-docs-$(VERSION)-html
	tar chf - QuantExt-docs-$(VERSION)-html \
	    | GZIP=--best gzip -c > QuantExt-docs-$(VERSION)-html.tar.gz
	rm QuantExt-docs-$(VERSION)-html

docs-man: .time-stamp-man
.time-stamp-man: .time-stamp
	$(SED) -e "s/index/QuantExt/" man/man3/index.3 > man/man3/QuantExt.3
	rm man/man3/index.3
	touch .time-stamp-man

docs-man-dist: docs-man
	ln -s man QuantExt-docs-$(VERSION)-man
	tar chf - QuantExt-docs-$(VERSION)-man \
	    | GZIP=--best gzip -c > QuantExt-docs-$(VERSION)-man.tar.gz
	rm QuantExt-docs-$(VERSION)-man


docs-clean:
	rm -Rf man
	rm -Rf html
	rm -f .time-stamp*


.time-stamp: $(DOXYGEN_CONFIG) $(DOXYGEN_CUSTOM) $(DOXYGEN_INPUT)
	$(SED) -e "s|qle_basepath|${BASEPATH}/|" \
           -e "s|qle_version|$(VERSION)|" \
	       quantext.doxy > .quantext.doxy
	$(DOXYGEN) .quantext.doxy
	rm -f .quantext.doxy
	rm -f html/*.md5
	rm -f html/*.map
	touch .time-stamp
