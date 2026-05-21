/*
 Copyright (C) 2026 Quaternion Risk Management Ltd.
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

#include <orea/simm/simmconcentrationisdav2_8_2512_2.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;

using std::map;
using std::set;
using std::string;

namespace ore {
namespace analytics {

SimmConcentration_ISDA_V2_8_2512_2::SimmConcentration_ISDA_V2_8_2512_2(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
    : simmBucketMapper_(simmBucketMapper) {

    // Populate IR categories that are used for concentration thresholds
    irCategories_ = {{"1", {}},{"2", {"EUR", "GBP", "USD"}},{"3", {"AUD", "CAD", "CHF", "DKK", "HKD", "KRW", "NOK", "NZD", "SEK", "SGD", "TWD"}},{"4", {"JPY"}}};

    // Populate FX categories that are used for concentration thresholds
    fxCategories_ = {{"1", {"AUD", "CAD", "CHF", "EUR", "GBP", "JPY", "USD"}},{"2", {"BRL", "CNY", "HKD", "INR", "KRW", "MXN", "NOK", "NZD", "RUB", "SEK", "SGD", "TRY", "ZAR"}},{"3", {}}};

    // Initialise the data
    // clang-format off

    // Populate flat thresholds
    flatThresholds_[CrifRecord::RiskType::CreditVol] = 300;
    flatThresholds_[CrifRecord::RiskType::CreditVolNonQ] = 2.1;

    // Populate bucketed thresholds
    bucketedThresholds_[CrifRecord::RiskType::IRCurve] = {
        { "1", 71 },
        { "2", 220 },
        { "3", 110 },
        { "4", 370 }
    };

    bucketedThresholds_[CrifRecord::RiskType::CreditQ] = {
        { "1", 1.0 },
        { "2", 0.19 },
        { "3", 0.19 },
        { "4", 0.19 },
        { "5", 0.19 },
        { "6", 0.19 },
        { "7", 1.0 },
        { "8", 0.19 },
        { "9", 0.19 },
        { "10", 0.19 },
        { "11", 0.19 },
        { "12", 0.19 },
        { "Residual", 0.19 }
    };

    bucketedThresholds_[CrifRecord::RiskType::CreditNonQ] = {
        { "1", 0.19 },
        { "2", 0.19 },
        { "Residual", 0.19 }
    };

    bucketedThresholds_[CrifRecord::RiskType::Equity] = {
        { "1", 4.0 },
        { "2", 4.0 },
        { "3", 4.0 },
        { "4", 4.0 },
        { "5", 20 },
        { "6", 20 },
        { "7", 20 },
        { "8", 20 },
        { "9", 0.98 },
        { "10", 0.40 },
        { "11", 650 },
        { "12", 650 },
        { "Residual", 0.40 }
    };

    bucketedThresholds_[CrifRecord::RiskType::Commodity] = {
        { "1", 310 },
        { "2", 2500 },
        { "3", 1700 },
        { "4", 1700 },
        { "5", 1700 },
        { "6", 2300 },
        { "7", 2300 },
        { "8", 1600 },
        { "9", 1600 },
        { "10", 52 },
        { "11", 570 },
        { "12", 1600 },
        { "13", 100 },
        { "14", 100 },
        { "15", 100 },
        { "16", 52 },
        { "17", 4000 }
    };

    bucketedThresholds_[CrifRecord::RiskType::FX] = {
        { "1", 2100 },
        { "2", 710 },
        { "3", 120 }
    };

    bucketedThresholds_[CrifRecord::RiskType::IRVol] = {
        { "1", 160 },
        { "2", 3800 },
        { "3", 520 },
        { "4", 1100 }
    };

    bucketedThresholds_[CrifRecord::RiskType::EquityVol] = {
        { "1", 370 },
        { "2", 370 },
        { "3", 370 },
        { "4", 370 },
        { "5", 1100 },
        { "6", 1100 },
        { "7", 1100 },
        { "8", 1100 },
        { "9", 100 },
        { "10", 260 },
        { "11", 3800 },
        { "12", 3800 },
        { "Residual", 100 }
    };

    bucketedThresholds_[CrifRecord::RiskType::CommodityVol] = {
        { "1", 200 },
        { "2", 2200 },
        { "3", 230 },
        { "4", 230 },
        { "5", 230 },
        { "6", 3300 },
        { "7", 3300 },
        { "8", 730 },
        { "9", 730 },
        { "10", 89 },
        { "11", 450 },
        { "12", 770 },
        { "13", 630 },
        { "14", 630 },
        { "15", 630 },
        { "16", 56 },
        { "17", 56 }
    };

    bucketedThresholds_[CrifRecord::RiskType::FXVol] = {
        { "1", 2900 },
        { "2", 1500 },
        { "3", 840 },
        { "4", 760 },
        { "5", 490 },
        { "6", 310 }
    };

    // clang-format on
}

Real SimmConcentration_ISDA_V2_8_2512_2::threshold(const CrifRecord::RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
