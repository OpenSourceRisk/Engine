/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/risk/simm.hpp>

namespace QuantExt {

    void Simm::genericMargin(SimmConfiguration::RiskType t, SimmConfiguration::ProductClass p) {

        Size qualifiers = data_->numberOfQualifiers(t);
        std::vector<Real> s(qualifiers, 0.0), ws(qualifiers, 0.0), cr(qualifiers, 0.0), K(qualifiers, 0.0);

        for (Size q = 0; q < qualifiers; ++q) {

            Real s = 0.0, ws = 0.0;
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
            cr[q] = std::max(1.0, std::sqrt(std::abs(s) / data_->concentrationThreshold(t, q)));

            // TODO exploit symmetry
            for (Size k = 0; k < data_->numberOfLabels1(t); ++k) {
                for (Size i = 0; k < data_->numberOfLabels2(t); ++i) {
                    for (Size l = 0; l < data_->numberOfLabels1(t); ++l) {
                        for (Size j = 0; j < data_->numberOfLabels2(t); ++j) {
                            for (Size b = 0; b < data_->numberOfBuckets(t); ++b) {
                                K[q] += conf->correlationLabels1(k, l) * conf->correlationsLabels2(i, j) *
                                        data_->amount(t, p, b, k, i) * conf->weight(t, b, k) * c;
                            } // for b
                        }     // for j
                    }         // for l
                }             // for i
            }                 // for k
            K[q] = std::sqrt(K[q])
        } // for q

        Real margin = 0.0;
        for (Size q1 = 0; q1 < qualifiers; ++q1) {
            margin += K[q] * K[q];
            Real s1 = std::max(std::min(ws[q1], -K[q1], K[q1]));
            for (Size q2 = 0; q2 < q; q2++) {
                Real s2 = std::max(std::min(ws[q2], -K[q2], K[q2]));
                Real g = std::min(cr[q1], cr[q2]) / std::max(cr[q1], cr[q2]);
                margin += s1 * s2 * conf->correlationQualifiers(q1, q2) * g;
            } // for q1
        }     // for q2

        return margin;

    } // Simm::genericMargin()

    void Simm::calculate() {

        boost::shared_ptr<SimmConfiguration> conf = data_->configuration();

        for (Size p = 0; p < data_->numberOfProductClasses(); ++p) {

            initialMargin_[boost::make_tuple(InterestRate, SimmConfiguration::ProductClass(p), Delta)] =
                genericMargin(SimmConfiguration::IR_Curve, SimmConfiguration::ProductClass(p));

        } // for p

    } // Simm::calculate()

} // namespace QuantExt

#endif
