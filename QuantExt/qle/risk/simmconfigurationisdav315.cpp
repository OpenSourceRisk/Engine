/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/risk/simmconfigurationisdav315.hpp>

#include <boost/assign/std/vector.hpp>

using namespace boost::assign;

namespace QuantExt {

    namespace {
        const static Real ir_curve_rw[3][13] = {
            {77.0, 77.0, 77.0, 64.0, 58.0, 49.0, 47.0, 47.0, 45.0, 45.0, 48.0, 56.0, 32.0},
            {10.0, 10.0, 10.0, 10.0, 13.0, 16.0, 18.0, 20.0, 25.0, 22.0, 22.0, 23.0, 32.0},
            {89.0, 89.0, 89.0, 94.0, 104.0, 99.0, 96.0, 99.0, 87.0, 97.0, 97.0, 98.0, 32.0}};
        const static Real creditq_rw[13] = {97.0,  110.0, 73.0,  65.0,  52.0,  39.0, 198.0,
                                            638.0, 210.0, 375.0, 240.0, 152.0, 638.0};
        const static Real creditnq_rw[3] = {169.0, 1646.0, 1646.0};
        const static Real equity_rw[12] = {22.0, 28.0, 28.0, 25.0, 18.0, 20.0, 24.0, 23.0, 26.0, 27.0, 15.0, 28.0};
        const static Real commodity_rw[16] = {9.0,  19.0, 18.0, 13.0, 24.0, 17.0, 21.0, 35.0,
                                              20.0, 50.0, 21.0, 19.0, 17.0, 15.0, 8.0,  50.0};
        const static Real ir_tenor_corr[13][13] = {
            {1.0, 1.0, 1.0, 0.782, 0.618, 0.498, 0.438, 0.361, 0.27, 0.196, 0.174, 0.129, 0.33},
            {1.0, 1.0, 1.0, 0.782, 0.618, 0.498, 0.438, 0.361, 0.27, 0.196, 0.174, 0.129, 0.33},
            {1.0, 1.0, 1.0, 0.782, 0.618, 0.498, 0.438, 0.361, 0.27, 0.196, 0.174, 0.129, 0.33},
            {0.782, 0.782, 0.782, 1.0, 0.84, 0.739, 0.667, 0.569, 0.444, 0.375, 0.349, 0.296, 0.33},
            {0.618, 0.618, 0.618, 0.84, 1.0, 0.917, 0.859, 0.757, 0.626, 0.555, 0.526, 0.471, 0.33},
            {0.498, 0.498, 0.498, 0.739, 0.917, 1.0, 0.976, 0.895, 0.749, 0.69, 0.66, 0.602, 0.33},
            {0.438, 0.438, 0.438, 0.667, 0.859, 0.976, 1.0, 0.958, 0.831, 0.779, 0.746, 0.69, 0.33},
            {0.361, 0.361, 0.361, 0.569, 0.757, 0.895, 0.958, 1.0, 0.925, 0.893, 0.859, 0.812, 0.33},
            {0.27, 0.27, 0.27, 0.444, 0.626, 0.749, 0.831, 0.925, 1.0, 0.98, 0.961, 0.931, 0.33},
            {0.196, 0.196, 0.196, 0.375, 0.555, 0.690, 0.779, 0.893, 0.98, 1.0, 0.989, 0.97, 0.33},
            {0.174, 0.174, 0.174, 0.349, 0.526, 0.66, 0.746, 0.859, 0.961, 0.989, 1.0, 0.988, 0.33},
            {0.129, 0.129, 0.129, 0.296, 0.471, 0.602, 0.69, 0.812, 0.931, 0.97, 0.988, 1.0, 0.33},
            {0.33, 0.33, 0.33, 0.33, 0.33, 0.33, 0.33, 0.33, 0.33, 0.33, 0.33, 0.33, 1.0}};
        const static Real crq_bucket_corr[12][12] = {
            {1.0, 0.51, 0.47, 0.49, 0.46, 0.47, 0.41, 0.36, 0.45, 0.47, 0.47, 0.43},
            {0.51, 1.0, 0.52, 0.52, 0.49, 0.52, 0.37, 0.41, 0.51, 0.50, 0.51, 0.46},
            {0.47, 0.52, 1.0, 0.54, 0.51, 0.55, 0.37, 0.37, 0.51, 0.49, 0.50, 0.47},
            {0.49, 0.52, 0.54, 1.0, 0.53, 0.56, 0.36, 0.37, 0.52, 0.51, 0.51, 0.46},
            {0.46, 0.49, 0.51, 0.53, 1.0, 0.54, 0.35, 0.35, 0.49, 0.48, 0.50, 0.44},
            {0.47, 0.52, 0.55, 0.56, 0.54, 1.0, 0.37, 0.37, 0.52, 0.49, 0.51, 0.48},
            {0.41, 0.37, 0.37, 0.36, 0.35, 0.37, 1.0, 0.29, 0.36, 0.34, 0.36, 0.36},
            {0.36, 0.41, 0.37, 0.37, 0.35, 0.37, 0.29, 1.0, 0.37, 0.36, 0.37, 0.33},
            {0.45, 0.51, 0.51, 0.52, 0.49, 0.52, 0.36, 0.37, 1.0, 0.49, 0.50, 0.46},
            {0.47, 0.50, 0.49, 0.51, 0.48, 0.49, 0.34, 0.36, 0.49, 1.0, 0.49, 0.46},
            {0.47, 0.51, 0.50, 0.51, 0.50, 0.51, 0.36, 0.37, 0.50, 0.49, 1.0, 0.46},
            {0.43, 0.46, 0.47, 0.46, 0.44, 0.48, 0.36, 0.33, 0.46, 0.46, 0.46, 1.0}};
        const static Real eq_bucket_corr[11][11] = {{1.0, 0.17, 0.18, 0.16, 0.08, 0.10, 0.10, 0.11, 0.16, 0.08, 0.18},
                                                    {0.17, 1.0, 0.24, 0.19, 0.07, 0.10, 0.09, 0.10, 0.19, 0.07, 0.18},
                                                    {0.18, 0.24, 1.0, 0.21, 0.09, 0.12, 0.13, 0.13, 0.20, 0.10, 0.24},
                                                    {0.16, 0.19, 0.21, 1.0, 0.13, 0.17, 0.16, 0.17, 0.20, 0.13, 0.30},
                                                    {0.08, 0.07, 0.09, 0.13, 1.0, 0.28, 0.24, 0.28, 0.10, 0.23, 0.38},
                                                    {0.10, 0.10, 0.12, 0.17, 0.28, 1.0, 0.3, 0.33, 0.13, 0.26, 0.45},
                                                    {0.10, 0.09, 0.13, 0.16, 0.24, 0.30, 1.0, 0.29, 0.13, 0.25, 0.42},
                                                    {0.11, 0.10, 0.13, 0.17, 0.28, 0.33, 0.29, 1.0, 0.14, 0.27, 0.45},
                                                    {0.16, 0.19, 0.20, 0.2, 0.1, 0.13, 0.13, 0.14, 1.0, 0.11, 0.25},
                                                    {0.08, 0.07, 0.10, 0.13, 0.23, 0.26, 0.25, 0.27, 0.11, 1.0, 0.34},
                                                    {0.18, 0.18, 0.24, 0.30, 0.38, 0.45, 0.42, 0.45, 0.25, 0.34, 1.0}};
        const static Real com_bucket_corr[16][16] = {
            {1.0, 0.11, 0.16, 0.13, 0.10, 0.06, 0.20, 0.05, 0.17, 0.03, 0.18, 0.09, 0.1, 0.05, 0.04, 0.0},
            {0.11, 1.0, 0.95, 0.95, 0.93, 0.15, 0.27, 0.19, 0.20, 0.14, 0.30, 0.31, 0.26, 0.26, 0.12, 0.0},
            {0.16, 0.95, 1.0, 0.92, 0.90, 0.17, 0.24, 0.14, 0.17, 0.12, 0.32, 0.26, 0.16, 0.22, 0.12, 0.0},
            {0.13, 0.95, 0.92, 1.0, 0.90, 0.18, 0.26, 0.08, 0.17, 0.08, 0.31, 0.25, 0.15, 0.20, 0.09, 0.0},
            {0.10, 0.93, 0.90, 0.90, 1.0, 0.18, 0.37, 0.13, 0.30, 0.21, 0.34, 0.32, 0.27, 0.29, 0.12, 0.0},
            {0.06, 0.15, 0.17, 0.18, 0.18, 1.0, 0.07, 0.62, 0.03, 0.15, 0.0, 0.0, 0.23, 0.15, 0.07, 0.0},
            {0.20, 0.27, 0.24, 0.26, 0.37, 0.07, 1.0, 0.07, 0.66, 0.20, 0.06, 0.06, 0.12, 0.09, 0.09, 0.0},
            {0.05, 0.19, 0.14, 0.08, 0.13, 0.62, 0.07, 1.0, 0.09, 0.12, -0.01, 0.0, 0.18, 0.11, 0.04, 0.0},
            {0.17, 0.20, 0.17, 0.17, 0.30, 0.03, 0.66, 0.09, 1.0, 0.12, 0.1, 0.06, 0.12, 0.1, 0.1, 0.0},
            {0.03, 0.14, 0.12, 0.08, 0.21, 0.15, 0.2, 0.12, 0.12, 1.0, 0.1, 0.07, 0.09, 0.1, 0.16, 0.0},
            {0.18, 0.3, 0.32, 0.31, 0.34, 0.0, 0.06, -0.01, 0.10, 0.10, 1.0, 0.46, 0.2, 0.26, 0.18, 0.0},
            {0.09, 0.31, 0.26, 0.25, 0.32, 0.0, 0.06, 0.0, 0.06, 0.07, 0.46, 1.0, 0.25, 0.23, 0.14, 0.0},
            {0.1, 0.26, 0.16, 0.15, 0.27, 0.23, 0.12, 0.18, 0.12, 0.09, 0.20, 0.25, 1.0, 0.29, 0.06, 0.0},
            {0.05, 0.26, 0.22, 0.2, 0.29, 0.15, 0.09, 0.11, 0.10, 0.10, 0.26, 0.23, 0.29, 1.0, 0.15, 0.0},
            {0.04, 0.12, 0.12, 0.09, 0.12, 0.07, 0.09, 0.04, 0.10, 0.16, 0.18, 0.14, 0.06, 0.15, 1.0, 0.0},
            {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0}};
        const static Real equity_inbucket_corr[12] = {0.14, 0.24, 0.25, 0.2,  0.26, 0.34,
                                                      0.33, 0.34, 0.21, 0.24, 0.63, 0.0};
        const static Real com_inbucket_corr[16] = {0.71, 0.92, 0.97, 0.97, 0.99, 0.98, 1.0,  0.69,
                                                   0.47, 0.01, 0.67, 0.70, 0.68, 0.22, 0.50, 0.0};
        const static Real riskclass_corr[6][6] = {
            {1.0, 0.09, 0.1, 0.18, 0.32, 0.27},  {0.09, 1.0, 0.24, 0.58, 0.34, 0.29},
            {0.1, 0.24, 1.0, 0.23, 0.24, 0.12},  {0.18, 0.58, 0.23, 1.0, 0.26, 0.31},
            {0.32, 0.34, 0.24, 0.26, 1.0, 0.37}, {0.27, 0.29, 0.12, 0.31, 0.37, 1.0}};
        // the last three numbers are not explicitly given in the methodology paper, 12a
        // the tenor structure is that of irTenor / volTenor below
        const static Real curvature_weight[12] = {0.5,   0.23,  0.077, 0.038,  0.019,  0.01,
                                                  0.006, 0.004, 0.002, 0.0013, 0.0010, 0.0006};

    } // anonymous namespace

    SimmConfiguration_ISDA_V315::SimmConfiguration_ISDA_V315()
        : SimmConfiguration(), name_("SIMM ISDA V315 (7 April 2016)") {
        std::vector<string> empty(1, "");
        // buckets
        std::vector<string> bucketsIRCurve, bucketsCreditQ, bucketsCreditNonQ, bucketsEquity, bucketsCommodity;
        bucketsIRCurve += "1", "2", "3";
        bucketsCreditQ += "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Residual";
        bucketsCreditNonQ += "1", "2", "Residual";
        bucketsEquity += "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "Residual";
        bucketsCommodity += "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16";
        buckets_[Risk_IRCurve] = bucketsIRCurve;
        buckets_[Risk_CreditQ] = bucketsCreditQ;
        buckets_[Risk_CreditVol] = bucketsCreditQ;
        buckets_[Risk_CreditNonQ] = bucketsCreditNonQ;
        buckets_[Risk_CreditVolNonQ] = bucketsCreditNonQ;
        buckets_[Risk_Equity] = bucketsEquity;
        buckets_[Risk_EquityVol] = bucketsEquity;
        buckets_[Risk_Commodity] = bucketsCommodity;
        buckets_[Risk_CommodityVol] = bucketsCommodity;
        buckets_[Risk_IRVol] = empty;
        buckets_[Risk_Inflation] = empty;
        buckets_[Risk_FX] = empty;
        buckets_[Risk_FXVol] = empty;

        // label1
        std::vector<string> irTenor, crTenor, volTenor;
        irTenor += "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y", "INF";
        crTenor += "1y", "2y", "3y", "5y", "10y";
        volTenor += "2w", "1m", "3m", "6m", "1y", "2y", "3y", "5y", "10y", "15y", "20y", "30y";
        labels1_[Risk_IRCurve] = irTenor;
        labels1_[Risk_IRVol] = volTenor;
        labels1_[Risk_CreditQ] = crTenor;
        labels1_[Risk_CreditVol] = volTenor;
        labels1_[Risk_CreditNonQ] = crTenor;
        labels1_[Risk_CreditVolNonQ] = volTenor;
        labels1_[Risk_Equity] = empty;
        labels1_[Risk_EquityVol] = volTenor;
        labels1_[Risk_Commodity] = empty;
        labels1_[Risk_CommodityVol] = volTenor;
        labels1_[Risk_Inflation] = empty;
        labels1_[Risk_FX] = empty;
        labels1_[Risk_FXVol] = volTenor;

        // label2
        std::vector<string> subcurve, sec;
        subcurve += "OIS", "Libor1m", "Libor3m", "Libor6m", "Libor12m", "Prime";
        sec += "", "Sec";
        labels2_[Risk_IRCurve] = subcurve;
        labels2_[Risk_IRVol] = empty;
        labels2_[Risk_CreditQ] = sec;
        labels2_[Risk_CreditVol] = empty;
        labels2_[Risk_CreditNonQ] = empty;
        labels2_[Risk_CreditVolNonQ] = empty;
        labels2_[Risk_Equity] = empty;
        labels2_[Risk_EquityVol] = empty;
        labels2_[Risk_Commodity] = empty;
        labels2_[Risk_CommodityVol] = empty;
        labels2_[Risk_Inflation] = empty;
        labels2_[Risk_FX] = empty;
        labels2_[Risk_FXVol] = empty;

        check();
    }

    Real SimmConfiguration_ISDA_V315::weight(const RiskType t, const Size bucketIdx, const Size label1Idx) const {
        QL_REQUIRE(buckets(t).size() == 0 || bucketIdx < buckets(t).size(),
                   "weight: bucketIdx (" << bucketIdx << ") out of range 0.." << (buckets(t).size() - 1)
                                         << " for risk type " << t);
        QL_REQUIRE(labels1(t).size() == 0 || label1Idx < labels1(t).size(),
                   "weight: label1Idx (" << label1Idx << ") out of range 0..." << (labels1(t).size() - 1)
                                         << " for risk type " << t);
        switch (t) {
        case Risk_IRCurve:
            return ir_curve_rw[bucketIdx][label1Idx];
        case Risk_IRVol:
            return 0.21;
        case Risk_CreditQ:
            return creditq_rw[bucketIdx];
        case Risk_CreditNonQ:
            return creditnq_rw[bucketIdx];
        case Risk_CreditVol:
            return 0.35;
        case Risk_CreditVolNonQ:
            return 0.35;
        case Risk_Equity:
            return equity_rw[bucketIdx];
        case Risk_EquityVol:
            return 0.21;
        case Risk_Commodity:
            return commodity_rw[bucketIdx];
        case Risk_CommodityVol:
            return 0.36;
        case Risk_FX:
            return 7.9;
        case Risk_FXVol:
            return 0.21;
        default:
            QL_FAIL("invalid risk type " << t);
        }
    }

    Real SimmConfiguration_ISDA_V315::curvatureWeight(const Size label1Idx) const {
        QL_REQUIRE(label1Idx < 12, "curvatureWeight: label1Idx " << label1Idx << " must be in 0...11");
        return curvature_weight[label1Idx];
    }

    Real SimmConfiguration_ISDA_V315::correlationLabels1(const RiskType t, const Size i, const Size j) const {
        QL_REQUIRE(i < labels1(t).size(), "correlation labels1: label1Idx (" << i << ") out of range 0..."
                                                                             << (labels1(t).size() - 1));
        QL_REQUIRE(j < labels1(t).size(), "correlation labels2: label1Idx (" << j << ") out of range 0..."
                                                                             << (labels1(t).size() - 1));
        switch (t) {
        case Risk_IRCurve:
        case Risk_IRVol:
            return ir_tenor_corr[i][j];
        default:
            return 1.0;
        }
    }

    Real SimmConfiguration_ISDA_V315::correlationLabels2(const RiskType t, const Size i, const Size j) const {
        switch (t) {
        case Risk_IRCurve:
            return i == j ? 1.0 : 0.982;
        default:
            return 1.0;
        }
    }

    Real SimmConfiguration_ISDA_V315::correlationBuckets(const RiskType t, const Size i, const Size j) const {
        Size resBuck = residualBucket(t) != Null<Size>() ? 1 : 0;
        QL_REQUIRE(i < buckets(t).size() - resBuck, "correlation buckets: bucketIdx "
                                                        << i << " out of range 0..."
                                                        << (buckets(t).size() - resBuck - -1));
        QL_REQUIRE(j < buckets(t).size() - resBuck,
                   "correlation buckets: bucketIdx " << j << " out of range 0..." << (buckets(t).size() - resBuck - 1));
        switch (t) {
        case Risk_CreditQ:
        case Risk_CreditVol:
            return crq_bucket_corr[i][j];
        case Risk_CreditNonQ:
        case Risk_CreditVolNonQ:
            return i == j ? 1.0 : 0.05;
        case Risk_Equity:
        case Risk_EquityVol:
            return eq_bucket_corr[i][j];
        case Risk_Commodity:
        case Risk_CommodityVol:
            return com_bucket_corr[i][j];
        default:
            return 1.0;
        }
    }

    Real SimmConfiguration_ISDA_V315::correlationQualifiers(const RiskType t) const {
        switch (t) {
        case Risk_IRCurve:
        case Risk_IRVol:
            return 0.27;
        default:
            return 1.0;
        }
    }

    Real SimmConfiguration_ISDA_V315::correlationWithinBucket(const RiskType t, const Size i) const {
        QL_REQUIRE(i < buckets(t).size(), "correlation within bucket: bucketIdx " << i << " out of range 0..."
                                                                                  << (buckets(t).size() - 1));
        switch (t) {
        case Risk_CreditQ:
        case Risk_CreditVol:
            if (i == residualBucket(t))
                return 0.50;
            else
                return 0.55;
        case Risk_CreditNonQ:
        case Risk_CreditVolNonQ:
            if (i == residualBucket(t))
                return 0.50;
            else
                return 0.21;
        case Risk_Equity:
        case Risk_EquityVol:
            return equity_inbucket_corr[i];
        case Risk_FX:
        case Risk_FXVol:
            return 0.5;
        case Risk_Commodity:
        case Risk_CommodityVol:
            return com_inbucket_corr[i];
        default:
            return 1.0;
        }
    }

    Real SimmConfiguration_ISDA_V315::correlationRiskClasses(const RiskClass c, const RiskClass d) const {
        QL_REQUIRE(c < numberOfRiskClasses, "correlation risk classes: invalid risk class " << c);
        QL_REQUIRE(d < numberOfRiskClasses, "correlation risk classes: invalid risk class " << d);
        return riskclass_corr[c][d];
    }

    string SimmConfiguration_ISDA_V315::referenceBucket(const string& qualifier) const {
        string br = "3";
        if (qualifier == "JPY")
            br = "2";
        if (qualifier == "USD" || qualifier == "EUR" || qualifier == "GBP" || qualifier == "CHF" ||
            qualifier == "AUD" || qualifier == "NZD" || qualifier == "CAD" || qualifier == "SEK" ||
            qualifier == "NOK" || qualifier == "DKK" || qualifier == "HKD" || qualifier == "KRW" ||
            qualifier == "SGD" || qualifier == "TWD")
            br = "1";
        return br;
    }

} // namespace QuantExt
