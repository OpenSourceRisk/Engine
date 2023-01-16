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

#include <ored/portfolio/builders/indexcreditdefaultswapoption.hpp>
#include <qle/pricingengines/blackindexcdsoptionengine.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

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

boost::shared_ptr<QuantLib::PricingEngine>
BlackIndexCdsOptionEngineBuilder::engineImpl(const QuantLib::Currency& ccy, const std::string& creditCurveId,
                                             const std::string& volCurveId,
                                             const std::vector<std::string>& creditCurveIds) {

    std::string curve = engineParameter("FepCurve", {}, false, "Underlying");

    QuantLib::Handle<QuantLib::YieldTermStructure> yts =
        market_->discountCurve(ccy.code(), configuration(ore::data::MarketContext::pricing));
    QuantLib::Handle<QuantExt::CreditVolCurve> vol =
        market_->cdsVol(volCurveId, configuration(ore::data::MarketContext::pricing));

    if (curve == "Index") {
        QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure> dpts =
            market_->defaultCurve(creditCurveId, configuration(ore::data::MarketContext::pricing))->curve();
        QuantLib::Handle<QuantLib::Quote> recovery =
            market_->recoveryRate(creditCurveId, configuration(ore::data::MarketContext::pricing));
        return boost::make_shared<QuantExt::BlackIndexCdsOptionEngine>(dpts, recovery->value(), yts, vol);
    } else if (curve == "Underlying") {
        std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>> dpts;
        std::vector<QuantLib::Real> recovery;
        for (auto& c : creditCurveIds) {
            auto tmp = market_->defaultCurve(c, configuration(ore::data::MarketContext::pricing))->curve();
            dpts.push_back(tmp);
            recovery.push_back(market_->recoveryRate(c, configuration(ore::data::MarketContext::pricing))->value());
        }

        // Try to get the index recovery rate.
        QuantLib::Real indexRecovery = QuantLib::Null<QuantLib::Real>();
        try {
            indexRecovery =
                market_->recoveryRate(creditCurveId, configuration(ore::data::MarketContext::pricing))->value();
        } catch (const QuantLib::Error&) {
        }

        return boost::make_shared<QuantExt::BlackIndexCdsOptionEngine>(dpts, recovery, yts, vol, indexRecovery);
    } else {
        QL_FAIL("BLackIndexCdsOptionEngineBuilder: Curve Parameter value \""
                << engineParameter("Curve") << "\" not recognised, expected Underlying or Index");
    }
}

} // namespace data
} // namespace ore
