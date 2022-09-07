#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>

#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <qle/termstructures/interpolatedcpivolatilitysurface.hpp>
#include <qle/termstructures/strippedcpivolatilitystructure.hpp>
#include <qle/utilities/inflation.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace std;

namespace {

struct CommonData {

    Date today;
    Real tolerance;
    DayCounter dayCounter;
    Calendar fixingCalendar;
    std::vector<Period> zeroCouponPillars{1 * Years, 2 * Years, 3 * Years, 5 * Years};
    std::vector<Rate> zeroCouponQuotes{0.06, 0.04, 0.03, 0.02};

    boost::shared_ptr<SimpleQuote> flatZero = boost::make_shared<SimpleQuote>(0.01);
    Period obsLag;
    Handle<YieldTermStructure> discountTS;

    std::map<Date, Rate> cpiFixings{{Date(1, May, 2021), 97.8744653499849},
                                    {Date(1, Jun, 2021), 98.0392156862745},
                                    {Date(1, Jul, 2021), 98.1989155376188},
                                    {Date(1, Aug, 2021), 98.3642120151039},
                                    {Date(1, Sep, 2021), 98.5297867331921},
                                    {Date(1, Oct, 2021), 98.6902856945937},
                                    {Date(1, Nov, 2021), 98.8564092866721},
                                    {Date(1, Dec, 2021), 99.0174402961208},
                                    {Date(1, Jan, 2022), 99.1841145816863},
                                    {Date(1, Feb, 2022), 99.3510694270946},
                                    {Date(1, Mar, 2022), 99.5021088919576},
                                    {Date(1, Apr, 2022), 99.6695990114986},
                                    {Date(1, May, 2022), 99.8319546569845},
                                    {Date(1, Jun, 2022), 100},
                                    {Date(1, July, 2022), 104}};

    std::vector<Rate> strikes{0.02, 0.04, 0.06, 0.08};
    std::vector<Period> tenors{1 * Years, 2 * Years, 3 * Years};

    vector<vector<Handle<Quote>>> vols{
        {Handle<Quote>(boost::make_shared<SimpleQuote>(0.3)), Handle<Quote>(boost::make_shared<SimpleQuote>(0.32)),
         Handle<Quote>(boost::make_shared<SimpleQuote>(0.34)), Handle<Quote>(boost::make_shared<SimpleQuote>(0.36))},
        {Handle<Quote>(boost::make_shared<SimpleQuote>(0.35)), Handle<Quote>(boost::make_shared<SimpleQuote>(0.37)),
         Handle<Quote>(boost::make_shared<SimpleQuote>(0.39)), Handle<Quote>(boost::make_shared<SimpleQuote>(0.41))},
        {Handle<Quote>(boost::make_shared<SimpleQuote>(0.04)), Handle<Quote>(boost::make_shared<SimpleQuote>(0.42)),
         Handle<Quote>(boost::make_shared<SimpleQuote>(0.44)), Handle<Quote>(boost::make_shared<SimpleQuote>(0.46))}};

    

    CommonData()
        : today(15, Aug, 2022), tolerance(1e-6), dayCounter(Actual365Fixed()), fixingCalendar(NullCalendar()),
          obsLag(2, Months), discountTS(Handle<YieldTermStructure>(boost::make_shared<FlatForward>(
                                 0, NullCalendar(), Handle<Quote>(flatZero), dayCounter))){};
};

void addFixings(const std::map<Date, Rate> fixings, ZeroInflationIndex& index) {
    index.clearFixings();
    for (const auto& fixing : fixings) {
        index.addFixing(fixing.first, fixing.second, true);
    }
};

boost::shared_ptr<Seasonality> buildSeasonalityCurve() {
    std::vector<double> factors{0.99, 1.01, 0.98, 1.02, 0.97, 1.03, 0.96, 1.04, 0.95, 1.05, 0.94, 1.06};
    Date seasonalityBaseDate(1, Jan, 2022);
    return boost::make_shared<MultiplicativePriceSeasonality>(seasonalityBaseDate, Monthly, factors);
}

boost::shared_ptr<ZeroInflationCurve>
buildZeroInflationCurve(CommonData& cd, bool useLastKnownFixing, const boost::shared_ptr<ZeroInflationIndex>& index,
                        const bool isInterpolated, const boost::shared_ptr<Seasonality>& seasonality = nullptr) {
    Date today = Settings::instance().evaluationDate();
    
    DayCounter dc = cd.dayCounter;

    BusinessDayConvention bdc = ModifiedFollowing;

    

    std::vector<boost::shared_ptr<QuantExt::ZeroInflationTraits::helper>> helpers;
    for (size_t i = 0; i < cd.zeroCouponQuotes.size(); ++i) {
        Date maturity = today + cd.zeroCouponPillars[i];
        Rate quote = cd.zeroCouponQuotes[i];
        boost::shared_ptr<QuantExt::ZeroInflationTraits::helper> instrument =
            boost::make_shared<ZeroCouponInflationSwapHelper>(
                Handle<Quote>(boost::make_shared<SimpleQuote>(quote)), cd.obsLag, maturity, cd.fixingCalendar, bdc, dc,
                index, isInterpolated ? CPI::Linear : CPI::Flat, Handle<YieldTermStructure>(cd.discountTS), today);
        helpers.push_back(instrument);
    }
    Rate baseRate = QuantExt::ZeroInflation::guessCurveBaseRate(useLastKnownFixing, today, cd.zeroCouponPillars[0],
                                                                cd.dayCounter, cd.obsLag, cd.zeroCouponQuotes[0],
                                                                cd.obsLag, cd.dayCounter, index, isInterpolated);
    boost::shared_ptr<ZeroInflationCurve> curve = boost::make_shared<QuantExt::PiecewiseZeroInflationCurve<Linear>>(
        today, cd.fixingCalendar, dc, cd.obsLag, index->frequency(), baseRate, helpers, 1e-10, index,
        useLastKnownFixing);
    if (seasonality) {
        curve->setSeasonality(seasonality);
    }
    return curve;
}

boost::shared_ptr<CPIVolatilitySurface> buildVolSurface(CommonData& cd,
                                                        const boost::shared_ptr<ZeroInflationIndex>& index, const bool useLastKnownFixing) {
    auto surface = boost::make_shared<QuantExt::InterpolatedCPIVolatilitySurface<Bilinear>>(
        cd.tenors, cd.strikes, cd.vols, index, 0 , cd.fixingCalendar, ModifiedFollowing, cd.dayCounter, cd.obsLag,
        useLastKnownFixing);
    surface->enableExtrapolation();
    return surface;
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(InflationCPIVolatilityTest)

BOOST_AUTO_TEST_CASE(testVolatiltiy) {
    CommonData cd;
    Settings::instance().evaluationDate() = cd.today;
    Date today = Settings::instance().evaluationDate();
    bool isInterpolated = false;
    bool useLastKnownFixingDateAsBaseDate = true;
    boost::shared_ptr<ZeroInflationIndex> curveBuildIndex = boost::make_shared<EUHICPXT>(false);
    addFixings(cd.cpiFixings, *curveBuildIndex);
    auto curve = buildZeroInflationCurve(cd, useLastKnownFixingDateAsBaseDate, curveBuildIndex, isInterpolated);

    BOOST_CHECK_NO_THROW(curve->zeroRate(1.0));

    auto index = curveBuildIndex->clone(Handle<ZeroInflationTermStructure>(curve));

    RelinkableHandle<CPIVolatilitySurface> volSurface;

    

    auto volSurfaceObsLag = buildVolSurface(cd, index, false);

    volSurface.linkTo(volSurfaceObsLag);

    auto expected_vol = 0.3;
    auto vol = volSurface->volatility(Date(1, Jun, 2023), 0.02, 0 * Days);

    
    BOOST_CHECK_EQUAL(volSurface->baseDate(), Date(1, Jun, 2022));
    BOOST_CHECK_CLOSE(volSurface->timeFromBase(Date(1, Jun, 2023), 0 * Days), 1.0, cd.tolerance);
    BOOST_CHECK_CLOSE(vol, expected_vol, cd.tolerance);

    auto volSurfaceLastFixing = buildVolSurface(cd, index, true);

    volSurface.linkTo(volSurfaceLastFixing);

    BOOST_CHECK_EQUAL(volSurface->baseDate(), Date(1, Jul, 2022));
    BOOST_CHECK_CLOSE(volSurface->timeFromBase(Date(1, Jun, 2023), 0 * Days),
                      cd.dayCounter.yearFraction(Date(1, July, 2022), Date(1, Jun, 2023)), cd.tolerance);
    BOOST_CHECK_CLOSE(volSurface->volatility(Date(1, Jun, 2023), 0.02, 0 * Days), expected_vol, cd.tolerance);

    // Pricing

    CPICapFloor put(Option::Put, 1, cd.today, Null<Real>(), Date(15, Aug, 2023), cd.fixingCalendar, ModifiedFollowing,
                    cd.fixingCalendar, ModifiedFollowing, 0.02, Handle<ZeroInflationIndex>(index), 2 * Months, CPI::Flat);

    CPICapFloor seasonedPut(Option::Put, 1, Date(15, Aug, 2021), Null<Real>(), Date(15, Aug, 2023), cd.fixingCalendar,
                            ModifiedFollowing,
                            cd.fixingCalendar, ModifiedFollowing, 0.025, Handle<ZeroInflationIndex>(index), 2 * Months,
                            CPI::Flat);

    auto pricingEngine = boost::make_shared<QuantExt::CPIBlackCapFloorEngine>(cd.discountTS, volSurface);

    put.setPricingEngine(pricingEngine);
    seasonedPut.setPricingEngine(pricingEngine);

    BOOST_CHECK_CLOSE(put.NPV(), 0.09889734672, cd.tolerance);   

    BOOST_CHECK_CLOSE(seasonedPut.NPV(), 0.11002621921, cd.tolerance);

}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
