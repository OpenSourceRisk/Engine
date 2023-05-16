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
#include <ored/utilities/to_string.hpp>

#include <qle/pricingengines/blackindexcdsoptionengine.hpp>
#include <qle/pricingengines/numericalintegrationindexcdsoptionengine.hpp>

#include <ql/pricingengine.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

CreditPortfolioSensitivityDecomposition IndexCreditDefaultSwapOptionEngineBuilder::sensitivityDecomposition() {
    return parseCreditPortfolioSensitivityDecomposition(
        engineParameter("SensitivityDecomposition", {}, false, "Underlying"));
}

std::vector<std::string>
IndexCreditDefaultSwapOptionEngineBuilder::keyImpl(const QuantLib::Currency& ccy, const std::string& creditCurveId,
                                                   const std::string& volCurveId,
                                                   const std::vector<std::string>& creditCurveIds) {

    std::vector<std::string> res{ccy.code()};
    res.insert(res.end(), creditCurveIds.begin(), creditCurveIds.end());
    res.push_back(creditCurveId);
    res.push_back(volCurveId);
    return res;
}

namespace {
template <class ENGINE>
boost::shared_ptr<QuantLib::PricingEngine>
genericEngineImpl(const std::string& curve, const boost::shared_ptr<Market> market, const std::string& configuration,
                  const QuantLib::Currency& ccy, const std::string& creditCurveId, const std::string& volCurveId,
                  const std::vector<std::string>& creditCurveIds) {

    QuantLib::Handle<QuantLib::YieldTermStructure> yts = market->discountCurve(ccy.code(), configuration);
    QuantLib::Handle<QuantExt::CreditVolCurve> vol = market->cdsVol(volCurveId, configuration);

    if (curve == "Index") {
        auto creditCurve = market->defaultCurve(creditCurveId, configuration);
        QuantLib::Handle<QuantLib::Quote> recovery = market->recoveryRate(creditCurveId, configuration);
        return boost::make_shared<ENGINE>(creditCurve->curve(), recovery->value(), creditCurve->rateCurve(), yts, vol);
    } else if (curve == "Underlying") {
        std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>> dpts;
        std::vector<QuantLib::Real> recovery;
        Handle<QuantExt::CreditCurve> firstCreditCurve;
        for (auto& c : creditCurveIds) {
            auto tmp = market->defaultCurve(c, configuration);
            dpts.push_back(tmp->curve());
            recovery.push_back(market->recoveryRate(c, configuration)->value());
            if (firstCreditCurve.empty())
                firstCreditCurve = tmp;
        }
        QL_REQUIRE(!firstCreditCurve.empty(),
                   "IndexCdsOptionEngineBuilder::engineImpl(): no credit curve ids given, can not extract inccy "
                   "discount rate curve");
        QuantLib::Real indexRecovery = QuantLib::Null<QuantLib::Real>();
        try {
            indexRecovery = market->recoveryRate(creditCurveId, configuration)->value();
        } catch (...) {
        }
        return boost::make_shared<ENGINE>(dpts, recovery, firstCreditCurve->rateCurve(), yts, vol, indexRecovery);
    } else {
        QL_FAIL("IndexCdsOptionEngineBuilder: Curve Parameter value \""
                << curve << "\" not recognised, expected Underlying or Index");
    }
}
} // namespace

boost::shared_ptr<QuantLib::PricingEngine>
BlackIndexCdsOptionEngineBuilder::engineImpl(const QuantLib::Currency& ccy, const std::string& creditCurveId,
                                             const std::string& volCurveId,
                                             const std::vector<std::string>& creditCurveIds) {
    std::string curve = engineParameter("FepCurve", {}, false, "Underlying");
    return genericEngineImpl<QuantExt::BlackIndexCdsOptionEngine>(curve, market_,
                                                                  configuration(ore::data::MarketContext::pricing), ccy,
                                                                  creditCurveId, volCurveId, creditCurveIds);
}

boost::shared_ptr<QuantLib::PricingEngine> NumericalIntegrationIndexCdsOptionEngineBuilder::engineImpl(
    const QuantLib::Currency& ccy, const std::string& creditCurveId, const std::string& volCurveId,
    const std::vector<std::string>& creditCurveIds) {
    std::string curve = engineParameter("FepCurve", {}, false, "Underlying");
    return genericEngineImpl<QuantExt::NumericalIntegrationIndexCdsOptionEngine>(
        curve, market_, configuration(ore::data::MarketContext::pricing), ccy, creditCurveId, volCurveId,
        creditCurveIds);
}

} // namespace data
} // namespace ore
