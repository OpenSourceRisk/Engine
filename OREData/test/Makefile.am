
OREDATA_TESTS = \
	testsuite.cpp \
	bond.hpp bond.cpp \
	calendars.hpp calendars.cpp \
	fxoption.hpp fxoption.cpp \
	ccyswapwithresets.hpp ccyswapwithresets.cpp \
	fxtriangulation.hpp fxtriangulation.cpp \
	indices.hpp indices.cpp \
	parser.hpp parser.cpp \
	schedule.hpp schedule.cpp \
	xmlmanipulation.hpp xmlmanipulation.cpp \
	legdata.hpp legdata.cpp \
	todaysmarket.hpp todaysmarket.cpp \
	fxswap.hpp fxswap.cpp \
	yieldcurve.hpp yieldcurve.cpp \
	crossassetmodeldata.hpp crossassetmodeldata.cpp

dist-hook:
	mkdir -p $(distdir)/build
#	mkdir -p $(distdir)/bin
#	cp -p $(srcdir)/bin/runtest.bat $(distdir)/bin
	rm -f $(distdir)/main.cpp

if BOOST_UNIT_TEST_FOUND

AM_CPPFLAGS = -I${top_srcdir} -I${top_builddir} -I${top_builddir}/../QuantExt ${BOOST_UNIT_TEST_DEFINE}

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

bin_PROGRAMS = ored-test-suite

ored_test_suite_SOURCES = ${OREDATA_TESTS}
ored_test_suite_LDADD = libUnitMain.la
ored_test_suite_LDFLAGS = \
    -lQuantLib \
    -l${BOOST_UNIT_TEST_LIB} \
    -L../../QuantExt/qle -lQuantExt \
    -L../ored -lOREData -lboost_date_time -lboost_serialization -lboost_regex

TESTS = ored-test-suite$(EXEEXT)
TESTS_ENVIRONMENT = BOOST_TEST_LOG_LEVEL=message

EXTRA_DIST = \
	OREDataTestSuite.vcxproj \
	OREDataTestSuite.vcxproj.filters

else

EXTRA_DIST = \
	${OREDATA_TESTS} \
	OREDataTestSuite.vcxproj \
	OREDataTestSuite.vcxproj.filters

endif
