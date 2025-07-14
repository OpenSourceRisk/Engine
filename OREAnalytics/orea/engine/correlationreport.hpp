/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file engine/varcalculator.hpp
    \brief Base class for a var calculation
    \ingroup engine
*/

#pragma once

#include <ored/report/report.hpp>
#include <qle/math/covariancesalvage.hpp>
#include <orea/engine/historicalsensipnlcalculator.hpp>
#include <orea/engine/historicalpnlgenerator.hpp>

namespace ore {
namespace analytics {


class CorrelationReport{
public:
    CorrelationReport(const QuantLib::ext::shared_ptr<ScenarioReader>& scenario,
                      const std::string& correlationMethod,
                      boost::optional<ore::data::TimePeriod> period,
                      const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen = nullptr,
                      const QuantLib::ext::shared_ptr<ScenarioShiftCalculator>& shiftCalc = nullptr)
        : scenario_(scenario), correlationMethod_(correlationMethod), period_(period), hisScenGen_(hisScenGen),
          shiftCalc_(shiftCalc) {
    }

    void writeReports(const QuantLib::ext::shared_ptr<Report>& report);
    void calculate(const QuantLib::ext::shared_ptr<Report>& report);


protected:
    QuantLib::ext::shared_ptr<ScenarioReader> scenario_;
    std::string correlationMethod_;
    boost::optional<ore::data::TimePeriod> period_;
    QuantLib::ext::shared_ptr<HistoricalSensiPnlCalculator> sensiPnlCalculator_;
    QuantLib::ext::shared_ptr<HistoricalScenarioGenerator> hisScenGen_;
    QuantLib::ext::shared_ptr<ScenarioShiftCalculator> shiftCalc_;
    QuantLib::Matrix covarianceMatrix_;
    QuantLib::Matrix correlationMatrix_;
    std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> correlationPairs_;
    std::vector<QuantLib::ext::shared_ptr<PNLCalculator>> pnlCalculators_;
    
    ore::data::TimePeriod covariancePeriod() const { return period_.value(); } 
    std::vector<ore::data::TimePeriod> timePeriods() { return {period_.get()}; }
};

} // namespace analytics
} // namespace ore
