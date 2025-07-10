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

/*! \file orea/engine/standardapproachcvacalculator.hpp
    \brief Class for the Standardized Approach  CVA calculation
*/

#pragma once

#include <orea/engine/sacvasensitivityrecord.hpp>
#include <ored/portfolio/counterpartymanager.hpp>
#include <ored/report/report.hpp>

#include <ql/types.hpp>
#include <map>
#include <string>

namespace ore {
namespace analytics {

struct SaCvaSummaryKey {
    std::string nettingSet;
    CvaRiskFactorKey::KeyType keyType;
    CvaRiskFactorKey::MarginType marginType;
    std::string bucket;

    SaCvaSummaryKey(std::string n, CvaRiskFactorKey::KeyType kt, CvaRiskFactorKey::MarginType mt, std::string b);

    bool operator<(const SaCvaSummaryKey& sr) const;
};

//! A class for calculating Standard Approach CVA capital charge
class StandardApproachCvaCalculator {
public:
    enum class ReportType { Summary, Detail };
    
    StandardApproachCvaCalculator(const std::string& calculationCcy, const SaCvaNetSensitivities& cvaNetSensitivities,
        const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager, 
        const std::map<ReportType, QuantLib::ext::shared_ptr<ore::data::Report>>& outReports, bool unhedgedSensitivity = false, 
        const std::vector<std::string>& perfectHedges = std::vector<std::string>());

    //! a virtual function for calculating the CVA captial charge
    void calculate();

    const std::map<SaCvaSummaryKey, QuantLib::Real> cvaRiskTypeResults() { return cvaRiskTypeResults_; }
    const std::map<std::string, QuantLib::Real> cvaNettingSetResults() { return cvaNettingSetResults_; }
    
    //! check if something is a valid risk factor
    void checkRiskFactor(const CvaRiskFactorKey::KeyType& riskType, const CvaRiskFactorKey::MarginType& marginType, const std::string& riskFactor);
    QuantLib::Real getRiskFactorCorrelation(const CvaRiskFactorKey::KeyType& riskType, const std::string& bucket, const CvaRiskFactorKey::MarginType& marginType, const std::string& riskFactor_1, const std::string& riskFactor_2);
    QuantLib::Real getBucketCorrelation(const CvaRiskFactorKey::KeyType& riskType, const std::string& bucket_1, const std::string& bucket_2);
    QuantLib::Real getRiskWeight(const CvaRiskFactorKey::KeyType& riskType, const std::string& bucket, const CvaRiskFactorKey::MarginType& marginType, const std::string& riskFactor);
    QuantLib::Real getHedgeSensi(const CvaRiskFactorKey::KeyType& riskType, const std::string& bucket, const CvaRiskFactorKey::MarginType& marginType, 
        const std::string& riskFactor, const QuantLib::Real& cvaSensi);

private:
    //! write any passed in reports
    void openReports();
    void closeReports();
    void addDetailRow(const std::string& nettingSetId, const CvaRiskFactorKey::KeyType& riskType, 
        const std::string& bucket, const CvaRiskFactorKey::MarginType& marginType, const std::string& riskFactor, 
        const CvaType& cvaType, QuantLib::Real sensi, QuantLib::Real riskWeight);
    void writeSummaryReport();

    SaCvaNetSensitivities cvaNetSensitivities_;
    QuantLib::ext::shared_ptr<ore::data::CounterpartyManager> counterpartyManager_;

    std::vector<std::string> irRiskWeightCcys_;
    std::map<std::string, QuantLib::Real> irDeltaRiskWeights_;
    std::vector<std::vector<QuantLib::Real>> irDeltaRiskCorrelations_;
    std::vector<std::vector<QuantLib::Real>> cptyDeltaBucketCorrelations_;

    std::map<std::pair<CvaRiskFactorKey::KeyType, CvaRiskFactorKey::MarginType>, std::vector<std::string>>  riskFactors_;

    std::map<SaCvaSummaryKey, QuantLib::Real> cvaRiskTypeResults_;
    std::map<std::string, QuantLib::Real> cvaNettingSetResults_;
    std::map<ReportType, QuantLib::ext::shared_ptr<ore::data::Report>> reports_;

    std::vector<std::pair<CvaRiskFactorKey::KeyType, CvaRiskFactorKey::MarginType>> perfectHedges_;
    std::set<std::string> nettingSets_;
    bool unhedged_;
};

} // namespace analytics
} // namespace ore
