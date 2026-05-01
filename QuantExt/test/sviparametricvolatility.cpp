/*
 Copyright (C) 2025 AcadiaSoft, Inc.
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

#include "toplevelfixture.hpp"

#include <qle/models/carrmadanarbitragecheck.hpp>
#include <qle/termstructures/svimodeltraits.hpp>
#include <qle/termstructures/sviparametricvolatility.hpp>

#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/pricingengines/blackformula.hpp>

#include <boost/make_shared.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
namespace bdata = boost::unit_test::data;

namespace QuantExt {
std::ostream& operator<<(std::ostream& out, SviParametricVolatility::ModelVariant m) {
    return out << static_cast<int>(m);
}
}

namespace {

// Variables to be used in the test
struct CommonData {

    // Constructor
    CommonData() {
        ParametricVolatility::MarketSmile marketSmile;
        marketSmile.timeToExpiry = 1.0;
        marketSmile.forward = 0.05;
        marketSmile.lognormalShift = 0.01;
        marketSmile.optionTypes = {Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call};
        marketSmile.strikes = {0.03, 0.04, 0.045, 0.05, 0.055, 0.06, 0.07};
        marketSmile.marketQuotes = {0.25, 0.20, 0.16, 0.15, 0.16, 0.18, 0.22};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 2.0;
        marketSmile.forward = 0.053;
        marketSmile.lognormalShift = 0.01;
        marketSmile.optionTypes = {Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call};
        marketSmile.strikes = {0.03, 0.04, 0.045, 0.05, 0.055, 0.06, 0.07};
        marketSmile.marketQuotes = {0.27, 0.23, 0.16, 0.155, 0.18, 0.20, 0.22};
        marketSmiles.push_back(marketSmile);

        rawModelParameters = { 0.01, 0.1, 0.003, 0.007, 0.07 }; // a, b, rho, m, sigma

        testStrikes = {0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08, 0.09};
    }
    std::vector<ParametricVolatility::MarketSmile> marketSmiles;
    std::vector<Real> testStrikes;
    std::vector<Real> rmseThreshold = {0.022, 0.046};
    std::vector<Real> quoteTolerance = {0.074, 0.113};
    std::vector<Real> rawModelParameters;

};

std::vector<ParametricVolatility::MarketSmile> makeEquitySmiles() {
    std::vector<ParametricVolatility::MarketSmile> marketSmiles;

    ParametricVolatility::MarketSmile marketSmile;
    marketSmile.lognormalShift = 0.0;

        marketSmile.timeToExpiry = 7.0 / 365;
        marketSmile.forward = 1210.165;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {1122.94, 1144.51, 1158.58, 1173.53, 1182.43, 1192.31, 1193.93, 1203.18, 1206.75, 1214.23, 1222.30, 1224.68, 1231.90, 1241.53, 1247.26, 1252.82, 1264.55};
        marketSmile.marketQuotes = {0.410316, 0.381766, 0.363500, 0.347710, 0.336493, 0.320933, 0.317560, 0.306574, 0.299896, 0.291993, 0.283417, 0.279219, 0.272729, 0.266710, 0.258742, 0.255447, 0.246115};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 14.0 / 365;
        marketSmile.forward = 1209.084;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {1099.41, 1124.05, 1145.36, 1158.26, 1172.51, 1179.33, 1190.89, 1200.78, 1207.99, 1217.31, 1224.50, 1233.79, 1244.11, 1250.74, 1259.39, 1267.86, 1280.90};
        marketSmile.marketQuotes = {0.400044, 0.376577, 0.356776, 0.345523, 0.330080, 0.319763, 0.309992, 0.299427, 0.288343, 0.279092, 0.274458, 0.267821, 0.258001, 0.252714, 0.244229, 0.236755, 0.230965};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 21.0 / 365;
        marketSmile.forward = 1206.166;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {1075.59, 1107.11, 1126.91, 1148.60, 1162.99, 1180.48, 1189.66, 1199.70, 1210.14, 1222.31, 1229.70, 1241.62, 1253.28, 1260.18, 1268.81, 1285.60, 1296.09};
        marketSmile.marketQuotes = {0.403621, 0.379285, 0.361075, 0.344408, 0.329564, 0.317312, 0.306874, 0.295942, 0.287708, 0.275504, 0.268199, 0.260995, 0.251052, 0.247579, 0.239204, 0.228724, 0.220916};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 30.0 / 365;
        marketSmile.forward = 1208.709;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {1044.97, 1084.92, 1112.62, 1140.16, 1158.77, 1170.80, 1187.09, 1198.32, 1216.20, 1227.57, 1234.45, 1248.16, 1259.08, 1268.41, 1284.63, 1295.02, 1312.73};
        marketSmile.marketQuotes = {0.409582, 0.382653, 0.366011, 0.341689, 0.327985, 0.316492, 0.305219, 0.293709, 0.283773, 0.272561, 0.263696, 0.255191, 0.248151, 0.244946, 0.235525, 0.223650, 0.213641};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 60.0 / 365;
        marketSmile.forward = 1205.713;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {985.44, 1036.16, 1079.68, 1109.24, 1134.60, 1160.25, 1182.55, 1197.48, 1215.82, 1237.38, 1250.35, 1264.25, 1280.49, 1294.35, 1311.29, 1332.23, 1356.31};
        marketSmile.marketQuotes = {0.421799, 0.393766, 0.370669, 0.350774, 0.333624, 0.319848, 0.303939, 0.293438, 0.284358, 0.272761, 0.262142, 0.253328, 0.246538, 0.240213, 0.229835, 0.223702, 0.213295};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 90.0 / 365;
        marketSmile.forward = 1202.009;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {944.63, 1007.48, 1054.91, 1092.41, 1126.63, 1153.85, 1180.67, 1196.90, 1220.61, 1240.47, 1264.24, 1278.18, 1297.55, 1319.64, 1339.72, 1363.21, 1391.89};
        marketSmile.marketQuotes = {0.418999, 0.389986, 0.365363, 0.346810, 0.326453, 0.314019, 0.300606, 0.289580, 0.284623, 0.276251, 0.265419, 0.255980, 0.248485, 0.240609, 0.230233, 0.223509, 0.215270};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 120.0 / 365;
        marketSmile.forward = 1201.001;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {918.63, 985.75, 1037.37, 1082.99, 1119.36, 1147.48, 1176.49, 1198.01, 1223.08, 1251.19, 1270.59, 1292.26, 1316.76, 1333.43, 1361.10, 1388.29, 1424.22};
        marketSmile.marketQuotes = {0.413837, 0.385065, 0.359964, 0.340852, 0.323843, 0.309977, 0.296963, 0.286033, 0.278605, 0.269365, 0.261194, 0.252634, 0.244983, 0.236339, 0.226830, 0.219789, 0.209707};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 150.0 / 365;
        marketSmile.forward = 1202.823;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {890.86, 966.37, 1024.96, 1068.50, 1107.58, 1144.56, 1170.38, 1201.22, 1231.73, 1257.33, 1274.95, 1300.23, 1325.49, 1355.63, 1381.70, 1412.55, 1444.96};
        marketSmile.marketQuotes = {0.411588, 0.382071, 0.357016, 0.337421, 0.321289, 0.306528, 0.293209, 0.284729, 0.274771, 0.266682, 0.258676, 0.248822, 0.241263, 0.232238, 0.224041, 0.215884, 0.207250};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 180.0 / 365;
        marketSmile.forward = 1201.546;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {869.29, 951.23, 1010.33, 1060.81, 1105.30, 1141.68, 1169.43, 1202.23, 1229.92, 1255.58, 1288.14, 1313.51, 1337.30, 1368.33, 1395.98, 1428.48, 1465.09};
        marketSmile.marketQuotes = {0.409878, 0.380371, 0.354548, 0.334387, 0.318767, 0.304104, 0.291200, 0.281878, 0.272870, 0.265591, 0.256400, 0.247827, 0.239775, 0.230068, 0.221728, 0.213619, 0.205467};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 270.0 / 365;
        marketSmile.forward = 1199.358;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {821.09, 912.84, 981.06, 1042.76, 1091.20, 1135.19, 1173.14, 1209.99, 1238.05, 1276.24, 1305.15, 1341.85, 1373.25, 1407.04, 1443.55, 1478.94, 1533.01};
        marketSmile.marketQuotes = {0.404928, 0.375090, 0.348356, 0.327751, 0.312948, 0.298135, 0.285556, 0.276822, 0.267281, 0.257639, 0.249180, 0.241201, 0.234008, 0.225853, 0.217684, 0.209539, 0.200184};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 360.0 / 365;
        marketSmile.forward = 1197.716;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {787.36, 888.23, 959.83, 1028.26, 1077.58, 1130.78, 1169.61, 1209.30, 1250.73, 1289.94, 1323.09, 1361.94, 1398.54, 1435.63, 1479.22, 1524.09, 1590.01};
        marketSmile.marketQuotes = {0.397650, 0.367483, 0.342872, 0.323182, 0.306671, 0.292869, 0.279114, 0.271074, 0.263733, 0.253265, 0.244636, 0.237483, 0.229768, 0.221055, 0.213485, 0.205907, 0.198745};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 720.0 / 365;
        marketSmile.forward = 1170.327;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {708.44, 822.79, 917.53, 993.93, 1063.72, 1123.39, 1177.85, 1235.13, 1283.90, 1336.73, 1387.93, 1439.38, 1491.69, 1544.06, 1601.83, 1663.54, 1751.02};
        marketSmile.marketQuotes = {0.372184, 0.345171, 0.321792, 0.303847, 0.289309, 0.276185, 0.264926, 0.260340, 0.253158, 0.244007, 0.234930, 0.226771, 0.218799, 0.211367, 0.203609, 0.195096, 0.186241};
        marketSmiles.push_back(marketSmile);

        marketSmile.timeToExpiry = 1080.0 / 365;
        marketSmile.forward = 1157.522;
        marketSmile.optionTypes = {
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put, Option::Type::Put, Option::Type::Put, Option::Type::Put,
            Option::Type::Put,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call,
            Option::Type::Call, Option::Type::Call, Option::Type::Call, Option::Type::Call
        };
        marketSmile.strikes = {658.24, 780.43, 886.53, 974.74, 1052.66, 1127.76, 1191.30, 1265.65, 1329.86, 1386.75, 1451.16, 1511.35, 1583.19, 1647.03, 1715.69, 1794.03, 1909.31};
        marketSmile.marketQuotes = {0.366081, 0.341689, 0.319777, 0.302824, 0.287198, 0.274434, 0.264799, 0.259771, 0.251818, 0.242530, 0.234414, 0.225842, 0.218427, 0.210355, 0.201884, 0.191400, 0.183379};
        marketSmiles.push_back(marketSmile);

    return marketSmiles;
}

struct SsviTestConfig {
    std::vector<Real> initialParameters;
    Real rmseThreshold;
    Real quoteErrorThreshold;
};

const std::map<SviParametricVolatility::ModelVariant, SsviTestConfig> ssviTestConfigs = {
    {SviParametricVolatility::ModelVariant::Gatheral2012SsviHeston,
     {{-0.5, 0.1}, 0.15, 0.42}},                                          // rho, lambda
    {SviParametricVolatility::ModelVariant::Gatheral2012SsviPowerLaw,
     {{-0.5, 0.5, 0.5}, 0.071, 0.36}},                                    // rho, eta, gamma
    {SviParametricVolatility::ModelVariant::CorbettaEtAl2019Essvi,
     {{0.0, 0.0, 0.0, 0.01}, 0.014, 0.042}},                              // k_star, theta_star, rho, psi
    {SviParametricVolatility::ModelVariant::Mingone2022EssviGJ,
     {{0.0, Null<Real>(), 0.5}, 0.008, 0.024}},    // rho, a=Null (ATM-implied), c
    {SviParametricVolatility::ModelVariant::Mingone2022EssviMM,
     {{0.0, Null<Real>(), 0.5}, 0.008, 0.024}},    // rho, a=Null (ATM-implied), c
};

struct CommonDataSsvi {
    std::vector<ParametricVolatility::MarketSmile> marketSmiles = makeEquitySmiles();
    Real spot = 1209.1116;
};

void checkStaticArbitrage(const std::vector<ParametricVolatility::MarketSmile>& marketSmiles,
                          const QuantLib::ext::shared_ptr<SviParametricVolatility>& svi,
                          const Real spot,
                          const std::string& label) {
    BOOST_TEST_MESSAGE("Checking for static arbitrage...");

    std::vector<Real> times;
    std::vector<Real> forwards;
    std::vector<std::vector<Real>> callPrices;

    // Define a common moneyness grid (shifted lognormal: K/F)
    std::vector<Real> moneyness;
    for (int i = 0; i < 100; ++i) {
        moneyness.push_back(0.50 + i * (1.80 - 0.50) / 99.0);
    }

    for (const auto& marketSmile : marketSmiles) {
        times.push_back(marketSmile.timeToExpiry);
        forwards.push_back(marketSmile.forward);

        std::vector<Real> sliceCallPrices;
        for (const auto& m : moneyness) {
            Real strike = m * marketSmile.forward;
            Real vol = svi->evaluate(
                marketSmile.timeToExpiry,
                marketSmile.underlyingLength,
                strike,
                marketSmile.forward,
                ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
                marketSmile.lognormalShift
            );

            Real callPrice = blackFormula(
                Option::Call,
                strike,
                marketSmile.forward,
                vol * std::sqrt(marketSmile.timeToExpiry),
                1.0
            );
            sliceCallPrices.push_back(callPrice);
        }
        callPrices.push_back(sliceCallPrices);
    }

    CarrMadanSurface cmSurface(times, moneyness, spot, forwards, callPrices, ShiftedLognormal, 0.0);

    BOOST_CHECK_MESSAGE(cmSurface.arbitrageFree(),
        "Static arbitrage detected in calibrated " << label << " surface:\n" << arbitrageAsString(cmSurface));

    if (!cmSurface.arbitrageFree()) {
        BOOST_TEST_MESSAGE("Arbitrage details:\n" << arbitrageAsString(cmSurface));
    }
}

std::map<std::pair<QuantLib::Real, QuantLib::Real>,
         std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>
buildModelParameterMap(const std::vector<ParametricVolatility::MarketSmile>& marketSmiles,
                       const std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>& modelParameter) {
    std::map<std::pair<QuantLib::Real, QuantLib::Real>,
             std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>> modelParameters;
    for (const auto& marketSmile : marketSmiles) {
        modelParameters[{marketSmile.timeToExpiry, marketSmile.underlyingLength}] = modelParameter;
    }
    return modelParameters;
}

void checkSmileCalibrationQuality(const QuantLib::ext::shared_ptr<SviParametricVolatility>& svi,
                                  const ParametricVolatility::MarketSmile& marketSmile,
                                  Real quoteErrorThreshold,
                                  Real rmseThreshold) {
    Real rmse = 0.0;
    Real maxQuote = *std::max_element(marketSmile.marketQuotes.begin(), marketSmile.marketQuotes.end());
    for (int j = 0; j < marketSmile.strikes.size(); ++j) {
        auto vol = svi->evaluate(
            marketSmile.timeToExpiry,
            marketSmile.underlyingLength,
            marketSmile.strikes[j],
            marketSmile.forward,
            ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
            marketSmile.lognormalShift
        );
        auto expected = marketSmile.marketQuotes[j];
        BOOST_CHECK_CLOSE(vol, expected, quoteErrorThreshold * 100);
        rmse += std::pow((vol - expected) / maxQuote, 2);
    }
    rmse = std::sqrt(rmse / marketSmile.strikes.size());
    BOOST_CHECK_SMALL(rmse, rmseThreshold);
}

void testModelVariantFixed(const CommonData& data, SviParametricVolatility::ModelVariant modelVariant) {
    const auto& marketSmiles = data.marketSmiles;
    const auto& testStrikes = data.testStrikes;

    auto newModelParameters = SviModelTraits::fromRawSvi(marketSmiles[0].timeToExpiry, data.rawModelParameters,
                                                         modelVariant);
    auto modelParameter = std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>(newModelParameters.size());
    for (Size i = 0; i < newModelParameters.size(); ++i) {
        BOOST_TEST_MESSAGE("Converted model parameter " << i << ": " << newModelParameters[i]);
        modelParameter[i] = {newModelParameters[i], ParametricVolatility::ParameterCalibration::Fixed};
    }
    auto modelParameters = buildModelParameterMap(marketSmiles, modelParameter);

    auto svi = QuantLib::ext::make_shared<SviParametricVolatility>(
        modelVariant,
        marketSmiles,
        ParametricVolatility::MarketModelType::Black76,
        ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.05, Actual365Fixed())),
        modelParameters);

    for (const auto& marketSmile : marketSmiles) {
        for (int i = 0; i < testStrikes.size(); ++i) {
            auto vol = svi->evaluate(
                marketSmile.timeToExpiry,
                marketSmile.underlyingLength,
                testStrikes[i],
                marketSmile.forward,
                ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility);
            auto logk = std::log((testStrikes[i] + marketSmile.lognormalShift) / (marketSmile.forward + marketSmile.lognormalShift));
            Real a, b, rho, m , sigma;
            std::tie(a, b, rho, m , sigma) =
                SviModelTraits::toRawSvi(marketSmile.timeToExpiry, newModelParameters, modelVariant);
            auto expected = a + b * (rho * (logk - m) + std::sqrt((logk - m) * (logk - m) + sigma * sigma));
            expected = std::sqrt(std::max(0.0, expected) / marketSmile.timeToExpiry);
            BOOST_CHECK_CLOSE(vol, expected, 1e-8);
        }
    }
}

void testModelVariantCalibrated(const CommonData& data, SviParametricVolatility::ModelVariant modelVariant) {
    const auto& marketSmiles = data.marketSmiles;

    auto newModelParameters = SviModelTraits::fromRawSvi(marketSmiles[0].timeToExpiry, data.rawModelParameters,
                                                         modelVariant);
    auto modelParameter = std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>(newModelParameters.size());
    for (Size i = 0; i < newModelParameters.size(); ++i) {
        BOOST_TEST_MESSAGE("Converted model parameter " << i << ": " << newModelParameters[i]);
        modelParameter[i] = {newModelParameters[i], ParametricVolatility::ParameterCalibration::Calibrated};
    }
    if (modelVariant == SviParametricVolatility::ModelVariant::Gatheral2012SsviHeston ||
        modelVariant == SviParametricVolatility::ModelVariant::Gatheral2012SsviPowerLaw) {
            modelParameter[0].second = ParametricVolatility::ParameterCalibration::Implied; // set atm vol as implied
    }
    auto modelParameters = buildModelParameterMap(marketSmiles, modelParameter);

    QuantLib::ext::shared_ptr<SviParametricVolatility> svi;
    svi.reset(new SviParametricVolatility(
        modelVariant,
        marketSmiles,
        ParametricVolatility::MarketModelType::Black76,
        ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.05, Actual365Fixed())),
        modelParameters));

    for (Size i = 0; i < marketSmiles.size(); ++i) {
        checkSmileCalibrationQuality(svi, marketSmiles[i], data.quoteTolerance[i], data.rmseThreshold[i]);
    }
}

void testModelVariantCalibratedSsvi(const CommonDataSsvi& data, SviParametricVolatility::ModelVariant modelVariant) {
    const auto& marketSmiles = data.marketSmiles;
    const auto& config = ssviTestConfigs.at(modelVariant);

    const auto& initialParameters = config.initialParameters;

    auto modelParameter = std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>(initialParameters.size() + 1);
    modelParameter[0] = { 0.0, ParametricVolatility::ParameterCalibration::Implied };
    for (Size i = 0; i < initialParameters.size(); ++i) {
        BOOST_TEST_MESSAGE("Initial model parameter " << i << ": " << initialParameters[i]);
        modelParameter[i + 1] = {initialParameters[i], ParametricVolatility::ParameterCalibration::Calibrated};
    }
    auto modelParameters = buildModelParameterMap(marketSmiles, modelParameter);

    QuantLib::ext::shared_ptr<SsviParametricVolatility> svi;
    svi.reset(new SsviParametricVolatility(
        modelVariant,
        marketSmiles,
        ParametricVolatility::MarketModelType::Black76,
        ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.05, Actual365Fixed())),
        modelParameters,
        std::map<QuantLib::Real, QuantLib::Real>(),
        100,
        0.005,
        0.3
    ));

    for (Size i = 0; i < marketSmiles.size(); ++i) {
        checkSmileCalibrationQuality(svi, marketSmiles[i], config.quoteErrorThreshold, config.rmseThreshold);
    }

    checkStaticArbitrage(marketSmiles, svi, data.spot, "SSVI");
}

void testModelVariantCalibratedEssvi(const CommonDataSsvi& data, SviParametricVolatility::ModelVariant modelVariant) {
    const auto& marketSmiles = data.marketSmiles;
    const auto& config = ssviTestConfigs.at(modelVariant);

    const auto& initialParameters = config.initialParameters;

    auto modelParameter = std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>(initialParameters.size());
    for (Size i = 0; i < initialParameters.size(); ++i) {
        BOOST_TEST_MESSAGE("Initial model parameter " << i << ": " << initialParameters[i]);
        modelParameter[i] = {initialParameters[i], ParametricVolatility::ParameterCalibration::Calibrated};
    }
    auto modelParameters = buildModelParameterMap(marketSmiles, modelParameter);

    QuantLib::ext::shared_ptr<SviParametricVolatility> svi;
    if (modelVariant == SviParametricVolatility::ModelVariant::CorbettaEtAl2019Essvi) {
        svi.reset(new SsviParametricVolatilityRobust(
            modelVariant,
            marketSmiles,
            ParametricVolatility::MarketModelType::Black76,
            ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0, Actual365Fixed())),
            modelParameters,
            std::map<QuantLib::Real, QuantLib::Real>(),
            100,
            0.005,
            0.2,
            true // no arbitrage
        ));
    } else {
        svi.reset(new SsviParametricVolatilityGlobal(
            modelVariant,
            marketSmiles,
            ParametricVolatility::MarketModelType::Black76,
            ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0, Actual365Fixed())),
            modelParameters,
            std::map<QuantLib::Real, QuantLib::Real>(),
            100,
            0.005,
            0.2,
            true // no arbitrage
        ));
    }

    for (Size i = 0; i < marketSmiles.size(); ++i) {
        checkSmileCalibrationQuality(svi, marketSmiles[i], config.quoteErrorThreshold, config.rmseThreshold);
    }

    checkStaticArbitrage(marketSmiles, svi, data.spot, "ESSVI");
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(SviParametricVolatilityTest)

// Test raw -> variant -> raw round-trip for bijective 5-param SVI variants
auto rawRoundTripVariants = bdata::make({
    SviParametricVolatility::ModelVariant::Gatheral2004SviRaw,
    SviParametricVolatility::ModelVariant::Gatheral2004SviNatural
});

BOOST_DATA_TEST_CASE(testRawRoundTripConversion, rawRoundTripVariants, variant) {
    std::vector<Real> rawParams = {0.01, 0.1, 0.003, 0.007, 0.07}; // a, b, rho, m, sigma
    Real timeToExpiry = 1.0;

    auto variantParams = SviModelTraits::fromRawSvi(timeToExpiry, rawParams, variant);
    Real a, b, rho, m, sigma;
    std::tie(a, b, rho, m, sigma) = SviModelTraits::toRawSvi(timeToExpiry, variantParams, variant);

    BOOST_CHECK_CLOSE(a, rawParams[0], 1e-8);
    BOOST_CHECK_CLOSE(b, rawParams[1], 1e-8);
    BOOST_CHECK_CLOSE(rho, rawParams[2], 1e-8);
    BOOST_CHECK_CLOSE(m, rawParams[3], 1e-8);
    BOOST_CHECK_CLOSE(sigma, rawParams[4], 1e-8);
}

auto fixedVariants = bdata::make({
    SviParametricVolatility::ModelVariant::Gatheral2004SviRaw,
    SviParametricVolatility::ModelVariant::Gatheral2004SviNatural,
    SviParametricVolatility::ModelVariant::Gatheral2004SviJw,
    SviParametricVolatility::ModelVariant::Gatheral2012SsviHeston,
    SviParametricVolatility::ModelVariant::Gatheral2012SsviPowerLaw
});

BOOST_DATA_TEST_CASE(testFixed, fixedVariants, variant) {
    CommonData data;
    testModelVariantFixed(data, variant);
}

auto calibratedSviVariants = bdata::make({
    SviParametricVolatility::ModelVariant::Gatheral2004SviRaw,
    SviParametricVolatility::ModelVariant::Gatheral2004SviNatural,
    SviParametricVolatility::ModelVariant::Gatheral2004SviJw
});

BOOST_DATA_TEST_CASE(testCalibratedSvi, calibratedSviVariants, variant) {
    CommonData data;
    testModelVariantCalibrated(data, variant);
}

auto calibratedSsviVariants = bdata::make({
    SviParametricVolatility::ModelVariant::Gatheral2012SsviHeston,
    SviParametricVolatility::ModelVariant::Gatheral2012SsviPowerLaw
});

BOOST_DATA_TEST_CASE(testCalibratedSsvi, calibratedSsviVariants, variant) {
    CommonDataSsvi data;
    testModelVariantCalibratedSsvi(data, variant);
}

auto calibratedEssviVariants = bdata::make({
    SviParametricVolatility::ModelVariant::CorbettaEtAl2019Essvi,
    SviParametricVolatility::ModelVariant::Mingone2022EssviGJ,
    SviParametricVolatility::ModelVariant::Mingone2022EssviMM
});

BOOST_DATA_TEST_CASE(testCalibratedEssvi, calibratedEssviVariants, variant) {
    CommonDataSsvi data;
    testModelVariantCalibratedEssvi(data, variant);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
