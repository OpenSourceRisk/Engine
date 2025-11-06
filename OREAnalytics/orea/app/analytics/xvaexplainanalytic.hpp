/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file orea/app/analytics/xvaanalytic.hpp
    \brief xva explain analytic
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/report/inmemoryreport.hpp>
namespace ore {
namespace analytics {

class XvaExplainResults {

public:
    struct XvaReportKey {
        std::string tradeId;
        std::string nettingSet;
    };

    XvaExplainResults(const QuantLib::ext::shared_ptr<InMemoryReport>& xvaReport);

    const std::map<XvaReportKey, double>& baseCvaData() const {return baseCvaData_;}
    const std::map<XvaReportKey, double>& fullRevalCva() const {return fullRevalCva_;}
    const std::map<RiskFactorKey, std::map<XvaReportKey, double>>& fullRevalScenarioCva() const {
        return fullRevalScenarioCva_;
    }
    const std::set<RiskFactorKey::KeyType> keyTypes() const { return keyTypes_; }

private:
    std::map<XvaReportKey, double> baseCvaData_;
    std::map<XvaReportKey, double> fullRevalCva_;
    std::map<RiskFactorKey, std::map<XvaReportKey, double>> fullRevalScenarioCva_;
    std::set<RiskFactorKey::KeyType> keyTypes_;
};

bool operator<(const XvaExplainResults::XvaReportKey& lhs, const XvaExplainResults::XvaReportKey& rhs);

//! Explain market implied XVA changes by full revalulation in par rate domain
//! Time and portfolio effects are excluded by this explain
class XvaExplainAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "XVA_EXPLAIN";
    explicit XvaExplainAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs);
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
    std::vector<QuantLib::Date> additionalMarketDates() const override;

private:
    QuantLib::ext::shared_ptr<StressTestScenarioData>
    createStressTestData(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) const;

    QuantLib::Date mporDate_;
};

class XvaExplainAnalytic : public Analytic {
public:
    explicit XvaExplainAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager)
        : Analytic(std::make_unique<XvaExplainAnalyticImpl>(inputs), {"XVA_EXPLAIN"}, inputs, analyticsManager, true,
                   false, false, false) {}
};

} // namespace analytics
} // namespace ore
