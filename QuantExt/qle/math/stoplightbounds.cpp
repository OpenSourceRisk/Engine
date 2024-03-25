/*
 Copyright (C) 2017 Quaternion Risk Management Ltd.
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

#include <qle/math/stoplightbounds.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>

// fix for boost 1.64, see https://lists.boost.org/Archives/boost/2016/11/231756.php
#if BOOST_VERSION >= 106400
#include <boost/serialization/array_wrapper.hpp>
#endif
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/framework/accumulator_set.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/tail_quantile.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/math/distributions/binomial.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <algorithm>

using namespace QuantLib;
using namespace boost::accumulators;

namespace QuantExt {
namespace {
void checkMatrix(const Matrix& m) {
    Size n = m.rows();
    QL_REQUIRE(n > 0, "matrix is null");
    for (Size i = 0; i < n; ++i) {
        QL_REQUIRE(QuantLib::close_enough(m[i][i], 1.0),
                   "correlation matrix has non unit diagonal element at (" << i << "," << i << ")");
        for (Size j = 0; j < n; ++j) {
            QL_REQUIRE(QuantLib::close_enough(m[i][j], m[j][i]), "correlation matrix is not symmetric, for (i,j)=("
                                                           << i << "," << j << "), values are " << m[i][j] << " and "
                                                           << m[j][i]);
            QL_REQUIRE(m[i][j] >= -1.0 && m[i][j] <= 1.0,
                       "correlation matrix entry out of bounds at (" << i << "," << j << "), value is " << m[i][j]);
        }
    }
} // checkMatrix

static constexpr Size tab_size = 637;
static constexpr std::array<Size, tab_size> tab1_obs = {
    1,    2,    3,    4,    5,    6,    7,    8,    9,    10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
    20,   30,   40,   50,   60,   70,   80,   90,   100,  110,  120,  130,  140,  150,  160,  170,  180,  190,  200,
    210,  220,  230,  240,  250,  260,  270,  280,  290,  300,  310,  320,  330,  340,  350,  360,  370,  380,  390,
    400,  410,  420,  430,  440,  450,  460,  470,  480,  490,  500,  510,  520,  530,  540,  550,  560,  570,  580,
    590,  600,  610,  620,  630,  640,  650,  660,  670,  680,  690,  700,  710,  720,  730,  740,  750,  760,  770,
    780,  790,  800,  810,  820,  830,  840,  850,  860,  870,  880,  890,  900,  910,  920,  930,  940,  950,  960,
    970,  980,  990,  1000, 1010, 1020, 1030, 1040, 1050, 1060, 1070, 1080, 1090, 1100, 1110, 1120, 1130, 1140, 1150,
    1160, 1170, 1180, 1190, 1200, 1210, 1220, 1230, 1240, 1250, 1260, 1270, 1280, 1290, 1300, 1310, 1320, 1330, 1340,
    1350, 1360, 1370, 1380, 1390, 1400, 1410, 1420, 1430, 1440, 1450, 1460, 1470, 1480, 1490, 1500, 1510, 1520, 1530,
    1540, 1550, 1560, 1570, 1580, 1590, 1600, 1610, 1620, 1630, 1640, 1650, 1660, 1670, 1680, 1690, 1700, 1710, 1720,
    1730, 1740, 1750, 1760, 1770, 1780, 1790, 1800, 1810, 1820, 1830, 1840, 1850, 1860, 1870, 1880, 1890, 1900, 1910,
    1920, 1930, 1940, 1950, 1960, 1970, 1980, 1990, 2000, 2010, 2020, 2030, 2040, 2050, 2060, 2070, 2080, 2090, 2100,
    2110, 2120, 2130, 2140, 2150, 2160, 2170, 2180, 2190, 2200, 2210, 2220, 2230, 2240, 2250, 2260, 2270, 2280, 2290,
    2300, 2310, 2320, 2330, 2340, 2350, 2360, 2370, 2380, 2390, 2400, 2410, 2420, 2430, 2440, 2450, 2460, 2470, 2480,
    2490, 2500, 2510, 2520, 2530, 2540, 2550, 2560, 2570, 2580, 2590, 2600, 2610, 2620, 2630, 2640, 2650, 2660, 2670,
    2680, 2690, 2700, 2710, 2720, 2730, 2740, 2750, 2760, 2770, 2780, 2790, 2800, 2810, 2820, 2830, 2840, 2850, 2860,
    2870, 2880, 2890, 2900, 2910, 2920, 2930, 2940, 2950, 2960, 2970, 2980, 2990, 3000, 3010, 3020, 3030, 3040, 3050,
    3060, 3070, 3080, 3090, 3100, 3110, 3120, 3130, 3140, 3150, 3160, 3170, 3180, 3190, 3200, 3210, 3220, 3230, 3240,
    3250, 3260, 3270, 3280, 3290, 3300, 3310, 3320, 3330, 3340, 3350, 3360, 3370, 3380, 3390, 3400, 3410, 3420, 3430,
    3440, 3450, 3460, 3470, 3480, 3490, 3500, 3510, 3520, 3530, 3540, 3550, 3560, 3570, 3580, 3590, 3600, 3610, 3620,
    3630, 3640, 3650, 3660, 3670, 3680, 3690, 3700, 3710, 3720, 3730, 3740, 3750, 3760, 3770, 3780, 3790, 3800, 3810,
    3820, 3830, 3840, 3850, 3860, 3870, 3880, 3890, 3900, 3910, 3920, 3930, 3940, 3950, 3960, 3970, 3980, 3990, 4000,
    4010, 4020, 4030, 4040, 4050, 4060, 4070, 4080, 4090, 4100, 4110, 4120, 4130, 4140, 4150, 4160, 4170, 4180, 4190,
    4200, 4210, 4220, 4230, 4240, 4250, 4260, 4270, 4280, 4290, 4300, 4310, 4320, 4330, 4340, 4350, 4360, 4370, 4380,
    4390, 4400, 4410, 4420, 4430, 4440, 4450, 4460, 4470, 4480, 4490, 4500, 4510, 4520, 4530, 4540, 4550, 4560, 4570,
    4580, 4590, 4600, 4610, 4620, 4630, 4640, 4650, 4660, 4670, 4680, 4690, 4700, 4710, 4720, 4730, 4740, 4750, 4760,
    4770, 4780, 4790, 4800, 4810, 4820, 4830, 4840, 4850, 4860, 4870, 4880, 4890, 4900, 4910, 4920, 4930, 4940, 4950,
    4960, 4970, 4980, 4990, 5000, 5010, 5020, 5030, 5040, 5050, 5060, 5070, 5080, 5090, 5100, 5110, 5120, 5130, 5140,
    5150, 5160, 5170, 5180, 5190, 5200, 5210, 5220, 5230, 5240, 5250, 5260, 5270, 5280, 5290, 5300, 5310, 5320, 5330,
    5340, 5350, 5360, 5370, 5380, 5390, 5400, 5410, 5420, 5430, 5440, 5450, 5460, 5470, 5480, 5490, 5500, 5510, 5520,
    5530, 5540, 5550, 5560, 5570, 5580, 5590, 5600, 5610, 5620, 5630, 5640, 5650, 5660, 5670, 5680, 5690, 5700, 5710,
    5720, 5730, 5740, 5750, 5760, 5770, 5780, 5790, 5800, 5810, 5820, 5830, 5840, 5850, 5860, 5870, 5880, 5890, 5900,
    5910, 5920, 5930, 5940, 5950, 5960, 5970, 5980, 5990, 6000, 6010, 6020, 6030, 6040, 6050, 6060, 6070, 6080, 6090,
    6100, 6110, 6120, 6130, 6140, 6150, 6160, 6170, 6180, 6190};

static constexpr std::array<Size, tab_size> tab1_green = {
    // generated using generateStopLightBoundTable(obs, {0.95,0.9999}, 1E8, 42, 10, 0.99)
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  2,  3,  3,  4,  4,  4,  5,
    5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  10, 10, 10, 10, 10, 11, 11, 11, 11,
    11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16,
    16, 16, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 21, 21,
    21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25,
    25, 25, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29,
    30, 30, 30, 30, 30, 30, 30, 31, 31, 31, 31, 31, 31, 31, 32, 32, 32, 32, 32, 32, 32, 33, 33, 33, 33, 33, 33, 33, 34,
    34, 34, 34, 34, 34, 34, 34, 35, 35, 35, 35, 35, 35, 35, 36, 36, 36, 36, 36, 36, 36, 37, 37, 37, 37, 37, 37, 37, 38,
    38, 38, 38, 38, 38, 38, 38, 39, 39, 39, 39, 39, 39, 39, 40, 40, 40, 40, 40, 40, 40, 41, 41, 41, 41, 41, 41, 41, 41,
    42, 42, 42, 42, 42, 42, 42, 43, 43, 43, 43, 43, 43, 43, 43, 44, 44, 44, 44, 44, 44, 44, 45, 45, 45, 45, 45, 45, 45,
    45, 46, 46, 46, 46, 46, 46, 46, 47, 47, 47, 47, 47, 47, 47, 47, 48, 48, 48, 48, 48, 48, 48, 48, 49, 49, 49, 49, 49,
    49, 49, 50, 50, 50, 50, 50, 50, 50, 50, 51, 51, 51, 51, 51, 51, 51, 51, 52, 52, 52, 52, 52, 52, 52, 53, 53, 53, 53,
    53, 53, 53, 53, 54, 54, 54, 54, 54, 54, 54, 54, 55, 55, 55, 55, 55, 55, 55, 55, 56, 56, 56, 56, 56, 56, 56, 57, 57,
    57, 57, 57, 57, 57, 57, 58, 58, 58, 58, 58, 58, 58, 58, 59, 59, 59, 59, 59, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60,
    60, 61, 61, 61, 61, 61, 61, 61, 62, 62, 62, 62, 62, 62, 62, 62, 63, 63, 63, 63, 63, 63, 63, 63, 64, 64, 64, 64, 64,
    64, 64, 64, 65, 65, 65, 65, 65, 65, 65, 65, 66, 66, 66, 66, 66, 66, 66, 66, 67, 67, 67, 67, 67, 67, 67, 67, 68, 68,
    68, 68, 68, 68, 68, 68, 69, 69, 69, 69, 69, 69, 69, 69, 70, 70, 70, 70, 70, 70, 70, 70, 71, 71, 71, 71, 71, 71, 71,
    71, 72, 72, 72, 72, 72, 72, 72, 72, 73, 73, 73, 73, 73, 73, 73, 73, 74, 74, 74, 74, 74, 74, 74, 74, 75, 75, 75, 75,
    75, 75, 75, 75, 76, 76, 76, 76, 76, 76, 76, 76, 77, 77, 77, 77, 77, 77, 77, 77, 77, 78, 78, 78, 78, 78, 78, 78, 78,
    79, 79, 79, 79, 79, 79, 79, 79, 80, 80, 80, 80, 80, 80, 80, 80, 81, 81, 81, 81, 81, 81, 81, 81, 82, 82, 82, 82, 82,
    82, 82, 82, 83, 83, 83, 83, 83, 83, 83, 83, 84, 84, 84, 84, 84, 84, 84, 84, 85, 85, 85, 85, 85, 85, 85, 85, 85, 86,
    86, 86, 86, 86, 86, 86, 86, 87, 87, 87, 87, 87, 87, 87, 87, 88, 88, 88, 88, 88, 88, 88, 88, 89, 89, 89, 89, 89};

static constexpr std::array<Size, tab_size> tab1_amber = {
    // generated using generateStopLightBoundTable(obs, {0.95,0.9999}, 1E8, 42, 10, 0.99)
    0,   1,   2,   3,   4,   5,   6,   7,   8,   8,   9,   9,   9,   10,  10,  10,  10,  11,  11,  11,  12,  14,  14,
    15,  16,  17,  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  22,  23,  23,  24,  24,  24,  25,  25,  25,
    26,  26,  26,  27,  27,  27,  28,  28,  28,  29,  29,  29,  30,  30,  30,  30,  31,  31,  31,  32,  32,  32,  32,
    33,  33,  33,  34,  34,  34,  34,  35,  35,  35,  35,  36,  36,  36,  37,  37,  37,  37,  37,  38,  38,  38,  39,
    39,  39,  39,  40,  40,  40,  40,  41,  41,  41,  41,  41,  42,  42,  42,  42,  42,  43,  43,  43,  43,  44,  44,
    44,  44,  44,  45,  45,  45,  45,  46,  46,  46,  46,  46,  47,  47,  47,  47,  48,  48,  48,  48,  48,  49,  49,
    49,  49,  50,  50,  50,  50,  50,  51,  51,  51,  51,  51,  52,  52,  52,  52,  52,  53,  53,  53,  53,  53,  54,
    54,  54,  54,  54,  55,  55,  55,  55,  55,  56,  56,  56,  56,  57,  57,  57,  57,  57,  57,  58,  58,  58,  58,
    58,  58,  59,  59,  59,  59,  60,  60,  60,  60,  60,  61,  61,  61,  61,  61,  62,  62,  62,  62,  62,  63,  63,
    63,  63,  63,  63,  63,  64,  64,  64,  64,  65,  65,  65,  65,  65,  66,  66,  66,  66,  67,  67,  67,  67,  67,
    67,  68,  68,  68,  68,  68,  68,  69,  69,  69,  69,  69,  69,  70,  70,  70,  70,  70,  71,  71,  71,  71,  71,
    71,  72,  72,  72,  72,  72,  72,  73,  73,  73,  73,  73,  74,  74,  74,  74,  74,  74,  75,  75,  75,  75,  75,
    76,  76,  76,  76,  76,  76,  77,  77,  77,  77,  77,  77,  78,  78,  78,  78,  78,  78,  79,  79,  79,  79,  79,
    80,  80,  80,  80,  80,  80,  81,  81,  81,  81,  81,  81,  82,  82,  82,  82,  82,  82,  83,  83,  83,  83,  83,
    83,  84,  84,  84,  84,  84,  85,  85,  85,  85,  85,  85,  86,  86,  86,  86,  86,  86,  87,  87,  87,  87,  87,
    87,  88,  88,  88,  88,  88,  88,  89,  89,  89,  89,  89,  89,  90,  90,  90,  90,  90,  90,  91,  91,  91,  91,
    91,  91,  91,  92,  92,  92,  92,  92,  92,  93,  93,  93,  93,  93,  93,  94,  94,  94,  94,  94,  94,  95,  95,
    95,  95,  95,  95,  96,  96,  96,  96,  96,  96,  96,  97,  97,  97,  97,  97,  97,  98,  98,  98,  98,  98,  98,
    99,  99,  99,  99,  99,  99,  100, 100, 100, 100, 100, 100, 100, 101, 101, 101, 101, 101, 101, 102, 102, 102, 102,
    102, 102, 103, 103, 103, 103, 103, 103, 104, 104, 104, 104, 104, 104, 105, 105, 105, 105, 105, 105, 105, 106, 106,
    106, 106, 106, 106, 106, 107, 107, 107, 107, 107, 107, 108, 108, 108, 108, 108, 108, 109, 109, 109, 109, 109, 109,
    109, 110, 110, 110, 110, 110, 110, 111, 111, 111, 111, 111, 111, 112, 112, 112, 112, 112, 112, 112, 113, 113, 113,
    113, 113, 113, 114, 114, 114, 114, 114, 114, 114, 115, 115, 115, 115, 115, 115, 116, 116, 116, 116, 116, 116, 116,
    117, 117, 117, 117, 117, 117, 118, 118, 118, 118, 118, 118, 118, 119, 119, 119, 119, 119, 119, 120, 120, 120, 120,
    120, 120, 120, 121, 121, 121, 121, 121, 121, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 124,
    124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127,
    127, 127, 127, 128, 128, 128, 128, 128, 128, 129, 129, 129, 129, 129, 129, 129, 130, 130, 130, 130, 130, 130, 130,
    131, 131, 131, 131, 131, 131, 132, 132, 132, 132, 132, 132, 132, 133, 133, 133};

} // anonymous namespace

std::vector<Size> stopLightBoundsTabulated(const std::vector<Real>& stopLightP, const Size observations,
                                           const Size numberOfDays, const Real p) {

    if (stopLightP.size() == 2 && QuantLib::close_enough(stopLightP[0], 0.95) &&
        QuantLib::close_enough(stopLightP[1], 0.9999) && numberOfDays == 10 && observations >= 1 &&
        observations <= (tab1_obs.back() + 9) && QuantLib::close_enough(p, 0.99)) {
        Size idx = std::upper_bound(tab1_obs.begin(), tab1_obs.end(), observations) - tab1_obs.begin();
        QL_REQUIRE(idx > 0 && idx <= tab1_green.size() && idx <= tab1_amber.size(),
                   "stopLightBoundsTabulated: unexpected index 0");
        return {tab1_green[idx - 1], tab1_amber[idx - 1]};
    } else {
        QL_FAIL(
            "stopLightBoundsTabulated: no tabulated values found for sl-p = "
            << boost::join(stopLightP | boost::adaptors::transformed([](double x) { return std::to_string(x); }), ",")
            << ", obs = " << observations << ", numberOfDays = " << numberOfDays << ", p = " << p);
    }
}

std::vector<Size> stopLightBounds(const std::vector<Real>& stopLightP, const Size observations, const Size numberOfDays,
                                  const Real p, const Size numberOfPortfolios, const Matrix& correlation,
                                  const Size samples, const Size seed, const SalvagingAlgorithm::Type salvaging,
                                  const Size exceptions, Real* cumProb) {
    checkMatrix(correlation);
    Size r = correlation.rows();

    QL_REQUIRE(stopLightP.size() > 0, "stopLightBounds: stopLightP is empty");
    QL_REQUIRE(numberOfDays > 0, "stopLightBounds: numberOfDays must be greater than zero");
    QL_REQUIRE(numberOfPortfolios > 0, "stopLightBounds: numberOfPortfolios must be greater than zero");
    QL_REQUIRE(numberOfPortfolios == r, "stopLightBounds: numberOfPortfolios ("
                                            << numberOfPortfolios << ") must match correlation matrix dimension (" << r
                                            << "x" << r);
    QL_REQUIRE(samples > 0, "stopLightBounds: samples must be greater than zero");
    QL_REQUIRE(p > 0.0, "stopLightBounds: p must be greater than zero");
    if (exceptions != Null<Size>()) {
        QL_REQUIRE(cumProb != nullptr, "stopLightBounds: cumProb is a null pointer");
        *cumProb = 0.0;
    }

    Matrix pseudoRoot = pseudoSqrt(correlation, salvaging);
    Size len = observations + (numberOfDays - 1);
    auto sgen = PseudoRandom::make_sequence_generator(len * r, seed);
    // auto sgen = LowDiscrepancy::make_sequence_generator(len * r, seed);
    Real h = InverseCumulativeNormal()(p) * std::sqrt(static_cast<Real>(numberOfDays));
    Real minP = *std::min_element(stopLightP.begin(), stopLightP.end());
    Size cache = Size(std::floor(static_cast<Real>(samples) * (1.0 - minP) + 0.5)) + 2;
    accumulator_set<Real, stats<tag::tail_quantile<right>>> acc(tag::tail<right>::cache_size = cache);
    for (Size i = 0; i < samples; ++i) {
        auto seq = sgen.nextSequence().value;
        Size exCount = 0;
        for (Size rr = 0; rr < r; ++rr) {
            Array oneDayPls(len, 0.0);
            for (Size l = 0; l < len; ++l) {
                for (Size kk = 0; kk < r; ++kk) {
                    oneDayPls[l] += pseudoRoot[rr][kk] * seq[len * kk + l];
                }
            }
            // compute the 10d PL only once ...
            Real pl = 0.0;
            for (Size dd = 0; dd < numberOfDays; ++dd) {
                pl += oneDayPls[dd];
            }
            if (pl > h)
                ++exCount;
            for (Size l = 0; l < observations - 1; ++l) {
                // and only correct for the tail and head
                pl += oneDayPls[l + numberOfDays] - oneDayPls[l];
                if (pl > h)
                    ++exCount;
            }
        } // for rr
        acc(static_cast<Real>(exCount));
        if (exceptions != Null<Size>() && exCount <= exceptions) {
            *cumProb += 1.0 / static_cast<Real>(samples);
        }
    } // for samples
    std::vector<Size> res;
    for (auto const p : stopLightP) {
        Size tmp = static_cast<Size>(quantile(acc, quantile_probability = p));
        res.push_back(tmp > 0 ? tmp - 1 : 0);
    }
    return res;
} // stopLightBounds (overlapping, correlated)

std::vector<Size> stopLightBounds(const std::vector<Real>& stopLightP, const Size observations, const Real p,
                                  const Size exceptions, Real* cumProb) {

    QL_REQUIRE(stopLightP.size() > 0, "stopLightBounds: stopLightP is empty");
    QL_REQUIRE(p > 0.0, "stopLightBounds: p must be greater than zero");
    if (exceptions != Null<Size>()) {
        QL_REQUIRE(cumProb != nullptr, "stopLightBounds: cumProb is a null pointer");
        *cumProb = 0.0;
    }
    boost::math::binomial_distribution<Real> b((Real)observations, (Real)(1.0) - p);
    std::vector<Size> res;
    for (auto const p : stopLightP) {
        res.push_back(std::max(static_cast<Size>(boost::math::quantile(b, p)), (Size)(1)) - 1);
    }
    if (exceptions != Null<Size>()) {
        *cumProb = boost::math::cdf(b, exceptions);
    }
    return res;
} // stopLightBounds (iid)

std::vector<std::pair<Size, std::vector<Size>>> generateStopLightBoundTable(const std::vector<Size>& observations,
                                                                            const std::vector<Real>& stopLightP,
                                                                            const Size samples, const Size seed,
                                                                            const Size numberOfDays, const Real p) {

    QL_REQUIRE(!observations.empty(), "generateStopLightBoundTable(): no observations given");
    QL_REQUIRE(!stopLightP.empty(), "generateStopLightBoundTable(): stopLightP is empty");
    QL_REQUIRE(numberOfDays > 0, "generateStopLightBoundTable(): numberOfDays must be greater than zero");
    QL_REQUIRE(samples > 0, "generateStopLightBoundTable(): samples must be greater than zero");
    QL_REQUIRE(p > 0.0, "generateStopLightBoundTable():: p must be greater than zero");

    for (Size i = 0; i < observations.size() - 1; ++i) {
        QL_REQUIRE(observations[i] > 0, "generateStopLightBoundTable(): observations must be positive, got 0 at " << i);
        QL_REQUIRE(observations[i] < observations[i + 1],
                   "generateStopLightBoundTable(): observations must be increasing, got "
                       << observations[i] << " at " << i << " and " << observations[i + 1] << " at " << i + 1);
    }

    Size len = observations.back() + (numberOfDays - 1);
    auto sgen = PseudoRandom::make_sequence_generator(len, seed);
    Real h = InverseCumulativeNormal()(p) * std::sqrt(static_cast<Real>(numberOfDays));

    /*! populate matrix where the rows correspond to the observations and column j
       contain P(exceptions == j) */
    // heuristic... is this ok, too low (will throw an excetpion below), too high (wastes memory)?
    Size cols;
    if (observations.back() <= 100)
        cols = observations.back() + 1;
    else if (observations.back() <= 500)
        cols = observations.back() / 5;
    else
        cols = observations.back() / 10;

    Matrix cumProb(observations.size(), cols, 0.0);

    for (Size i = 0; i < samples; ++i) {
        auto seq = sgen.nextSequence().value;
        Size exCount = 0, obsIdx = 0;
        Real pl = 0.0;
        for (Size l = 0; l < observations.back(); ++l) {
            if (l == 0) {
                // compute the 10d PL only once ...
                for (Size dd = 0; dd < numberOfDays; ++dd) {
                    pl += seq[dd];
                }
            } else {
                // and only correct for the tail and head
                pl += seq[l - 1 + numberOfDays] - seq[l - 1];
            }
            if (pl > h)
                ++exCount;
            if (l == observations[obsIdx] - 1) {
                if (exCount < cols)
                    cumProb(obsIdx, exCount) += 1.0 / static_cast<Real>(samples);
                obsIdx++;
            }
        }
    }

    // compute result table
    std::vector<std::pair<Size, std::vector<Size>>> result;
    for (Size i = 0; i < observations.size(); ++i) {
        Real P = 0.0;
        Size idx = 0;
        std::vector<Size> b;
        for (Size j = 0; j < cols && idx < stopLightP.size(); ++j) {
            P += cumProb(i, j);
            while (idx < stopLightP.size() && (P >= stopLightP[idx] || QuantLib::close_enough(P, stopLightP[idx]))) {
                b.push_back(std::max<Size>(j, 1) - 1);
                idx++;
            }
        }
        QL_REQUIRE(b.size() == stopLightP.size(),
                   "generateStopLightBoundTable(): could not determine bound for observations = "
                       << observations[i] << " and stopLightP = " << stopLightP[b.size()]
                       << " - try to increase number of cols in matrix cumProb");
        result.push_back(std::make_pair(observations[i], b));
    }

    return result;
}

} // namespace QuantExt
