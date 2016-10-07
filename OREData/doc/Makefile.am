
EXTRA_DIST = \
             ored.doxy \
             oredextra.css \
             oredfooter.html \
             oredheader.html

DOXYGEN_CONFIG := ored.doxy
DOXYGEN_CUSTOM := oredextra.css oredheader.html oredfooter.html
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
	ln -s html OREData-docs-$(VERSION)-html
	tar chf - OREData-docs-$(VERSION)-html \
	    | GZIP=--best gzip -c > OREData-docs-$(VERSION)-html.tar.gz
	rm OREData-docs-$(VERSION)-html

docs-man: .time-stamp-man
.time-stamp-man: .time-stamp
	$(SED) -e "s/index/OREData/" man/man3/index.3 > man/man3/OREData.3
	rm man/man3/index.3
	touch .time-stamp-man

docs-man-dist: docs-man
	ln -s man OREData-docs-$(VERSION)-man
	tar chf - OREData-docs-$(VERSION)-man \
	    | GZIP=--best gzip -c > OREData-docs-$(VERSION)-man.tar.gz
	rm OREData-docs-$(VERSION)-man


docs-clean:
	rm -Rf man
	rm -Rf html
	rm -f .time-stamp*


.time-stamp: $(DOXYGEN_CONFIG) $(DOXYGEN_CUSTOM) $(DOXYGEN_INPUT)
	$(SED) -e "s|ored_basepath|${BASEPATH}/|" \
           -e "s|ored_version|$(VERSION)|" \
	       ored.doxy > .ored.doxy
	$(DOXYGEN) .ored.doxy
	rm -f .ored.doxy
	rm -f html/*.md5
	rm -f html/*.map
	touch .time-stamp
