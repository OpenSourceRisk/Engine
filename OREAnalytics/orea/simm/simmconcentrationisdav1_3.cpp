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

#include <orea/simm/simmconcentrationisdav1_3.hpp>
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

SimmConcentration_ISDA_V1_3::SimmConcentration_ISDA_V1_3(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
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
    flatThresholds_[RiskType::CreditVol] = 210;
    flatThresholds_[RiskType::CreditVolNonQ] = 49;

    // Populate bucketed thresholds
    bucketedThresholds_[RiskType::IRCurve] = {
        { "1", 7.4 },
        { "2", 250 },
        { "3", 25 },
        { "4", 170 }
    };

    bucketedThresholds_[RiskType::CreditQ] = {
        { "1", 1.00 },
        { "2", 0.36 },
        { "3", 0.36 },
        { "4", 0.36 },
        { "5", 0.36 },
        { "6", 0.36 },
        { "7", 1.00 },
        { "8", 0.36 },
        { "9", 0.36 },
        { "10", 0.36 },
        { "11", 0.36 },
        { "12", 0.36 },
        { "Residual", 0.36 }
    };

    bucketedThresholds_[RiskType::CreditNonQ] = {
        { "1", 9.5 },
        { "2", 0.5 },
        { "Residual", 0.5 }
    };

    bucketedThresholds_[RiskType::Equity] = {
        { "1", 3.1 },
        { "2", 3.1 },
        { "3", 3.1 },
        { "4", 3.1 },
        { "5", 31 },
        { "6", 31 },
        { "7", 31 },
        { "8", 31 },
        { "9", 0.7 },
        { "10", 2.1 },
        { "11", 690 },
        { "Residual", 0.7 }
    };

    bucketedThresholds_[RiskType::Commodity] = {
        { "1", 700 },
        { "2", 23000 },
        { "3", 3200 },
        { "4", 3800 },
        { "5", 1800 },
        { "6", 6500 },
        { "7", 400 },
        { "8", 45 },
        { "9", 300 },
        { "10", 1.2 },
        { "11", 1800 },
        { "12", 5600 },
        { "13", 480 },
        { "14", 750 },
        { "15", 3.5 },
        { "16", 1.2 }
    };

    bucketedThresholds_[RiskType::FX] = {
        { "1", 5200 },
        { "2", 1300 },
        { "3", 260 }
    };

    bucketedThresholds_[RiskType::IRVol] = {
        { "1", 120 },
        { "2", 3070 },
        { "3", 160 },
        { "4", 960 }
    };

    bucketedThresholds_[RiskType::EquityVol] = {
        { "1", 1100 },
        { "2", 1100 },
        { "3", 1100 },
        { "4", 1100 },
        { "5", 11000 },
        { "6", 11000 },
        { "7", 11000 },
        { "8", 11000 },
        { "9", 170 },
        { "10", 500 },
        { "11", 39000 },
        { "Residual", 170 }
    };

    bucketedThresholds_[RiskType::CommodityVol] = {
        { "1", 4.9 },
        { "2", 1900 },
        { "3", 330 },
        { "4", 590 },
        { "5", 590 },
        { "6", 560 },
        { "7", 350 },
        { "8", 120 },
        { "9", 330 },
        { "10", 110 },
        { "11", 400 },
        { "12", 420 },
        { "13", 56 },
        { "14", 66 },
        { "15", 26 },
        { "16", 27 }
    };

    bucketedThresholds_[RiskType::FXVol] = {
        { "1", 5500 },
        { "2", 3020 },
        { "3", 520 },
        { "4", 87 },
        { "5", 87 },
        { "6", 87 }
    };

    // clang-format on
}

Real SimmConcentration_ISDA_V1_3::threshold(const RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
