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

#include <orea/simm/simmconcentrationisdav2_1.hpp>
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

SimmConcentration_ISDA_V2_1::SimmConcentration_ISDA_V2_1(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
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
    flatThresholds_[RiskType::CreditVol] = 250;
    flatThresholds_[RiskType::CreditVolNonQ] = 54;

    // Populate bucketed thresholds
    bucketedThresholds_[RiskType::IRCurve] = {
        { "1", 12 },
        { "2", 210 },
        { "3", 27 },
        { "4", 170 }
    };

    bucketedThresholds_[RiskType::CreditQ] = {
        { "1", 1.0 },
        { "2", 0.24 },
        { "3", 0.24 },
        { "4", 0.24 },
        { "5", 0.24 },
        { "6", 0.24 },
        { "7", 1.0 },
        { "8", 0.24 },
        { "9", 0.24 },
        { "10", 0.24 },
        { "11", 0.24 },
        { "12", 0.24 },
        { "Residual", 0.24 }
    };

    bucketedThresholds_[RiskType::CreditNonQ] = {
        { "1", 9.5 },
        { "2", 0.5 },
        { "Residual", 0.5 }
    };

    bucketedThresholds_[RiskType::Equity] = {
        { "1", 8.4 },
        { "2", 8.4 },
        { "3", 8.4 },
        { "4", 8.4 },
        { "5", 26 },
        { "6", 26 },
        { "7", 26 },
        { "8", 26 },
        { "9", 1.8 },
        { "10", 1.9 },
        { "11", 540 },
        { "12", 540 },
        { "Residual", 1.8 }
    };

    bucketedThresholds_[RiskType::Commodity] = {
        { "1", 700 },
        { "2", 3600 },
        { "3", 2700 },
        { "4", 2700 },
        { "5", 2700 },
        { "6", 2600 },
        { "7", 2600 },
        { "8", 1900 },
        { "9", 1900 },
        { "10", 52 },
        { "11", 2000 },
        { "12", 3200 },
        { "13", 1100 },
        { "14", 1100 },
        { "15", 1100 },
        { "16", 52 },
        { "17", 5200 }
    };

    bucketedThresholds_[RiskType::FX] = {
        { "1", 9700 },
        { "2", 2900 },
        { "3", 450 }
    };

    bucketedThresholds_[RiskType::IRVol] = {
        { "1", 120 },
        { "2", 2200 },
        { "3", 190 },
        { "4", 770 }
    };

    bucketedThresholds_[RiskType::EquityVol] = {
        { "1", 220 },
        { "2", 220 },
        { "3", 220 },
        { "4", 220 },
        { "5", 2300 },
        { "6", 2300 },
        { "7", 2300 },
        { "8", 2300 },
        { "9", 43 },
        { "10", 250 },
        { "11", 8100 },
        { "12", 8100 },
        { "Residual", 43 }
    };

    bucketedThresholds_[RiskType::CommodityVol] = {
        { "1", 250 },
        { "2", 1800 },
        { "3", 320 },
        { "4", 320 },
        { "5", 320 },
        { "6", 2200 },
        { "7", 2200 },
        { "8", 780 },
        { "9", 780 },
        { "10", 99 },
        { "11", 420 },
        { "12", 650 },
        { "13", 570 },
        { "14", 570 },
        { "15", 570 },
        { "16", 99 },
        { "17", 330 }
    };

    bucketedThresholds_[RiskType::FXVol] = {
        { "1", 2000 },
        { "2", 1000 },
        { "3", 320 },
        { "4", 410 },
        { "5", 210 },
        { "6", 150 }
    };

    // clang-format on
}

Real SimmConcentration_ISDA_V2_1::threshold(const RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
