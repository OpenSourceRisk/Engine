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

#include "sacva.hpp"

#include <ored/utilities/log.hpp>
#include <ored/utilities/osutils.hpp>
#include <ored/utilities/to_string.hpp>
#include <orea/engine/sacvasensitivityrecord.hpp>
#include <orea/engine/standardapproachcvacalculator.hpp>
#include <orea/engine/sacvasensitivityloader.hpp>
#include <oret/datapaths.hpp>
#include <oret/fileutilities.hpp>
#include <test/oreatoplevelfixture.hpp>


using ore::test::TopLevelFixture;
using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

using boost::timer::cpu_timer;
using boost::timer::default_places;

// Struct to hold cached expected results
struct RiskFactorCorrelationData {
    CvaRiskFactorKey::KeyType keyType;
    string bucket;
    CvaRiskFactorKey::MarginType marginType;
    string riskFactor_1;
    string riskFactor_2;
    Real riskFactorCorrelation;
};

vector<RiskFactorCorrelationData> cachedRiskFactorCorrelationData() {
    // clang-format off
    vector<RiskFactorCorrelationData> data = {
    //    CvaRiskFactorKey::KeyType,               bucket,CvaRiskFactorKey::MarginType,        riskFactor_1, riskFactor_2, RiskFactorCorrelation
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "1Y",         "1Y",         1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "1Y",         "2Y",         0.91 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "1Y",         "5Y",         0.72 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "1Y",         "10Y",        0.55 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "1Y",         "30Y",        0.31 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "1Y",         "Inflation",  0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "2Y",         "2Y",         1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "2Y",         "5Y",         0.87 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "2Y",         "10Y",        0.72 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "2Y",         "30Y",        0.45 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "2Y",         "Inflation",  0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "5Y",         "5Y",         1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "5Y",         "10Y",        0.91 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "5Y",         "30Y",        0.68 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "5Y",         "Inflation",  0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "10Y",         "10Y",       1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "10Y",         "30Y",       0.83 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "10Y",         "Inflation", 0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "30Y",         "30Y",       1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "30Y",         "Inflation", 0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Delta, "Inflation",   "Inflation", 1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "1Y",         "1Y",         1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "1Y",         "2Y",         0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "1Y",         "5Y",         0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "1Y",         "10Y",        0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "1Y",         "30Y",        0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "1Y",         "Inflation",  0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "2Y",         "2Y",         1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "2Y",         "5Y",         0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "2Y",         "10Y",        0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "2Y",         "30Y",        0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "2Y",         "Inflation",  0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "5Y",         "5Y",         1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "5Y",         "10Y",        0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "5Y",         "30Y",        0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "5Y",         "Inflation",  0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "10Y",         "10Y",       1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "10Y",         "30Y",       0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "10Y",         "Inflation", 0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "30Y",         "30Y",       1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "30Y",         "Inflation", 0.40 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK", CvaRiskFactorKey::MarginType::Delta, "Inflation",   "Inflation", 1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Vega,  "Inflation",   "Inflation", 1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Vega,  "IRVolatility","IRVolatility", 1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Vega,  "InflationVolatilty","InflationVolatilty", 1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD", CvaRiskFactorKey::MarginType::Vega,  "IRVolatility","InflationVolatilty", 0.40 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1", CvaRiskFactorKey::MarginType::Delta,  "CPTY_A/1Y","CPTY_A/2Y", 0.90 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1", CvaRiskFactorKey::MarginType::Delta,  "CPTY_B/2Y","CPTY_A/2Y", 0.50 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "8", CvaRiskFactorKey::MarginType::Delta,  "CPTY_C/2Y","CPTY_D/2Y", 0.64 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "8", CvaRiskFactorKey::MarginType::Delta,  "CPTY_C/1Y","CPTY_D/2Y", 0.576 }

    };
    // clang-format on

    return data;
}

struct BucketCorrelationData {
    CvaRiskFactorKey::KeyType keyType;
    string bucket_1;
    string bucket_2;
    Real bucketCorrelation;
};

vector<BucketCorrelationData> cachedBucketCorrelationData() {
    // clang-format off
    vector<BucketCorrelationData> data = {
    //    CvaRiskFactorKey::KeyType,               bucket_1, bucket_2, bucketCorrelation
        { CvaRiskFactorKey::KeyType::InterestRate, "USD",     "USD",         1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK",     "NOK",         1.00 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK",     "USD",         0.50 },
        { CvaRiskFactorKey::KeyType::ForeignExchange, "EUR",     "EUR",         1.00 },
        { CvaRiskFactorKey::KeyType::ForeignExchange, "NOK",     "NOK",         1.00 },
        { CvaRiskFactorKey::KeyType::ForeignExchange, "NOK",     "EUR",         0.60 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1",     "1",         1.00 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1",     "2",         0.1 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1",     "3",         0.2 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1",     "4",         0.25 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1",     "5",         0.2 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1",     "6",         0.15 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1",     "7",         0 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1",     "8",         0.45 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "2",     "2",         1.00 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "2",     "3",         0.05 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "2",     "4",         0.15 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "2",     "5",         0.2 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "2",     "6",         0.05 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "2",     "7",         0 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "2",     "8",         0.45 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "3",     "3",         1 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "3",     "4",         0.2 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "3",     "5",         0.25 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "3",     "6",         0.05 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "3",     "7",         0 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "3",     "8",         0.45 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "4",     "4",         1.00 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "4",     "5",         0.25 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "4",     "6",         0.05 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "4",     "7",         0 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "4",     "8",         0.45 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "5",     "5",         1.00 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "5",     "6",         0.05 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "5",     "7",         0 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "5",     "8",         0.45 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "6",     "6",         1.00 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "6",     "7",         0 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "6",     "8",         0.45 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "7",     "7",         1 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "7",     "8",         0 },
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "8",     "8",         1 }

    };
    // clang-format on

    return data;
}

struct RiskWeightData {
    CvaRiskFactorKey::KeyType keyType;
    string bucket;
    CvaRiskFactorKey::MarginType marginType;
    string riskFactor;
    Real riskWeight;
};

vector<RiskWeightData> cachedRiskWeightData() {
    // clang-format off
    vector<RiskWeightData> data = {
    //    CvaRiskFactorKey::KeyType,               bucket, CvaRiskFactorKey::MarginType,        riskFactor, riskWeight
        { CvaRiskFactorKey::KeyType::InterestRate, "USD",  CvaRiskFactorKey::MarginType::Delta,  "1Y",      0.0111 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD",  CvaRiskFactorKey::MarginType::Delta,  "2Y",      0.0093 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD",  CvaRiskFactorKey::MarginType::Delta,  "5Y",      0.0074 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD",  CvaRiskFactorKey::MarginType::Delta,  "10Y",      0.0074 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD",  CvaRiskFactorKey::MarginType::Delta,  "30Y",      0.0074 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD",  CvaRiskFactorKey::MarginType::Delta,  "Inflation",0.0111 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK",  CvaRiskFactorKey::MarginType::Delta,  "30Y",      0.0158 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK",  CvaRiskFactorKey::MarginType::Delta,  "Inflation",0.0158 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD",  CvaRiskFactorKey::MarginType::Vega,  "IRVolatility",  1 },
        { CvaRiskFactorKey::KeyType::InterestRate, "USD",  CvaRiskFactorKey::MarginType::Vega,  "InflationVolatilty",  1 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK",  CvaRiskFactorKey::MarginType::Vega,  "IRVolatility",  1 },
        { CvaRiskFactorKey::KeyType::InterestRate, "NOK",  CvaRiskFactorKey::MarginType::Vega,  "InflationVolatilty",  1 },
        { CvaRiskFactorKey::KeyType::ForeignExchange, "NOK",  CvaRiskFactorKey::MarginType::Delta, "FXSpot", 0.11 },
        { CvaRiskFactorKey::KeyType::ForeignExchange, "NOK",  CvaRiskFactorKey::MarginType::Vega,  "FXVolatility", 1},
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1",  CvaRiskFactorKey::MarginType::Delta,  "CPTY_A/1Y", 0.6},
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1",  CvaRiskFactorKey::MarginType::Delta,  "CPTY_B/5Y", 0.7},
        { CvaRiskFactorKey::KeyType::CreditCounterparty, "1",  CvaRiskFactorKey::MarginType::Delta,  "CPTY_C/5Y", 0.8}
    };
    // clang-format on

    return data;
}

struct SaCvaResultData {
    string nettingSet;
    CvaRiskFactorKey::KeyType keyType;
    CvaRiskFactorKey::MarginType marginType;
    string bucket;
    Real K;
};

vector<SaCvaResultData> cachedFxDeltaResult() {
    // clang-format off
    vector<SaCvaResultData> data = {
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "BRL", 4.870650908},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "EUR", 3.268944081},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "GBP", 1.436809545},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "HKD", 3.481232375},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "HUF", 8.033325859},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "IND", 8.161023061},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "JPY", 6.605561032},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "All", 28.773042685156},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "BRL", 4.870650908},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "EUR", 3.268944081},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "GBP", 1.436809545},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "HKD", 3.481232375},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "HUF", 8.033325859},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "IND", 8.161023061},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "JPY", 6.605561032},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta, "All", 28.773042685156}

    };
    // clang-format on

    return data;
}

vector<SaCvaResultData> cachedFxVegaResult() {
    // clang-format off
    vector<SaCvaResultData> data = {
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "BRL", 50.878931292235},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "EUR", 62.874726790659},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "GBP", 49.702148066658},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "HKD", 86.093091113109},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "HUF", 34.422449640315},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "IND", 4.873484457757},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "JPY", 17.482316243851},
        { "", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "All", 247.236492813541},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "BRL", 50.878931292235},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "EUR", 62.874726790659},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "GBP", 49.702148066658},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "HKD", 86.093091113109},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "HUF", 34.422449640315},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "IND", 4.873484457757},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "JPY", 17.482316243851},
        { "CPTY_A", CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega, "All", 247.236492813541}
    };
    // clang-format on

    return data;
}

vector<SaCvaResultData> cachedIrVegaResult() {
    // clang-format off
    vector<SaCvaResultData> data = {
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "BRL", 23.702016066993},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "EUR", 23.129027376005},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "GBP", 37.860381786242},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "HKD", 51.254762584174},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "HUF", 57.400528382673},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "IND", 18.985641411604},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "JPY", 28.754174417639},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "USD", 20.439677229594},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "All", 74.073402775018},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "BRL", 23.702016066993},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate,CvaRiskFactorKey::MarginType::Vega,  "EUR", 23.129027376005},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "GBP", 37.860381786242},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "HKD", 51.254762584174},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "HUF", 57.400528382673},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "IND", 18.985641411604},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "JPY", 28.754174417639},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "USD", 20.439677229594},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega, "All", 74.073402775018}
    };
    // clang-format on

    return data;
}

vector<SaCvaResultData> cachedIrDeltaResult() {
    // clang-format off
    vector<SaCvaResultData> data = {
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "BRL", 0.137909887056},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "EUR", 0.555844056},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "GBP", 0.924235656},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "HKD", 0.375428083},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "HUF", 0.039103225253},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "IND", 0.503771550878},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "JPY", 0.671534501903},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "USD", 0.675865227993},
        { "", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "All", 1.667580196504},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "BRL", 0.137909887056},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "EUR", 0.555844056},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "GBP", 0.924235656},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "HKD", 0.375428083},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "HUF", 0.039103225253},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "IND", 0.503771550878},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "JPY", 0.671534501903},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "USD", 0.675865227993},
        { "CPTY_A", CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta, "All", 1.667580196504}
    };
    // clang-format on

    return data;
}

namespace testsuite {

void SaCvaTest::testSACVA_RiskFactorCorrelation() {
    vector<RiskFactorCorrelationData> expectedResults = cachedRiskFactorCorrelationData();
    QuantLib::ext::shared_ptr<ore::data::CounterpartyManager> countypartyManager =
        QuantLib::ext::make_shared<ore::data::CounterpartyManager>();
    QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_a =
        QuantLib::ext::make_shared<ore::data::CounterpartyInformation>(
            "CPTY_A", false, ore::data::CounterpartyCreditQuality::HY, 0.6, 0.5);
    QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_b =
        QuantLib::ext::make_shared<ore::data::CounterpartyInformation>(
            "CPTY_B", false, ore::data::CounterpartyCreditQuality::HY, 0.7, 0.5);
    QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_c =
        QuantLib::ext::make_shared<ore::data::CounterpartyInformation>(
            "CPTY_C", false, ore::data::CounterpartyCreditQuality::NR, 0.8, 0.5);
    QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_d =
        QuantLib::ext::make_shared<ore::data::CounterpartyInformation>(
            "CPTY_D", false, ore::data::CounterpartyCreditQuality::HY, 0.8, 0.5);
    countypartyManager->add(cp_a);
    countypartyManager->add(cp_b);
    countypartyManager->add(cp_c);
    countypartyManager->add(cp_d);

    countypartyManager->addCorrelation("CPTY_A", "CPTY_B", 0.5);
    countypartyManager->addCorrelation("CPTY_C", "CPTY_D", 0.8);

    SaCvaNetSensitivities cvaNetSensitivities;
    std::map<StandardApproachCvaCalculator::ReportType, QuantLib::ext::shared_ptr<ore::data::Report>> outReports;
    // run sa-cva
    QuantLib::ext::shared_ptr<StandardApproachCvaCalculator> sacva =
        QuantLib::ext::make_shared<StandardApproachCvaCalculator>("USD", cvaNetSensitivities, countypartyManager, outReports);

    for (auto e : expectedResults) {
        BOOST_TEST_MESSAGE("checking result " << e.keyType << " " << e.bucket << " " << e.marginType << " "
                                              << e.riskFactor_1 << " " << e.riskFactor_2);

        Real tolerance = 0.0001;
        BOOST_CHECK_CLOSE(
            e.riskFactorCorrelation,
            sacva->getRiskFactorCorrelation(e.keyType, e.bucket, e.marginType, e.riskFactor_1, e.riskFactor_2),
            tolerance);
        BOOST_CHECK_CLOSE(
            e.riskFactorCorrelation,
            sacva->getRiskFactorCorrelation(e.keyType, e.bucket, e.marginType, e.riskFactor_2, e.riskFactor_1),
            tolerance);
    }

    // there should only be one FX sensitivity riskfactor for each currency
    // so we check that a search for a riskFactor correlation fails appropriately
    BOOST_CHECK_THROW(sacva->getRiskFactorCorrelation(CvaRiskFactorKey::KeyType::ForeignExchange, "USD",
                                                      CvaRiskFactorKey::MarginType::Delta, "FXSpot", "FXSpot2"),
                      QuantLib::Error);
    BOOST_CHECK_THROW(sacva->getRiskFactorCorrelation(CvaRiskFactorKey::KeyType::ForeignExchange, "USD",
                                                      CvaRiskFactorKey::MarginType::Vega, "FXVolatility",
                                                      "FXVolatility2"),
                      QuantLib::Error);
}

void SaCvaTest::testSACVA_BucketCorrelation() {
    vector<BucketCorrelationData> expectedResults = cachedBucketCorrelationData();
    QuantLib::ext::shared_ptr<ore::data::CounterpartyManager> countypartyManager =
        QuantLib::ext::make_shared<ore::data::CounterpartyManager>();

    SaCvaNetSensitivities cvaNetSensitivities;
    std::map<StandardApproachCvaCalculator::ReportType, QuantLib::ext::shared_ptr<ore::data::Report>> outReports;
    // run sa-cva
    QuantLib::ext::shared_ptr<StandardApproachCvaCalculator> sacva =
        QuantLib::ext::make_shared<StandardApproachCvaCalculator>("USD", cvaNetSensitivities, countypartyManager, outReports);

    for (auto e : expectedResults) {
        BOOST_TEST_MESSAGE("checking result " << e.keyType << " " << e.bucket_1 << " " << e.bucket_2);

        Real tolerance = 0.0001;
        BOOST_CHECK_CLOSE(e.bucketCorrelation, sacva->getBucketCorrelation(e.keyType, e.bucket_1, e.bucket_2),
                          tolerance);
        BOOST_CHECK_CLOSE(e.bucketCorrelation, sacva->getBucketCorrelation(e.keyType, e.bucket_2, e.bucket_1),
                          tolerance);
    }
}

void SaCvaTest::testSACVA_RiskWeight() {
    vector<RiskWeightData> expectedResults = cachedRiskWeightData();
    QuantLib::ext::shared_ptr<ore::data::CounterpartyManager> countypartyManager =
        QuantLib::ext::make_shared<ore::data::CounterpartyManager>();
    QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_a =
        QuantLib::ext::make_shared<ore::data::CounterpartyInformation>(
            "CPTY_A", false, ore::data::CounterpartyCreditQuality::HY, 0.6, 0.5);
    QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_b =
        QuantLib::ext::make_shared<ore::data::CounterpartyInformation>(
            "CPTY_B", false, ore::data::CounterpartyCreditQuality::HY, 0.7, 0.5);
    QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_c =
        QuantLib::ext::make_shared<ore::data::CounterpartyInformation>(
            "CPTY_C", false, ore::data::CounterpartyCreditQuality::NR, 0.8, 0.5);
    countypartyManager->add(cp_a);
    countypartyManager->add(cp_b);
    countypartyManager->add(cp_c);

    SaCvaNetSensitivities cvaNetSensitivities;
    std::map<StandardApproachCvaCalculator::ReportType, QuantLib::ext::shared_ptr<ore::data::Report>> outReports;
    // run sa-cva
    QuantLib::ext::shared_ptr<StandardApproachCvaCalculator> sacva =
        QuantLib::ext::make_shared<StandardApproachCvaCalculator>("USD", cvaNetSensitivities, countypartyManager, outReports);

    for (auto e : expectedResults) {
        BOOST_TEST_MESSAGE("checking result " << e.keyType << " " << e.bucket << " " << e.marginType << " "
                                              << e.riskFactor);

        Real tolerance = 0.0001;
        BOOST_CHECK_CLOSE(e.riskWeight, sacva->getRiskWeight(e.keyType, e.bucket, e.marginType, e.riskFactor),
                          tolerance);
    }
}

void SaCvaTest::testSACVA_FxDeltaCalc() {
    BOOST_TEST_MESSAGE("testing Fx Delta SA-CVA calc...");
    vector<SaCvaResultData> expectedResults = cachedFxDeltaResult();

    QuantLib::ext::shared_ptr<ore::data::CounterpartyManager> countypartyManager =
        QuantLib::ext::make_shared<ore::data::CounterpartyManager>();
    QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_a =
        QuantLib::ext::make_shared<ore::data::CounterpartyInformation>(
            "CPTY_A", false, ore::data::CounterpartyCreditQuality::HY, 0.05, 0.5);
    countypartyManager->add(cp_a);

    SaCvaSensitivityLoader cvaLoader;
    char eol = '\n';
    char delim = ',';
    cvaLoader.load(TEST_INPUT_FILE("cva_sensi_fx_delta.csv"), eol, delim);
    SaCvaNetSensitivities cvaNetSensitivities = cvaLoader.netRecords();

    bool unhedgedSensitivity = false;
    std::vector<std::string> perfectHedges;
    std::map<StandardApproachCvaCalculator::ReportType, QuantLib::ext::shared_ptr<ore::data::Report>> outReports;
    // run sa-cva
    QuantLib::ext::shared_ptr<StandardApproachCvaCalculator> sacva = QuantLib::ext::make_shared<StandardApproachCvaCalculator>(
        "USD", cvaNetSensitivities, countypartyManager, outReports, unhedgedSensitivity, perfectHedges);

    sacva->calculate();

    std::map<SaCvaSummaryKey, Real> cvaRiskTypeResults = sacva->cvaRiskTypeResults();

    for (auto e : expectedResults) {
        BOOST_TEST_MESSAGE("checking result " << e.nettingSet << " " << e.keyType << " " << e.marginType << " "
                                              << e.bucket);

        Real tolerance = 0.0001;
        BOOST_CHECK(cvaRiskTypeResults.find(SaCvaSummaryKey(e.nettingSet, e.keyType, e.marginType, e.bucket)) !=
                    cvaRiskTypeResults.end());
        auto it = cvaRiskTypeResults.find(SaCvaSummaryKey(e.nettingSet, e.keyType, e.marginType, e.bucket));
        BOOST_CHECK_CLOSE(e.K, it->second, tolerance);
    }
}

void SaCvaTest::testSACVA_FxVegaCalc() {
    BOOST_TEST_MESSAGE("testing Fx Vega SA-CVA calc...");
    vector<SaCvaResultData> expectedResults = cachedFxVegaResult();

    QuantLib::ext::shared_ptr<ore::data::CounterpartyManager> countypartyManager =
        QuantLib::ext::make_shared<ore::data::CounterpartyManager>();
    QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_a =
        QuantLib::ext::make_shared<ore::data::CounterpartyInformation>(
            "CPTY_A", false, ore::data::CounterpartyCreditQuality::HY, 0.05, 0.5);
    countypartyManager->add(cp_a);

    SaCvaSensitivityLoader cvaLoader;
    char eol = '\n';
    char delim = ',';
    cvaLoader.load(TEST_INPUT_FILE("cva_sensi_fx_vega.csv"), eol, delim);
    SaCvaNetSensitivities cvaNetSensitivities = cvaLoader.netRecords();

    bool unhedgedSensitivity = false;
    std::vector<std::string> perfectHedges;
    std::map<StandardApproachCvaCalculator::ReportType, QuantLib::ext::shared_ptr<ore::data::Report>> outReports;
    // run sa-cva
    QuantLib::ext::shared_ptr<StandardApproachCvaCalculator> sacva = QuantLib::ext::make_shared<StandardApproachCvaCalculator>(
        "USD", cvaNetSensitivities, countypartyManager, outReports, unhedgedSensitivity, perfectHedges);

    sacva->calculate();

    std::map<SaCvaSummaryKey, QuantLib::Real> cvaRiskTypeResults = sacva->cvaRiskTypeResults();

    for (auto e : expectedResults) {
        BOOST_TEST_MESSAGE("checking result " << e.nettingSet << " " << e.keyType << " " << e.marginType << " "
                                              << e.bucket);

        Real tolerance = 0.0001;
        BOOST_CHECK(cvaRiskTypeResults.find(SaCvaSummaryKey(e.nettingSet, e.keyType, e.marginType, e.bucket)) !=
                    cvaRiskTypeResults.end());
        auto it = cvaRiskTypeResults.find(SaCvaSummaryKey(e.nettingSet, e.keyType, e.marginType, e.bucket));
        BOOST_CHECK_CLOSE(e.K, it->second, tolerance);
    }
}

void SaCvaTest::testSACVA_IrVegaCalc() {
    BOOST_TEST_MESSAGE("testing IR Vega SA-CVA calc...");
    vector<SaCvaResultData> expectedResults = cachedIrVegaResult();

    QuantLib::ext::shared_ptr<ore::data::CounterpartyManager> countypartyManager =
        QuantLib::ext::make_shared<ore::data::CounterpartyManager>();
    QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_a =
        QuantLib::ext::make_shared<ore::data::CounterpartyInformation>(
            "CPTY_A", false, ore::data::CounterpartyCreditQuality::HY, 0.05, 0.5);
    countypartyManager->add(cp_a);

    SaCvaSensitivityLoader cvaLoader;
    char eol = '\n';
    char delim = ',';
    cvaLoader.load(TEST_INPUT_FILE("cva_sensi_ir_vega.csv"), eol, delim);
    SaCvaNetSensitivities cvaNetSensitivities = cvaLoader.netRecords();

    bool unhedgedSensitivity = false;
    std::vector<std::string> perfectHedges;
    std::map<StandardApproachCvaCalculator::ReportType, QuantLib::ext::shared_ptr<ore::data::Report>> outReports;
    // run sa-cva
    QuantLib::ext::shared_ptr<StandardApproachCvaCalculator> sacva = QuantLib::ext::make_shared<StandardApproachCvaCalculator>(
        "USD", cvaNetSensitivities, countypartyManager, outReports, unhedgedSensitivity, perfectHedges);

    sacva->calculate();

    std::map<SaCvaSummaryKey, QuantLib::Real> cvaRiskTypeResults = sacva->cvaRiskTypeResults();

    for (auto e : expectedResults) {
        BOOST_TEST_MESSAGE("checking result " << e.nettingSet << " " << e.keyType << " " << e.marginType << " "
                                              << e.bucket);

        Real tolerance = 0.0001;
        BOOST_CHECK(cvaRiskTypeResults.find(SaCvaSummaryKey(e.nettingSet, e.keyType, e.marginType, e.bucket)) !=
                    cvaRiskTypeResults.end());
        auto it = cvaRiskTypeResults.find(SaCvaSummaryKey(e.nettingSet, e.keyType, e.marginType, e.bucket));
        BOOST_CHECK_CLOSE(e.K, it->second, tolerance);
    }
}

void SaCvaTest::testSACVA_IrDeltaCalc() {
    BOOST_TEST_MESSAGE("testing IR Delta SA-CVA calc...");
    vector<SaCvaResultData> expectedResults = cachedIrDeltaResult();

    QuantLib::ext::shared_ptr<ore::data::CounterpartyManager> countypartyManager =
        QuantLib::ext::make_shared<ore::data::CounterpartyManager>();
    QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_a =
        QuantLib::ext::make_shared<ore::data::CounterpartyInformation>(
            "CPTY_A", false, ore::data::CounterpartyCreditQuality::HY, 0.05, 0.5);
    countypartyManager->add(cp_a);

    SaCvaSensitivityLoader cvaLoader;
    char eol = '\n';
    char delim = ',';
    cvaLoader.load(TEST_INPUT_FILE("cva_sensi_ir_delta.csv"), eol, delim);
    SaCvaNetSensitivities cvaNetSensitivities = cvaLoader.netRecords();

    bool unhedgedSensitivity = false;
    std::vector<std::string> perfectHedges;
    std::map<StandardApproachCvaCalculator::ReportType, QuantLib::ext::shared_ptr<ore::data::Report>> outReports;
    // run sa-cva
    QuantLib::ext::shared_ptr<StandardApproachCvaCalculator> sacva = QuantLib::ext::make_shared<StandardApproachCvaCalculator>(
        "USD", cvaNetSensitivities, countypartyManager, outReports, unhedgedSensitivity, perfectHedges);

    sacva->calculate();

    std::map<SaCvaSummaryKey, QuantLib::Real> cvaRiskTypeResults = sacva->cvaRiskTypeResults();

    for (auto e : expectedResults) {
        BOOST_TEST_MESSAGE("checking result " << e.nettingSet << " " << e.keyType << " " << e.marginType << " "
                                              << e.bucket);

        Real tolerance = 0.0001;
        BOOST_CHECK(cvaRiskTypeResults.find(SaCvaSummaryKey(e.nettingSet, e.keyType, e.marginType, e.bucket)) !=
                    cvaRiskTypeResults.end());
        auto it = cvaRiskTypeResults.find(SaCvaSummaryKey(e.nettingSet, e.keyType, e.marginType, e.bucket));
        BOOST_CHECK_CLOSE(e.K, it->second, tolerance);
    }
}


BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(SACVATest)

BOOST_AUTO_TEST_CASE(testRiskFactorCorrelation) { 
    BOOST_TEST_MESSAGE("Testing SACVA Risk Factor Correlation");
    SaCvaTest::testSACVA_RiskFactorCorrelation();
}

BOOST_AUTO_TEST_CASE(testBucketCorrelation) {
    BOOST_TEST_MESSAGE("Testing SACVA Bucket Correlation");
    SaCvaTest::testSACVA_BucketCorrelation();
}

BOOST_AUTO_TEST_CASE(testRiskWeightCorrelation) {
    BOOST_TEST_MESSAGE("Testing SACVA Risk Weight Correlation");
    SaCvaTest::testSACVA_RiskWeight();
}

BOOST_AUTO_TEST_CASE(testFxDeltaCalc) {
    BOOST_TEST_MESSAGE("Testing SACVA FX Delta Calculation");
    SaCvaTest::testSACVA_FxDeltaCalc();
}

BOOST_AUTO_TEST_CASE(testFxVegaCalc) {
    BOOST_TEST_MESSAGE("Testing SACVA FX Vega Calculation");
    SaCvaTest::testSACVA_FxVegaCalc();
}

BOOST_AUTO_TEST_CASE(testIrVegaCalc) {
    BOOST_TEST_MESSAGE("Testing SACVA IR Vega Calculation");
    SaCvaTest::testSACVA_IrVegaCalc();
}

BOOST_AUTO_TEST_CASE(testIrDeltaCalc) {
    BOOST_TEST_MESSAGE("Testing SACVA IR Delta Calculation");
    SaCvaTest::testSACVA_IrDeltaCalc();
}

} // namespace testsuite

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
