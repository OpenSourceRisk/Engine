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

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/builders/indexcreditdefaultswapoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/pricingengines/blackindexcdsoptionengine.hpp>
#include <qle/pricingengines/numericalintegrationindexcdsoptionengine.hpp>
#include <qle/utilities/creditindexconstituentcurvecalibration.hpp>

#include <ql/pricingengine.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

CreditPortfolioSensitivityDecomposition IndexCreditDefaultSwapOptionEngineBuilder::sensitivityDecomposition() {
    return parseCreditPortfolioSensitivityDecomposition(
        engineParameter("SensitivityDecomposition", {}, false, "Underlying"));
}

bool IndexCreditDefaultSwapOptionEngineBuilder::calibrateUnderlyingCurves() const {
    
    std::string runType = "";
    auto it = globalParameters_.find("RunType");
    if (it != globalParameters_.end()) {
        runType = it->second;
    }
    bool calibrateUnderlyingCurves = parseBool(engineParameter("CalibrateUnderlyingCurves", {}, false, "false"));
    return calibrateUnderlyingCurves && runType != "PortfolioAnalyser";
}

std::vector<std::string> IndexCreditDefaultSwapOptionEngineBuilder::keyImpl(
    const QuantLib::Currency& ccy, const std::string& creditCurveId, const std::string& volCurveId,
    const std::vector<std::string>& creditCurveIds, const std::vector<double>& constituentNotionals) {

    std::vector<std::string> res{ccy.code()};
    res.insert(res.end(), creditCurveIds.begin(), creditCurveIds.end());
    res.push_back(creditCurveId);
    res.push_back(volCurveId);
    return res;
}

namespace {
template <class ENGINE>
QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
genericEngineImpl(const std::string& curve, const QuantLib::ext::shared_ptr<Market> market,
                  const std::string& configurationInCcy, const std::string& configurationPricing,
                  const QuantLib::Currency& ccy, const std::string& creditCurveId, const std::string& volCurveId,
                  const std::vector<std::string>& creditCurveIds, const bool generateAdditionalResults,
                  const bool calibrateToIndexLevel, const std::vector<double>& constituentNotionals) {

    QuantLib::Handle<QuantLib::YieldTermStructure> ytsInCcy = market->discountCurve(ccy.code(), configurationInCcy);
    QuantLib::Handle<QuantLib::YieldTermStructure> ytsPricing = market->discountCurve(ccy.code(), configurationPricing);
    QuantLib::Handle<QuantExt::CreditVolCurve> vol = market->cdsVol(volCurveId, configurationPricing);

    if (curve == "Index") {
        auto creditCurve = market->defaultCurve(creditCurveId, configurationPricing);
        QuantLib::Handle<QuantLib::Quote> recovery = market->recoveryRate(creditCurveId, configurationPricing);
        return QuantLib::ext::make_shared<ENGINE>(creditCurve->curve(), recovery->value(), ytsInCcy, ytsPricing, vol,
                                                  generateAdditionalResults);
    } else if (curve == "Underlying") {
        QuantLib::Real indexRecovery = QuantLib::Null<QuantLib::Real>();
        try {
            indexRecovery = market->recoveryRate(creditCurveId, configurationPricing)->value();
        } catch (...) {
        }
        std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>> dpts;
        std::vector<QuantLib::Real> recovery;
        for (auto& c : creditCurveIds) {
            auto tmp = market->defaultCurve(c, configurationPricing);
            dpts.push_back(tmp->curve());
            recovery.push_back(market->recoveryRate(c, configurationPricing)->value());
        }
        
        if(calibrateToIndexLevel) {
            TLOG("IndexCreditDefaultSwapOption: Calibrate constituent curves to index spread");
            QL_REQUIRE(!creditCurveId.empty(),
                       "IndexCreditDefaultSwapOption: cannot calibrate constituent curves to index spread "
                       "if index credit curve ID is not set");
            auto indexCreditCurve = indexCdsDefaultCurve(market, creditCurveId, configurationPricing);
            QL_REQUIRE(indexCreditCurve->refData().startDate != Null<Date>(),
                       "IndexCreditDefaultSwapOption: cannot calibrate constituent curves to index spread "
                       "if index credit curve start date is not set, please check index credit curve configuration");
            QL_REQUIRE(indexCreditCurve->refData().indexTerm != 0 * Days,
                       "IndexCreditDefaultSwapOption: cannot calibrate constituent curves to index spread "
                       "if index credit curve index term is not set, please check index credit curve configuration");
            QL_REQUIRE(indexCreditCurve->refData().runningSpread != Null<Real>(),
                       "IndexCreditDefaultSwapOption: cannot calibrate constituent curves to index spread "
                       "if index credit curve running spread is not set, please check index credit curve configuration");
            auto curveCalibration = ext::make_shared<QuantExt::CreditIndexConstituentCurveCalibration>(indexCreditCurve);
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
                ALOG("IndexCreditDefaultSwapOption: Calibration of constituent curves to index spread failed, "
                     "proceeding with non-calibrated "
                     "curves. Got "
                     << res.errorMessage << " continue with non-calibrated curves.");
            }
        }
        return QuantLib::ext::make_shared<ENGINE>(dpts, recovery, ytsInCcy, ytsPricing, vol, indexRecovery,
                                                  generateAdditionalResults);
    } else {
        QL_FAIL("IndexCdsOptionEngineBuilder: Curve Parameter value \""
                << curve << "\" not recognised, expected Underlying or Index");
    }
}
} // namespace

QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
BlackIndexCdsOptionEngineBuilder::engineImpl(const QuantLib::Currency& ccy, const std::string& creditCurveId,
                                             const std::string& volCurveId,
                                             const std::vector<std::string>& creditCurveIds,
                                             const std::vector<double>& constituentNotionals) {

    bool generateAdditionalResults = false;
    if (auto genAddParam = globalParameters_.find("GenerateAdditionalResults");
        genAddParam != globalParameters_.end()) {
        generateAdditionalResults = parseBool(genAddParam->second);
    }
    std::string curve = engineParameter("FepCurve", {}, false, "Underlying");

    return genericEngineImpl<QuantExt::BlackIndexCdsOptionEngine>(
        curve, market_, configuration(ore::data::MarketContext::irCalibration),
        configuration(ore::data::MarketContext::pricing), ccy, creditCurveId, volCurveId, creditCurveIds,
        generateAdditionalResults, calibrateUnderlyingCurves(), constituentNotionals);
}

QuantLib::ext::shared_ptr<QuantLib::PricingEngine> NumericalIntegrationIndexCdsOptionEngineBuilder::engineImpl(
    const QuantLib::Currency& ccy, const std::string& creditCurveId, const std::string& volCurveId,
    const std::vector<std::string>& creditCurveIds, const std::vector<double>& constituentNotionals) {

    bool generateAdditionalResults = false;
    if (auto genAddParam = globalParameters_.find("GenerateAdditionalResults");
        genAddParam != globalParameters_.end()) {
        generateAdditionalResults = parseBool(genAddParam->second);
    }

    std::string curve = engineParameter("FepCurve", {}, false, "Underlying");

    return genericEngineImpl<QuantExt::NumericalIntegrationIndexCdsOptionEngine>(
        curve, market_, configuration(ore::data::MarketContext::irCalibration),
        configuration(ore::data::MarketContext::pricing), ccy, creditCurveId, volCurveId, creditCurveIds,
        generateAdditionalResults, calibrateUnderlyingCurves(), constituentNotionals);
}

} // namespace data
} // namespace ore
