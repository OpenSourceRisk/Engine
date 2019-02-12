
OREANALYTICS_TESTS = \
	testsuite.cpp \
	aggregationscenariodata.cpp \
	cube.cpp \
	scenariosimmarket.cpp \
	swapperformance.cpp \
	scenariogenerator.cpp \
	sensitivityanalysis.cpp \
	sensitivityanalysisanalytic.cpp \
	testmarket.cpp \
	testportfolio.cpp \
	observationmode.cpp \
	stresstest.cpp \
	sensitivityperformance.cpp \
	shiftscenariogenerator.cpp \
	sensitivityaggregator.cpp

dist-hook:
	mkdir -p $(distdir)/build

if BOOST_UNIT_TEST_FOUND

AM_CPPFLAGS = -DBOOST_ALL_DYN_LINK -DBOOST_TEST_DYN_LINK -I${top_srcdir} -I${top_builddir} -I${top_builddir}/../ORETest -I${top_builddir}/../QuantExt ${BOOST_UNIT_TEST_DEFINE} -I${top_builddir}/../OREData

bin_PROGRAMS = orea-test-suite

orea_test_suite_SOURCES = ${OREANALYTICS_TESTS}
orea_test_suite_LDADD =
orea_test_suite_LDFLAGS = \
    -lQuantLib \
    -L../../QuantExt/qle -lQuantExt \
    -L../../OREData/ored -lOREData \
    -L../orea -lOREAnalytics \
    -lboost_date_time -lboost_serialization -lboost_regex -lboost_filesystem -lboost_system -lboost_unit_test_framework

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
