/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/risk/simmconfiguration.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>

namespace QuantExt {

    void SimmConfiguration::check() const {
        // check inflation setup (since this has to be incorporated as a tenor into IRCurve)
        QL_REQUIRE(buckets(Risk_Inflation).size() == 1, "Risk_Inflation must have one bucket, but has "
                                                            << buckets(Risk_Inflation).size());
        QL_REQUIRE(labels1(Risk_Inflation).size() == 1, "Risk_Inflation must have one labels1, but has "
                                                            << labels1(Risk_Inflation).size());
        QL_REQUIRE(labels2(Risk_Inflation).size() == 1, "Risk_Inflation must have one labels2, but has "
                                                            << labels2(Risk_Inflation).size());
        // check correlation matrices
        for (Size t = 0; t < numberOfRiskTypes; ++t) {
            RiskType rt = RiskType(t);
            Size lab1 = labels1(rt).size();
            Size lab2 = labels2(rt).size();
            Size resBucket = residualBucket(rt) == Null<Size>() ? 0 : 1;
            Size buck = buckets(rt).size() - resBucket;
            Matrix l1(lab1, lab1), l2(lab2, lab2), b(buck, buck);
            for (Size i = 0; i < lab1; ++i) {
                for (Size j = 0; j < lab1; ++j) {
                    l1[i][j] = correlationLabels1(rt, i, j);
                }
            }
            for (Size i = 0; i < lab2; ++i) {
                for (Size j = 0; j < lab2; ++j) {
                    l2[i][j] = correlationLabels2(rt, i, j);
                }
            }
            for (Size i = 0; i < buck; ++i) {
                for (Size j = 0; j < buck; ++j) {
                    b[i][j] = correlationBuckets(rt, i, j);
                }
            }
            checkMatrix(l1, rt, "labels1");
            checkMatrix(l2, rt, "labels2");
            checkMatrix(b, rt, "buckets");
        }
        Matrix rc(numberOfRiskClasses, numberOfRiskClasses);
        for (Size i = 0; i < numberOfRiskClasses; ++i) {
            for (Size j = 0; j < numberOfRiskClasses; ++j) {
                rc[i][j] = correlationRiskClasses(RiskClass(i), RiskClass(j));
            }
        }
        // risk type doesn't matter here, ...
        checkMatrix(rc, Risk_Commodity, "risk classes");
    }

    void SimmConfiguration::checkMatrix(const Matrix& m, const RiskType t, const string& label) const {
        Size n = m.rows();
        QL_REQUIRE(n > 0, "matrix is null, risk type " << t << ", label " << label);
        for (Size i = 0; i < n; ++i) {
            QL_REQUIRE(close_enough(m[i][i], 1.0), "correlation matrix has non unit diagonal element at ("
                                                       << i << "," << i << "), risk type " << t << ", label " << label);
            for (Size j = 0; j < n; ++j) {
                QL_REQUIRE(close_enough(m[i][j], m[j][i]), "correlation matrix is not symmetric, for (i,j)=("
                                                               << i << "," << j << "), values are " << m[i][j]
                                                               << " and " << m[j][i] << ", risk type " << t
                                                               << ", label " << label);
                QL_REQUIRE(m[i][j] >= -1.0 && m[i][j] <= 1.0, "correlation matrix entry out of bounds at ("
                                                                  << i << "," << j << "), value is " << m[i][j]
                                                                  << ", risk type " << t << ", label " << label);
            }
        }
        SymmetricSchurDecomposition ssd(m);
        for (Size i = 0; i < ssd.eigenvalues().size(); ++i) {
            QL_REQUIRE(ssd.eigenvalues()[i] >= 0.0, "correlation matrix has negative eigenvalue at "
                                                        << i << " (" << ssd.eigenvalues()[i] << "), risk type " << t
                                                        << ", label " << label);
        }
    }

    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::RiskClass x) {
        switch (x) {
        case SimmConfiguration::RiskClass_InterestRate:
            return out << "InterestRate";
        case SimmConfiguration::RiskClass_CreditQualifying:
            return out << "CreditQualifying";
        case SimmConfiguration::RiskClass_CreditNonQualifying:
            return out << "CreditNonQualifying";
        case SimmConfiguration::RiskClass_Equity:
            return out << "Equity";
        case SimmConfiguration::RiskClass_Commodity:
            return out << "Commodity";
        case SimmConfiguration::RiskClass_FX:
            return out << "FX";
        default:
            return out << "Unknown simm risk class (" << x << ")";
        }
    }

    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::RiskType x) {
        switch (x) {
        case SimmConfiguration::Risk_Commodity:
            return out << "Risk_Commodity";
        case SimmConfiguration::Risk_CommodityVol:
            return out << "Risk_CommodityVol";
        case SimmConfiguration::Risk_CreditNonQ:
            return out << "Risk_CreditNonQ";
        case SimmConfiguration::Risk_CreditQ:
            return out << "Risk_CreditQ";
        case SimmConfiguration::Risk_CreditVol:
            return out << "Risk_CreditVol";
        case SimmConfiguration::Risk_CreditVolNonQ:
            return out << "Risk_CreditVolNonQ";
        case SimmConfiguration::Risk_Equity:
            return out << "Risk_Equity";
        case SimmConfiguration::Risk_EquityVol:
            return out << "Risk_EquityVol";
        case SimmConfiguration::Risk_FX:
            return out << "Risk_FX";
        case SimmConfiguration::Risk_FXVol:
            return out << "Risk_FXVol";
        case SimmConfiguration::Risk_Inflation:
            return out << "Risk_Inflation";
        case SimmConfiguration::Risk_IRCurve:
            return out << "Risk_IRCurve";
        case SimmConfiguration::Risk_IRVol:
            return out << "Risk_IRVol";
        default:
            return out << "Unkonwn simm risk type (" << x << ")";
        }
    }

    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::MarginType x) {
        switch (x) {
        case SimmConfiguration::Delta:
            return out << "Delta";
        case SimmConfiguration::Vega:
            return out << "Vega";
        case SimmConfiguration::Curvature:
            return out << "Curvature";
        default:
            return out << "Unknown simm margin type (" << x << ")";
        }
    }

    std::ostream& operator<<(std::ostream& out, const SimmConfiguration::ProductClass x) {
        switch (x) {
        case SimmConfiguration::ProductClass_RatesFX:
            return out << "RateFX";
        case SimmConfiguration::ProductClass_Credit:
            return out << "Credit";
        case SimmConfiguration::ProductClass_Equity:
            return out << "Equity";
        case SimmConfiguration::ProductClass_Commodity:
            return out << "Commodity";
        default:
            return out << "Unknown simm product class (" << x << ")";
        }
    }

} // namespace QuantExt
