
OREANALYTICS_TESTS = \
	testsuite.cpp \
	aggregationscenariodata.hpp aggregationscenariodata.cpp \
	cube.hpp cube.cpp \
	scenariosimmarket.hpp scenariosimmarket.cpp \
	swapperformance.hpp swapperformance.cpp \
	scenariogenerator.hpp scenariogenerator.cpp \
	sensitivityanalysis.hpp sensitivityanalysis.cpp \
	sensitivityanalysis2.hpp sensitivityanalysis2.cpp \
	testmarket.hpp \
	testportfolio.hpp testportfolio.cpp \
	observationmode.hpp observationmode.cpp \
	stresstest.hpp stresstest.cpp \
	sensitivityperformance.hpp sensitivityperformance.cpp

dist-hook:
	mkdir -p $(distdir)/build
#	mkdir -p $(distdir)/bin
#	cp -p $(srcdir)/bin/runtest.bat $(distdir)/bin
#	rm -f $(distdir)/main.cpp

if BOOST_UNIT_TEST_FOUND

AM_CPPFLAGS = -I${top_srcdir} -I${top_builddir} -I${top_builddir}/../QuantExt ${BOOST_UNIT_TEST_DEFINE} -I${top_builddir}/../OREData

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

bin_PROGRAMS = orea-test-suite

orea_test_suite_SOURCES = ${OREANALYTICS_TESTS}
orea_test_suite_LDADD = libUnitMain.la
orea_test_suite_LDFLAGS = \
    -lQuantLib \
    -l${BOOST_UNIT_TEST_LIB} \
    -L../../QuantExt/qle -lQuantExt \
    -L../../OREData/ored -lOREData \
    -L../orea -lOREAnalytics \
    -lboost_date_time -lboost_serialization -lboost_regex -lboost_filesystem -lboost_system

TESTS = orea-test-suite$(EXEEXT)
TESTS_ENVIRONMENT = BOOST_TEST_LOG_LEVEL=message

EXTRA_DIST = \
	OREAnalyticsTestSuite.vcxproj \
	OREAnalyticsTestSuite.vcxproj.filters

else

EXTRA_DIST = \
	${OREANALYTICS_TESTS} \
	OREAnalyticsTestSuite.vcxproj \
	OREAnalyticsTestSuite.vcxproj.filters

endif
