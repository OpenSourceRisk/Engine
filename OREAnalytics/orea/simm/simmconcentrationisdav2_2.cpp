/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <orea/simm/simmconcentrationisdav2_2.hpp>
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

SimmConcentration_ISDA_V2_2::SimmConcentration_ISDA_V2_2(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
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
    flatThresholds_[RiskType::CreditVol] = 280;
    flatThresholds_[RiskType::CreditVolNonQ] = 59;

    // Populate bucketed thresholds
    bucketedThresholds_[RiskType::IRCurve] = {
        { "1", 6.9 },
        { "2", 230 },
        { "3", 30 },
        { "4", 150 }
    };

    bucketedThresholds_[RiskType::CreditQ] = {
        { "1", 0.94 },
        { "2", 0.18 },
        { "3", 0.18 },
        { "4", 0.18 },
        { "5", 0.18 },
        { "6", 0.18 },
        { "7", 0.94 },
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
        { "1", 11 },
        { "2", 11 },
        { "3", 11 },
        { "4", 11 },
        { "5", 37 },
        { "6", 37 },
        { "7", 37 },
        { "8", 37 },
        { "9", 5.1 },
        { "10", 2.4 },
        { "11", 1800 },
        { "12", 1800 },
        { "Residual", 2.4 }
    };

    bucketedThresholds_[RiskType::Commodity] = {
        { "1", 250 },
        { "2", 2300 },
        { "3", 1600 },
        { "4", 1600 },
        { "5", 1600 },
        { "6", 2200 },
        { "7", 2200 },
        { "8", 2200 },
        { "9", 2200 },
        { "10", 51 },
        { "11", 370 },
        { "12", 870 },
        { "13", 27 },
        { "14", 27 },
        { "15", 27 },
        { "16", 27 },
        { "17", 4100 }
    };

    bucketedThresholds_[RiskType::FX] = {
        { "1", 9100 },
        { "2", 1600 },
        { "3", 290 }
    };

    bucketedThresholds_[RiskType::IRVol] = {
        { "1", 170 },
        { "2", 2300 },
        { "3", 190 },
        { "4", 770 }
    };

    bucketedThresholds_[RiskType::EquityVol] = {
        { "1", 150 },
        { "2", 150 },
        { "3", 150 },
        { "4", 150 },
        { "5", 1100 },
        { "6", 1100 },
        { "7", 1100 },
        { "8", 1100 },
        { "9", 40 },
        { "10", 200 },
        { "11", 9200 },
        { "12", 9200 },
        { "Residual", 40 }
    };

    bucketedThresholds_[RiskType::CommodityVol] = {
        { "1", 290 },
        { "2", 1500 },
        { "3", 230 },
        { "4", 230 },
        { "5", 230 },
        { "6", 2600 },
        { "7", 2600 },
        { "8", 900 },
        { "9", 900 },
        { "10", 100 },
        { "11", 390 },
        { "12", 600 },
        { "13", 680 },
        { "14", 680 },
        { "15", 680 },
        { "16", 100 },
        { "17", 270 }
    };

    bucketedThresholds_[RiskType::FXVol] = {
        { "1", 3700 },
        { "2", 1900 },
        { "3", 640 },
        { "4", 570 },
        { "5", 390 },
        { "6", 220 }
    };

    // clang-format on
}

Real SimmConcentration_ISDA_V2_2::threshold(const RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
