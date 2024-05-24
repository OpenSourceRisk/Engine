/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <orea/simm/simmconcentrationisdav2_5a.hpp>
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

SimmConcentration_ISDA_V2_5A::SimmConcentration_ISDA_V2_5A(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
    : simmBucketMapper_(simmBucketMapper) {

    // Populate IR categories that are used for concentration thresholds
    irCategories_ = {{"1", {}},{"2", {"USD", "EUR", "GBP"}},{"3", {"AUD", "CAD", "CHF", "DKK", "HKD", "KRW", "NOK", "NZD", "SEK", "SGD", "TWD"}},{"4", {"JPY"}}};

    // Populate FX categories that are used for concentration thresholds
    fxCategories_ = {{"1", {"USD", "EUR", "JPY", "GBP", "AUD", "CHF", "CAD"}},{"2", {"BRL", "CNY", "HKD", "INR", "KRW", "MXN", "NOK", "NZD", "RUB", "SEK", "SGD", "TRY", "ZAR"}},{"3", {}}};

    // Initialise the data
    // clang-format off

    // Populate flat thresholds
    flatThresholds_[RiskType::CreditVol] = 260;
    flatThresholds_[RiskType::CreditVolNonQ] = 145;

    // Populate bucketed thresholds
    bucketedThresholds_[RiskType::IRCurve] = {
        { "1", 33 },
        { "2", 230 },
        { "3", 44 },
        { "4", 70 }
    };

    bucketedThresholds_[RiskType::CreditQ] = {
        { "1", 0.91 },
        { "2", 0.19 },
        { "3", 0.19 },
        { "4", 0.19 },
        { "5", 0.19 },
        { "6", 0.19 },
        { "7", 0.91 },
        { "8", 0.19 },
        { "9", 0.19 },
        { "10", 0.19 },
        { "11", 0.19 },
        { "12", 0.19 },
        { "Residual", 0.19 }
    };

    bucketedThresholds_[RiskType::CreditNonQ] = {
        { "1", 9.5 },
        { "2", 0.5 },
        { "Residual", 0.5 }
    };

    bucketedThresholds_[RiskType::Equity] = {
        { "1", 10 },
        { "2", 10 },
        { "3", 10 },
        { "4", 10 },
        { "5", 21 },
        { "6", 21 },
        { "7", 21 },
        { "8", 21 },
        { "9", 1.4 },
        { "10", 0.6 },
        { "11", 2100 },
        { "12", 2100 },
        { "Residual", 0.6 }
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
        { "11", 530 },
        { "12", 1600 },
        { "13", 100 },
        { "14", 100 },
        { "15", 100 },
        { "16", 52 },
        { "17", 4000 }
    };

    bucketedThresholds_[RiskType::FX] = {
        { "1", 5100 },
        { "2", 1200 },
        { "3", 190 }
    };

    bucketedThresholds_[RiskType::IRVol] = {
        { "1", 120 },
        { "2", 3300 },
        { "3", 470 },
        { "4", 570 }
    };

    bucketedThresholds_[RiskType::EquityVol] = {
        { "1", 210 },
        { "2", 210 },
        { "3", 210 },
        { "4", 210 },
        { "5", 1300 },
        { "6", 1300 },
        { "7", 1300 },
        { "8", 1300 },
        { "9", 40 },
        { "10", 200 },
        { "11", 5900 },
        { "12", 5900 },
        { "Residual", 40 }
    };

    bucketedThresholds_[RiskType::CommodityVol] = {
        { "1", 210 },
        { "2", 2700 },
        { "3", 290 },
        { "4", 290 },
        { "5", 290 },
        { "6", 5000 },
        { "7", 5000 },
        { "8", 920 },
        { "9", 920 },
        { "10", 100 },
        { "11", 350 },
        { "12", 720 },
        { "13", 500 },
        { "14", 500 },
        { "15", 500 },
        { "16", 65 },
        { "17", 65 }
    };

    bucketedThresholds_[RiskType::FXVol] = {
        { "1", 2800 },
        { "2", 1300 },
        { "3", 550 },
        { "4", 490 },
        { "5", 310 },
        { "6", 200 }
    };

    // clang-format on
}

Real SimmConcentration_ISDA_V2_5A::threshold(const RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
