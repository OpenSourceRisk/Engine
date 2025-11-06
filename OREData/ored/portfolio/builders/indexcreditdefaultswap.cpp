/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/midpointindexcdsengine.hpp>
#include <qle/utilities/creditindexconstituentcurvecalibration.hpp>

#include <ored/portfolio/builders/indexcreditdefaultswap.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

CreditPortfolioSensitivityDecomposition IndexCreditDefaultSwapEngineBuilder::sensitivityDecomposition() {
    return parseCreditPortfolioSensitivityDecomposition(
        engineParameter("SensitivityDecomposition", {}, false, "Underlying"));
}

vector<string> IndexCreditDefaultSwapEngineBuilder::keyImpl(const Currency& ccy, const string& creditCurveId,
                                                            const vector<string>& creditCurveIds,
                                                            const QuantLib::ext::optional<string>& overrideCurve,
                                                            const QuantLib::ext::optional<bool>& calibrateConstituentCurvesOverride,
                                                            const QuantLib::Date& indexStartDate,
                                                            const QuantLib::Period& indexTerm,
                                                            const QuantLib::Real& indexCoupon,
                                                            const std::vector<double>& constituentNotional, Real recoveryRate, 
                                                            const bool inCcyDiscountCurve) {
    vector<string> res{ccy.code()};
    res.insert(res.end(), creditCurveIds.begin(), creditCurveIds.end());
    res.push_back(creditCurveId);
    res.push_back(overrideCurve ? *overrideCurve : "");
    if (recoveryRate != Null<Real>())
        res.push_back(to_string(recoveryRate));
    res.push_back(inCcyDiscountCurve ? "1" : "0");
    res.push_back(
        calibrateConstituentCurvesOverride.has_value() ? (calibrateConstituentCurvesOverride.value() ? "1" : "0") : "");
    res.push_back(to_string(indexStartDate));
    res.push_back(to_string(indexTerm));
    res.push_back(to_string(indexCoupon));
    for (const auto& notional : constituentNotional) {
        res.push_back(to_string(notional));
    }
    return res;
}

QuantLib::ext::shared_ptr<PricingEngine> MidPointIndexCdsEngineBuilder::engineImpl(
    const Currency& ccy, const string& creditCurveId, const vector<string>& creditCurveIds,
    const QuantLib::ext::optional<string>& overrideCurve,
    const QuantLib::ext::optional<bool>& calibrateConstituentCurvesOverride, const QuantLib::Date& indexStartDate,
    const QuantLib::Period& indexTerm, const QuantLib::Real& indexCoupon,
    const std::vector<double>& constituentNotionals, Real recoveryRate, const bool inCcyDiscountCurve) {

    std::string curve = overrideCurve ? *overrideCurve : engineParameter("Curve", {}, false, "Underlying");
    
    if (curve == "Index") {
        auto creditCurve = indexCdsDefaultCurve(market_, creditCurveId, configuration(MarketContext::pricing));
        Handle<Quote> mktRecovery = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));
        Real recovery = recoveryRate != Null<Real>() ? recoveryRate : mktRecovery->value();
        return QuantLib::ext::make_shared<QuantExt::MidPointIndexCdsEngine>(
            creditCurve->curve(), recovery,
            market_->discountCurve(
                ccy.code(), configuration(inCcyDiscountCurve ? MarketContext::irCalibration : MarketContext::pricing)));
    } else if (curve == "Underlying") {
        std::vector<Handle<DefaultProbabilityTermStructure>> dpts;
        std::vector<Real> recovery;
        for (auto& c : creditCurveIds) {
            auto tmp = market_->defaultCurve(c, configuration(MarketContext::pricing));
            auto tmp2 = market_->recoveryRate(c, configuration(MarketContext::pricing));
            dpts.push_back(tmp->curve());
            recovery.push_back(recoveryRate != Null<Real>() ? recoveryRate : tmp2->value());
        }
        auto discountCurve = market_->discountCurve(
                ccy.code(), configuration(inCcyDiscountCurve ? MarketContext::irCalibration : MarketContext::pricing));

        bool calibrateConstituentCurves =
            calibrateConstituentCurvesOverride
                ? *calibrateConstituentCurvesOverride
                : parseBool(engineParameter("CalibrateUnderlyingCurves", {}, false, "false"));
        std::string runType = "";
        auto it = globalParameters_.find("RunType");
        if (it != globalParameters_.end()) {
            runType = it->second;
        }
        calibrateConstituentCurves = calibrateConstituentCurves && runType != "PortfolioAnalyser";
        if (calibrateConstituentCurves && !creditCurveId.empty()) {
            TLOG("IndexCreditDefaultSwap: Calibrate constituent curves to index spread");
            auto indexCreditCurve = indexCdsDefaultCurve(market_, creditCurveId, configuration(MarketContext::pricing));
            QuantLib::Handle<QuantLib::Quote> indexRecovery =
                market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));
            auto curveCalibration = ext::make_shared<QuantExt::CreditIndexConstituentCurveCalibration>(
                indexStartDate, indexTerm, indexCoupon, indexRecovery, indexCreditCurve->curve(), discountCurve);
            auto res = curveCalibration->calibratedCurves(creditCurveIds, constituentNotionals, dpts, recovery);
            TLOG("Calibration success: " << res.success);            
            if (res.success) {
                TLOG("maturity,marketNPV,impliedNPV,calibrationFactor:");
                for (Size i = 0; i < res.marketNpv.size(); ++i) {
                    TLOG(res.cdsMaturity[i] << res.marketNpv[i] << "," << res.impliedNpv[i] << ","
                                            << res.calibrationFactor[i]);
                }
                dpts = res.curves;
            } else {
                ALOG("IndexCreditDefaultSwap: Calibration of constituent curves to index spread failed, "
                     "proceeding with non-calibrated "
                     "curves. Got "
                     << res.errorMessage << " continue with non-calibrated curves.");
            }
        }
        return QuantLib::ext::make_shared<QuantExt::MidPointIndexCdsEngine>(dpts, recovery, discountCurve);
    } else {
        QL_FAIL("MidPointIndexCdsEngineBuilder: Curve Parameter value \""
                << engineParameter("Curve") << "\" not recognised, expected Underlying or Index");
    }
}

} // namespace data
} // namespace ore
