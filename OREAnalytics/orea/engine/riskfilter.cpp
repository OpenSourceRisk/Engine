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

#include <orea/engine/riskfilter.hpp>

#include <ql/errors.hpp>

namespace ore {
namespace analytics {

const std::vector<std::string> RiskFilter::riskClassLabel_ = {"(all)",  "InterestRate", "Inflation",
                                                              "Credit", "Equity",       "FX"};
const std::vector<std::string> RiskFilter::riskTypeLabel_ = {"(all)", "DeltaGamma", "Vega", "BaseCorrelation"};

RiskFilter::RiskFilter(const Size riskClassIndex, const Size riskTypeIndex)
    : riskClassIndex_(riskClassIndex), riskTypeIndex_(riskTypeIndex) {

    QL_REQUIRE(riskClassIndex_ < riskClassLabel_.size(),
               "RiskFilter: riskClassIndex " << riskClassIndex_ << " not allowed.");
    QL_REQUIRE(riskTypeIndex_ < riskTypeLabel_.size(),
               "RiskFilter: riskTypeIndex " << riskTypeIndex_ << " not allowed.");

    static const std::set<RiskFactorKey::KeyType> all = {RiskFactorKey::KeyType::DiscountCurve,
                                                         RiskFactorKey::KeyType::YieldCurve,
                                                         RiskFactorKey::KeyType::IndexCurve,
                                                         RiskFactorKey::KeyType::SwaptionVolatility,
                                                         RiskFactorKey::KeyType::OptionletVolatility,
                                                         RiskFactorKey::KeyType::FXSpot,
                                                         RiskFactorKey::KeyType::FXVolatility,
                                                         RiskFactorKey::KeyType::EquitySpot,
                                                         RiskFactorKey::KeyType::EquityVolatility,
                                                         RiskFactorKey::KeyType::DividendYield,
                                                         RiskFactorKey::KeyType::SurvivalProbability,
                                                         RiskFactorKey::KeyType::RecoveryRate,
                                                         RiskFactorKey::KeyType::CDSVolatility,
                                                         RiskFactorKey::KeyType::BaseCorrelation,
                                                         RiskFactorKey::KeyType::CPIIndex,
                                                         RiskFactorKey::KeyType::ZeroInflationCurve,
                                                         RiskFactorKey::KeyType::YoYInflationCurve,
                                                         RiskFactorKey::KeyType::YoYInflationCapFloorVolatility,
                                                         RiskFactorKey::KeyType::SecuritySpread,
                                                         RiskFactorKey::KeyType::YieldVolatility};

    std::set<RiskFactorKey::KeyType> allowed_type, allowed;

    if (riskTypeIndex_ == 0) {
        allowed_type = all;
    } else {
        switch (riskTypeIndex_) {
        case 1:
            allowed_type = {RiskFactorKey::KeyType::DiscountCurve,
                            RiskFactorKey::KeyType::YieldCurve,
                            RiskFactorKey::KeyType::IndexCurve,
                            RiskFactorKey::KeyType::FXSpot,
                            RiskFactorKey::KeyType::EquitySpot,
                            RiskFactorKey::KeyType::DividendYield,
                            RiskFactorKey::KeyType::SurvivalProbability,
                            RiskFactorKey::KeyType::RecoveryRate,
                            RiskFactorKey::KeyType::CPIIndex,
                            RiskFactorKey::KeyType::ZeroInflationCurve,
                            RiskFactorKey::KeyType::YoYInflationCurve,
                            RiskFactorKey::KeyType::SecuritySpread};
            break;
        case 2:
            allowed_type = {RiskFactorKey::KeyType::SwaptionVolatility,
                            RiskFactorKey::KeyType::OptionletVolatility,
                            RiskFactorKey::KeyType::FXVolatility,
                            RiskFactorKey::KeyType::EquityVolatility,
                            RiskFactorKey::KeyType::CDSVolatility,
                            RiskFactorKey::KeyType::YieldVolatility,
                            RiskFactorKey::KeyType::YoYInflationCapFloorVolatility};
            break;
        case 3:
            allowed_type = {RiskFactorKey::KeyType::BaseCorrelation};
            break;
        default:
            QL_FAIL("unexpected riskTypeIndex " << riskTypeIndex_);
        }
    }

    if (riskClassIndex_ == 0) {
        allowed = allowed_type;
    } else {
        std::set<RiskFactorKey::KeyType> allowed_class;
        switch (riskClassIndex_) {
        case 1:
            allowed_class = {
                RiskFactorKey::KeyType::DiscountCurve,       RiskFactorKey::KeyType::YieldCurve,
                RiskFactorKey::KeyType::IndexCurve,          RiskFactorKey::KeyType::SwaptionVolatility,
                RiskFactorKey::KeyType::OptionletVolatility, RiskFactorKey::KeyType::SecuritySpread,
                RiskFactorKey::KeyType::YieldVolatility,     RiskFactorKey::KeyType::YoYInflationCapFloorVolatility};
            break;
        case 2:
            allowed_class = {RiskFactorKey::KeyType::CPIIndex, RiskFactorKey::KeyType::ZeroInflationCurve,
                             RiskFactorKey::KeyType::YoYInflationCurve};
            break;
        case 3:
            allowed_class = {RiskFactorKey::KeyType::SurvivalProbability, RiskFactorKey::KeyType::RecoveryRate,
                             RiskFactorKey::KeyType::CDSVolatility, RiskFactorKey::KeyType::BaseCorrelation};
            break;
        case 4:
            allowed_class = {RiskFactorKey::KeyType::EquitySpot, RiskFactorKey::KeyType::EquityVolatility,
                             RiskFactorKey::KeyType::DividendYield};
            break;
        case 5:
            allowed_class = {RiskFactorKey::KeyType::FXSpot, RiskFactorKey::KeyType::FXVolatility};
            break;
        default:
            QL_FAIL("unexpected riskClassIndex " << riskClassIndex_);
        }
        std::set_intersection(allowed_type.begin(), allowed_type.end(), allowed_class.begin(), allowed_class.end(),
                              std::inserter(allowed, allowed.begin()));
    }
    // use the complement if smaller, the check is called frequently
    if (allowed.size() > all.size() / 2) {
        std::set_difference(all.begin(), all.end(), allowed.begin(), allowed.end(),
                            std::inserter(allowed_, allowed_.begin()));
        neg_ = true;
    } else {
        allowed_ = allowed;
        neg_ = false;
    }
}

bool RiskFilter::allow(const RiskFactorKey& t) const {
    bool found = std::find(allowed_.begin(), allowed_.end(), t.keytype) != allowed_.end();
    return neg_ ? !found : found;
}

} // namespace analytics
} // namespace ore
