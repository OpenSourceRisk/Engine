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

#include <ored/portfolio/builders/indexcreditdefaultswap.hpp>
#include <qle/pricingengines/midpointindexcdsengine.hpp>

#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/log.hpp>
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
                                                            const boost::optional<string>& overrideCurve,
                                                            Real recoveryRate) {
    vector<string> res{ccy.code()};
    res.insert(res.end(), creditCurveIds.begin(), creditCurveIds.end());
    res.push_back(creditCurveId);
    res.push_back(overrideCurve ? *overrideCurve : "");
    if (recoveryRate != Null<Real>())
        res.push_back(to_string(recoveryRate));
    return res;
}

boost::shared_ptr<PricingEngine>
MidPointIndexCdsEngineBuilder::engineImpl(const Currency& ccy, const string& creditCurveId,
                                          const vector<string>& creditCurveIds,
                                          const boost::optional<string>& overrideCurve,
                                          Real recoveryRate) {

    Handle<YieldTermStructure> yts = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
    std::string curve = overrideCurve ? *overrideCurve : engineParameter("Curve", {}, false, "Underlying");

    if (curve == "Index") {
        Handle<DefaultProbabilityTermStructure> dpts =
            indexCdsDefaultCurve(market_, creditCurveId, configuration(MarketContext::pricing))->curve();
        Handle<Quote> mktRecovery = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));
        Real recovery = recoveryRate != Null<Real>() ? recoveryRate : mktRecovery->value();
        return boost::make_shared<QuantExt::MidPointIndexCdsEngine>(dpts, recovery, yts);
    } else if (curve == "Underlying") {
        std::vector<Handle<DefaultProbabilityTermStructure>> dpts;
        std::vector<Real> recovery;
        for (auto& c : creditCurveIds) {
            auto tmp = market_->defaultCurve(c, configuration(MarketContext::pricing))->curve();
            auto tmp2 = market_->recoveryRate(c, configuration(MarketContext::pricing));
            dpts.push_back(tmp);
            recovery.push_back(recoveryRate != Null<Real>() ? recoveryRate : tmp2->value());
        }
        return boost::make_shared<QuantExt::MidPointIndexCdsEngine>(dpts, recovery, yts);
    } else {
        QL_FAIL("MidPointIndexCdsEngineBuilder: Curve Parameter value \""
                << engineParameter("Curve") << "\" not recognised, expected Underlying or Index");
    }
}

} // namespace data
} // namespace ore
