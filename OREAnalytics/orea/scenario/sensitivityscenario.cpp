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

/*! \file scenario/sensitivityscenario.cpp
    \brief A class to hold Scenario parameters for scenarioSimMarket
    \ingroup scenario
*/

#include <ored/utilities/log.hpp>
#include <orea/analytics/sensitivityscenario.hpp>
// #include <qlw/analytics/scenarioconfig.hpp>
// #include <qlw/data/riskmarket.hpp>
// #include <qlw/wrapexception.hpp>
#include <ql/utilities/dataparsers.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

SensitivityScenario::SensitivityScenario(Real absoluteYieldShift, bool shiftIndexCurves, Real relativeFxRateShift,
                                         Real absoluteVolShift)
    : absoluteYieldShift_(absoluteYieldShift), shiftIndexCurves_(shiftIndexCurves),
      relativeFxRateShift_(relativeFxRateShift), absoluteVolShift_(absoluteVolShift) {
        ScenarioConfiguration& sc = ScenarioConfiguration::instance();
        Risk::Market& market = Risk::Market::instance();
        QLW_REQUIRE(market.isInitialised(), "market is not initialised");

        // Discount curves
        for (Size i = 0; i < sc.currencies().size(); i++) {
            std::string ccy = sc.currencies()[i];
            std::string key = ccy;
            std::string baseLabel = ccy + "-DISCOUNT";
            Handle<YieldTermStructure> ts = market.discountYts(ccy);
            generateYieldCurveScenarios(true, ts, key, ccy, baseLabel);
        } 

        // Index curves
        if (shiftIndexCurves_) {
            std::map<std::string,std::string>::iterator it;
            for (it = sc.iborIndexSpecs().begin(); it != sc.iborIndexSpecs().end(); it++) {
                std::string key = it->first;
                std::string ccy = market.iborIndex(key)->currency().code();
                std::string baseLabel = key;
                Handle<YieldTermStructure> ts = market.indexYts(key);
                generateYieldCurveScenarios(false, ts, key, ccy, baseLabel);
            } 
        }

        // FX rates
        Date asof = Settings::instance().evaluationDate();
        std::map<std::string,std::string>::iterator it;
        for (it = sc.fxSpecs().begin(); it != sc.fxSpecs().end(); it++) {
            std::string ccypair = it->first;
            PTR<Scenario> scenario(new Scenario(asof, "FX-"+ccypair));
            Real rate = market.fx(ccypair)->value();
            Real newRate = rate * (1.0 + relativeFxRateShift_);
            scenario->addFxRate(newRate, ccypair);
            scenarioData_.push_back(scenario);
            scenarioLabel_.push_back("FX-"+ccypair);
            scenarioCcy_.push_back(ccypair);
            scenarioKey_.push_back(ccypair);
            scenarioType_.push_back(SensitivityScenario::FX);
        }

        // Swaption Vols
        for (it = sc.swaptionVolSpecs().begin(); it != sc.swaptionVolSpecs().end(); it++) {
            std::string ccy = it->first;
            PTR<Scenario> scenario(new Scenario(asof, "SWAPTIONVOL-"+ccy));

            std::vector<std::vector<Real> > 
                shiftedVolMatrix(sc.swaptionExpiries().size(), 
                                 std::vector<Real>(sc.underlyingSwapTerms().size()));
            // Parallel shift: same shift applied to all grid points
            for (Size i = 0; i < sc.swaptionExpiries().size(); i++) {
                for (Size j = 0; j < sc.underlyingSwapTerms().size(); j++) {
                    Real vol = market.svts(ccy)->volatility(sc.swaptionExpiries()[i],
                                                            sc.underlyingSwapTerms()[j],
                                                            0.0, // strike 0 is ok as we are reading from a matrix
                                                            true); // extrapolate if necessary
                    shiftedVolMatrix[i][j] = vol + absoluteVolShift_;
                }
            }

            //scenario->addSwaptionVol(absoluteVolShift, ccy);
            scenario->addSwaptionVol(shiftedVolMatrix, ccy);
            scenarioData_.push_back(scenario);
            scenarioLabel_.push_back("SWAPTIONVOL-"+ccy);
            scenarioCcy_.push_back(ccy);
            scenarioKey_.push_back("SWAPTIONVOL-"+ccy);
            scenarioType_.push_back(SensitivityScenario::SWAPTIONVOL);
        }

        // Cap/Floor Vols
        for (it = sc.capVolSpecs().begin(); it != sc.capVolSpecs().end(); it++) {
            std::string ccy = it->first;
            PTR<Scenario> scenario(new Scenario(asof, "CAPVOL-"+ccy));

            std::vector<std::vector<Real> > 
                shiftedVolMatrix(sc.capTerms().size(), 
                                 std::vector<Real>(sc.capStrikes().size()));
            // Parallel shift: same shift applied to all grid points
            for (Size i = 0; i < sc.capTerms().size(); i++) {
                for (Size j = 0; j < sc.capStrikes().size(); j++) {
                    Real vol = market.cvts(ccy)->volatility(sc.capTerms()[i],
                                                            sc.capStrikes()[j],
                                                            true); // extrapolate if necessary
                    shiftedVolMatrix[i][j] = vol + absoluteVolShift_;
                }
            }

            //scenario->addCapVol(absoluteVolShift, ccy); 
            scenario->addCapVol(shiftedVolMatrix, ccy); 
            scenarioData_.push_back(scenario);
            scenarioLabel_.push_back("CAPVOL-"+ccy);
            scenarioCcy_.push_back(ccy);
            scenarioKey_.push_back("CAPVOL-"+ccy);
            scenarioType_.push_back(SensitivityScenario::CAPVOL);
        }

        // FX Vols
        // for (it = sc.fxVolSpecs().begin(); it != sc.fxVolSpecs().end(); it++) {
        //     std::string ccypair = it->first;
        //     PTR<Scenario> scenario(new Scenario(asof, "FXVOL-"+ccypair));
        //     scenario->addFxVol(absoluteVolShift_, ccypair);
        //     scenarioData_.push_back(scenario);
        //     scenarioLabel_.push_back("FXVOL-"+ccypair);
        //     scenarioCcy_.push_back(ccypair);
        //     scenarioKey_.push_back("FXVOL-"+ccypair);
        //     scenarioType_.push_back(SensitivityScenario::FXVOL);
        // }
    }

	void SensitivityScenario::generateYieldCurveScenarios(bool discountCurves,
                                                          Handle<YieldTermStructure> ts, 
                                                          std::string key,
                                                          std::string ccy,
                                                          std::string baseLabel) {
        ScenarioConfiguration& sc = ScenarioConfiguration::instance();
        Date asof = Settings::instance().evaluationDate();
        DayCounter zerodc = Actual365Fixed(); // FIXME: Get this from yield curve specs?

        // original curve
        std::vector<Real> discounts(sc.yieldCurveTenors().size());
        std::vector<Real> zeros(sc.yieldCurveTenors().size());
        std::vector<Real> times(sc.yieldCurveTenors().size());            
        std::vector<Date> dates(sc.yieldCurveTenors().size());
        for (Size j = 0; j < sc.yieldCurveTenors().size(); j++) {
            Date d = asof + sc.yieldCurveTenors()[j];
            dates[j] = d;
            discounts[j] = ts->discount(d);
            zeros[j] = ts->zeroRate(d, zerodc, Continuous);
            times[j] = ts->dayCounter().yearFraction(asof, d);
        }
        
        /****************************************************************************
         * Apply triangular shaped zero rate shifts to the underlying curve
         * where the triangle reaches from the previous to the next shift tenor point
         * with peak at the current shift tenor point. At the initial and final shift
         * tenor the shape is replaced such that the full shift is applied to all curve 
         * grid points to the left of the first shift point and to the right of the last
         * shift point, respectively. The procedure guarantees that no sensitivity to 
         * original curve points is "missed" when the shift curve is less granular, e.g.
         * original curve |...|...|...|...|...|...|...|...|...|
         * shift curve    ......|...........|...........|......
         */
        // FIXME: Check case where the shift curve is more granular than the original

        std::vector<std::string> tenors = sc.yieldCurveShiftTenorStrings();
        QLW_REQUIRE(tenors.size() > 0, "shift tenors not specified");
        for (Size j = 0; j < tenors.size(); j++) {
            
            PTR<Scenario> scenario(new Scenario(asof, baseLabel + "-" + sc.yieldCurveShiftTenorStrings()[j]));
            std::vector<Real> shiftedZeros = zeros;
            Date currentShiftDate = asof + PeriodParser::parse(tenors[j]);
            Real t1 = ts->dayCounter().yearFraction(asof, currentShiftDate);
            
            if (tenors.size() == 1) { // single shift tenor means parallel shift
                for(Size k = 0; k < times.size(); k++)
                    shiftedZeros[k] += absoluteYieldShift_; 
            }
            else if (j == 0) { // first shift tenor, flat extrapolation to the left
                Date nextShiftDate = asof + PeriodParser::parse(tenors[j+1]);
                Real t2 = ts->dayCounter().yearFraction(asof, nextShiftDate);
                for(Size k = 0; k < times.size(); k++) {
                    if (times[k] <= t1) // full shift
                        shiftedZeros[k] += absoluteYieldShift_; 
                    else if (times[k] <= t2) // linear interpolation in t1 < times[k] < t2
                        shiftedZeros[k] += absoluteYieldShift_ * (t2 - times[k]) / (t2 - t1);
                    else break;
                }
            }
            else if (j == tenors.size() - 1) { // last shift tenor, flat extrapolation to the right
                Date previousShiftDate = asof + PeriodParser::parse(tenors[j-1]);
                Real t0 = ts->dayCounter().yearFraction(asof, previousShiftDate);
                for(Size k = 0; k < times.size(); k++) {
                    if (times[k] >= t0 && times[k] <= t1) // linear interpolation in t0 < times[k] < t1
                        shiftedZeros[k] += absoluteYieldShift_ * (times[k] - t0) / (t1 - t0); 
                    else if (times[k] > t1) // full shift
                        shiftedZeros[k] += absoluteYieldShift_;
                }
            }
            else { // intermediate shift tenor
                Date previousShiftDate = asof + PeriodParser::parse(tenors[j-1]);
                Real t0 = ts->dayCounter().yearFraction(asof, previousShiftDate);
                Date nextShiftDate = asof + PeriodParser::parse(tenors[j+1]);
                Real t2 = ts->dayCounter().yearFraction(asof, nextShiftDate);
                for (Size k = 0; k < times.size(); k++) {
                    if (times[k] >= t0 && times[k] <= t1) // linear interpolation in t0 < times[k] < t1
                        shiftedZeros[k] += absoluteYieldShift_ * (times[k] - t0) / (t1 - t0); 
                    else if (times[k] > t1 && times[k] <= t2) // linear interpolation in t1 < times[k] < t2
                        shiftedZeros[k] += absoluteYieldShift_ * (t2 - times[k]) / (t2 - t1); 
                }
            }
            
            // convert into a shifted discount curve
            std::vector<Real> shiftedDiscounts(shiftedZeros.size());
            for (Size k = 0; k < shiftedZeros.size(); k++) {
                Real time = zerodc.yearFraction(asof, dates[k]);
                shiftedDiscounts[k] = exp(-shiftedZeros[k] * time);
            }

            // store as scenario
            shiftedDiscounts.insert(shiftedDiscounts.begin(), 1.0);
            if (discountCurves) {
                scenario->addDiscountCurve(shiftedDiscounts, key);
                scenarioType_.push_back(SensitivityScenario::DISCOUNT);
            }
            else {
                scenario->addIndexCurve(shiftedDiscounts, key);
                scenarioType_.push_back(SensitivityScenario::INDEX);
            }
            scenarioData_.push_back(scenario);
            scenarioLabel_.push_back(baseLabel + "-" + sc.yieldCurveShiftTenorStrings()[j]);
            scenarioCcy_.push_back(ccy);
            scenarioKey_.push_back(key);
            LOG("Sensitivity scenario " << scenarioLabel_.back() << " created");
            
        } // end of shift curve tenors

        // Add the base scenario as last case for each currency
        // Assuming that scenarios are applied in the order they are created here, this will
        // restore the original state before moving on to the next currency.
        PTR<Scenario> scenario(new Scenario(asof, baseLabel + "-BASE"));
        discounts.insert(discounts.begin(), 1.0);
        if (discountCurves) {
            scenario->addDiscountCurve(discounts, key);
            scenarioType_.push_back(SensitivityScenario::DISCOUNT);
        }
        else {
            scenario->addIndexCurve(discounts, key);
            scenarioType_.push_back(SensitivityScenario::INDEX);
        }
        scenarioData_.push_back(scenario);
        scenarioLabel_.push_back(baseLabel + "-BASE");
        scenarioCcy_.push_back(ccy);
        scenarioKey_.push_back(key);
        LOG("Sensitivity scenario " << scenarioLabel_.back() << " created");
    }

    PTR<Scenario> SensitivityScenario::getScenario(Size i) {
        QLW_REQUIRE(i < scenarioData_.size(), "index out of range");
        return scenarioData_[i];
    }

    std::string SensitivityScenario::getScenarioLabel(Size i) {
        QLW_REQUIRE(i < scenarioLabel_.size(), "index out of range");
        return scenarioLabel_[i];
    }

    std::string SensitivityScenario::getScenarioCurrency(Size i) {
        QLW_REQUIRE(i < scenarioCcy_.size(), "index out of range");
        return scenarioCcy_[i];
    }
        
    std::string SensitivityScenario::getScenarioKey(Size i) {
        QLW_REQUIRE(i < scenarioKey_.size(), "index out of range");
        return scenarioKey_[i];
    }

    SensitivityScenario::Type SensitivityScenario::getScenarioType(Size i) {
        QLW_REQUIRE(i < scenarioType_.size(), "index out of range");
        return scenarioType_[i];
    }
}
