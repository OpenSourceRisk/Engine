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
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/voltermstructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/indexes/inflation/ukrpi.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>

using namespace QuantLib;
using namespace ore::data;
using std::vector;
using std::pair;

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
            new FlatForward(Settings::instance().evaluationDate(), forward, ActualActual()));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward) {
        boost::shared_ptr<BlackVolTermStructure> fxv(
            new BlackConstantVol(Settings::instance().evaluationDate(), NullCalendar(), forward, ActualActual()));
        return Handle<BlackVolTermStructure>(fxv);
    }
    Handle<YieldTermStructure> flatRateDiv(Real dividend) {
        boost::shared_ptr<YieldTermStructure> yts(
            new FlatForward(Settings::instance().evaluationDate(), dividend, ActualActual()));
        return Handle<YieldTermStructure>(yts);
    }
    Handle<QuantLib::SwaptionVolatilityStructure>
    flatRateSvs(Volatility forward, VolatilityType type = ShiftedLognormal, Real shift = 0.0) {
        boost::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
            new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                     ModifiedFollowing, forward, ActualActual(), type, shift));
        return Handle<QuantLib::SwaptionVolatilityStructure>(svs);
    }
    Handle<DefaultProbabilityTermStructure> flatRateDcs(Volatility forward) {
        boost::shared_ptr<DefaultProbabilityTermStructure> dcs(new FlatHazardRate(asof_, forward, ActualActual()));
        return Handle<DefaultProbabilityTermStructure>(dcs);
    }
    Handle<OptionletVolatilityStructure> flatRateCvs(Volatility vol, VolatilityType type = Normal, Real shift = 0.0) {
        boost::shared_ptr<OptionletVolatilityStructure> ts(
            new QuantLib::ConstantOptionletVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                      ModifiedFollowing, vol, ActualActual(), type, shift));
        return Handle<OptionletVolatilityStructure>(ts);
    }
    Handle<ZeroInflationIndex> makeZeroInflationIndex(string index, vector<Date> dates, vector<Rate> rates, boost::shared_ptr<ZeroInflationIndex> ii, Handle<YieldTermStructure> yts);
    Handle<YoYInflationIndex> makeYoYInflationIndex(string index, vector<Date> dates, vector<Rate> rates, boost::shared_ptr<YoYInflationIndex> ii, Handle<YieldTermStructure> yts);
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
    //! Conventions instance
    static boost::shared_ptr<ore::data::Conventions> conv();
};
} // namespace testsuite
