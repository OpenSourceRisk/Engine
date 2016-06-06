/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/risk/simm.hpp>

namespace QuantExt {

    void Simm::DeltaMarginIR(SimmConfiguration::ProductClass p) {

        RiskType t = SimmConfiguration::Risk_IRCurve;
        Size qualifiers = data_->numberOfQualifiers(t);
        std::vector<Real> s(qualifiers, 0.0), ws(qualifiers, 0.0), cr(qualifiers, 0.0), K(qualifiers, 0.0);

        for (Size q = 0; q < qualifiers; ++q) {

            for (Size k = 0; k < data_->numberOfLabels1(t); ++k) {
                for (Size i = 0; k < data_->numberOfLabels2(t); ++i) {
                    for (Size b = 0; b < data_->numberOfBuckets(t); ++b) {
                        Real amount = data_->amount(t, p, b, k, i);
                        Real weight = conf->weight(t, b, k);
                        s[q] += amount;
                        ws[q] += weight * amount;
                    } // for b
                }     // for i
            }         // for k
            cr[q] = std::max(1.0, std::sqrt(std::abs(s) / data_->concentrationThreshold(t, b, q)));

            // TODO exploit symmetry
            for (Size k = 0; k < data_->numberOfLabels1(t); ++k) {
                for (Size i = 0; k < data_->numberOfLabels2(t); ++i) {
                    for (Size l = 0; l < data_->numberOfLabels1(t); ++l) {
                        for (Size j = 0; j < data_->numberOfLabels2(t); ++j) {
                            for (Size b = 0; b < data_->numberOfBuckets(t); ++b) {
                                K[q] += conf->correlationLabels1(t, k, l) * conf->correlationsLabels2(t, i, j) *
                                        data_->amount(t, p, b, k, i) * conf->weight(t, b, k) * c *
                                        data_->amount(t, p, b, l, j) * conf->weight(t, b, l) * c;
                            } // for b
                        }     // for j
                    }         // for l
                }             // for i
            }                 // for k
            K[q] = std::sqrt(K[q])
        } // for q

        Real margin = 0.0;
        for (Size q1 = 0; q1 < qualifiers; ++q1) {
            margin += K[q1] * K[q1];
            Real s1 = std::max(std::min(ws[q1], -K[q1], K[q1]));
            for (Size q2 = 0; q2 < q1; q2++) {
                Real s2 = std::max(std::min(ws[q2], -K[q2], K[q2]));
                Real g = std::min(cr[q1], cr[q2]) / std::max(cr[q1], cr[q2]);
                margin += s1 * s2 * conf->correlationQualifiers(q1, q2) * g;
            } // for q1
        }     // for q2

        margin = std::sqrt(margin);

        return margin;

    } // Simm::genericDeltaMarginIR()

    void Simm::DeltaMarginCRQ(SimmConfiguration::ProductClass p) {

        RiskType t = SimmConfiguration::Risk_CreditQ;
        Size qualifiers = data_->numberOfQualifiers(t);
        Size buckets = data_->numberOfBuckets(t);
        std::vector<Real> s(qualifiers, 0.0), cr(qualifiers, 0.0);
        std::vector<Real> ws(bucket, 0.0), K(buckets, 0.0);

        for (Size q = 0; q < qualifiers; ++q) {
            for (Size k = 0; k < data_->numberOfLabels1(t); ++k) {
                for (Size i = 0; k < data_->numberOfLabels2(t); ++i) {
                    Size b = data_->bucket(q);
                    Real amount = dat_->amount(t, p, b, k, i);
                    Real weight = conf->weight(t, b, k);
                    s[q] += amount;
                    ws[b] += weight * amount;
                } // for i
            }     // for k
            cr[q] = std::max(1.0, std::sqrt(std::abs(s[q]) / data_->concentrationThreshold(t, b, q)));
        }

        for (Size q1 = 0; q1 < qualifiers; ++q1) {
            for (Size q2 = 0; q2 < qualifiers; ++q2) {
                if (data_->bucket(q1) == data_->bucket(q2)) {
                    Real f = std::min(cr[q1][q2]) / std::max(c1[q1][q2]);
                    K[data_->bucket(q1)] += ws[q1] * ws[q2] * conf->correlationQualifiers(t, q1, q2) * f;
                }
            }
        }

        Size resBuck = data_->configuration()->hasResidualBucket(t) ? 1 : 0;
        Real margin = 0.0;
        for (Size b1 = 0; b1 < buckets - resBuck; ++b1) {
            margin += K[b1] * K[b1];
            Real s1 = std::max(std::min(ws[b1], -K[b1], K[b1]));
            for (Size b2 = 0; b2 < buckets - resBuck; b2++) {
                Real s2 = std::max(std::min(ws[b2], -K[b2], K[b2]));
                margin += s1 * s2 * conf->correlationBuckets(t, b1, b2);
            } // for b1
        }     // for b2

        margin = std::sqrt(margin);

        if (data_->configuration()->hasResidualBucket(t)) {
            margin += K.back();
        }

        return margin;

    } // Simm::DeltaMarginCRQ

    void Simm::calculate() {

        boost::shared_ptr<SimmConfiguration> conf = data_->configuration();

        for (Size p = 0; p < data_->numberOfProductClasses(); ++p) {

            initialMargin_[boost::make_tuple(InterestRate, SimmConfiguration::ProductClass(p), Delta)] =
                DeltaMarginIR(SimmConfiguration::ProductClass(p));

            initialMargin_[boost::make_tuple(CreditQualifying, SimmConfiguration::ProductClass(p), Delta)] =
                DeltaMarginGeneric(SimmConfiguration::RiskCreditQ, SimmConfiguration::ProductClass(p));

            initialMargin_[boost::make_tuple(CreditNonQualifying, SimmConfiguration::ProductClass(p), Delta)] =
                DeltaMarginGeneric(SimmConfiguration::RiskCreditNonQ, SimmConfiguration::ProductClass(p));

            initialMargin_[boost::make_tuple(CreditNonQualifying, SimmConfiguration::ProductClass(p), Delta)] =
                DeltaMarginGeneric(SimmConfiguration::Equity, SimmConfiguration::ProductClass(p));

            initialMargin_[boost::make_tuple(CreditNonQualifying, SimmConfiguration::ProductClass(p), Delta)] =
                DeltaMarginGeneric(SimmConfiguration::Commodity, SimmConfiguration::ProductClass(p));

            initialMargin_[boost::make_tuple(CreditNonQualifying, SimmConfiguration::ProductClass(p), Delta)] =
                DeltaMarginGeneric(SimmConfiguration::FX, SimmConfiguration::ProductClass(p));

        } // for p

    } // Simm::calculate()

} // namespace QuantExt

#endif
