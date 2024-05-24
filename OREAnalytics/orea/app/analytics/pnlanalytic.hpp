/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file orea/app/analytics/pnlanalytic.hpp
    \brief ORE NPV Lagged Analytic
*/
#pragma once

#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/analytics/pricinganalytic.hpp>
#include <orea/app/analytics/scenarioanalytic.hpp>
#include <orea/app/analytic.hpp>

namespace ore {
namespace analytics {

class PnlAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "PNL";
    static constexpr const char* mporLookupKey = "MPOR";

    PnlAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic::Impl(inputs), useSpreadedTermStructures_(true) {
        setLabel(LABEL);
        mporDate_ = inputs_->mporDate() != Date()
                        ? inputs_->mporDate()
                        : inputs_->mporCalendar().advance(inputs_->asof(), int(inputs_->mporDays()), QuantExt::Days);
        LOG("ASOF date " << io::iso_date(inputs_->asof()));
        LOG("MPOR date " << io::iso_date(mporDate_));
    
        auto mporAnalytic = AnalyticFactory::instance().build("SCENARIO", inputs);
        if (mporAnalytic.second) {
            auto sai = static_cast<ScenarioAnalyticImpl*>(mporAnalytic.second->impl().get());
            sai->setUseSpreadedTermStructures(true);
            addDependentAnalytic(mporLookupKey, mporAnalytic.second);
        }
    }
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

    bool useSpreadedTermStructures() const { return useSpreadedTermStructures_; }
    const QuantLib::Date& mporDate() const { return mporDate_; }
    std::vector<QuantLib::Date> additionalMarketDates() const override { return {mporDate_}; }
    
    const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& t0Scenario() const { return t0Scenario_; }
    const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& t1Scenario() const { return t1Scenario_; }
    void setT0Scenario(const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& scenario) { t0Scenario_ = scenario; }
    void setT1Scenario(const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& scenario) { t1Scenario_ = scenario; }

private:
    bool useSpreadedTermStructures_ = true;
    QuantLib::Date mporDate_;
    QuantLib::ext::shared_ptr<ore::analytics::Scenario> t0Scenario_, t1Scenario_;
};

/*!
  The P&L Analytic generates a P&L report as main output with the following columns
  - TradeId and 
  - Maturity and MaturityTime
  - StartDate and EndDate of the P&L period, referred to as t0 and t1 below
  - NPV(t0)
  - NPV(asof=t0;mkt=t1)
  - NPV(asof=t1;mkt=t0)
  - NPV(t1)
  - PeriodCashFlow: Aggregate of trade flows in the period, converted into the P&L currency below, compounding?
  - Theta: NPV(asof=t1;mkt=t0) - NPV(t0), Cash Flows?
  - HypotheticalCleanPnL: NPV(asof=t0;mkt=t1) - NPV(t0)
  - CleanPnL: NPV(t1) - NPV(t0) + PeriodCashFlow 
  - DirtyPnL: NPV(t1) - NPV(t0)
  - Currency

  Moreover
  - four corresponding NPV and additional results reports are generated
  - market scenarios used for the two "lagged" NPV calculations are written as reports 
  
*/
class PnlAnalytic : public Analytic {
public:
    PnlAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic(std::make_unique<PnlAnalyticImpl>(inputs), {"PNL"}, inputs, false, false, false, false) {}

};

} // namespace analytics
} // namespace ore
