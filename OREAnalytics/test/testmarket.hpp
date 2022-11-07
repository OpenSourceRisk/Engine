/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/inflation/constantcpivolatility.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/voltermstructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/strippedcpivolatilitystructure.hpp>

using namespace QuantLib;
using namespace ore::data;
using std::pair;
using std::vector;

namespace testsuite {

//! Simple flat market setup to be used in the test suite
/*!
  \ingroup tests
*/
class TestMarket : public ore::data::MarketImpl {
public:
    TestMarket(Date asof);

private:
    Handle<YieldTermStructure> flatRateYts(Real forward) {
        boost::shared_ptr<YieldTermStructure> yts(
            new FlatForward(Settings::instance().evaluationDate(), forward, ActualActual(ActualActual::ISDA)));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward) {
        boost::shared_ptr<BlackVolTermStructure> fxv(
            new BlackConstantVol(Settings::instance().evaluationDate(), NullCalendar(), forward, ActualActual(ActualActual::ISDA)));
        return Handle<BlackVolTermStructure>(fxv);
    }
    Handle<YieldTermStructure> flatRateDiv(Real dividend) {
        boost::shared_ptr<YieldTermStructure> yts(
            new FlatForward(Settings::instance().evaluationDate(), dividend, ActualActual(ActualActual::ISDA)));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<QuantLib::SwaptionVolatilityStructure>
    flatRateSvs(Volatility forward, VolatilityType type = ShiftedLognormal, Real shift = 0.0) {
        boost::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
            new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                     ModifiedFollowing, forward, ActualActual(ActualActual::ISDA), type, shift));
        return Handle<QuantLib::SwaptionVolatilityStructure>(svs);
    }
    Handle<QuantExt::CreditCurve> flatRateDcs(Volatility forward) {
        boost::shared_ptr<DefaultProbabilityTermStructure> dcs(
            new FlatHazardRate(asof_, forward, ActualActual(ActualActual::ISDA)));
        return Handle<QuantExt::CreditCurve>(
            boost::make_shared<QuantExt::CreditCurve>(Handle<DefaultProbabilityTermStructure>(dcs)));
    }
    Handle<OptionletVolatilityStructure> flatRateCvs(Volatility vol, VolatilityType type = Normal, Real shift = 0.0) {
        boost::shared_ptr<OptionletVolatilityStructure> ts(
            new QuantLib::ConstantOptionletVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                      ModifiedFollowing, vol, ActualActual(ActualActual::ISDA), type, shift));
        return Handle<OptionletVolatilityStructure>(ts);
    }
    Handle<QuantExt::CorrelationTermStructure> flatCorrelation(Real correlation = 0.0) {
        boost::shared_ptr<QuantExt::CorrelationTermStructure> ts(
            new QuantExt::FlatCorrelation(Settings::instance().evaluationDate(), correlation, ActualActual(ActualActual::ISDA)));
        return Handle<QuantExt::CorrelationTermStructure>(ts);
    }
    Handle<CPICapFloorTermPriceSurface> flatRateCps(Handle<ZeroInflationIndex> infIndex,
                                                    const std::vector<Rate> cStrikes, std::vector<Rate> fStrikes,
                                                    std::vector<Period> cfMaturities, Matrix cPrice, Matrix fPrice) {
        boost::shared_ptr<CPICapFloorTermPriceSurface> ts(
            new InterpolatedCPICapFloorTermPriceSurface<QuantLib::Bilinear>(
                1.0, 0.0, infIndex->availabilityLag(), infIndex->zeroInflationTermStructure()->calendar(), Following,
                ActualActual(ActualActual::ISDA), infIndex.currentLink(), CPI::AsIndex, discountCurve(infIndex->currency().code()), cStrikes, fStrikes, cfMaturities,
                cPrice, fPrice));
        return Handle<CPICapFloorTermPriceSurface>(ts);
    }
    Handle<QuantLib::CPIVolatilitySurface> flatCpiVolSurface(Volatility v) {
        Natural settleDays = 0;
        Calendar cal = TARGET();
        BusinessDayConvention bdc = Following;
        DayCounter dc = Actual365Fixed();
        Period lag = 2 * Months;
        Frequency freq = Annual;
        bool interp = false;
        boost::shared_ptr<ConstantCPIVolatility> cpiCapFloorVolSurface =
            boost::make_shared<ConstantCPIVolatility>(v, settleDays, cal, bdc, dc, lag, freq, interp);

        return Handle<QuantLib::CPIVolatilitySurface>(cpiCapFloorVolSurface);
    }
    Handle<ZeroInflationIndex> makeZeroInflationIndex(string index, vector<Date> dates, vector<Rate> rates,
                                                      boost::shared_ptr<ZeroInflationIndex> ii,
                                                      Handle<YieldTermStructure> yts);
    Handle<YoYInflationIndex> makeYoYInflationIndex(string index, vector<Date> dates, vector<Rate> rates,
                                                    boost::shared_ptr<YoYInflationIndex> ii,
                                                    Handle<YieldTermStructure> yts);
};

//! Static class to allow for easy construction of configuration objects for use within tests
class TestConfigurationObjects {
public:
    //! ScenarioSimMarketParameters instance, 2 currencies
    static boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> setupSimMarketData2();
    //! ScenarioSimMarketParameters instance, 5 currencies
    static boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters> setupSimMarketData5();
    //! SensitivityScenarioData instance, 2 currencies
    static boost::shared_ptr<ore::analytics::SensitivityScenarioData> setupSensitivityScenarioData2();
    //! SensitivityScenarioData instance, 5 currencies
    static boost::shared_ptr<ore::analytics::SensitivityScenarioData> setupSensitivityScenarioData5();
    //! SensitivityScenarioData instance, 2 currencies, shifts more granular than base curve
    static boost::shared_ptr<ore::analytics::SensitivityScenarioData> setupSensitivityScenarioData2b();
    //! Set Conventions
    static void setConventions();
};
} // namespace testsuite
