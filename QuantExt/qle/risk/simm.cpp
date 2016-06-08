/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/risk/simm.hpp>

#include <ql/math/comparison.hpp>

#include <iostream>

namespace QuantExt {

    Real Simm::marginIR(RiskType t, ProductClass p) {
        Size qualifiers = data_->numberOfQualifiers(t, p);
        std::vector<Real> s(qualifiers, 0.0), ws(qualifiers, 0.0), cr(qualifiers, 0.0), K(qualifiers, 0.0);

        const boost::shared_ptr<SimmConfiguration> conf = data_->configuration();
        Size labels1 = conf->labels1(t).size();
        Size labels2 = conf->labels2(t).size();

        for (Size q = 0; q < qualifiers; ++q) {
            Size b = data_->bucket(t, p, q);
            for (Size k = 0; k < labels1; ++k) {
                for (Size i = 0; i < labels2; ++i) {
                    Real amount = data_->amount(t, p, q, k, i);
                    Real weight = conf->weight(t, b, k);
                    s[q] += amount;
                    ws[q] += weight * amount;
                } // for i
            }     // for k
            cr[q] = std::max(1.0, std::sqrt(std::abs(s[q]) / conf->concentrationThreshold()));

            // TODO exploit symmetry
            for (Size k = 0; k < labels1; ++k) {
                for (Size i = 0; i < labels2; ++i) {
                    for (Size l = 0; l < labels1; ++l) {
                        for (Size j = 0; j < labels2; ++j) {
                            K[q] += conf->correlationLabels1(t, k, l) * conf->correlationLabels2(t, i, j) *
                                    data_->amount(t, p, q, k, i) * conf->weight(t, b, k) * cr[q] *
                                    data_->amount(t, p, q, l, j) * conf->weight(t, b, l) * cr[q];
                        } // for j
                    }     // for l
                }         // for i
            }             // for k
            K[q] = std::sqrt(std::max(K[q], 0.0));
        } // for q

        Real margin = 0.0;
        for (Size q1 = 0; q1 < qualifiers; ++q1) {
            margin += K[q1] * K[q1];
            Real s1 = std::max(std::min(ws[q1], K[q1]), -K[q1]);
            for (Size q2 = 0; q2 < q1; q2++) {
                Real s2 = std::max(std::min(ws[q2], K[q2]), -K[q2]);
                Real g = std::min(cr[q1], cr[q2]) / std::max(cr[q1], cr[q2]);
                margin += s1 * s2 * conf->correlationQualifiers(t) * g;
            } // for q1
        }     // for q2

        margin = std::sqrt(std::max(margin, 0.0));
        return margin;
    } // Simm::marginIR()

    Real Simm::curvatureMarginIR(ProductClass p) {
        RiskType t = SimmConfiguration::Risk_IRVol;
        Size qualifiers = data_->numberOfQualifiers(t, p);
        std::vector<Real> ws(qualifiers, 0.0), K(qualifiers);
        Real wssum = 0.0, wsabs = 0.0;

        const boost::shared_ptr<SimmConfiguration> conf = data_->configuration();
        Size labels1 = conf->labels1(t).size();
        Size labels2 = conf->labels2(t).size();
        QL_REQUIRE(labels2 == 1, "curvatureMarginIR: labels2 should be 1, but is " << labels2);

        for (Size q = 0; q < qualifiers; ++q) {
            Size b = data_->bucket(t, p, q);
            for (Size k = 0; k < labels1; ++k) {
                Real amount = data_->amount(t, p, q, k, 0);
                Real weight = conf->weight(t, b, k) * conf->curvatureWeight(k);
                wssum += weight * amount;
                wsabs += std::abs(weight * amount);
            } // for k

            // TODO exploit symmetry
            for (Size k = 0; k < labels1; ++k) {
                for (Size l = 0; l < labels1; ++l) {
                    K[q] += conf->correlationLabels1(t, k, l) * conf->correlationLabels1(t, k, l) *
                            data_->amount(t, p, q, k, 0) * conf->weight(t, b, k) * conf->curvatureWeight(k) *
                            data_->amount(t, p, q, l, 0) * conf->weight(t, b, l) * conf->curvatureWeight(l);
                } // for l
            }     // for k
            K[q] = std::sqrt(std::max(K[q], 0.0));
        } // for q

        if (close_enough(wsabs, 0.0))
            return 0.0;

        Real theta = std::min(wssum / wsabs, 0.0);

        Real lambda = (6.634896601021214 - 1.0) * (1.0 + theta) - theta;

        Real margin = 0.0;
        for (Size q1 = 0; q1 < qualifiers; ++q1) {
            margin += K[q1] * K[q1];
            Real s1 = std::max(std::min(ws[q1], K[q1]), -K[q1]);
            for (Size q2 = 0; q2 < q1; q2++) {
                Real s2 = std::max(std::min(ws[q2], K[q2]), -K[q2]);
                margin += s1 * s2 * conf->correlationQualifiers(t);
            } // for q1
        }     // for q2

        margin = std::max(lambda * std::sqrt(std::max(margin, 0.0)) + wssum, 0.0);
        return margin;
    } // Simm::curvatureMarginIR()

    // Real Simm::deltaMarginGeneric(RiskType t, ProductClass p) {

    //     Size qualifiers = data_->numberOfQualifiers(t);
    //     Size buckets = data_->configuration().buckets().size();
    //     std::vector<Real> s(qualifiers, 0.0), cr(qualifiers, 0.0);
    //     std::vector<Real> ws(bucket, 0.0), K(buckets, 0.0);

    //     for (Size q = 0; q < qualifiers; ++q) {
    //         for (Size k = 0; k < data_->numberOfLabels1(t); ++k) {
    //             for (Size i = 0; k < data_->numberOfLabels2(t); ++i) {
    //                 Size b = data_->bucket(q);
    //                 Real amount = data_->amount(t, p, k, i);
    //                 Real weight = conf->weight(t, q, k);
    //                 s[q] += amount;
    //                 ws[b] += weight * amount;
    //             } // for i
    //         }     // for k
    //         cr[q] = std::max(1.0, std::sqrt(std::abs(s[q]) / data_->concentrationThreshold(t, b, q)));
    //     }

    //     for (Size q1 = 0; q1 < qualifiers; ++q1) {
    //         for (Size q2 = 0; q2 < qualifiers; ++q2) {
    //             if (data_->bucket(q1) == data_->bucket(q2)) {
    //                 Real f = std::min(cr[q1][q2]) / std::max(cr[q1][q2]);
    //                 K[data_->bucket(q1)] += ws[q1] * ws[q2] * conf->correlationWithinBucket(t, data_->bucket(q1)) *
    //                 f;
    //             }
    //         }
    //     }

    //     Real margin = 0.0;
    //     for (Size b1 = 0; b1 < buckets; ++b1) {
    //         if (b1 != data_->configuration()->residualBucket(t)) {
    //             margin += K[b1] * K[b1];
    //             Real s1 = std::max(std::min(ws[b1], K[b1], -K[b1]));
    //             for (Size b2 = 0; b2 < buckets - resBuck; b2++) {
    //                 Real s2 = std::max(std::min(ws[b2], K[b2], -K[b2]));
    //                 margin += s1 * s2 * conf->correlationBuckets(t, b1, b2);
    //             }
    //         } // for b1
    //     }     // for b2

    //     margin = std::sqrt(std::max(margin,0.0));

    //     if (data_->configuration()->residualBucket(t) != Null<Size>()) {
    //         margin += K[data_->configuration()->residualBucket(t);
    //     }

    //     return margin;
    // } // Simm::deltaMarginGeneric()

    // Real Simm::vegaMarginGeneric(RiskType t, ProductClass p) {

    //     Size qualifiers = data_->numberOfQualifiers(t);
    //     Size buckets = data_->configuration().buckets().size();
    //     std::vector<Real> s(qualifiers, 0.0), cr(qualifiers, 0.0);
    //     std::vector<Real> ws(bucket, 0.0), K(buckets, 0.0);

    //     for (Size q = 0; q < qualifiers; ++q) {
    //         for (Size k = 0; k < data_->numberOfLabels1(t); ++k) {
    //             Size b = data_->bucket(q);
    //             Real amount = dat_->amount(t, p, k, 0);
    //             Real weight = conf->weight(t, q, k);
    //             s[q] += amount;
    //             ws[b] += weight * amount;
    //         } // for k
    //         cr[q] = std::max(1.0, std::sqrt(std::abs(s[q]) / data_->concentrationThreshold(t, b, q)));
    //     }

    //     for (Size q1 = 0; q1 < qualifiers; ++q1) {
    //         for (Size q2 = 0; q2 < qualifiers; ++q2) {
    //             if (data_->bucket(q1) == data_->bucket(q2)) {
    //                 Real f = std::min(cr[q1][q2]) / std::max(c1[q1][q2]);
    //                 K[data_->bucket(q1)] += ws[q1] * ws[q2] * conf->correlationQualifiers(t, q1, q2) * f;
    //             }
    //         }
    //     }

    //     Real margin = 0.0;
    //     for (Size b1 = 0; b1 < buckets; ++b1) {
    //         if(data_->configuration->residualBucket() != b1) {
    //         margin += K[b1] * K[b1];
    //         Real s1 = std::max(std::min(ws[b1], K[b1], -K[b1]));
    //         for (Size b2 = 0; b2 < buckets - resBuck; b2++) {
    //             Real s2 = std::max(std::min(ws[b2], K[b2], -K[b2]));
    //             margin += s1 * s2 * conf->correlationBuckets(t, b1, b2) * g;
    //         } // for b1
    //     }     // for b2

    //     margin = std::sqrt(std::max(margin,0.0));

    //     if (data_->configuration()->hasResidualBucket(t)) {
    //         margin += K.back();
    //     }

    //     return margin;
    // } // Simm::vegaMarginGeneric()

    // Real Simm::curvatureMarginGeneric(RiskType t, ProductClass p) {

    //     Size qualifiers = data_->numberOfQualifiers(t);
    //     Size buckets = data_->configuration().buckets().size();
    //     std::vector<Real> K(buckets, 0.0), ws(buckets, 0.0);
    //     Real wssum = 0.0, wsabs = 0.0, wssumres = 0.0, wsabsres = 0.0;

    //     for (Size q = 0; q < qualifiers; ++q) {
    //         for (Size k = 0; k < data_->numberOfLabels1(t); ++k) {
    //             Size b = data_->bucket(q);
    //             Real amount = dat_->amount(t, p, k, 0);
    //             Real weight = conf->weight(t, q, k) * conf->curvatureWeight(t, k);
    //             ws[q] += weight * amount;
    //             if (b != data_configuration()->residualBucket(t)) {
    //                 wssum += weight * amount;
    //                 wsabs += std::abs(weight * amount);
    //             } else {
    //                 wssumres += weight * amount;
    //                 wsabsres += std::abs(weight * amount);
    //             }
    //         } // for k
    //     }

    //     for (Size q1 = 0; q1 < qualifiers; ++q1) {
    //         for (Size q2 = 0; q2 < qualifiers; ++q2) {
    //             if (data_->bucket(q1) == data_->bucket(q2)) {
    //                 K[data_->bucket(q1)] += ws[q1] * ws[q2] * conf->correlationQualifiers(t, q1, q2) *
    //                                         conf->correlationQualifiers(t, q1, q2);
    //             }
    //         }
    //     }

    //     Real theta = std::min(wssum / wsabs, 0.0);
    //     Real lambda = (6.634896601021214 - 1.0) * (1.0 + theta) - theta;

    //     Real margin = 0.0;
    //     for (Size b1 = 0; b1 < buckets; ++b1) {
    //         if(
    //         margin += K[b1] * K[b1];
    //         Real s1 = std::max(std::min(ws[b1], K[b1], -K[b1]));
    //         for (Size b2 = 0; b2 < buckets - resBuck; b2++) {
    //             Real s2 = std::max(std::min(ws[b2], K[b2], -K[b2]));
    //             margin += s1 * s2 * conf->correlationBuckets(t, b1, b2) * g;
    //         } // for b1
    //     }     // for b2

    //     margin = std::max(lambda * std::sqrt(std::max(margin,0.0)) + wssum, 0.0);

    //     if (data_->configuration()->residualBucket(t) != Null<Size>()) {
    //         Real thetares = std::min(wssumres / wsabsres, 0.0);
    //         Real lambdares = (6.634896601021214 - 1.0) * (1.0 + thetares) - thetares;
    //         margin += std::max(lambda * K[data_->configuration()->residualBucket(t)] + wssumres, 0.0);
    //     }

    //     return margin;
    // } // Simm::curvatureMarginGeneric()

    void Simm::calculate() {

        boost::shared_ptr<SimmConfiguration> conf = data_->configuration();

        // debug

        boost::shared_ptr<SimmDataByKey> dataKey = boost::static_pointer_cast<SimmDataByKey>(data_);

        // output aggregated data
        // if (dataKey != NULL) {
        //     for (Size t = 0; t < conf->numberOfRiskTypes; ++t) {
        //         RiskType rt = SimmConfiguration::RiskType(t);
        //         for (Size p = 0; p < dataKey->numberOfProductClasses(); ++p) {
        //             ProductClass pc = SimmConfiguration::ProductClass(p);
        //             for (Size q = 0; q < dataKey->numberOfQualifiers(rt, pc); ++q) {
        //                 for (Size l1 = 0; l1 < conf->labels1(rt).size(); ++l1) {
        //                     for (Size l2 = 0; l2 < conf->labels2(rt).size(); ++l2) {
        //                         Real amount = dataKey->amount(rt, pc, q, l1, l2);
        //                         if(!QuantLib::close_enough(amount,0.0)) {
        //                         std::clog << rt << "\t" << pc << "\t" << dataKey->qualifiers(rt, pc)[q] << "\t"
        //                                   << "\t" << conf->labels1(rt)[l1] << "\t" << conf->labels2(rt)[l2] << "\t"
        //                                   << conf->buckets(rt)[dataKey->bucket(rt, pc, q)] << "\t"
        //                                   << dataKey->amount(rt, pc, q, l1, l2) << std::endl;
        //                         }
        //                     }
        //                 }
        //             }
        //         }
        //     }
        // }

        // output qualifiers, text, bucket
        // if (dataKey != NULL) {
        //     for (Size t = 0; t < conf->numberOfRiskTypes; ++t) {
        //         RiskType rt = SimmConfiguration::RiskType(t);
        //         for (Size p = 0; p < dataKey->numberOfProductClasses(); ++p) {
        //             ProductClass pc = SimmConfiguration::ProductClass(p);
        //             for (Size q = 0; q < dataKey->numberOfQualifiers(rt, pc); ++q) {
        //                 std::clog << rt << "\t" << pc << "\t" << q << "\t" << dataKey->qualifiers(rt, pc)[q] << "\t"
        //                           << conf->buckets(rt)[dataKey->bucket(rt, pc, q)] << std::endl;
        //             }
        //         }
        //     }
        // }
        // end debug

        for (Size p = 0; p < data_->numberOfProductClasses(); ++p) {

            ProductClass pc = ProductClass(p);

            // TODO put the single calls below into a loop once it is clear how many different
            // methods we need.

            // delta

            std::clog << pc << " IRDeltaMargin" << std::endl;
            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_InterestRate, SimmConfiguration::Delta)] =
                marginIR(SimmConfiguration::Risk_IRCurve, pc);

            // initialMargin_[boost::make_tuple(CreditQualifying, ProductClass(p), Delta)] =
            //     deltaMarginGeneric(RiskCreditQ, ProductClass(p));

            // initialMargin_[boost::make_tuple(CreditNonQualifying, ProductClass(p), Delta)] =
            //     deltaMarginGeneric(RiskCreditNonQ, ProductClass(p));

            // initialMargin_[boost::make_tuple(Equity, ProductClass(p), Delta)] =
            //     deltaMarginGeneric(Equity, ProductClass(p));

            // initialMargin_[boost::make_tuple(Commodity, ProductClass(p), Delta)] =
            //     deltaMarginGeneric(Commodity, ProductClass(p));

            // initialMargin_[boost::make_tuple(FX, ProductClass(p), Delta)] =
            //     deltaMarginGeneric(FX, ProductClass(p));

            // vega

            std::clog << pc << " IRVegaMargin" << std::endl;
            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_InterestRate, SimmConfiguration::Vega)] =
                marginIR(SimmConfiguration::Risk_IRVol, pc);

            // initialMargin_[boost::make_tuple(CreditQualifying, ProductClass(p), Vega)] =
            //     vegaMarginGeneric(RiskCreditVol, ProductClass(p));

            // initialMargin_[boost::make_tuple(CreditNonQualifying, ProductClass(p), Vega)] =
            //     vegaMarginGeneric(RiskCreditVolNonQ, ProductClass(p));

            // initialMargin_[boost::make_tuple(Equity, ProductClass(p), Vega)] =
            //     vegaMarginGeneric(RiskEquityVol, ProductClass(p));

            // initialMargin_[boost::make_tuple(Commodity, ProductClass(p), Vega)] =
            //     vegaMarginGeneric(CommodityVol, ProductClass(p));

            // initialMargin_[boost::make_tuple(FXVol, ProductClass(p), Vega)] =
            //     vegaMarginGeneric(FXVol, ProductClass(p));

            // curvature

            std::clog << pc << " IRCurvatureMargin" << std::endl;
            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_InterestRate,
                                             SimmConfiguration::Curvature)] = curvatureMarginIR(ProductClass(p));

            // initialMargin_[boost::make_tuple(CreditQualifying, ProductClass(p), Curvature)] =
            //     curvatureMarginGeneric(RiskCreditVol, ProductClass(p));

            // initialMargin_[boost::make_tuple(CreditNonQualifying, ProductClass(p), Curvature)] =
            //     curvatureMarginGeneric(RiskCreditVolNonQ, ProductClass(p));

            // initialMargin_[boost::make_tuple(Equity, ProductClass(p), Curvature)] =
            //     curvatureMarginGeneric(RiskEquipctyVol, ProductClass(p));

            // initialMargin_[boost::make_tuple(Commodity, ProductClass(p), Curvature)] =
            //     curvatureMarginGeneric(CommodityVol, ProductClass(p));

            // initialMargin_[boost::make_tuple(FXVol, ProductClass(p), Curvature)] =
            //     curvatureMarginGeneric(FXVol, ProductClass(p));

        } // for p

    } // Simm::calculate()

} // namespace QuantExt
