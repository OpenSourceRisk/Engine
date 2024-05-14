/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <orea/simm/simmconcentrationisdav2_3_8.hpp>
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

SimmConcentration_ISDA_V2_3_8::SimmConcentration_ISDA_V2_3_8(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
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
    flatThresholds_[RiskType::CreditVol] = 310;
    flatThresholds_[RiskType::CreditVolNonQ] = 85;

    // Populate bucketed thresholds
     bucketedThresholds_[RiskType::IRCurve] = {
        { "1", 22 },
        { "2", 240 },
        { "3", 44 },
        { "4", 120 }
    };

    bucketedThresholds_[RiskType::CreditQ] = {
        { "1", 0.49 },
        { "2", 0.22 },
        { "3", 0.22 },
        { "4", 0.22 },
        { "5", 0.22 },
        { "6", 0.22 },
        { "7", 0.49 },
        { "8", 0.22 },
        { "9", 0.22 },
        { "10", 0.22 },
        { "11", 0.22 },
        { "12", 0.22 },
        { "Residual", 0.22 }
    };

    bucketedThresholds_[RiskType::CreditNonQ] = {
        { "1", 9.5 },
        { "2", 0.5 },
        { "Residual", 0.5 }
    };

    bucketedThresholds_[RiskType::Equity] = {
        { "1", 9.0 },
        { "2", 9.0 },
        { "3", 9.0 },
        { "4", 9.0 },
        { "5", 18 },
        { "6", 18 },
        { "7", 18 },
        { "8", 18 },
        { "9", 1.2 },
        { "10", 0.9 },
        { "11", 1300 },
        { "12", 1300 },
        { "Residual", 0.9 }
    };

    bucketedThresholds_[RiskType::Commodity] = {
        { "1", 310 },
        { "2", 2100 },
        { "3", 1700 },
        { "4", 1700 },
        { "5", 1700 },
        { "6", 3200 },
        { "7", 3200 },
        { "8", 2700 },
        { "9", 2700 },
        { "10", 52 },
        { "11", 600 },
        { "12", 1600 },
        { "13", 100 },
        { "14", 100 },
        { "15", 100 },
        { "16", 52 },
        { "17", 4000 }
    };

    bucketedThresholds_[RiskType::FX] = {
        { "1", 8300 },
        { "2", 1900 },
        { "3", 240 }
    };

    bucketedThresholds_[RiskType::IRVol] = {
        { "1", 83 },
        { "2", 2600 },
        { "3", 270 },
        { "4", 980 }
    };

    bucketedThresholds_[RiskType::EquityVol] = {
        { "1", 160 },
        { "2", 160 },
        { "3", 160 },
        { "4", 160 },
        { "5", 1600 },
        { "6", 1600 },
        { "7", 1600 },
        { "8", 1600 },
        { "9", 38 },
        { "10", 260 },
        { "11", 7000 },
        { "12", 7000 },
        { "Residual", 38 }
    };

    bucketedThresholds_[RiskType::CommodityVol] = {
        { "1", 160 },
        { "2", 2600 },
        { "3", 280 },
        { "4", 280 },
        { "5", 280 },
        { "6", 3500 },
        { "7", 3500 },
        { "8", 750 },
        { "9", 750 },
        { "10", 89 },
        { "11", 340 },
        { "12", 720 },
        { "13", 500 },
        { "14", 500 },
        { "15", 500 },
        { "16", 63 },
        { "17", 63 }
    };

    bucketedThresholds_[RiskType::FXVol] = {
        { "1", 3000 },
        { "2", 1400 },
        { "3", 610 },
        { "4", 640 },
        { "5", 420 },
        { "6", 240 }
    };

    // clang-format on
}

Real SimmConcentration_ISDA_V2_3_8::threshold(const RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
