/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include "utilities.hpp"

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/cpicouponpricer.hpp>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/experimental/inflation/cpicapfloorengines.hpp>
#include <ql/experimental/inflation/cpicapfloortermpricesurface.hpp>
#include <ql/indexes/ibor/gbplibor.hpp>
#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/instruments/bonds/cpibond.hpp>
#include <ql/instruments/cpicapfloor.hpp>
#include <ql/instruments/cpiswap.hpp>
#include <ql/instruments/zerocouponinflationswap.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/bond/discountingbondengine.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/bootstraphelper.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/termstructures/volatility/inflation/constantcpivolatility.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/time/calendars/unitedkingdom.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/types.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
#include <qle/termstructures/interpolatedcpivolatilitysurface.hpp>
#include <qle/termstructures/strippedcpivolatilitystructure.hpp>
#include <qle/time/yearcounter.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace std;

namespace {
struct Datum {
    Date date;
    Rate rate;
};

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CPICapFloorTest)

template <class T, class U, class I>
std::vector<QuantLib::ext::shared_ptr<BootstrapHelper<T> > > makeHelpers(Datum iiData[], Size N, const QuantLib::ext::shared_ptr<I>& ii,
                                                                 const Period& observationLag, const Calendar& calendar,
                                                                 const BusinessDayConvention& bdc, const DayCounter& dc,
                                                                 Handle<YieldTermStructure> yts) {

    std::vector<QuantLib::ext::shared_ptr<BootstrapHelper<T> > > instruments;
    for (Size i = 0; i < N; i++) {
        Date maturity = iiData[i].date;
        Handle<Quote> quote(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(iiData[i].rate / 100.0)));
        QuantLib::ext::shared_ptr<BootstrapHelper<T> > anInstrument(
            new U(quote, observationLag, maturity, calendar, bdc, dc, ii, CPI::AsIndex, yts));
        instruments.push_back(anInstrument);
    }

    return instruments;
}

// Copy of the test setup from QuantLib/test-suite/inflationcpicapfloor.cpp
namespace {
struct CommonVars {
    // common data

    Size length;
    Date startDate;
    Rate baseZeroRate;
    Real volatility;

    Frequency frequency;
    std::vector<Real> nominals;
    Calendar calendar;
    BusinessDayConvention convention;
    Natural fixingDays;
    Date evaluationDate;
    Natural settlementDays;
    Date settlement;
    Period observationLag, contractObservationLag;
    CPI::InterpolationType contractObservationInterpolation;
    DayCounter dcZCIIS, dcNominal;
    std::vector<Date> zciisD;
    std::vector<Rate> zciisR;
    QuantLib::ext::shared_ptr<UKRPI> ii;
    RelinkableHandle<ZeroInflationIndex> hii;
    Size zciisDataLength;

    RelinkableHandle<YieldTermStructure> nominalUK;
    RelinkableHandle<ZeroInflationTermStructure> cpiUK;
    RelinkableHandle<ZeroInflationTermStructure> hcpi;

    vector<Rate> cStrikesUK;
    vector<Rate> fStrikesUK;
    vector<Period> cfMaturitiesUK;
    QuantLib::ext::shared_ptr<Matrix> cPriceUK;
    QuantLib::ext::shared_ptr<Matrix> fPriceUK;

    QuantLib::ext::shared_ptr<CPICapFloorTermPriceSurface> cpiCFsurfUK;

    // cleanup

    SavedSettings backup;

    // setup
    CommonVars() : nominals(1, 1000000) {
        // option variables
        frequency = Annual;
        // usual setup
        volatility = 0.01;
        length = 7;
        calendar = UnitedKingdom();
        convention = ModifiedFollowing;
        Date today(1, June, 2010);
        evaluationDate = calendar.adjust(today);
        Settings::instance().evaluationDate() = evaluationDate;
        settlementDays = 0;
        fixingDays = 0;
        settlement = calendar.advance(today, settlementDays, Days);
        startDate = settlement;
        dcZCIIS = ActualActual(ActualActual::ISDA);
        dcNominal = ActualActual(ActualActual::ISDA);

        // uk rpi index
        //      fixing data
        Date from(1, July, 2007);
        Date to(1, June, 2010);
        Schedule rpiSchedule = MakeSchedule()
                                   .from(from)
                                   .to(to)
                                   .withTenor(1 * Months)
                                   .withCalendar(UnitedKingdom())
                                   .withConvention(ModifiedFollowing);
        Real fixData[] = { 206.1, 207.3, 208.0, 208.9, 209.7, 210.9, 209.8, 211.4, 212.1, 214.0, 215.1, 216.8, //  2008
                           216.5, 217.2, 218.4, 217.7, 216.0, 212.9, 210.1, 211.4, 211.3, 211.5, 212.8, 213.4, //  2009
                           213.4, 214.4, 215.3, 216.0, 216.6, 218.0, 217.9, 219.2, 220.7, 222.8, -999,  -999,  //  2010
                           -999 };

        // link from cpi index to cpi TS
        ii = QuantLib::ext::make_shared<UKRPI>(hcpi);
        for (Size i = 0; i < rpiSchedule.size(); i++) {
            ii->addFixing(rpiSchedule[i], fixData[i], true); // force overwrite in case multiple use
        };

        Datum nominalData[] = { { Date(2, June, 2010), 0.499997 },
                                { Date(3, June, 2010), 0.524992 },
                                { Date(8, June, 2010), 0.524974 },
                                { Date(15, June, 2010), 0.549942 },
                                { Date(22, June, 2010), 0.549913 },
                                { Date(1, July, 2010), 0.574864 },
                                { Date(2, August, 2010), 0.624668 },
                                { Date(1, September, 2010), 0.724338 },
                                { Date(16, September, 2010), 0.769461 },
                                { Date(1, December, 2010), 0.997501 },
                                //{ Date( 16, December, 2010), 0.838164 },
                                { Date(17, March, 2011), 0.916996 },
                                { Date(16, June, 2011), 0.984339 },
                                { Date(22, September, 2011), 1.06085 },
                                { Date(22, December, 2011), 1.141788 },
                                { Date(1, June, 2012), 1.504426 },
                                { Date(3, June, 2013), 1.92064 },
                                { Date(2, June, 2014), 2.290824 },
                                { Date(1, June, 2015), 2.614394 },
                                { Date(1, June, 2016), 2.887445 },
                                { Date(1, June, 2017), 3.122128 },
                                { Date(1, June, 2018), 3.322511 },
                                { Date(3, June, 2019), 3.483997 },
                                { Date(1, June, 2020), 3.616896 },
                                { Date(1, June, 2022), 3.8281 },
                                { Date(2, June, 2025), 4.0341 },
                                { Date(3, June, 2030), 4.070854 },
                                { Date(1, June, 2035), 4.023202 },
                                { Date(1, June, 2040), 3.954748 },
                                { Date(1, June, 2050), 3.870953 },
                                { Date(1, June, 2060), 3.85298 },
                                { Date(2, June, 2070), 3.757542 },
                                { Date(3, June, 2080), 3.651379 } };
        const Size nominalDataLength = 33 - 1;

        std::vector<Date> nomD;
        std::vector<Rate> nomR;
        for (Size i = 0; i < nominalDataLength; i++) {
            nomD.push_back(nominalData[i].date);
            nomR.push_back(nominalData[i].rate / 100.0);
        }
        QuantLib::ext::shared_ptr<YieldTermStructure> nominalTS =
            QuantLib::ext::make_shared<InterpolatedZeroCurve<Linear> >(nomD, nomR, dcNominal);

        nominalUK.linkTo(nominalTS);

        // now build the zero inflation curve
        observationLag = Period(2, Months);
        contractObservationLag = Period(3, Months);
        contractObservationInterpolation = CPI::Flat;

        Datum zciisData[] = {
            { Date(1, June, 2011), 3.087 }, { Date(1, June, 2012), 3.12 },  { Date(1, June, 2013), 3.059 },
            { Date(1, June, 2014), 3.11 },  { Date(1, June, 2015), 3.15 },  { Date(1, June, 2016), 3.207 },
            { Date(1, June, 2017), 3.253 }, { Date(1, June, 2018), 3.288 }, { Date(1, June, 2019), 3.314 },
            { Date(1, June, 2020), 3.401 }, { Date(1, June, 2022), 3.458 }, { Date(1, June, 2025), 3.52 },
            { Date(1, June, 2030), 3.655 }, { Date(1, June, 2035), 3.668 }, { Date(1, June, 2040), 3.695 },
            { Date(1, June, 2050), 3.634 }, { Date(1, June, 2060), 3.629 },
        };
        zciisDataLength = 17;
        for (Size i = 0; i < zciisDataLength; i++) {
            zciisD.push_back(zciisData[i].date);
            zciisR.push_back(zciisData[i].rate);
        }

        // now build the helpers ...
        std::vector<QuantLib::ext::shared_ptr<BootstrapHelper<ZeroInflationTermStructure> > > helpers =
            makeHelpers<ZeroInflationTermStructure, ZeroCouponInflationSwapHelper, ZeroInflationIndex>(
                zciisData, zciisDataLength, ii, observationLag, calendar, convention, dcZCIIS,
                Handle<YieldTermStructure>(nominalTS));

        // we can use historical or first ZCIIS for this
        // we know historical is WAY off market-implied, so use market implied flat.
        baseZeroRate = zciisData[0].rate / 100.0;
        QuantLib::ext::shared_ptr<PiecewiseZeroInflationCurve<Linear>> pCPIts(
            new PiecewiseZeroInflationCurve<Linear>(evaluationDate, calendar, dcZCIIS, observationLag, ii->frequency(),
                                                    baseZeroRate, helpers));
        pCPIts->recalculate();
        cpiUK.linkTo(pCPIts);
        hii.linkTo(ii);

        // make sure that the index has the latest zero inflation term structure
        hcpi.linkTo(pCPIts);

        // cpi CF price surf data
        Period cfMat[] = { 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years };
        Real cStrike[] = { 0.03, 0.04, 0.05, 0.06 };
        Real fStrike[] = { -0.01, 0, 0.01, 0.02 };
        Size ncStrikes = 4, nfStrikes = 4, ncfMaturities = 7;

        Real cPrice[7][4] = { { 227.6, 100.27, 38.8, 14.94 },    { 345.32, 127.9, 40.59, 14.11 },
                              { 477.95, 170.19, 50.62, 16.88 },  { 757.81, 303.95, 107.62, 43.61 },
                              { 1140.73, 481.89, 168.4, 63.65 }, { 1537.6, 607.72, 172.27, 54.87 },
                              { 2211.67, 839.24, 184.75, 45.03 } };
        Real fPrice[7][4] = { { 15.62, 28.38, 53.61, 104.6 },   { 21.45, 36.73, 66.66, 129.6 },
                              { 24.45, 42.08, 77.04, 152.24 },  { 39.25, 63.52, 109.2, 203.44 },
                              { 36.82, 63.62, 116.97, 232.73 }, { 39.7, 67.47, 121.79, 238.56 },
                              { 41.48, 73.9, 139.75, 286.75 } };

        // now load the data into vector and Matrix classes
        cStrikesUK.clear();
        fStrikesUK.clear();
        cfMaturitiesUK.clear();
        for (Size i = 0; i < ncStrikes; i++)
            cStrikesUK.push_back(cStrike[i]);
        for (Size i = 0; i < nfStrikes; i++)
            fStrikesUK.push_back(fStrike[i]);
        for (Size i = 0; i < ncfMaturities; i++)
            cfMaturitiesUK.push_back(cfMat[i]);
        cPriceUK = QuantLib::ext::make_shared<Matrix>(ncStrikes, ncfMaturities);
        fPriceUK = QuantLib::ext::make_shared<Matrix>(nfStrikes, ncfMaturities);
        for (Size i = 0; i < ncStrikes; i++) {
            for (Size j = 0; j < ncfMaturities; j++) {
                (*cPriceUK)[i][j] = cPrice[j][i] / 10000.0;
            }
        }
        for (Size i = 0; i < nfStrikes; i++) {
            for (Size j = 0; j < ncfMaturities; j++) {
                (*fPriceUK)[i][j] = fPrice[j][i] / 10000.0;
            }
        }

        Real nominal = 1.0;
        QuantLib::ext::shared_ptr<InterpolatedCPICapFloorTermPriceSurface<Bilinear> > intplCpiCFsurfUK(
            new InterpolatedCPICapFloorTermPriceSurface<Bilinear>(
                nominal, baseZeroRate, observationLag, calendar, convention, dcZCIIS, ii, CPI::AsIndex, nominalUK, cStrikesUK,
                fStrikesUK, cfMaturitiesUK, *(cPriceUK), *(fPriceUK)));

        cpiCFsurfUK = intplCpiCFsurfUK;
    }
};

class FlatZeroInflationTermStructure : public ZeroInflationTermStructure {
public:
    FlatZeroInflationTermStructure(const Date& referenceDate, const Calendar& calendar, const DayCounter& dayCounter,
                                   Rate zeroRate, const Period& observationLag, Frequency frequency, bool indexIsInterp,
                                   const Handle<YieldTermStructure>& ts)
        : ZeroInflationTermStructure(referenceDate, calendar, dayCounter, zeroRate, observationLag, frequency),
          zeroRate_(zeroRate), indexIsInterp_(indexIsInterp) {}

    Date maxDate() const override { return Date::maxDate(); }
    // Base date consistent with observation lag, interpolation and frequency
    Date baseDate() const override {
        Date base = referenceDate() - observationLag();
        if (!indexIsInterp_) {
            std::pair<Date, Date> ips = inflationPeriod(base, frequency());
            base = ips.first;
        }
        return base;
    }

private:
    Rate zeroRateImpl(Time t) const override { return zeroRate_; }
    Real zeroRate_;
    bool indexIsInterp_;
};

} // namespace

// additional test cases from here

BOOST_AUTO_TEST_CASE(testVolatilitySurface) {
    CommonVars common;

    Handle<YieldTermStructure> nominalTS = common.nominalUK;

    // this engine is used to imply the volatility that reproduces a quoted price
    QuantLib::ext::shared_ptr<QuantExt::CPIBlackCapFloorEngine> blackEngine =
        QuantLib::ext::make_shared<QuantExt::CPIBlackCapFloorEngine>(nominalTS,
                                                             QuantLib::Handle<QuantLib::CPIVolatilitySurface>());

    // wrap the pointer into a handle
    Handle<CPICapFloorTermPriceSurface> cpiPriceSurfaceHandle(common.cpiCFsurfUK);

    // initialize the vol surface, taking the price surface as an input and running the vol imply calculations
    QuantExt::PriceQuotePreference type = QuantExt::CapFloor;
    QuantLib::ext::shared_ptr<QuantExt::StrippedCPIVolatilitySurface<QuantLib::Bilinear> > cpiVolSurface =
        QuantLib::ext::make_shared<QuantExt::StrippedCPIVolatilitySurface<QuantLib::Bilinear> >(type, cpiPriceSurfaceHandle,
                                                                                        common.ii, blackEngine);

    // attach the implied vol surface to the engine
    blackEngine->setVolatility(QuantLib::Handle<QuantLib::CPIVolatilitySurface>(cpiVolSurface));

    // reprice and check that we recover the quotes

    Real nominal = 1.0;

    Date startDate = Settings::instance().evaluationDate();
    Calendar fixCalendar = UnitedKingdom(), payCalendar = UnitedKingdom();
    BusinessDayConvention fixConvention(Unadjusted), payConvention(ModifiedFollowing);
    Real baseCPI = common.hii->fixing(fixCalendar.adjust(startDate - common.observationLag, fixConvention));
    CPI::InterpolationType observationInterpolation = CPI::AsIndex;

    Handle<CPICapFloorTermPriceSurface> cpiCFsurfUKh(common.cpiCFsurfUK);
    QuantLib::ext::shared_ptr<PricingEngine> engine(new InterpolatingCPICapFloorEngine(cpiCFsurfUKh));

    for (Size i = 0; i < common.cStrikesUK.size(); i++) {
        Rate strike = common.cStrikesUK[i];
        for (Size j = 0; j < common.cfMaturitiesUK.size(); j++) {
            Period maturity = common.cfMaturitiesUK[j];
            Date maturityDate = startDate + maturity;

            CPICapFloor aCap(Option::Call, nominal, startDate, baseCPI, maturityDate, fixCalendar, fixConvention,
                             payCalendar, payConvention, strike, common.hii.currentLink(), common.observationLag,
                             observationInterpolation);

            aCap.setPricingEngine(engine);

            Real cached = (*common.cPriceUK)[i][j] * 10000;
            Real npv1 = aCap.NPV() * 10000;
            // QL_REQUIRE(fabs(cached - npv1) < 1e-10,
            //            "InterpolatingCPICapFloorEngine does not reproduce cached price: " << cached << " vs " <<
            //            npv1);
            BOOST_CHECK_SMALL(fabs(cached - npv1), 1e-10);

            aCap.setPricingEngine(blackEngine);
            Real npv2 = aCap.NPV() * 10000;
            // QL_REQUIRE(fabs(cached - npv2) < 0.1,
            //            "CPIBlackCapFloorEngine does not reproduce cached price: " << cached << " vs " << npv2);
            BOOST_CHECK_SMALL(fabs(cached - npv2), 1e-5); // depends on the default solver tolerance 1e-8

            BOOST_TEST_MESSAGE("Cap " << fixed << std::setprecision(2) << strike << " " << setw(3) << maturity
                                      << ":  cached " << setw(7) << cached << "  QL " << setw(8) << npv1 << "  QLE "
                                      << setw(8) << npv2 << "  diff " << setw(8) << npv2 - npv1);
        }
    }

    for (Size i = 0; i < common.fStrikesUK.size(); i++) {
        Rate strike = common.fStrikesUK[i];
        //       fStrikesUK.push_back(fStrike[i]);
        for (Size j = 0; j < common.cfMaturitiesUK.size(); j++) {
            Period maturity = common.cfMaturitiesUK[j];
            Date maturityDate = startDate + maturity;

            CPICapFloor aFloor(Option::Put, nominal, startDate, baseCPI, maturityDate, fixCalendar, fixConvention,
                               payCalendar, payConvention, strike, common.hii.currentLink(), common.observationLag,
                               observationInterpolation);

            aFloor.setPricingEngine(engine);

            Real cached = (*common.fPriceUK)[i][j] * 10000;
            Real npv1 = aFloor.NPV() * 10000;
            // QL_REQUIRE(fabs(cached - npv1) < 1e-10,
            //            "InterpolatingCPICapFloorEngine does not reproduce cached price: " << cached << " vs " <<
            //            npv1);
            BOOST_CHECK_SMALL(fabs(cached - npv1), 1e-10);

            aFloor.setPricingEngine(blackEngine);
            Real npv2 = aFloor.NPV() * 10000;
            // QL_REQUIRE(fabs(cached - npv2) < 0.1,
            //            "CPIBlackCapFloorEngine does not reproduce cached price: " << cached << " vs " << npv2);
            BOOST_CHECK_SMALL(fabs(cached - npv2), 1e-5); // depends on the default solver tolerance 1e-8

            BOOST_TEST_MESSAGE("Floor " << fixed << std::setprecision(2) << strike << " " << setw(3) << maturity
                                        << ":  cached " << setw(7) << cached << "  QL " << setw(8) << npv1 << "  QLE "
                                        << setw(8) << npv2 << "  diff " << setw(8) << npv2 - npv1);
        }
    }
}

BOOST_AUTO_TEST_CASE(testPutCallParity) {

    CommonVars common;

    Handle<YieldTermStructure> nominalTS = common.nominalUK;

    // this engine is used to imply the volatility that reproduces a quoted price
    QuantLib::ext::shared_ptr<QuantExt::CPIBlackCapFloorEngine> blackEngine =
        QuantLib::ext::make_shared<QuantExt::CPIBlackCapFloorEngine>(nominalTS,
                                                             QuantLib::Handle<QuantLib::CPIVolatilitySurface>());

    // wrap the pointer into a handle
    Handle<CPICapFloorTermPriceSurface> cpiPriceSurfaceHandle(common.cpiCFsurfUK);

    // initialize the vol surface, taking the price surface as an input and running the vol imply calculations
    QuantExt::PriceQuotePreference type = QuantExt::CapFloor;
    QuantLib::ext::shared_ptr<QuantExt::StrippedCPIVolatilitySurface<QuantLib::Bilinear> > cpiVolSurface =
        QuantLib::ext::make_shared<QuantExt::StrippedCPIVolatilitySurface<QuantLib::Bilinear> >(type, cpiPriceSurfaceHandle,
                                                                                        common.ii, blackEngine);

    // attach the implied vol surface to the engine
    blackEngine->setVolatility(QuantLib::Handle<QuantLib::CPIVolatilitySurface>(cpiVolSurface));

    Period mat[] = { 3 * Years,  4 * Years,  5 * Years,  6 * Years,  7 * Years,  8 * Years, 9 * Years,
                     10 * Years, 12 * Years, 15 * Years, 20 * Years, 25 * Years, 30 * Years };
    Size nMat = 13;

    Real strike[] = { 0.0, 0.005, 0.01, 0.015, 0.02, 0.025, 0.03, 0.035, 0.04, 0.045, 0.05 };
    Size nStrike = 11;

    Real nominal = 1.0;

    Date startDate = Settings::instance().evaluationDate();
    Calendar fixCalendar = UnitedKingdom(), payCalendar = UnitedKingdom();
    BusinessDayConvention fixConvention(Unadjusted), payConvention(ModifiedFollowing);
    Real baseCPI = common.hii->fixing(fixCalendar.adjust(startDate - common.observationLag, fixConvention));
    CPI::InterpolationType observationInterpolation = CPI::AsIndex;

    Date effectiveStart = startDate - common.observationLag;
    std::pair<Date, Date> ips = inflationPeriod(effectiveStart, common.ii->frequency());
    effectiveStart = ips.first;
    
    for (Size i = 0; i < nStrike; ++i) {

        for (Size j = 0; j < nMat; ++j) {
            Date maturityDate = startDate + mat[j];

            CPICapFloor aCap(Option::Call, nominal, startDate, baseCPI, maturityDate, fixCalendar, fixConvention,
                             payCalendar, payConvention, strike[i], common.hii.currentLink(), common.observationLag,
                             observationInterpolation);
            aCap.setPricingEngine(blackEngine);

            CPICapFloor aFloor(Option::Put, nominal, startDate, baseCPI, maturityDate, fixCalendar, fixConvention,
                               payCalendar, payConvention, strike[i], common.hii.currentLink(), common.observationLag,
                               observationInterpolation);
            aFloor.setPricingEngine(blackEngine);

            Real capPrice = aCap.NPV() * 10000;
            Real floorPrice = aFloor.NPV() * 10000;

            // build CPI leg price manually
            Date fixingDate = maturityDate - common.observationLag;
            Date effectiveMaturity = fixingDate;
            Real timeFromStart =
                common.ii->zeroInflationTermStructure()->dayCounter().yearFraction(effectiveStart, effectiveMaturity);
            Real K = pow(1.0 + strike[i], timeFromStart);
            Real F = common.ii->fixing(effectiveMaturity) / baseCPI;
            DiscountFactor disc = nominalTS->discount(maturityDate);
            Real cpilegPrice = disc * (F - K) * 10000;

            Real parity = capPrice - floorPrice - cpilegPrice;
            BOOST_TEST_MESSAGE(std::setprecision(3) << fixed << "strike=" << strike[i] << " mat=" << mat[j] << " cap="
                                                    << capPrice << " floor=" << floorPrice << " cpileg=" << cpilegPrice
                                                    << " parity=cap-floor-cpileg=" << parity);

            // FIXME: investigate why the parity check yields ~bp upfront error in some cases
            BOOST_CHECK_SMALL(fabs(parity), 1.1); // bp upfront parity error
        }
    }
}

BOOST_AUTO_TEST_CASE(testInterpolatedVolatilitySurface) {
    CommonVars common;

    Handle<YieldTermStructure> nominalTS = common.nominalUK;

    // this engine is used to imply the volatility that reproduces a quoted price
    QuantLib::ext::shared_ptr<QuantExt::CPIBlackCapFloorEngine> blackEngine =
        QuantLib::ext::make_shared<QuantExt::CPIBlackCapFloorEngine>(nominalTS,
                                                             QuantLib::Handle<QuantLib::CPIVolatilitySurface>());

    // wrap the pointer into a handle
    Handle<CPICapFloorTermPriceSurface> cpiPriceSurfaceHandle(common.cpiCFsurfUK);

    // initialize the vol surface, taking the price surface as an input and running the vol imply calculations
    QuantExt::PriceQuotePreference type = QuantExt::CapFloor;
    QuantLib::ext::shared_ptr<QuantExt::StrippedCPIVolatilitySurface<QuantLib::Bilinear> > cpiVolSurface =
        QuantLib::ext::make_shared<QuantExt::StrippedCPIVolatilitySurface<QuantLib::Bilinear> >(type, cpiPriceSurfaceHandle,
                                                                                        common.ii, blackEngine);

    std::vector<Period> optionTenors = cpiVolSurface->maturities();
    std::vector<Real> strikes = cpiVolSurface->strikes();
    vector<vector<Handle<Quote> > > quotes(optionTenors.size(),
                                           vector<Handle<Quote> >(strikes.size(), Handle<Quote>()));
    for (Size i = 0; i < optionTenors.size(); ++i) {
        for (Size j = 0; j < strikes.size(); ++j) {
            Real vol = cpiVolSurface->volatility(optionTenors[i], strikes[j]);
            QuantLib::ext::shared_ptr<SimpleQuote> q(new SimpleQuote(vol));
            quotes[i][j] = Handle<Quote>(q);
        }
    }
    QuantLib::ext::shared_ptr<QuantExt::InterpolatedCPIVolatilitySurface<Bilinear> > interpolatedCpiVol =
        QuantLib::ext::make_shared<QuantExt::InterpolatedCPIVolatilitySurface<Bilinear> >(
            optionTenors, strikes, quotes, common.hii.currentLink(), cpiVolSurface->settlementDays(),
            cpiVolSurface->calendar(), cpiVolSurface->businessDayConvention(), cpiVolSurface->dayCounter(),
            cpiVolSurface->observationLag());

    for (Size i = 0; i < optionTenors.size(); ++i) {
        Date d = cpiVolSurface->optionDateFromTenor(optionTenors[i]);
        for (Size j = 0; j < strikes.size(); ++j) {
            Real vol1 = cpiVolSurface->volatility(d, strikes[j]);
            Real vol2 = interpolatedCpiVol->volatility(d, strikes[j]);
            BOOST_CHECK_SMALL(fabs(vol1 - vol2), 1.0e-10);
        }
    }
}

BOOST_AUTO_TEST_CASE(testSimpleCapFloor) {
    CommonVars common;

    Real rate = 0.03;
    Real inflationRate = 0.02;
    Real inflationBlackVol = 0.05;
    BusinessDayConvention bdc = Unadjusted;
    DayCounter dc = ActualActual(ActualActual::ISDA);
    Period observationLag = 3 * Months; // EUHICPXT Caps/Swaps
    Handle<YieldTermStructure> discountCurve(QuantLib::ext::make_shared<FlatForward>(common.evaluationDate, rate, dc));
    RelinkableHandle<ZeroInflationTermStructure> inflationCurve;
    Handle<ZeroInflationIndex> index(QuantLib::ext::make_shared<EUHICPXT>(inflationCurve));
    // Make sure we use the correct index publication frequency and interpolation, consistent with the index we want to
    // project. We therefore create the index first, then the term structure, then relink the curve. Otherwise time
    // calculations will be inconsistent, and ATM strikes do not generate equal put/call or cap/floor prices.
    QuantLib::ext::shared_ptr<ZeroInflationTermStructure> inflationCurvePtr =
        QuantLib::ext::make_shared<FlatZeroInflationTermStructure>(common.evaluationDate, index->fixingCalendar(), dc,
                                                           inflationRate, observationLag, index->frequency(),
                                                           false, discountCurve);
    inflationCurve.linkTo(inflationCurvePtr);
    // Make sure we use the same observation lag as in the inflation curve, and same index publication frequency and
    // interpolation. The vol surface observation lag is used in the engine for lag difference calculations compared to
    // the instrument's lag.
    Handle<CPIVolatilitySurface> inflationVol(QuantLib::ext::make_shared<ConstantCPIVolatility>(
        inflationBlackVol, 0, inflationCurve->calendar(), bdc, dc, inflationCurve->observationLag(),
        inflationCurve->frequency(), false));

    QuantLib::ext::shared_ptr<PricingEngine> engine =
        QuantLib::ext::make_shared<QuantExt::CPIBlackCapFloorEngine>(discountCurve, inflationVol);

    Real nominal = 10000.0;
    Date start = common.evaluationDate;
    Date end = start + 10 * Years;
    Real baseCPI = 100.0;
    Calendar fixCalendar = index->fixingCalendar();
    Calendar payCalendar = index->fixingCalendar();

    Rate capStrike = 0.03;
    CPICapFloor atmCap(Option::Call, nominal, start, baseCPI, end, fixCalendar, bdc, payCalendar, bdc, inflationRate,
                       index.currentLink(), observationLag);
    atmCap.setPricingEngine(engine);
    CPICapFloor cap(Option::Call, nominal, start, baseCPI, end, fixCalendar, bdc, payCalendar, bdc, capStrike,
                    index.currentLink(), observationLag);
    cap.setPricingEngine(engine);

    Rate floorStrike = 0.01;
    CPICapFloor atmFloor(Option::Put, nominal, start, baseCPI, end, fixCalendar, bdc, fixCalendar, bdc, inflationRate,
                         index.currentLink(), observationLag);
    atmFloor.setPricingEngine(engine);
    CPICapFloor floor(Option::Put, nominal, start, baseCPI, end, fixCalendar, bdc, fixCalendar, bdc, floorStrike,
                      index.currentLink(), observationLag);
    floor.setPricingEngine(engine);

    DayCounter cpiDayCounter = QuantExt::YearCounter();

    // use base CPI as base fixing
    index->addFixing(inflationCurve->baseDate(), baseCPI);

    // Do cap and floor match at the money?
    BOOST_CHECK_CLOSE(atmCap.NPV(), atmFloor.NPV(), 1e-8);

    // Pedestrian CPI Cap/Floor valuation assuming baseCPI = base fixing
    Real t = 10.0; // time to maturity is 10 (base to effective maturity)
    Real stdDev = inflationBlackVol * sqrt(t);
    Real forward = pow(1.0 + inflationRate, t);
    Real floorStrikePrice = pow(1.0 + floorStrike, t);
    Real capStrikePrice = pow(1.0 + capStrike, t);
    Real discount = exp(-rate * t);
    Real expectedCapNPV = nominal * blackFormula(Option::Call, capStrikePrice, forward, stdDev, discount);
    Real expectedFloorNPV = nominal * blackFormula(Option::Put, floorStrikePrice, forward, stdDev, discount);

    BOOST_TEST_MESSAGE("CPI Cap NPV " << cap.NPV() << " " << expectedCapNPV);
    BOOST_TEST_MESSAGE("CPI Floor NPV " << floor.NPV() << " " << expectedFloorNPV);

    BOOST_CHECK_CLOSE(cap.NPV(), expectedCapNPV, 0.01);     // relative difference of 0.01%
    BOOST_CHECK_CLOSE(floor.NPV(), expectedFloorNPV, 0.01); // relative difference of 0.01%
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

} // namespace
