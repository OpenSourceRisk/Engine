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

#include <orea/simm/simmconcentrationisdav1_3_38.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;

using std::map;
using std::set;
using std::string;

namespace ore {
namespace analytics {

// Ease syntax
using RiskType = CrifRecord::RiskType;

SimmConcentration_ISDA_V1_3_38::SimmConcentration_ISDA_V1_3_38(
    const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
    : simmBucketMapper_(simmBucketMapper) {

    // Populate IR categories that are used for concentration thresholds
    irCategories_ = {{"1", {}},
                     {"2", {"USD", "EUR", "GBP"}},
                     {"3", {"AUD", "CAD", "CHF", "DKK", "HKD", "KRW", "NOK", "NZD", "SEK", "SGD", "TWD"}},
                     {"4", {"JPY"}}};

    // Populate FX categories that are used for concentration thresholds
    fxCategories_ = {{"1", {"USD", "EUR", "JPY", "GBP", "AUD", "CHF", "CAD"}},
                     {"2", {"BRL", "CNY", "HKD", "INR", "KRW", "MXN", "NOK", "NZD", "RUB", "SEK", "SGD", "TRY", "ZAR"}},
                     {"3", {}}};

    // Initialise the data
    // clang-format off

    // Populate flat thresholds
    flatThresholds_[RiskType::CreditVol] = 290;
    flatThresholds_[RiskType::CreditVolNonQ] = 60;

    // Populate bucketed thresholds
    bucketedThresholds_[RiskType::IRCurve] = {
        { "1", 8 },
        { "2", 230 },
        { "3", 28 },
        { "4", 82 }
    };

    bucketedThresholds_[RiskType::CreditQ] = {
        { "1", 0.95 },
        { "2", 0.29 },
        { "3", 0.29 },
        { "4", 0.29 },
        { "5", 0.29 },
        { "6", 0.29 },
        { "7", 0.95 },
        { "8", 0.29 },
        { "9", 0.29 },
        { "10", 0.29 },
        { "11", 0.29 },
        { "12", 0.29 },
        { "Residual", 0.29 }
    };

    bucketedThresholds_[RiskType::CreditNonQ] = {
        { "1", 9.5 },
        { "2", 0.5 },
        { "Residual", 0.5 }
    };

    bucketedThresholds_[RiskType::Equity] = {
        { "1", 3.3 },
        { "2", 3.3 },
        { "3", 3.3 },
        { "4", 3.3 },
        { "5", 30 },
        { "6", 30 },
        { "7", 30 },
        { "8", 30 },
        { "9", 0.6 },
        { "10", 2.6 },
        { "11", 900 },
        { "12", 900 },
        { "Residual", 0.6 }
    };

    bucketedThresholds_[RiskType::Commodity] = {
        { "1", 1400 },
        { "2", 20000 },
        { "3", 3500 },
        { "4", 3500 },
        { "5", 3500 },
        { "6", 6400 },
        { "7", 6400 },
        { "8", 2500 },
        { "9", 2500 },
        { "10", 300 },
        { "11", 2900 },
        { "12", 7600 },
        { "13", 3900 },
        { "14", 3900 },
        { "15", 3900 },
        { "16", 300 },
        { "17", 12000 }
    };

    bucketedThresholds_[RiskType::FX] = {
        { "1", 8400 },
        { "2", 1900 },
        { "3", 560 }
    };

    bucketedThresholds_[RiskType::IRVol] = {
        { "1", 110 },
        { "2", 2700 },
        { "3", 150 },
        { "4", 960 }
    };

    bucketedThresholds_[RiskType::EquityVol] = {
        { "1", 810 },
        { "2", 810 },
        { "3", 810 },
        { "4", 810 },
        { "5", 7400 },
        { "6", 7400 },
        { "7", 7400 },
        { "8", 7400 },
        { "9", 70 },
        { "10", 300 },
        { "11", 32000 },
        { "12", 32000 },
        { "Residual", 70 }
    };

    bucketedThresholds_[RiskType::CommodityVol] = {
        { "1", 250 },
        { "2", 2000 },
        { "3", 540 },
        { "4", 540 },
        { "5", 540 },
        { "6", 1800 },
        { "7", 1800 },
        { "8", 870 },
        { "9", 870 },
        { "10", 220 },
        { "11", 450 },
        { "12", 740 },
        { "13", 380 },
        { "14", 380 },
        { "15", 380 },
        { "16", 220 },
        { "17", 2400 }
    };

    bucketedThresholds_[RiskType::FXVol] = {
        { "1", 4000 },
        { "2", 1900 },
        { "3", 320 },
        { "4", 120 },
        { "5", 120 },
        { "6", 64 }
    };

    // clang-format on
}

Real SimmConcentration_ISDA_V1_3_38::threshold(const RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
