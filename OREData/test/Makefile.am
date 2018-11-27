
OREDATA_TESTS = \
	testsuite.cpp \
	bond.cpp \
	calendars.cpp \
	fxoption.cpp \
	ccyswapwithresets.cpp \
	cpiswap.cpp \
	cms.cpp \
	fxtriangulation.cpp \
	indices.cpp \
	parser.cpp \
	schedule.cpp \
	xmlmanipulation.cpp \
	legdata.cpp \
	todaysmarket.cpp \
	fxswap.cpp \
	yieldcurve.cpp \
	crossassetmodeldata.cpp \
	equitymarketdata.cpp \
	equitytrades.cpp \
	swaption.cpp \
	portfolio.cpp \
	curveconfig.cpp \
	ored_commodityforward.cpp \
	commoditycurveconfig.cpp \
	commoditycurve.cpp \
	commodityoption.cpp \
	commodityvolcurveconfig.cpp \
	commodityvolcurve.cpp \
	cds.cpp \
	inflationcapfloor.cpp \
	equityswap.cpp \
	conventions.cpp

dist-hook:
	mkdir -p $(distdir)/build
	rm -f $(distdir)/main.cpp

if BOOST_UNIT_TEST_FOUND

AM_CPPFLAGS = -I${top_srcdir} -I${top_builddir} -I${top_builddir}/../ORETest -I${top_builddir}/../QuantExt ${BOOST_UNIT_TEST_DEFINE}

bin_PROGRAMS = ored-test-suite

ored_test_suite_SOURCES = ${OREDATA_TESTS}
ored_test_suite_LDADD = 
ored_test_suite_LDFLAGS = \
    -lQuantLib \
    -L../../QuantExt/qle -lQuantExt \
    -L../ored -lOREData -lboost_date_time -lboost_serialization -lboost_regex -lboost_system -lboost_filesystem

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
