/*
 Copyright (C) 2024 Quaternion Risk Management Ltd.
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

#include <orea/simm/simmconcentrationisdav2_6_5.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;

using std::map;
using std::set;
using std::string;

namespace ore {
namespace analytics {

SimmConcentration_ISDA_V2_6_5::SimmConcentration_ISDA_V2_6_5(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
    : simmBucketMapper_(simmBucketMapper) {

    // Populate IR categories that are used for concentration thresholds
    irCategories_ = {{"1", {}},{"2", {"USD", "EUR", "GBP"}},{"3", {"AUD", "CAD", "CHF", "DKK", "HKD", "KRW", "NOK", "NZD", "SEK", "SGD", "TWD"}},{"4", {"JPY"}}};

    // Populate FX categories that are used for concentration thresholds
    fxCategories_ = {{"1", {"USD", "EUR", "JPY", "GBP", "AUD", "CHF", "CAD"}},{"2", {"BRL", "CNY", "HKD", "INR", "KRW", "MXN", "NOK", "NZD", "RUB", "SEK", "SGD", "TRY", "ZAR"}},{"3", {}}};

    // Initialise the data
    // clang-format off

    // Populate flat thresholds
    flatThresholds_[CrifRecord::RiskType::CreditVol] = 290;
    flatThresholds_[CrifRecord::RiskType::CreditVolNonQ] = 21;

    // Populate bucketed thresholds
    bucketedThresholds_[CrifRecord::RiskType::IRCurve] = {
        { "1", 29 },
        { "2", 340 },
        { "3", 61 },
        { "4", 150 }
    };

    bucketedThresholds_[CrifRecord::RiskType::CreditQ] = {
        { "1", 0.98 },
        { "2", 0.18 },
        { "3", 0.18 },
        { "4", 0.18 },
        { "5", 0.18 },
        { "6", 0.18 },
        { "7", 0.98 },
        { "8", 0.18 },
        { "9", 0.18 },
        { "10", 0.18 },
        { "11", 0.18 },
        { "12", 0.18 },
        { "Residual", 0.18 }
    };

    bucketedThresholds_[CrifRecord::RiskType::CreditNonQ] = {
        { "1", 3.3 },
        { "2", 0.18 },
        { "Residual", 0.18 }
    };

    bucketedThresholds_[CrifRecord::RiskType::Equity] = {
        { "1", 2.5 },
        { "2", 2.5 },
        { "3", 2.5 },
        { "4", 2.5 },
        { "5", 10 },
        { "6", 10 },
        { "7", 10 },
        { "8", 10 },
        { "9", 0.61 },
        { "10", 0.30 },
        { "11", 710 },
        { "12", 710 },
        { "Residual", 0.30 }
    };

    bucketedThresholds_[CrifRecord::RiskType::Commodity] = {
        { "1", 310 },
        { "2", 2500 },
        { "3", 1700 },
        { "4", 1700 },
        { "5", 1700 },
        { "6", 2400 },
        { "7", 2400 },
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
        { "1", 2000 },
        { "2", 630 },
        { "3", 120 }
    };

    bucketedThresholds_[CrifRecord::RiskType::IRVol] = {
        { "1", 76 },
        { "2", 4900 },
        { "3", 550 },
        { "4", 890 }
    };

    bucketedThresholds_[CrifRecord::RiskType::EquityVol] = {
        { "1", 300 },
        { "2", 300 },
        { "3", 300 },
        { "4", 300 },
        { "5", 1500 },
        { "6", 1500 },
        { "7", 1500 },
        { "8", 1500 },
        { "9", 74 },
        { "10", 280 },
        { "11", 4300 },
        { "12", 4300 },
        { "Residual", 74 }
    };

    bucketedThresholds_[CrifRecord::RiskType::CommodityVol] = {
        { "1", 450 },
        { "2", 2300 },
        { "3", 240 },
        { "4", 240 },
        { "5", 240 },
        { "6", 6400 },
        { "7", 6400 },
        { "8", 1300 },
        { "9", 1300 },
        { "10", 94 },
        { "11", 490 },
        { "12", 810 },
        { "13", 730 },
        { "14", 730 },
        { "15", 730 },
        { "16", 59 },
        { "17", 59 }
    };

    bucketedThresholds_[CrifRecord::RiskType::FXVol] = {
        { "1", 3000 },
        { "2", 1500 },
        { "3", 670 },
        { "4", 600 },
        { "5", 390 },
        { "6", 240 }
    };

    // clang-format on
}

Real SimmConcentration_ISDA_V2_6_5::threshold(const CrifRecord::RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
