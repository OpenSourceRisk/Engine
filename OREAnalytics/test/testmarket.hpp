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

#pragma once

#include <boost/make_shared.hpp>

#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>

#include <ored/marketdata/marketimpl.hpp>
#include <ored/utilities/indexparser.hpp>

#include <qle/indexes/fxindex.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/strippedcpivolatilitystructure.hpp>
#include <qle/termstructures/crossccybasisswaphelper.hpp>
#include <qle/termstructures/oisratehelper.hpp>

#include <ql/math/interpolations/flatextrapolation2d.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/credit/piecewisedefaultcurve.hpp>
#include <ql/termstructures/credit/defaultprobabilityhelpers.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/inflation/constantcpivolatility.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/voltermstructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/actual360.hpp>

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
    TestMarket(Date asof, bool swapVolCube = false);

private:
    Handle<YieldTermStructure> flatRateYts(Real forward);
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward);
    Handle<YieldTermStructure> flatRateDiv(Real dividend);
    Handle<QuantLib::SwaptionVolatilityStructure> flatRateSvs(Volatility forward, VolatilityType type = ShiftedLognormal,
                                                              Real shift = 0.0);
    Handle<QuantExt::CreditCurve> flatRateDcs(Volatility forward);
    Handle<OptionletVolatilityStructure> flatRateCvs(Volatility vol, VolatilityType type = Normal, Real shift = 0.0);
    Handle<QuantExt::CorrelationTermStructure> flatCorrelation(Real correlation = 0.0);
    Handle<CPICapFloorTermPriceSurface> flatRateCps(Handle<ZeroInflationIndex> infIndex,
                                                    const std::vector<Rate> cStrikes, std::vector<Rate> fStrikes,
                                                    std::vector<Period> cfMaturities, Matrix cPrice, Matrix fPrice);
    Handle<QuantLib::CPIVolatilitySurface> flatCpiVolSurface(Volatility v);
    Handle<ZeroInflationIndex> makeZeroInflationIndex(string index, vector<Date> dates, vector<Rate> rates,
                                                      QuantLib::ext::shared_ptr<ZeroInflationIndex> ii,
                                                      Handle<YieldTermStructure> yts);
    Handle<YoYInflationIndex> makeYoYInflationIndex(string index, vector<Date> dates, vector<Rate> rates,
                                                    QuantLib::ext::shared_ptr<YoYInflationIndex> ii,
                                                    Handle<YieldTermStructure> yts);
    Handle<ZeroInflationTermStructure> flatZeroInflationCurve(Real inflationRate, Rate nominalRate);
    Handle<YoYInflationTermStructure> flatYoYInflationCurve(Real inflationRate, Rate nominalRate);
    Handle<YoYOptionletVolatilitySurface> flatYoYOptionletVolatilitySurface(Real normalVol);
};

class TestMarketParCurves : public ore::data::MarketImpl {
public:
    TestMarketParCurves(const Date& asof);

    const map<string, vector<string>>& discountRateHelpersInstMap() const { return discountRateHelperInstMap_; }
    const map<string, vector<string>>& equityForecastRateHelpersInstMap() const {
        return equityForecastRateHelperInstMap_;
    }
    const map<string, vector<string>>& indexCurveRateHelperInstMap() const { return indexCurveRateHelperInstMap_; }
    const map<string, vector<string>>& defaultRateHelpersInstMap() const { return defaultRateHelperInstMap_; }
    const map<string, vector<string>>& zeroInflationRateHelperInstMap() const {
        return zeroInflationRateHelperInstMap_;
    }
    const map<string, vector<string>>& yoyInflationRateHelperInstMap() const { return yoyInflationRateHelperInstMap_; }
    const map<string, vector<Period>>& discountRateHelperTenorsMap() const { return discountRateHelperTenorsMap_; }
    const map<string, vector<Period>>& equityForecastRateHelperTenorsMap() const {
        return equityForecastRateHelperTenorsMap_;
    }
    const map<string, vector<Period>>& indexCurveRateHelperTenorsMap() const { return indexCurveRateHelperTenorsMap_; }
    const map<string, vector<Period>>& defaultRateHelperTenorsMap() const { return defaultRateHelperTenorsMap_; }
    const map<string, vector<Period>>& cdsVolRateHelperTenorsMap() const { return cdsVolRateHelperTenorsMap_; }
    const map<string, vector<Period>>& swaptionVolRateHelperTenorsMap() const {
        return swaptionVolRateHelperTenorsMap_;
    }
    const map<string, vector<Period>>& swaptionVolRateHelperSwapTenorsMap() const {
        return swaptionVolRateHelperSwapTenorsMap_;
    }
    const map<string, vector<Period>>& equityVolRateHelperTenorsMap() const { return equityVolRateHelperTenorsMap_; }
    const map<string, vector<Period>>& baseCorrRateHelperTenorsMap() const { return baseCorrRateHelperTenorsMap_; }
    const map<string, vector<string>>& baseCorrLossLevelsMap() const { return baseCorrLossLevelsMap_; }
    const map<string, vector<Period>>& zeroInflationRateHelperTenorsMap() const {
        return zeroInflationRateHelperTenorsMap_;
    }
    const map<string, vector<Period>>& yoyInflationRateHelperTenorsMap() const {
        return yoyInflationRateHelperTenorsMap_;
    }
    const map<string, vector<QuantLib::ext::shared_ptr<RateHelper>>>& equityForecastRateHelpersMap() const {
        return equityForecastRateHelpersMap_;
    }
    const map<string, vector<QuantLib::ext::shared_ptr<RateHelper>>>& discountRateHelpersMap() const {
        return discountRateHelpersMap_;
    }
    const map<string, vector<QuantLib::ext::shared_ptr<RateHelper>>>& indexCurveRateHelpersMap() const {
        return indexCurveRateHelpersMap_;
    }
    const map<string, vector<QuantLib::ext::shared_ptr<QuantExt::DefaultProbabilityHelper>>>& defaultRateHelpersMap() const {
        return defaultRateHelpersMap_;
    }
    const map<string, vector<Handle<Quote>>>& discountRateHelperValuesMap() const {
        return discountRateHelperValuesMap_;
    }
    const map<string, vector<Handle<Quote>>>& equityForecastRateHelperValuesMap() const {
        return equityForecastRateHelperValuesMap_;
    }
    const map<string, vector<Handle<Quote>>>& indexCurveRateHelperValuesMap() const {
        return indexCurveRateHelperValuesMap_;
    }
    const map<string, vector<Handle<Quote>>>& defaultRateHelperValuesMap() const { return defaultRateHelperValuesMap_; }
    const map<string, vector<Handle<Quote>>>& cdsVolRateHelperValuesMap() const { return cdsVolRateHelperValuesMap_; }
    const map<string, vector<Handle<Quote>>>& swaptionVolRateHelperValuesMap() const {
        return swaptionVolRateHelperValuesMap_;
    }
    const map<string, vector<Handle<Quote>>>& equityVolRateHelperValuesMap() const {
        return equityVolRateHelperValuesMap_;
    }
    const map<string, vector<Handle<Quote>>>& baseCorrRateHelperValuesMap() const {
        return baseCorrRateHelperValuesMap_;
    }
    const map<string, vector<Handle<Quote>>>& zeroInflationRateHelperValuesMap() const {
        return zeroInflationRateHelperValuesMap_;
    }
    const map<string, vector<Handle<Quote>>>& yoyInflationRateHelperValuesMap() const {
        return yoyInflationRateHelperValuesMap_;
    }

private:
    void createDiscountCurve(const string& ccy, const vector<string>& parInst, const vector<Period>& parTenor,
                             const vector<Real>& parRates);

    void createEquityForecastCurve(const string& name, const string& ccy, const vector<string>& parInst,
                                   const vector<Period>& parTenor, const vector<Real>& parRates);

    void createXccyDiscountCurve(const string& ccy, const string& baseCcy, const vector<string>& parInst,
                                 const vector<Period>& parTenor, const vector<Real>& parRates);

    void createIborIndex(const string& idxName, const vector<string>& parInst, const vector<Period>& parTenor,
                         const vector<Real>& parRates, bool singleCurve);

    void createDefaultCurve(const string& name, const string& ccy, const vector<string>& parInst,
                            const vector<Period>& parTenor, const vector<Real>& parRates);

    void createCdsVolCurve(const string& name, const vector<Period>& parTenor, const vector<Real>& parRates);
    void createEquityVolCurve(const string& name, const string& ccy, const vector<Period>& parTenor,
                              const vector<Real>& parRates);
    void createBaseCorrel(const string& name, const vector<Period>& tenors, const vector<string>& lossLevel,
                          const vector<Real> quotes);
    void createSwaptionVolCurve(const string& name, const vector<Period>& optionTenors,
                                const vector<Period>& swapTenors, const vector<Real>& strikeSpreads,
                                const vector<Real>& parRates);
    void createZeroInflationIndex(const string& idxName, const vector<string>& parInst, const vector<Period>& parTenor,
                                  const vector<Real>& parRates, bool singleCurve);
    void createYoYInflationIndex(const string& idxName, const vector<string>& parInst, const vector<Period>& parTenor,
                                 const vector<Real>& parRates, bool singleCurve);

    Handle<YieldTermStructure> flatRateYts(Real forward);
    Handle<BlackVolTermStructure> flatRateFxv(Volatility forward);
    Handle<QuantLib::SwaptionVolatilityStructure> flatRateSvs(Volatility forward,
                                                              VolatilityType type = ShiftedLognormal, Real shift = 0.0);
    Handle<DefaultProbabilityTermStructure> flatRateDcs(Volatility forward);
    Handle<OptionletVolatilityStructure> flatRateCvs(Volatility vol, VolatilityType type = Normal, Real shift = 0.0);

    map<string, vector<string>> discountRateHelperInstMap_, equityForecastRateHelperInstMap_,
        indexCurveRateHelperInstMap_, defaultRateHelperInstMap_, zeroInflationRateHelperInstMap_,
        yoyInflationRateHelperInstMap_;
    map<string, vector<Period>> discountRateHelperTenorsMap_, equityForecastRateHelperTenorsMap_,
        indexCurveRateHelperTenorsMap_, defaultRateHelperTenorsMap_, cdsVolRateHelperTenorsMap_,
        swaptionVolRateHelperTenorsMap_, swaptionVolRateHelperSwapTenorsMap_, equityVolRateHelperTenorsMap_,
        baseCorrRateHelperTenorsMap_, zeroInflationRateHelperTenorsMap_, yoyInflationRateHelperTenorsMap_;
    map<string, vector<string>> baseCorrLossLevelsMap_;
    map<string, vector<QuantLib::ext::shared_ptr<RateHelper>>> discountRateHelpersMap_, equityForecastRateHelpersMap_,
        indexCurveRateHelpersMap_;
    map<string, vector<QuantLib::ext::shared_ptr<QuantExt::DefaultProbabilityHelper>>> defaultRateHelpersMap_;
    map<string, vector<Handle<Quote>>> discountRateHelperValuesMap_, equityForecastRateHelperValuesMap_,
        indexCurveRateHelperValuesMap_, defaultRateHelperValuesMap_, cdsVolRateHelperValuesMap_,
        swaptionVolRateHelperValuesMap_, equityVolRateHelperValuesMap_, baseCorrRateHelperValuesMap_,
        zeroInflationRateHelperValuesMap_, yoyInflationRateHelperValuesMap_;
};

    
//! Static class to allow for easy construction of configuration objects for use within tests
class TestConfigurationObjects {
public:
    //! ScenarioSimMarketParameters instance
    static QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>
    setupSimMarketData(bool hasSwapVolCube = false, bool hasYYCapVols = false);
    //! SensitivityScenarioData instance
    static QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>
    setupSensitivityScenarioData(bool hasSwapVolCube = false, bool hasYYCapVols = false, bool parConversion = false);
    //! ScenarioSimMarketParameters instance, 2 currencies
    static QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> setupSimMarketData2();
    //! ScenarioSimMarketParameters instance, 5 currencies
    static QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> setupSimMarketData5();
    //! SensitivityScenarioData instance, 2 currencies
    static QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> setupSensitivityScenarioData2();
    //! SensitivityScenarioData instance, 5 currencies
    static QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> setupSensitivityScenarioData5();
    //! SensitivityScenarioData instance, 2 currencies, shifts more granular than base curve
    static QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> setupSensitivityScenarioData2b();
    //! Set Conventions
    static void setConventions();
    static void setConventions2();
};
} // namespace testsuite
