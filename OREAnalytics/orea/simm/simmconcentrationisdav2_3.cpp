/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/simm/simmconcentrationisdav2_3.hpp>
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

SimmConcentration_ISDA_V2_3::SimmConcentration_ISDA_V2_3(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
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
    flatThresholds_[RiskType::CreditVol] = 240;
    flatThresholds_[RiskType::CreditVolNonQ] = 56;

    // Populate bucketed thresholds
    bucketedThresholds_[RiskType::IRCurve] = {
        { "1", 31 },
        { "2", 220 },
        { "3", 41 },
        { "4", 99 }
    };

    bucketedThresholds_[RiskType::CreditQ] = {
        { "1", 0.95 },
        { "2", 0.18 },
        { "3", 0.18 },
        { "4", 0.18 },
        { "5", 0.18 },
        { "6", 0.18 },
        { "7", 0.95 },
        { "8", 0.18 },
        { "9", 0.18 },
        { "10", 0.18 },
        { "11", 0.18 },
        { "12", 0.18 },
        { "Residual", 0.18 }
    };

    bucketedThresholds_[RiskType::CreditNonQ] = {
        { "1", 9.5 },
        { "2", 0.5 },
        { "Residual", 0.5 }
    };

    bucketedThresholds_[RiskType::Equity] = {
        { "1", 7.3 },
        { "2", 7.3 },
        { "3", 7.3 },
        { "4", 7.3 },
        { "5", 30 },
        { "6", 30 },
        { "7", 30 },
        { "8", 30 },
        { "9", 2.4 },
        { "10", 2.4 },
        { "11", 1400 },
        { "12", 1400 },
        { "Residual", 2.4 }
    };

    bucketedThresholds_[RiskType::Commodity] = {
        { "1", 310 },
        { "2", 1700 },
        { "3", 1300 },
        { "4", 1300 },
        { "5", 1300 },
        { "6", 2800 },
        { "7", 2800 },
        { "8", 2200 },
        { "9", 2200 },
        { "10", 52 },
        { "11", 490 },
        { "12", 1300 },
        { "13", 73 },
        { "14", 73 },
        { "15", 73 },
        { "16", 52 },
        { "17", 4000 }
    };

    bucketedThresholds_[RiskType::FX] = {
        { "1", 8900 },
        { "2", 2000 },
        { "3", 250 }
    };

    bucketedThresholds_[RiskType::IRVol] = {
        { "1", 93 },
        { "2", 2400 },
        { "3", 240 },
        { "4", 740 }
    };

    bucketedThresholds_[RiskType::EquityVol] = {
        { "1", 140 },
        { "2", 140 },
        { "3", 140 },
        { "4", 140 },
        { "5", 1600 },
        { "6", 1600 },
        { "7", 1600 },
        { "8", 1600 },
        { "9", 38 },
        { "10", 240 },
        { "11", 9800 },
        { "12", 9800 },
        { "Residual", 38 }
    };

    bucketedThresholds_[RiskType::CommodityVol] = {
        { "1", 130 },
        { "2", 1700 },
        { "3", 290 },
        { "4", 290 },
        { "5", 290 },
        { "6", 2300 },
        { "7", 2300 },
        { "8", 800 },
        { "9", 800 },
        { "10", 74 },
        { "11", 420 },
        { "12", 700 },
        { "13", 560 },
        { "14", 560 },
        { "15", 560 },
        { "16", 74 },
        { "17", 300 }
    };

    bucketedThresholds_[RiskType::FXVol] = {
        { "1", 3900 },
        { "2", 1400 },
        { "3", 640 },
        { "4", 690 },
        { "5", 440 },
        { "6", 280 }
    };

    // clang-format on
}

Real SimmConcentration_ISDA_V2_3::threshold(const RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
