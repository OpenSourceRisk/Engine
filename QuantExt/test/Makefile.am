
QLE_TESTS = \
	testsuite.cpp \
	analyticlgmswaptionengine.cpp \
	staticallycorrectedyieldtermstructure.cpp \
	crossassetmodel.cpp \
	crossassetmodel2.cpp \
	crossassetmodelparametrizations.cpp \
	currency.cpp \
	discountcurve.cpp \
	dynamicblackvoltermstructure.cpp \
	dynamicswaptionvolmatrix.cpp \
	index.cpp \
	blackvariancecurve.cpp \
	logquote.cpp \
	cashflow.cpp \
	swaptionvolatilityconverter.cpp \
	optionletstripper.cpp \
	deposit.cpp \
	ratehelpers.cpp \
	stabilisedglls.cpp \
	survivalprobabilitycurve.cpp \
	swaptionvolconstantspread.cpp \
	fxvolsmile.cpp \
	payment.cpp \
	deltagammavar.cpp \
	pricecurve.cpp \
	commodityforward.cpp \
	discountingcommodityforwardengine.cpp \
	pricetermstructureadapter.cpp \
	bonds.cpp \
	interpolatedyoycapfloortermpricesurface.cpp \
	qle_calendars.cpp \
	discountratiomodifiedcurve.cpp \
	crossccyfixfloatswap.cpp \
	crossccyfixfloatswaphelper.cpp \
	crossccybasismtmresetswap.cpp \
	crossccybasismtmresetswaphelper.cpp \
	cpicapfloor.cpp 
	correlationtermstructure.cpp \
	cpicapfloor.cpp \
	strippedoptionletadapter.cpp \
	blackvolatilitysurfacemoneyness.cpp

dist-hook:
	mkdir -p $(distdir)/build

if BOOST_UNIT_TEST_FOUND

AM_CPPFLAGS = -DBOOST_ALL_DYN_LINK -DBOOST_TEST_DYN_LINK -I${top_srcdir} -I${top_builddir} ${BOOST_UNIT_TEST_DEFINE}

bin_PROGRAMS = quantext-test-suite

quantext_test_suite_SOURCES = ${QLE_TESTS}
quantext_test_suite_LDADD =
quantext_test_suite_LDFLAGS = \
	-lQuantLib \
	-L../qle -lQuantExt \
	-lboost_unit_test_framework

TESTS = quantext-test-suite$(EXEEXT)
TESTS_ENVIRONMENT = BOOST_TEST_LOG_LEVEL=message

EXTRA_DIST = \
	testsuite.vcxproj \
	testsuite.vcxproj.filters

else

EXTRA_DIST = \
	${QLE_TESTS} \
	testsuite.vcxproj \
	testsuite.vcxproj.filters

endif
