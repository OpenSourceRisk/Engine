/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <orea/app/reportwriters/scenarioreportwriter.hpp>

#include <orea/scenario/historicalscenariogenerator.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/scenariowriter.hpp>

#include <ored/utilities/to_string.hpp>

#include <qle/math/distributioncount.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/kurtosis.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/skewness.hpp>
#include <boost/accumulators/statistics/stats.hpp>

#include <ostream>

using ore::data::to_string;
using QuantLib::Date;
using QuantExt::distributionCount;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

void ScenarioReportWriter::writeScenarioStatistics(const QuantLib::ext::shared_ptr<ScenarioGenerator>& generator,
    const std::vector<RiskFactorKey>& keys, const Size numPaths,
    const std::vector<Date>& dates, ore::data::Report& report) {
    report.addColumn("Date", Date())
        .addColumn("Key", string())
        .addColumn("min", double(), 8)
        .addColumn("mean", double(), 8)
        .addColumn("max", double(), 8)
        .addColumn("stddev", double(), 8)
        .addColumn("skewness", double(), 8)
        .addColumn("kurtosis", double(), 8);

    std::vector<boost::accumulators::accumulator_set<
        double, boost::accumulators::stats<boost::accumulators::tag::min, boost::accumulators::tag::max,
        boost::accumulators::tag::mean, boost::accumulators::tag::variance,
        boost::accumulators::tag::skewness, boost::accumulators::tag::kurtosis>>>
        acc(keys.size() * dates.size());

    for (Size i = 0; i < numPaths; ++i) {
        for (Size d = 0; d < dates.size(); ++d) {
            QuantLib::ext::shared_ptr<Scenario> currentScenario = generator->next(dates[d]);
            for (Size k = 0; k < keys.size(); ++k) {
                acc[d * keys.size() + k](currentScenario->get(keys[k]));
            }
        }
    }
    for (Size d = 0; d < dates.size(); ++d) {
        for (Size k = 0; k < keys.size(); ++k) {
            Size idx = d * keys.size() + k;
            report.next()
                .add(dates[d])
                .add(ore::data::to_string(keys[k]))
                .add(boost::accumulators::min(acc[idx]))
                .add(boost::accumulators::mean(acc[idx]))
                .add(boost::accumulators::max(acc[idx]))
                .add(std::sqrt(boost::accumulators::variance(acc[idx])));
            if (!close_enough(boost::accumulators::variance(acc[idx]), 0.0)) {
                report.add(boost::accumulators::skewness(acc[idx])).add(boost::accumulators::kurtosis(acc[idx]));
            }
            else {
                // avoid ReportWriter::non-sensical output
                report.add(0.0).add(0.0);
            }
        }
    }
    report.end();
}

void ScenarioReportWriter::writeScenarioDistributions(const QuantLib::ext::shared_ptr<ScenarioGenerator>& generator,
                                              const std::vector<RiskFactorKey>& keys, const Size numPaths,
                                              const std::vector<Date>& dates, const Size distSteps,
                                              ore::data::Report& report) {
    report.addColumn("Date", Date())
        .addColumn("Key", string())
        .addColumn("Bound", double(), 8)
        .addColumn("Count", Size());

    std::vector<std::vector<std::vector<Real>>> values(
        dates.size(), std::vector<std::vector<Real>>(keys.size(), std::vector<Real>(numPaths, 0.0)));

    for (Size i = 0; i < numPaths; ++i) {
        for (Size d = 0; d < dates.size(); ++d) {
            QuantLib::ext::shared_ptr<Scenario> currentScenario = generator->next(dates[d]);
            for (Size k = 0; k < keys.size(); ++k) {
                values[d][k][i] = currentScenario->get(keys[k]);
            }
        }
    }

    std::vector<Real> bounds;
    std::vector<Size> counts;
    for (Size d = 0; d < dates.size(); ++d) {
        for (Size k = 0; k < keys.size(); ++k) {
            distributionCount(values[d][k].begin(), values[d][k].end(), distSteps, bounds, counts);
            for (Size i = 0; i < distSteps; ++i) {
                report.next().add(dates[d]).add(ore::data::to_string(keys[k])).add(bounds[i]).add(counts[i]);
            }
        }
    }
    report.end();
}

void ScenarioReportWriter::writeHistoricalScenarioDetails(
    const QuantLib::ext::shared_ptr<ore::analytics::HistoricalScenarioGenerator>& generator, ore::data::Report& report) {

    report.addColumn("PLDate1", Date())
        .addColumn("PLDate2", Date())
        .addColumn("Key", string())
        .addColumn("BaseValue", double(), 8)
        .addColumn("AdjustmentFactor1", double(), 8)
        .addColumn("AdjustmentFactor2", double(), 8)
        .addColumn("ScenarioValue1", double(), 8)
        .addColumn("ScenarioValue2", double(), 8)
        .addColumn("ShiftType", string())
        .addColumn("Displacement", double(), 8)
        .addColumn("Return", double(), 8)
        .addColumn("ScenarioValue", double(), 8);

    Date asof = generator->baseScenario()->asof();
    for (Size i = 0; i < generator->startDates().size(); ++i) {
        std::ignore = generator->next(asof);
        for (auto const& d : generator->lastHistoricalScenarioCalculationDetails()) {
            report.next()
                .add(d.scenarioDate1)
                .add(d.scenarioDate2)
                .add(ore::data::to_string(d.key))
                .add(d.baseValue)
                .add(d.adjustmentFactor1)
                .add(d.adjustmentFactor2)
                .add(d.scenarioValue1)
                .add(d.scenarioValue2)
                .add(ore::data::to_string(d.returnType))
                .add(d.displacement)
                .add(d.returnValue)
                .add(d.scenarioValue);
        }
    }
    report.end();
}

void ScenarioReportWriter::writeHistoricalScenarioDistributions(
    QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hsgen,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
    QuantLib::ext::shared_ptr<ore::data::Report> histScenDetailsReport, QuantLib::ext::shared_ptr<ore::data::Report> statReport,
    QuantLib::ext::shared_ptr<ore::data::Report> distReport, Size distSteps) {

    // Don't leave it up to the caller to do this
    simMarket->scenarioGenerator() = hsgen;
    hsgen->baseScenario() = simMarket->baseScenario();

    // If both report pointers are null, return early
    if (!statReport && !distReport)
        return;

    // Make a transformed generator i.e. discount -> zero etc.
    auto hsgent = QuantLib::ext::make_shared<HistoricalScenarioGeneratorTransform>(hsgen, simMarket, simMarketParams);

    const vector<RiskFactorKey>& keys = hsgen->baseScenario()->keys();
    Size numScen = hsgen->numScenarios();
    Date asof = hsgen->baseScenario()->asof();

    // Write the statistics report if requested
    if (statReport) {
        hsgent->reset();
        writeScenarioStatistics(hsgent, keys, numScen, {asof}, *statReport);
    }

    // Write the distribution report if requested
    if (distReport) {
        QL_REQUIRE(distSteps != Null<Size>(),
                   "When creating a distribution report, a valid distribution step size is required");
        hsgent->reset();
        writeScenarioDistributions(hsgent, keys, numScen, {asof}, distSteps, *distReport);
    }

    // Write the scenario report if requested
    if (histScenDetailsReport) {
        hsgent->reset();
        writeHistoricalScenarioDetails(hsgent, *histScenDetailsReport);
    }
}

void ScenarioReportWriter::writeHistoricalScenarios(const QuantLib::ext::shared_ptr<HistoricalScenarioLoader>& hsloader,
                                            const QuantLib::ext::shared_ptr<ore::data::Report>& report) {
    // each scenario might have a different set of keys, so we collect the union of all keys
    // and write them out (missing keys will be written as NA to the report)
    std::set<RiskFactorKey> allKeys;
    for (const auto& s : hsloader->scenarios()[0])
        allKeys.insert(s.second->keys().begin(), s.second-> keys().end());
    ScenarioWriter sw(nullptr, report, std::vector<RiskFactorKey>(allKeys.begin(), allKeys.end()));
    bool writeHeader = true;
    for (const auto& s : hsloader->scenarios()[0]) {
        sw.writeScenario(s.second, writeHeader);
        writeHeader = false;
    }
}

} // namespace analytics
} // namespace ore
