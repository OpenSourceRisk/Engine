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

#include <orea/engine/standardapproachcvacalculator.hpp>

#include <numeric>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>
#include <ql/tuple.hpp>
#include <boost/math/distributions/normal.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <iostream>

using std::set;
using std::string;
using std::vector;
using std::map;
using std::pair;
using std::make_pair;
using std::sqrt;
using std::accumulate;
using std::pair;

using QuantLib::Real;
using QuantLib::Size;
using QuantLib::close_enough;
using ore::data::to_string;

namespace ore {
namespace analytics {

SaCvaSummaryKey::SaCvaSummaryKey(std::string n, CvaRiskFactorKey::KeyType kt, CvaRiskFactorKey::MarginType mt, std::string b) :
    nettingSet(n), keyType(kt), marginType(mt), bucket(b) {};

bool SaCvaSummaryKey::operator<(const SaCvaSummaryKey& sr) const {
    return  std::tie(nettingSet, keyType, marginType, bucket) <
        std::tie(sr.nettingSet, sr.keyType, sr.marginType, sr.bucket);
}

StandardApproachCvaCalculator::StandardApproachCvaCalculator(const std::string& calculationCcy, const SaCvaNetSensitivities& cvaNetSensitivities,
    const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager, 
    const std::map<ReportType, QuantLib::ext::shared_ptr<ore::data::Report>>& outReports, bool unhedgedSensitivity, const vector<std::string>& perfectHedges) : 
    cvaNetSensitivities_(cvaNetSensitivities), counterpartyManager_(counterpartyManager), reports_(outReports), unhedged_(unhedgedSensitivity) {

    for (auto p : perfectHedges) {
        vector<string> tokens;
        boost::split(tokens, p, boost::is_any_of("|"));
        QL_REQUIRE(tokens.size() == 2, "only 2 tokens expected");

        CvaRiskFactorKey::KeyType rt = parseCvaRiskFactorKeyType(tokens[0]);
        CvaRiskFactorKey::MarginType mt = parseCvaRiskFactorMarginType(tokens[1]);

        perfectHedges_.push_back(std::make_pair(rt, mt));
    }
    
    irRiskWeightCcys_ = { "USD", "EUR", "GBP", "AUD", "CAD", "SEK", "JPY"};
    if (std::find(irRiskWeightCcys_.begin(), irRiskWeightCcys_.end(), calculationCcy) == irRiskWeightCcys_.end())
        irRiskWeightCcys_.push_back(calculationCcy);
        
    irDeltaRiskWeights_ = {{"1Y", 0.0111}, {"2Y", 0.0093}, {"5Y", 0.0074}, {"10Y", 0.0074}, {"30Y", 0.0074}, {"Inflation", 0.0111}};

    irDeltaRiskCorrelations_ = {
        {1.00, 0.91, 0.72, 0.55, 0.31, 0.40},
        {0.91, 1.00, 0.87, 0.72, 0.45, 0.40},
        {0.72, 0.87, 1.00, 0.91, 0.68, 0.40},
        {0.55, 0.72, 0.91, 1.00, 0.83, 0.40},
        {0.31, 0.45, 0.68, 0.83, 1.00, 0.40},
        {0.40, 0.40, 0.40, 0.40, 0.40, 1.00}
    };

    cptyDeltaBucketCorrelations_ = {
        {1.00, 0.10, 0.20, 0.25, 0.20, 0.15, 0.00, 0.45},
        {0.10, 1.00, 0.05, 0.15, 0.20, 0.05, 0.00, 0.45},
        {0.20, 0.05, 1.00, 0.20, 0.25, 0.05, 0.00, 0.45},
        {0.25, 0.15, 0.20, 1.00, 0.25, 0.05, 0.00, 0.45},
        {0.20, 0.20, 0.25, 0.25, 1.00, 0.05, 0.00, 0.45},
        {0.15, 0.05, 0.05, 0.05, 0.05, 1.00, 0.00, 0.45},
        {0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1.00, 0.00},
        {0.45, 0.45, 0.45, 0.45, 0.45, 0.45, 0.00, 1.00}
    };

    riskFactors_[make_pair(CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Delta)] =  {"1Y", "2Y", "5Y", "10Y", "30Y", "Inflation"};
    riskFactors_[make_pair(CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Delta)] =  {"FXSpot"};
    riskFactors_[make_pair(CvaRiskFactorKey::KeyType::InterestRate, CvaRiskFactorKey::MarginType::Vega)] =  {"IRVolatility", "InflationVolatilty"};
    riskFactors_[make_pair(CvaRiskFactorKey::KeyType::ForeignExchange, CvaRiskFactorKey::MarginType::Vega)] =  {"FXVolatility"};
    
    nettingSets_.insert("");
    for (const auto& cr : cvaNetSensitivities) {
        nettingSets_.insert(cr.nettingSetId);
    }

}

void StandardApproachCvaCalculator::checkRiskFactor(const CvaRiskFactorKey::KeyType& riskType, const CvaRiskFactorKey::MarginType& marginType, 
    const std::string& riskFactor) {

    if (riskType == CvaRiskFactorKey::KeyType::CreditCounterparty) {
        vector<string> tokens;
        boost::split(tokens, riskFactor, boost::is_any_of("/"));
        QL_REQUIRE(tokens.size() == 2, "only 2 tokens expected");
        // check that token parses to period
        ore::data::parsePeriod(tokens[1]);
        return;
    }
    std::pair<CvaRiskFactorKey::KeyType, CvaRiskFactorKey::MarginType> pair(riskType, marginType);
    QL_REQUIRE(riskFactors_.find(pair) != riskFactors_.end(), "no risk factors found for " << riskType << "/" << marginType);

    QL_REQUIRE(std::find(riskFactors_[pair].begin(), riskFactors_[pair].end(), riskFactor) != riskFactors_[pair].end(),
        "risk factor not found " << riskFactor);
}

Real StandardApproachCvaCalculator::getRiskFactorCorrelation(const CvaRiskFactorKey::KeyType& riskType, const std::string& bucket, 
    const CvaRiskFactorKey::MarginType& marginType, const std::string& riskFactor_1, const std::string& riskFactor_2) {
    if (riskFactor_1 == riskFactor_2) 
        return 1;
    if (riskType == CvaRiskFactorKey::KeyType::InterestRate) {
        if (marginType == CvaRiskFactorKey::MarginType::Delta) {
            if (std::find(irRiskWeightCcys_.begin(), irRiskWeightCcys_.end(), bucket) == irRiskWeightCcys_.end()) {
                return 0.4;
            } else {
                std::pair<CvaRiskFactorKey::KeyType, CvaRiskFactorKey::MarginType> pair(riskType, marginType);
                auto itr_1 = std::find(riskFactors_[pair].begin(), riskFactors_[pair].end(), riskFactor_1);
                auto idx_1 = std::distance(riskFactors_[pair].begin(), itr_1);
                auto itr_2 = std::find(riskFactors_[pair].begin(), riskFactors_[pair].end(), riskFactor_2);
                auto idx_2 = std::distance(riskFactors_[pair].begin(), itr_2);

                return irDeltaRiskCorrelations_[idx_1][idx_2];
            }
        } else if (marginType == CvaRiskFactorKey::MarginType::Vega) {
            return 0.4;
        } else {
            QL_FAIL("marginType " << marginType << " is not currently supported");
        }
    } else if (riskType == CvaRiskFactorKey::KeyType::ForeignExchange) {
        QL_FAIL("there should only be one risk factor for an Fx Sensitivity, but two (" << riskFactor_1 << " " << riskFactor_2 << ") have been provided");
    } else if (riskType == CvaRiskFactorKey::KeyType::CreditCounterparty) {
        Size bucket_int = ore::data::parseInteger(bucket);
        QL_REQUIRE(bucket_int > 0 && bucket_int < 9, "bucket is expected to be between 1 and 8");

        vector<string> tokens_1;
        boost::split(tokens_1, riskFactor_1, boost::is_any_of("/"));

        vector<string> tokens_2;
        boost::split(tokens_2, riskFactor_2, boost::is_any_of("/"));

        Real corrTenor = (tokens_1[1] == tokens_2[1]) ? 1.0 : 0.9;
        QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_1 = counterpartyManager_->get(tokens_1[0]);
        QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp_2 = counterpartyManager_->get(tokens_2[0]);
        Real corrQuality = (cp_1->creditQuality() == cp_2->creditQuality()) ? 1.0 : 0.8;
        
        Real corrName = counterpartyManager_->counterpartyCorrelations()->lookup(tokens_1[0], tokens_2[0]);
        return corrTenor * corrName * corrQuality;
    } else {
        QL_FAIL("riskType: " << riskType << " is not currently supported");
    }
}

Real StandardApproachCvaCalculator::getBucketCorrelation(const CvaRiskFactorKey::KeyType& riskType, const std::string& bucket_1, const std::string& bucket_2) {
    if (bucket_1 == bucket_2) 
        return 1;
    if (riskType == CvaRiskFactorKey::KeyType::InterestRate) {
        // For interest rate delta and vega risks, cross-bucket correlation is 0.5 for all currency pairs.
        return 0.5;
    } else if (riskType == CvaRiskFactorKey::KeyType::ForeignExchange) {
        // For FX delta and vega risks, cross-bucket correlation is 0.6 for all currency pairs.
        return 0.6;
    } else if (riskType == CvaRiskFactorKey::KeyType::CreditCounterparty) {
        Size idx_1 = ore::data::parseInteger(bucket_1) - 1;
        Size idx_2 = ore::data::parseInteger(bucket_2) - 1;

        QL_REQUIRE(idx_1 < cptyDeltaBucketCorrelations_.size(), "No known correlation for " << bucket_1);
        QL_REQUIRE(idx_2 < cptyDeltaBucketCorrelations_[idx_1].size(), "No known correlation for " << bucket_2);

        return cptyDeltaBucketCorrelations_[idx_1][idx_2];
    } else {
        QL_FAIL("riskType: " << riskType << " is not currently supported");
    }
}

QuantLib::Real StandardApproachCvaCalculator::getRiskWeight(const CvaRiskFactorKey::KeyType& riskType, const std::string& bucket, const CvaRiskFactorKey::MarginType& marginType, 
    const std::string& riskFactor) {
    if (riskType == CvaRiskFactorKey::KeyType::InterestRate) {
        if (marginType == CvaRiskFactorKey::MarginType::Delta) {
            if (std::find(irRiskWeightCcys_.begin(), irRiskWeightCcys_.end(), bucket) == irRiskWeightCcys_.end()) {
                return 0.0158;
            } else {
                QL_REQUIRE(irDeltaRiskWeights_.find(riskFactor) != irDeltaRiskWeights_.end(), "no IR risk weight found for risk factor " << riskFactor);
                return irDeltaRiskWeights_[riskFactor];
            }
        } else if (marginType == CvaRiskFactorKey::MarginType::Vega) {
            return 1.0;
        } else {
            QL_FAIL("marginType " << marginType << " is not currently supported");
        }
    } else if (riskType == CvaRiskFactorKey::KeyType::ForeignExchange) {
        if (marginType == CvaRiskFactorKey::MarginType::Delta) {
            return 0.11;
        } else if (marginType == CvaRiskFactorKey::MarginType::Vega) {
            return 1.0;
        } else {
            QL_FAIL("marginType " << marginType << " is not currently supported");
        }
    } else if (riskType == CvaRiskFactorKey::KeyType::CreditCounterparty) {
        if (marginType == CvaRiskFactorKey::MarginType::Delta) {
            vector<string> tokens;
            boost::split(tokens, riskFactor, boost::is_any_of("/"));

            QuantLib::ext::shared_ptr<ore::data::CounterpartyInformation> cp = counterpartyManager_->get(tokens[0]);
            Real riskWeight = cp->baCvaRiskWeight();
            QL_REQUIRE(riskWeight != QuantLib::Null<Real>(), "missing risk weight for " << tokens[0]);
        
            return riskWeight;
        } else {
            QL_FAIL("Only Delta margin is supported for " << riskType);
        }
    } else {
        QL_FAIL("riskType: " << riskType << " is not currently supported");
    }
}

QuantLib::Real StandardApproachCvaCalculator::getHedgeSensi(const CvaRiskFactorKey::KeyType& rt, const std::string& b, const CvaRiskFactorKey::MarginType& mt, const std::string& rf, const QuantLib::Real& cvaSensi) {
    
    if (std::find(perfectHedges_.begin(), perfectHedges_.end(), make_pair(rt, mt)) != perfectHedges_.end()) {
        return cvaSensi;
    } else if (unhedged_) {
        return 0.0;
    } else {
        auto& indexRiskFactor = cvaNetSensitivities_.get<CvaRiskFactorTag>();
        if (indexRiskFactor.count(QuantLib::ext::make_tuple("", rt, b, mt, rf, CvaType::CvaHedge)) == 0)
            QL_FAIL("no hedge sensitivity found for " << rt << " " << b << " " << mt << " " << rf << " ");
        auto it = indexRiskFactor.find(QuantLib::ext::make_tuple("", rt, b, mt, rf, CvaType::CvaHedge));
        return it->value;
    }
}

void StandardApproachCvaCalculator::calculate() {
    openReports();
    auto& indexRiskType = cvaNetSensitivities_.get<CvaRiskTypeTag>();
    auto& indexRiskFactor = cvaNetSensitivities_.get<CvaRiskFactorTag>();
    
    vector<CvaRiskFactorKey::KeyType> riskTypes = { CvaRiskFactorKey::KeyType::InterestRate, 
                        CvaRiskFactorKey::KeyType::ForeignExchange, 
                        CvaRiskFactorKey::KeyType::CreditCounterparty,
                        CvaRiskFactorKey::KeyType::CreditReference,
                        CvaRiskFactorKey::KeyType::Equity,
                        CvaRiskFactorKey::KeyType::Commodity};
    vector<CvaRiskFactorKey::MarginType> marginTypes = { CvaRiskFactorKey::MarginType::Delta, 
                        CvaRiskFactorKey::MarginType::Vega};
    
    for (auto n : nettingSets_) {
        Real cva = 0;
        for (auto rt : riskTypes) {
            Real cvaRiskType = 0;
            for (auto mt : marginTypes) {
                set<std::string> buckets;
                set<std::string> riskFactors;
                auto p = indexRiskType.equal_range(QuantLib::ext::make_tuple(n, rt, mt));
                while (p.first != p.second) {
                    buckets.insert(p.first->bucket);
                    checkRiskFactor(rt, mt, p.first->riskFactor);
                    riskFactors.insert(p.first->riskFactor);
                    ++p.first;
                }
                map<std::string, Real> kb;
                map<std::string, Real> sb;
                for (auto b : buckets) {
                    map<string, Real> ws;
                    vector<Real> ws_hdg;
                    for (auto rf : riskFactors) {
                        if (indexRiskFactor.count(QuantLib::ext::make_tuple(n, rt, b, mt, rf, CvaType::CvaAggregate)) != 0) { 
                            auto it = indexRiskFactor.find(QuantLib::ext::make_tuple(n, rt, b, mt, rf, CvaType::CvaAggregate));
                            Real sk_cva = it->value;
                            Real sk_hdg = getHedgeSensi(rt, b, mt, rf, sk_cva);
                            Real rw = getRiskWeight(rt, b, mt, rf);
                            ws.insert(make_pair(rf, rw * (sk_cva - sk_hdg)));
                            ws_hdg.push_back(rw * sk_hdg);
                            
                            addDetailRow(n, rt, b, mt, rf, CvaType::CvaAggregate, sk_cva, rw);
                            addDetailRow(n, rt, b, mt, rf, CvaType::CvaHedge, sk_hdg, rw);
                        }
                    }

                    Real sum_ws = 0;
                    Real sum_ws_sq = 0;
                    Real sum_ws_hdg_sq = 0;
                    Real R = 0.01;

                    for (auto it_1 : ws) {
                        sum_ws += it_1.second;
                        for (auto it_2 : ws) {
                            sum_ws_sq += it_1.second * it_2.second * getRiskFactorCorrelation(rt, b, mt, it_1.first, it_2.first);
                        }
                    }

                    for (auto h : ws_hdg) {
                        sum_ws_hdg_sq += h * h;
                    }

                    Real k = std::sqrt(sum_ws_sq + R * sum_ws_hdg_sq);
                    sb[b] = std::max( - 1 * k, std::min(sum_ws, k));
                    kb[b] = k;

                    cvaRiskTypeResults_[SaCvaSummaryKey(n, rt, mt, b)] = k;
                }

                Real mcva = 1;// A bank’s relevant supervisor may require a bank to use a higher value of mCVA
                Real sum_kb = 0;
                Real sum_sb = 0;
                for (auto it : kb) {
                    sum_kb += it.second * it.second;
                }
                for (auto it_1 : sb) {
                    for (auto it_2 : sb) {
                        if (it_1.first != it_2.first)
                            sum_sb += it_1.second * it_2.second * getBucketCorrelation(rt, it_1.first, it_2.first);
                    }
                }
                Real marginRisk = mcva * std::sqrt(sum_kb + sum_sb);
                cvaRiskType += marginRisk;
                cvaRiskTypeResults_[SaCvaSummaryKey(n, rt, mt, "All")] = marginRisk;

            }
            cva += cvaRiskType;
        }
        cvaNettingSetResults_[n] = cva;
    }

    writeSummaryReport();
    closeReports();
}

void StandardApproachCvaCalculator::openReports() {
    if (reports_.count(ReportType::Detail) > 0) {
        reports_.at(ReportType::Detail)
            ->addColumn("NettingSetId", string())
            .addColumn("RiskType", string())
            .addColumn("Bucket", string())
            .addColumn("MarginType", string())
            .addColumn("RiskFactor", string())
            .addColumn("CvaType", string())
            .addColumn("Sensitivity", double(), 4)
            .addColumn("RiskWeight", double(), 4);
    }

    if (reports_.count(ReportType::Summary) > 0 ) {

        reports_.at(ReportType::Summary)
            ->addColumn("NettingSetId", string())
            .addColumn("RiskType", string())
            .addColumn("MarginType", string())
            .addColumn("Bucket", string())
            .addColumn("Analytic", string())
            .addColumn("Value", double(), 4);
    }
}

void StandardApproachCvaCalculator::closeReports() {
    if (reports_.count(ReportType::Summary) > 0) {
        reports_.at(ReportType::Summary)->end();
    }

    if (reports_.count(ReportType::Detail) > 0) {
        reports_.at(ReportType::Detail)->end();
    }
}

void StandardApproachCvaCalculator::addDetailRow(const std::string& nettingSetId, const CvaRiskFactorKey::KeyType& riskType, 
        const std::string& bucket, const CvaRiskFactorKey::MarginType& marginType, const std::string& riskFactor, 
        const CvaType& cvaType, QuantLib::Real sensi, QuantLib::Real riskWeight) {

    if (reports_.count(ReportType::Detail) == 0)
        return;

    reports_.at(ReportType::Detail)
        ->next()
        .add(nettingSetId)
        .add(to_string(riskType))
        .add(bucket)
        .add(to_string(marginType))
        .add(riskFactor)
        .add(to_string(cvaType))
        .add(sensi)
        .add(riskWeight);

}

void StandardApproachCvaCalculator::writeSummaryReport() {

    if (reports_.count(ReportType::Summary) > 0 ) {
        for (auto c : cvaNettingSetResults_) {
            reports_.at(ReportType::Summary)->next()
                .add(c.first)
                .add("All")
                .add("All")
                .add("All")
                .add("SA_CVA_CAPITAL")
                .add(c.second);

        }
        for (auto r : cvaRiskTypeResults()) {
            if (r.second > 0) {
                reports_.at(ReportType::Summary)->next()
                    .add(r.first.nettingSet)
                    .add(to_string(r.first.keyType))
                    .add(to_string(r.first.marginType))
                    .add(r.first.bucket)
                    .add("SA_CVA_CAPITAL")
                    .add(r.second);
            }
        }
    }

}
} // namespace analytics
} // namespace ore
