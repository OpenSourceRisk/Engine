/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.
*/
#include <orea/simm/simmconcentrationisdav2_6.hpp>
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

SimmConcentration_ISDA_V2_6::SimmConcentration_ISDA_V2_6(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
    : simmBucketMapper_(simmBucketMapper) {

    // Populate IR categories that are used for concentration thresholds
    irCategories_ = {{"1", {}},{"2", {"USD", "EUR", "GBP"}},{"3", {"AUD", "CAD", "CHF", "DKK", "HKD", "KRW", "NOK", "NZD", "SEK", "SGD", "TWD"}},{"4", {"JPY"}}};

    // Populate FX categories that are used for concentration thresholds
    fxCategories_ = {{"1", {"USD", "EUR", "JPY", "GBP", "AUD", "CHF", "CAD"}},{"2", {"BRL", "CNY", "HKD", "INR", "KRW", "MXN", "NOK", "NZD", "RUB", "SEK", "SGD", "TRY", "ZAR"}},{"3", {}}};

    // Initialise the data
    // clang-format off

    // Populate flat thresholds
    flatThresholds_[RiskType::CreditVol] = 360;
    flatThresholds_[RiskType::CreditVolNonQ] = 70;

    // Populate bucketed thresholds
    bucketedThresholds_[RiskType::IRCurve] = {
        { "1", 30 },
        { "2", 330 },
        { "3", 130 },
        { "4", 61 }
    };

    bucketedThresholds_[RiskType::CreditQ] = {
        { "1", 1 },
        { "2", 0.17 },
        { "3", 0.17 },
        { "4", 0.17 },
        { "5", 0.17 },
        { "6", 0.17 },
        { "7", 1 },
        { "8", 0.17 },
        { "9", 0.17 },
        { "10", 0.17 },
        { "11", 0.17 },
        { "12", 0.17 },
        { "Residual", 0.17 }
    };

    bucketedThresholds_[RiskType::CreditNonQ] = {
        { "1", 9.5 },
        { "2", 0.5 },
        { "Residual", 0.5 }
    };

    bucketedThresholds_[RiskType::Equity] = {
        { "1", 3 },
        { "2", 3 },
        { "3", 3 },
        { "4", 3 },
        { "5", 12 },
        { "6", 12 },
        { "7", 12 },
        { "8", 12 },
        { "9", 0.64 },
        { "10", 0.37 },
        { "11", 810 },
        { "12", 810 },
        { "Residual", 0.37 }
    };

    bucketedThresholds_[RiskType::Commodity] = {
        { "1", 310 },
        { "2", 2100 },
        { "3", 1700 },
        { "4", 1700 },
        { "5", 1700 },
        { "6", 2800 },
        { "7", 2800 },
        { "8", 2700 },
        { "9", 2700 },
        { "10", 52 },
        { "11", 530 },
        { "12", 1300 },
        { "13", 100 },
        { "14", 100 },
        { "15", 100 },
        { "16", 52 },
        { "17", 4000 }
    };

    bucketedThresholds_[RiskType::FX] = {
        { "1", 3300 },
        { "2", 880 },
        { "3", 170 }
    };

    bucketedThresholds_[RiskType::IRVol] = {
        { "1", 74 },
        { "2", 4900 },
        { "3", 520 },
        { "4", 970 }
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
        { "9", 39 },
        { "10", 190 },
        { "11", 6400 },
        { "12", 6400 },
        { "Residual", 39 }
    };

    bucketedThresholds_[RiskType::CommodityVol] = {
        { "1", 390 },
        { "2", 2900 },
        { "3", 310 },
        { "4", 310 },
        { "5", 310 },
        { "6", 6300 },
        { "7", 6300 },
        { "8", 1200 },
        { "9", 1200 },
        { "10", 120 },
        { "11", 390 },
        { "12", 1300 },
        { "13", 590 },
        { "14", 590 },
        { "15", 590 },
        { "16", 69 },
        { "17", 69 }
    };

    bucketedThresholds_[RiskType::FXVol] = {
        { "1", 2800 },
        { "2", 1400 },
        { "3", 590 },
        { "4", 520 },
        { "5", 340 },
        { "6", 210 }
    };

    // clang-format on
}

Real SimmConcentration_ISDA_V2_6::threshold(const RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
