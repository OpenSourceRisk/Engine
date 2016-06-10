/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
 All rights reserved.
*/

#include <qle/risk/simm.hpp>

#include <ql/math/comparison.hpp>

#include <iostream>

namespace QuantExt {

    Real Simm::initialMargin(const ProductClass p, const RiskClass c, const MarginType m) const {
        boost::tuple<ProductClass, RiskClass, MarginType> key = boost::make_tuple(p, c, m);
        return initialMargin_.at(key);
    }

    Real Simm::initialMargin(const ProductClass p, const RiskClass c) const {
        Real sum = 0.0;
        for (Size m = 0; m < data_->configuration()->numberOfMarginTypes; ++m) {
            boost::tuple<ProductClass, RiskClass, MarginType> key =
                boost::make_tuple(p, c, SimmConfiguration::MarginType(m));
            sum += initialMargin_.at(key);
        }
        return sum;
    }

    Real Simm::initialMargin(const ProductClass p) const {
        Real sum = 0.0;
        for (Size c = 0; c < data_->configuration()->numberOfRiskClasses; ++c) {
            sum +=
                initialMargin(p, SimmConfiguration::RiskClass(c)) * initialMargin(p, SimmConfiguration::RiskClass(c));
            for (Size d = 0; d < c; ++d) {
                sum += 2.0 * initialMargin(p, SimmConfiguration::RiskClass(c)) *
                       initialMargin(p, SimmConfiguration::RiskClass(d)) *
                       data_->configuration()->correlationRiskClasses(SimmConfiguration::RiskClass(c),
                                                                      SimmConfiguration::RiskClass(d));
            }
        }
        return std::sqrt(std::max(sum, 0.0));
    }

    Real Simm::initialMargin() const {
        Real sum = 0.0;
        for (Size p = 0; p < data_->numberOfProductClasses(); ++p) {
            sum += initialMargin(SimmConfiguration::ProductClass(p));
        }
        return sum;
    }

    Real Simm::marginIR(RiskType t, ProductClass p) const {
        // if (t != SimmConfiguration::Risk_IRCurve || p != SimmConfiguration::ProductClass_RatesFX)
        //     return 0.0;
        std::clog << "===== margin IR ========== " << p << std::endl;
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

            for (Size k = 0; k < labels1; ++k) {
                for (Size i = 0; i < labels2; ++i) {
                    K[q] += data_->amount(t, p, q, k, i) * conf->weight(t, b, k) * cr[q] *
                            data_->amount(t, p, q, k, i) * conf->weight(t, b, k) * cr[q];
                    for (Size l = 0; l <= k; ++l) {
                        for (Size j = 0; (l < k && j < labels2) || (l == k && j < i); ++j) {
                            // for the special inflation tenor the subcurve correlation must
                            // be set to 1 !
                            Real corrLabel2 =
                                (k == labels1 - 1 || l == labels1 - 1 ? 1.0 : conf->correlationLabels2(t, i, j));
                            K[q] += 2.0 * conf->correlationLabels1(t, k, l) * corrLabel2 *
                                    data_->amount(t, p, q, k, i) * conf->weight(t, b, k) * cr[q] *
                                    data_->amount(t, p, q, l, j) * conf->weight(t, b, l) * cr[q];
                        } // for j
                    }     // for l
                }         // for i
            }             // for k
            K[q] = std::sqrt(std::max(K[q], 0.0));
            std::clog << "q=" << q << " ws[q]=" << ws[q] << " K[q]=" << K[q] << std::endl;
        } // for q

        Real margin = 0.0;
        for (Size q1 = 0; q1 < qualifiers; ++q1) {
            margin += K[q1] * K[q1];
            Real s1 = std::max(std::min(ws[q1], K[q1]), -K[q1]);
            std::clog << "q=" << q1 << " Sb=" << s1 << std::endl;
            for (Size q2 = 0; q2 < q1; q2++) {
                Real s2 = std::max(std::min(ws[q2], K[q2]), -K[q2]);
                Real g = std::min(cr[q1], cr[q2]) / std::max(cr[q1], cr[q2]);
                margin += 2.0 * s1 * s2 * conf->correlationQualifiers(t) * g;
            } // for q1
        }     // for q2

        margin = std::sqrt(std::max(margin, 0.0));
        return margin;
    } // Simm::marginIR()

    Real Simm::curvatureMarginIR(ProductClass p) const {
        std::clog << "===== curvature margin IR  ========== " << p << std::endl;
        RiskType t = SimmConfiguration::Risk_IRVol;
        Size qualifiers = data_->numberOfQualifiers(t, p);
        std::vector<Real> ws(qualifiers, 0.0), K(qualifiers);
        Real wssum = 0.0, wssumabs = 0.0;

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
                wssumabs += std::abs(weight * amount);
            } // for k

            for (Size k = 0; k < labels1; ++k) {
                K[q] += data_->amount(t, p, q, k, 0) * conf->weight(t, b, k) * conf->curvatureWeight(k) *
                        data_->amount(t, p, q, k, 0) * conf->weight(t, b, k) * conf->curvatureWeight(k);
                for (Size l = 0; l < k; ++l) {
                    K[q] += 2.0 * conf->correlationLabels1(t, k, l) * conf->correlationLabels1(t, k, l) *
                            data_->amount(t, p, q, k, 0) * conf->weight(t, b, k) * conf->curvatureWeight(k) *
                            data_->amount(t, p, q, l, 0) * conf->weight(t, b, l) * conf->curvatureWeight(l);
                } // for l
            }     // for k
            K[q] = std::sqrt(std::max(K[q], 0.0));
        } // for q

        if (close_enough(wssumabs, 0.0))
            return 0.0;

        Real theta = std::min(wssum / wssumabs, 0.0);
        Real lambda = (phiInv995Sq_ - 1.0) * (1.0 + theta) - theta;

        Real margin = 0.0;
        for (Size q1 = 0; q1 < qualifiers; ++q1) {
            margin += K[q1] * K[q1];
            Real s1 = std::max(std::min(ws[q1], K[q1]), -K[q1]);
            for (Size q2 = 0; q2 < q1; q2++) {
                Real s2 = std::max(std::min(ws[q2], K[q2]), -K[q2]);
                margin += s1 * s2 * conf->correlationQualifiers(t) * conf->correlationQualifiers(t);
            } // for q1
        }     // for q2

        margin = std::max(lambda * std::sqrt(std::max(margin, 0.0)) + wssum, 0.0);
        return margin;
    } // Simm::curvatureMarginIR()

    Real Simm::marginGeneric(RiskType t, ProductClass p) const {
        std::clog << "===== margin " << t << " ========== " << p << std::endl;
        Size qualifier = data_->numberOfQualifiers(t, p);
        const boost::shared_ptr<SimmConfiguration> conf = data_->configuration();
        Size buckets = conf->buckets(t).size();
        Size labels1 = conf->labels1(t).size();
        Size labels2 = conf->labels2(t).size();
        std::vector<Real> s(buckets, 0.0), ws(buckets, 0.0), K(buckets, 0.0), cr(qualifier, 0.0), sq(qualifier, 0.0),
            wsq(qualifier, 0.0);

        for (Size q = 0; q < qualifier; ++q) {
            Size b = data_->bucket(t, p, q);
            for (Size k = 0; k < labels1; ++k) {
                for (Size i = 0; i < labels2; ++i) {
                    Real amount = data_->amount(t, p, q, k, i);
                    Real weight = conf->weight(t, b, k);
                    s[b] += amount;
                    sq[q] += amount;
                    ws[b] += weight * amount;
                    wsq[q] += weight * amount;
                } // for i
            }     // for k
            cr[q] = std::max(1.0, std::sqrt(std::abs(sq[q]) / conf->concentrationThreshold()));
        }

        // new
        for (Size b = 0; b < buckets; ++b) {
            const std::set<Size>& qual = data_->qualifierPerBucket(t, p, b);
            for (std::set<Size>::const_iterator it1 = qual.cbegin(); it1 != qual.cend(); it1++) {
                for (std::set<Size>::const_iterator it2 = qual.cbegin(); it2 != qual.cend(); ++it2) {
                    Real f = std::min(cr[*it1], cr[*it2]) / std::max(cr[*it1], cr[*it2]);
                    K[b] += wsq[*it1] * wsq[*it2] * cr[*it1] * cr[*it2] *
                            (*it1 == *it2 ? 1.0 : conf->correlationWithinBucket(t, b)) * f;
                }
            }
        }

        // old
        // for (Size q1 = 0; q1 < qualifier; ++q1) {
        //     Size b1 = data_->bucket(t, p, q1);
        //     for (Size q2 = 0; q2 < qualifier; ++q2) {
        //         Size b2 = data_->bucket(t, p, q2);
        //         Real f = std::min(cr[q1], cr[q2]) / std::max(cr[q1], cr[q2]);
        //         // TODO optimize this with a map bucket => qualifier in data
        //         if (b1 == b2) {
        //             K[b1] += wsq[q1] * wsq[q2] * cr[q1] * cr[q2] *
        //                      (q1 == q2 ? 1.0 : conf->correlationWithinBucket(t, b1)) * f;
        //         }
        //     }
        // }

        for (Size b = 0; b < buckets; ++b) {
            K[b] = std::sqrt(std::max(K[b], 0.0));
        }

        Real margin = 0.0;
        for (Size b1 = 0; b1 < buckets; ++b1) {
            if (b1 != data_->configuration()->residualBucket(t)) {
                margin += K[b1] * K[b1];
                Real s1 = std::max(std::min(ws[b1], K[b1]), -K[b1]);
                for (Size b2 = 0; b2 < b1; b2++) {
                    if (b2 != data_->configuration()->residualBucket(t)) {
                        Real s2 = std::max(std::min(ws[b2], K[b2]), -K[b2]);
                        margin += 2.0 * s1 * s2 * conf->correlationBuckets(t, b1, b2);
                    }
                }
            } // for b1
        }     // for b2

        margin = std::sqrt(std::max(margin, 0.0));

        if (conf->residualBucket(t) != Null<Size>()) {
            margin += K[conf->residualBucket(t)];
        }

        return margin;
    } // Simm::marginGeneric()

    Real Simm::curvatureMarginGeneric(RiskType t, ProductClass p) const {
        std::clog << "===== curvature margin " << t << " ========== " << p << std::endl;
        Size qualifier = data_->numberOfQualifiers(t, p);
        const boost::shared_ptr<SimmConfiguration> conf = data_->configuration();
        Size buckets = conf->buckets(t).size();
        Size labels1 = conf->labels1(t).size();
        Size labels2 = conf->labels2(t).size();
        std::vector<Real> s(buckets, 0.0), ws(buckets, 0.0), K(buckets, 0.0), sq(qualifier, 0.0), wsq(qualifier, 0.0);
        Real wssum = 0.0, wssumabs = 0.0, wssumres = 0.0, wssumabsres = 0.0;

        for (Size q = 0; q < qualifier; ++q) {
            Size b = data_->bucket(t, p, q);
            for (Size k = 0; k < labels1; ++k) {
                for (Size i = 0; i < labels2; ++i) {
                    Real amount = data_->amount(t, p, q, k, i);
                    Real weight = conf->weight(t, b, k);
                    s[b] += amount;
                    sq[q] += amount;
                    ws[b] += weight * amount;
                    wsq[q] += weight * amount;
                    if (b != conf->residualBucket(t)) {
                        wssum += weight * amount;
                        wssumabs += std::abs(weight * amount);
                    } else {
                        wssumres += weight * amount;
                        wssumabsres += std::abs(weight * amount);
                    }
                } // for i
            }     // for k
        }

        // new
        for (Size b = 0; b < buckets; ++b) {
            const std::set<Size>& qual = data_->qualifierPerBucket(t, p, b);
            for (std::set<Size>::const_iterator it1 = qual.cbegin(); it1 != qual.cend(); it1++) {
                for (std::set<Size>::const_iterator it2 = qual.cbegin(); it2 != qual.cend(); ++it2) {
                    K[b] += wsq[*it1] * wsq[*it2] *
                            (*it1 == *it2 ? 1.0
                                          : conf->correlationWithinBucket(t, b) * conf->correlationWithinBucket(t, b));
                }
            }
        }

        // old
        // for (Size q1 = 0; q1 < qualifier; ++q1) {
        //     Size b1 = data_->bucket(t, p, q1);
        //     for (Size q2 = 0; q2 < qualifier; ++q2) {
        //         Size b2 = data_->bucket(t, p, q2);
        //         // TODO optimize this with a map bucket => qualifier in data
        //         if (b1 == b2) {
        //             K[b1] +=
        //                 wsq[q1] * wsq[q2] *
        //                 (q1 == q2 ? 1.0 : conf->correlationWithinBucket(t, b1) * conf->correlationWithinBucket(t, b1));
        //         }
        //     }
        // }

        for (Size b = 0; b < buckets; ++b) {
            K[b] = std::sqrt(std::max(K[b], 0.0));
        }

        Real margin = 0.0;

        if (!close_enough(wssumabs, 0.0)) {
            Real theta = std::min(wssum / wssumabs, 0.0);
            Real lambda = (phiInv995Sq_ - 1.0) * (1.0 + theta) - theta;
            for (Size b1 = 0; b1 < buckets; ++b1) {
                if (b1 != data_->configuration()->residualBucket(t)) {
                    margin += K[b1] * K[b1];
                    Real s1 = std::max(std::min(ws[b1], K[b1]), -K[b1]);
                    for (Size b2 = 0; b2 < b1; b2++) {
                        if (b2 != data_->configuration()->residualBucket(t)) {
                            Real s2 = std::max(std::min(ws[b2], K[b2]), -K[b2]);
                            margin += 2.0 * s1 * s2 * conf->correlationBuckets(t, b1, b2) *
                                      conf->correlationBuckets(t, b1, b2);
                        }
                    }
                } // for b1
            }     // for b2
            margin = std::max(lambda * std::sqrt(std::max(margin, 0.0)) + wssum, 0.0);
        }

        if (conf->residualBucket(t) != Null<Size>() && !close_enough(wssumabsres, 0.0)) {
            Real theta = std::min(wssumres / wssumabsres, 0.0);
            Real lambda = (phiInv995Sq_ - 1.0) * (1.0 + theta) - theta;
            margin += std::max(wssum + lambda * K[conf->residualBucket(t)], 0.0);
        }

        return margin;
    } // Simm::curvatureMarginGeneric()

    void Simm::calculate() {

        boost::shared_ptr<SimmConfiguration> conf = data_->configuration();

        // debug

        boost::shared_ptr<SimmDataByKey> dataKey = boost::static_pointer_cast<SimmDataByKey>(data_);

        std::clog << "\nAggregated Data:" << std::endl;
        // output aggregated data
        if (dataKey != NULL) {
            for (Size t = 0; t < conf->numberOfRiskTypes; ++t) {
                RiskType rt = SimmConfiguration::RiskType(t);
                for (Size p = 0; p < dataKey->numberOfProductClasses(); ++p) {
                    ProductClass pc = SimmConfiguration::ProductClass(p);
                    for (Size q = 0; q < dataKey->numberOfQualifiers(rt, pc); ++q) {
                        for (Size l1 = 0; l1 < conf->labels1(rt).size(); ++l1) {
                            for (Size l2 = 0; l2 < conf->labels2(rt).size(); ++l2) {
                                Real amount = dataKey->amount(rt, pc, q, l1, l2);
                                // FILTER goes here
                                if (!QuantLib::close_enough(amount, 0.0) && rt == SimmConfiguration::Risk_IRCurve &&
                                    pc == SimmConfiguration::ProductClass_RatesFX) {
                                    std::clog << rt << "\t" << pc << "\t" << dataKey->qualifiers(rt, pc)[q] << " (" << q
                                              << ") \t"
                                              << "\t" << conf->labels1(rt)[l1] << "\t" << conf->labels2(rt)[l2] << "\t"
                                              << (dataKey->bucket(rt, pc, q) == Null<Size>()
                                                      ? "na"
                                                      : conf->buckets(rt)[dataKey->bucket(rt, pc, q)])
                                              << "\t" << amount << "\t"
                                              << conf->weight(rt, dataKey->bucket(rt, pc, q), l1) << "\t"
                                              << conf->weight(rt, dataKey->bucket(rt, pc, q), l1) * amount << std::endl;
                                }
                            }
                        }
                    }
                }
            }
        }

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

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_InterestRate, SimmConfiguration::Delta)] =
                marginIR(SimmConfiguration::Risk_IRCurve, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_CreditQualifying,
                                             SimmConfiguration::Delta)] =
                marginGeneric(SimmConfiguration::Risk_CreditQ, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_CreditNonQualifying,
                                             SimmConfiguration::Delta)] =
                marginGeneric(SimmConfiguration::Risk_CreditNonQ, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_Equity, SimmConfiguration::Delta)] =
                marginGeneric(SimmConfiguration::Risk_Equity, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_Commodity, SimmConfiguration::Delta)] =
                marginGeneric(SimmConfiguration::Risk_Commodity, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_FX, SimmConfiguration::Delta)] =
                marginGeneric(SimmConfiguration::Risk_FX, pc);

            // vega

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_InterestRate, SimmConfiguration::Vega)] =
                marginIR(SimmConfiguration::Risk_IRVol, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_CreditQualifying,
                                             SimmConfiguration::Vega)] =
                marginGeneric(SimmConfiguration::Risk_CreditVol, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_CreditNonQualifying,
                                             SimmConfiguration::Vega)] =
                marginGeneric(SimmConfiguration::Risk_CreditVolNonQ, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_Equity, SimmConfiguration::Vega)] =
                marginGeneric(SimmConfiguration::Risk_EquityVol, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_Commodity, SimmConfiguration::Vega)] =
                marginGeneric(SimmConfiguration::Risk_CommodityVol, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_FX, SimmConfiguration::Vega)] =
                marginGeneric(SimmConfiguration::Risk_FXVol, pc);

            // curvature

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_InterestRate,
                                             SimmConfiguration::Curvature)] = curvatureMarginIR(pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_CreditQualifying,
                                             SimmConfiguration::Curvature)] =
                curvatureMarginGeneric(SimmConfiguration::Risk_CreditVol, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_CreditNonQualifying,
                                             SimmConfiguration::Curvature)] =
                curvatureMarginGeneric(SimmConfiguration::Risk_CreditVolNonQ, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_Equity, SimmConfiguration::Curvature)] =
                curvatureMarginGeneric(SimmConfiguration::Risk_EquityVol, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_Commodity,
                                             SimmConfiguration::Curvature)] =
                curvatureMarginGeneric(SimmConfiguration::Risk_CommodityVol, pc);

            initialMargin_[boost::make_tuple(pc, SimmConfiguration::RiskClass_FX, SimmConfiguration::Curvature)] =
                curvatureMarginGeneric(SimmConfiguration::Risk_FXVol, pc);

        } // for p

    } // Simm::calculate()

} // namespace QuantExt
