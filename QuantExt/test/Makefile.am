
QLE_TESTS = \
	testsuite.cpp \
	utilities.hpp utilities.cpp \
	analyticlgmswaptionengine.hpp analyticlgmswaptionengine.cpp \
	currency.hpp currency.cpp \
	discountcurve.hpp discountcurve.cpp \
	dynamicblackvoltermstructure.hpp dynamicblackvoltermstructure.cpp \
	index.hpp index.cpp \
	logquote.hpp logquote.cpp \
	xassetmodel.hpp xassetmodel.cpp \
	xassetmodel2.hpp xassetmodel2.cpp \
	xassetmodelparametrizations.hpp xassetmodelparametrizations.cpp

dist-hook:
	mkdir -p $(distdir)/build
	mkdir -p $(distdir)/bin
	cp -p $(srcdir)/bin/runtest.bat $(distdir)/bin
	rm -f $(distdir)/main.cpp

if BOOST_UNIT_TEST_FOUND

AM_CPPFLAGS = -I${top_srcdir} -I${top_builddir} ${BOOST_UNIT_TEST_DEFINE}

noinst_LTLIBRARIES = libUnitMain.la
libUnitMain_la_SOURCES = main.cpp
libUnitMain_la_CXXFLAGS = ${BOOST_UNIT_TEST_MAIN_CXXFLAGS}

main.cpp: Makefile.am
	echo "#ifdef BOOST_TEST_DYN_LINK" > $@
	echo "#include <boost/test/unit_test.hpp>" >> $@
	echo "#include <boost/test/framework.hpp>" >> $@
	echo "using namespace boost::unit_test;" >> $@
	echo "test_suite* init_unit_test_suite(int, char* []);" >> $@
	echo "bool init_function() {" >> $@
	echo "framework::master_test_suite().add(init_unit_test_suite(0,0));" >> $@
	echo "return true;" >> $@
	echo "}" >> $@
	echo "int main( int argc, char* argv[] ) {" >> $@
	echo "return ::boost::unit_test::unit_test_main( &init_function, argc, argv );" >> $@
	echo "}" >> $@
	echo "#endif" >> $@


bin_PROGRAMS = quantext-test-suite

quantext_test_suite_SOURCES = ${QLE_TESTS}
quantext_test_suite_LDADD = libUnitMain.la
quantext_test_suite_LDFLAGS = \
    -lQuantLib \
    -l${BOOST_UNIT_TEST_LIB} \
    -L../qle -lQuantExt

TESTS = quantext-test-suite$(EXEEXT)
TESTS_ENVIRONMENT = BOOST_TEST_LOG_LEVEL=message

EXTRA_DIST =

else

EXTRA_DIST = \
	${QLE_TESTS}

endif
