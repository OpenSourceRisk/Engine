/*
 Copyright (C) 2025 Quaternion Risk Management Ltd.
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

#include <orea/simm/simmconcentrationisdav2_7_2412.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;

using std::map;
using std::set;
using std::string;

namespace ore {
namespace analytics {

SimmConcentration_ISDA_V2_7_2412::SimmConcentration_ISDA_V2_7_2412(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
    : simmBucketMapper_(simmBucketMapper) {

    // Populate IR categories that are used for concentration thresholds
    irCategories_ = {{"1", {}},{"2", {"EUR", "GBP", "USD"}},{"3", {"AUD", "CAD", "CHF", "DKK", "HKD", "KRW", "NOK", "NZD", "SEK", "SGD", "TWD"}},{"4", {"JPY"}}};

    // Populate FX categories that are used for concentration thresholds
    fxCategories_ = {{"1", {"AUD", "CAD", "CHF", "EUR", "GBP", "JPY", "USD"}},{"2", {"BRL", "CNY", "HKD", "INR", "KRW", "MXN", "NOK", "NZD", "RUB", "SEK", "SGD", "TRY", "ZAR"}},{"3", {}}};

    // Initialise the data
    // clang-format off

    // Populate flat thresholds
    flatThresholds_[CrifRecord::RiskType::CreditVol] = 270;
    flatThresholds_[CrifRecord::RiskType::CreditVolNonQ] = 22;

    // Populate bucketed thresholds
    bucketedThresholds_[CrifRecord::RiskType::IRCurve] = {
        { "1", 51 },
        { "2", 210 },
        { "3", 100 },
        { "4", 230 }
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
        { "1", 4.2 },
        { "2", 0.19 },
        { "Residual", 0.19 }
    };

    bucketedThresholds_[CrifRecord::RiskType::Equity] = {
        { "1", 2.8 },
        { "2", 2.8 },
        { "3", 2.8 },
        { "4", 2.8 },
        { "5", 14 },
        { "6", 14 },
        { "7", 14 },
        { "8", 14 },
        { "9", 0.70 },
        { "10", 0.33 },
        { "11", 730 },
        { "12", 730 },
        { "Residual", 0.33 }
    };

    bucketedThresholds_[CrifRecord::RiskType::Commodity] = {
        { "1", 310 },
        { "2", 2500 },
        { "3", 1700 },
        { "4", 1700 },
        { "5", 1700 },
        { "6", 2300 },
        { "7", 2300 },
        { "8", 1800 },
        { "9", 1800 },
        { "10", 52 },
        { "11", 530 },
        { "12", 1600 },
        { "13", 100 },
        { "14", 100 },
        { "15", 100 },
        { "16", 52 },
        { "17", 4000 }
    };

    bucketedThresholds_[CrifRecord::RiskType::FX] = {
        { "1", 3100 },
        { "2", 950 },
        { "3", 160 }
    };

    bucketedThresholds_[CrifRecord::RiskType::IRVol] = {
        { "1", 110 },
        { "2", 4400 },
        { "3", 480 },
        { "4", 860 }
    };

    bucketedThresholds_[CrifRecord::RiskType::EquityVol] = {
        { "1", 270 },
        { "2", 270 },
        { "3", 270 },
        { "4", 270 },
        { "5", 780 },
        { "6", 780 },
        { "7", 780 },
        { "8", 780 },
        { "9", 84 },
        { "10", 290 },
        { "11", 3200 },
        { "12", 3200 },
        { "Residual", 84 }
    };

    bucketedThresholds_[CrifRecord::RiskType::CommodityVol] = {
        { "1", 480 },
        { "2", 2400 },
        { "3", 250 },
        { "4", 250 },
        { "5", 250 },
        { "6", 7000 },
        { "7", 7000 },
        { "8", 1300 },
        { "9", 1300 },
        { "10", 100 },
        { "11", 520 },
        { "12", 740 },
        { "13", 790 },
        { "14", 790 },
        { "15", 790 },
        { "16", 62 },
        { "17", 62 }
    };

    bucketedThresholds_[CrifRecord::RiskType::FXVol] = {
        { "1", 2800 },
        { "2", 1400 },
        { "3", 740 },
        { "4", 670 },
        { "5", 440 },
        { "6", 270 }
    };

    // clang-format on
}

Real SimmConcentration_ISDA_V2_7_2412::threshold(const CrifRecord::RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
