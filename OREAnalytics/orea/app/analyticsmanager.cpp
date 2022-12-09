/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <orea/app/analyticsmanager.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>

#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>

using namespace std;
using namespace boost::filesystem;
using ore::data::InMemoryReport;

namespace ore {
namespace analytics {

Size matches(const std::vector<std::string>& requested, const std::vector<std::string>& available) {
    Size count = 0;
    for (auto r : requested) {
        if (std::find(available.begin(), available.end(), r) != available.end())
            count++;
    }
    return count;
}
    
AnalyticsManager::AnalyticsManager(const boost::shared_ptr<InputParameters>& inputs, 
                                   const boost::shared_ptr<MarketDataLoader>& marketDataLoader,
                                   std::ostream& out)
    : inputs_(inputs), marketDataLoader_(marketDataLoader), out_(out) {    
    
    addAnalytic("NPV", boost::make_shared<PricingAnalytic>(inputs_, out_));
    addAnalytic("VAR", boost::make_shared<VarAnalytic>(inputs_, out_));
    addAnalytic("XVA", boost::make_shared<XvaAnalytic>(inputs_, out_));
}

void AnalyticsManager::addAnalytic(const std::string& label, const boost::shared_ptr<Analytic>& analytic) {
    QL_REQUIRE(analytics_.find(label) == analytics_.end(), "trying to add an analytic with duplicate label: " << label);
    // Label is not necessarily a valid analytics type
    // Get the latter via analytic->analyticTypes()
    LOG("register analytic with label '" << label << "' and sub-analytics " << to_string(analytic->analyticTypes()));
    analytics_[label] = analytic;
    // force this so that we update valid analytics vector with the next call to validAnalytics()
    validAnalytics_.clear();
}
    
const std::vector<std::string>& AnalyticsManager::validAnalytics() {
    if (validAnalytics_.size() == 0) {
        for (auto a : analytics_) {
            const std::vector<std::string>& types = a.second->analyticTypes();
            validAnalytics_.insert(validAnalytics_.end(), types.begin(), types.end());
        }
    }
    return validAnalytics_;
}

bool AnalyticsManager::isValidAnalytic(const std::string& type) {
    const std::vector<std::string>& va = validAnalytics();
    return std::find(va.begin(), va.end(), type) != va.end();
}

void AnalyticsManager::runAnalytics(const std::vector<std::string>& runTypes,
                                    const boost::shared_ptr<MarketCalibrationReport>& marketCalibrationReport) {

    if (analytics_.size() == 0)
        return;

    std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> tmps;
    for (const auto& a : analytics_) {
        std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> atmps = a.second->todaysMarketParams();
        tmps.insert(end(tmps), begin(atmps), end(atmps));
    }

    // QuantExt::Date mporDate = QuantExt::Date();
    // if (laggedMarket_)
    //     mporDate = inputs_->mporCalendar().advance(inputs_->asof(), inputs_->mporDays(), QuantExt::Days);

    // Do we need market data
    bool requireMarketData = false;
    for (const auto& tmp : tmps) {
        if (!tmp->empty())
            requireMarketData = true;
    }

    LOG("AnalyticsManager::runAnalytics: requireMarketData " << (requireMarketData ? "Y" : "N"));
    
    if (requireMarketData) {
        // load the market data
        if (tmps.size() > 0) {
            LOG("AnalyticsManager::runAnalytics: populate loader");
            marketDataLoader_->populateLoader(tmps);
        }
        
        // write out market data files
        boost::shared_ptr<InMemoryReport> mdReport = boost::make_shared<InMemoryReport>();
        boost::shared_ptr<InMemoryReport> fixingReport = boost::make_shared<InMemoryReport>();
        boost::shared_ptr<InMemoryReport> dividendReport = boost::make_shared<InMemoryReport>();

        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeMarketData(*mdReport, marketDataLoader_->loader(), inputs_->asof(), marketDataLoader_->quotes(),
                             !inputs_->entireMarket());
        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeFixings(*fixingReport, marketDataLoader_->loader());
        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeDividends(*dividendReport, marketDataLoader_->loader());

        /*
        mdReport->toFile(path(inputs_->resultsPath() / "marketdata.csv").string(), ',', true, inputs_->csvQuoteChar(),
                         inputs_->reportNaString());
        fixingReport->toFile(path(inputs_->resultsPath() / "fixings.csv").string(), ',', true, inputs_->csvQuoteChar(),
                             inputs_->reportNaString());
        dividendReport->toFile(path(inputs_->resultsPath() / "dividends.csv").string(), ',', true, inputs_->csvQuoteChar(),
                             inputs_->reportNaString());
        */
        
        marketDataReports_["MARKETDATA"][""] = mdReport;
        marketDataReports_["FIXINGS"][""] = fixingReport;
        marketDataReports_["DIVIDENDS"][""] = dividendReport;
    }

    // run requested run types across all analytics
    for (auto a : analytics_) {
        if (matches(runTypes, a.second->analyticTypes()) > 0) {
            LOG("run analytic with label '" << a.first << "'");
            a.second->runAnalytic(marketDataLoader_->loader(), runTypes);
            LOG("run analytic with label '" << a.first << "' finished.");
            // then populate the market calibration report if required
            if (marketCalibrationReport)
                a.second->marketCalibration(marketCalibrationReport);
        }
    }
    if (marketCalibrationReport)
        marketCalibrationReport->outputCalibrationReport();
}

Analytic::analytic_reports const AnalyticsManager::reports() {
    Analytic::analytic_reports reports = marketDataReports_;    
    for (auto a : analytics_) {
        auto rs = a.second->reports();
        reports.insert(rs.begin(), rs.end());
    }
    return reports;
}

Analytic::analytic_npvcubes const AnalyticsManager::npvCubes() {
    Analytic::analytic_npvcubes results;
    for (auto a : analytics_) {
        auto rs = a.second->npvCubes();
        results.insert(rs.begin(), rs.end());
    }
    return results;
}

Analytic::analytic_mktcubes const AnalyticsManager::mktCubes() {
    Analytic::analytic_mktcubes results;
    for (auto a : analytics_) {
        auto rs = a.second->mktCubes();
        results.insert(rs.begin(), rs.end());
    }
    return results;
}

// boost::shared_ptr<AnalyticsManager> parseAnalytics(const std::string& s,
//     const boost::shared_ptr<InputParameters>& inputs,
//     const boost::shared_ptr<MarketDataLoader>& marketDataLoader) {
//     DLOG("Parse Analytics Request " << s);
//     vector<string> analyticStrs;
//     boost::split(analyticStrs, s, boost::is_any_of(", |;:"));
//     return boost::make_shared<AnalyticsManager>(analyticStrs, inputs, marketDataLoader);
// }

}
}
