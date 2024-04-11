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

#include <ored/portfolio/builders/formulabasedcoupon.hpp>
#include <qle/cashflows/mcgaussianformulabasedcouponpricer.hpp>

#include <ql/math/matrixutilities/pseudosqrt.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>

#include <ored/configuration/correlationcurveconfig.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/algorithm/string.hpp>

#include <qle/termstructures/flatcorrelation.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<FloatingRateCouponPricer> FormulaBasedCouponPricerBuilder::engineImpl(
    const std::string& paymentCcy,
    const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::IborCouponPricer>>& iborPricers,
    const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::CmsCouponPricer>>& cmsPricers,
    const std::map<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>& indexMaps) {

    // MC parameters
    auto samples = parseInteger(engineParameters_.at("Samples"));
    auto seed = parseInteger(engineParameters_.at("Seed"));
    auto useSobol = parseBool(engineParameters_.at("Sobol"));
    SalvagingAlgorithm::Type salvaging = parseBool(engineParameters_.at("SalvageCorrelationMatrix"))
                                             ? SalvagingAlgorithm::Spectral
                                             : SalvagingAlgorithm::None;

    // build fx vol map
    std::map<std::string, Handle<BlackVolTermStructure>> fxVols;
    for (auto const& i : indexMaps) {
        std::string indexCcy = i.second->currency().code();
        if (indexCcy != paymentCcy) {
            fxVols[indexCcy] = (market_->fxVol(paymentCcy + indexCcy, configuration(MarketContext::pricing)));
        }
    }

    // build correlation map (index-index or index-FX)
    // Index/Index correlation
    std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>> correlation;
    std::string fxSource = modelParameters_.at("FXSource");
    std::string index1, index2, indexQL1, indexQL2;

    for (auto it1 = indexMaps.begin(); it1 != std::prev(indexMaps.end(), 1); it1++) {
        index1 = it1->first;
        indexQL1 = it1->second->name();
        for (auto it2 = std::next(it1, 1); it2 != indexMaps.end(); it2++) {
            index2 = it2->first;
            indexQL2 = it2->second->name();
            QuantLib::Handle<QuantExt::CorrelationTermStructure> corrCurve(
                QuantLib::ext::make_shared<FlatCorrelation>(0, NullCalendar(), 0.0, Actual365Fixed()));
            try {
                corrCurve = market_->correlationCurve(index1, index2, configuration(MarketContext::pricing));
            } catch (...) {
                WLOG("no correlation curve found for " << index1 << ", " << index2
                                                       << " found, fall back to zero correlation.");
            }
            correlation[std::make_pair(indexQL1, indexQL2)] = corrCurve;
        }
    }

    std::string index, indexQL, indexCcy, fxIndex;
    for (auto const& it : indexMaps) {
        index = it.first;
        indexQL = it.second->name();
        std::vector<std::string> result;
        boost::split(result, index, boost::is_any_of("-"));
        indexCcy = result[0];
        if (indexCcy != paymentCcy) {
            fxIndex = "FX-" + fxSource + "-" + indexCcy + "-" + paymentCcy;
            QuantLib::Handle<QuantExt::CorrelationTermStructure> corrCurve(
                QuantLib::ext::make_shared<FlatCorrelation>(0, NullCalendar(), 0.0, Actual365Fixed()));
            try {
                corrCurve = market_->correlationCurve(index, fxIndex, configuration(MarketContext::pricing));
            } catch (...) {
                WLOG("no correlation curve found for " << index << ", " << fxIndex
                                                       << " found, fall back to zero correlation.");
            }
            correlation[std::make_pair(indexQL, "FX")] = corrCurve;
        }
    }

    auto discount = market_->discountCurve(paymentCcy, configuration(MarketContext::pricing));
    return QuantLib::ext::make_shared<QuantExt::MCGaussianFormulaBasedCouponPricer>(
        paymentCcy, iborPricers, cmsPricers, fxVols, correlation, discount, samples, seed, useSobol, salvaging);
}

} // namespace data
} // namespace ore
