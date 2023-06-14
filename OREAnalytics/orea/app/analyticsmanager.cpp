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
#include <orea/app/analytics/pricinganalytic.hpp>
#include <orea/app/analytics/varanalytic.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
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

Size matches(const std::set<std::string>& requested, const std::set<std::string>& available) {
    Size count = 0;
    for (auto r : requested) {
        if (available.find(r) != available.end())
            count++;
    }
    return count;
}
    
AnalyticsManager::AnalyticsManager(const boost::shared_ptr<InputParameters>& inputs, 
                                   const boost::shared_ptr<MarketDataLoader>& marketDataLoader)
    : inputs_(inputs), marketDataLoader_(marketDataLoader) {    
    
    addAnalytic("MARKETDATA", boost::make_shared<MarketDataAnalytic>(inputs));
    addAnalytic("PRICING", boost::make_shared<PricingAnalytic>(inputs));
    addAnalytic("VAR", boost::make_shared<VarAnalytic>(inputs_));
    addAnalytic("XVA", boost::make_shared<XvaAnalytic>(inputs_));
}

void AnalyticsManager::clear() {
    LOG("AnalyticsManager: Remove all analytics currently registered");
    analytics_.clear();
    validAnalytics_.clear();
}
    
void AnalyticsManager::addAnalytic(const std::string& label, const boost::shared_ptr<Analytic>& analytic) {
    // Allow overriding, but warn 
    if (analytics_.find(label) != analytics_.end()) {
        WLOG("Overwriting analytic with label " << label);
    }

    // Label is not necessarily a valid analytics type
    // Get the latter via analytic->analyticTypes()
    LOG("register analytic with label '" << label << "' and sub-analytics " << to_string(analytic->analyticTypes()));
    analytics_[label] = analytic;
    // This forces an update of valid analytics vector with the next call to validAnalytics()
    validAnalytics_.clear();
}

const std::set<std::string>& AnalyticsManager::validAnalytics() {
    if (validAnalytics_.size() == 0) {
        for (auto a : analytics_) {
            const std::set<std::string>& types = a.second->analyticTypes();
            validAnalytics_.insert(types.begin(), types.end());
        }
    }
    return validAnalytics_;
}

const std::set<std::string>& AnalyticsManager::requestedAnalytics() {
    return requestedAnalytics_;
}
    
bool AnalyticsManager::hasAnalytic(const std::string& type) {
    const std::set<std::string>& va = validAnalytics();
    return va.find(type) != va.end();
}

const boost::shared_ptr<Analytic>& AnalyticsManager::getAnalytic(const std::string& type) const {
    for (const auto& a : analytics_) {
        const std::set<std::string>& types = a.second->analyticTypes();
        if (types.find(type) != types.end())
            return a.second;
    }
    QL_FAIL("analytic type " << type << " not found, check validAnalytics()");
}

void AnalyticsManager::runAnalytics(const std::set<std::string>& analyticTypes,
                                    const boost::shared_ptr<MarketCalibrationReport>& marketCalibrationReport) {

    requestedAnalytics_ = analyticTypes;
    
    if (analytics_.size() == 0)
        return;

    std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> tmps;
    std::set<Date> marketDates;
    for (const auto& a : analytics_) {
        std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> atmps = a.second->todaysMarketParams();
        tmps.insert(end(tmps), begin(atmps), end(atmps));
        auto mdates = a.second->marketDates();
        marketDates.insert(mdates.begin(), mdates.end());
    }

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
            marketDataLoader_->populateLoader(tmps, marketDates);
        }
        
        boost::shared_ptr<InMemoryReport> mdReport = boost::make_shared<InMemoryReport>();
        boost::shared_ptr<InMemoryReport> fixingReport = boost::make_shared<InMemoryReport>();
        boost::shared_ptr<InMemoryReport> dividendReport = boost::make_shared<InMemoryReport>();

        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeMarketData(*mdReport, marketDataLoader_->loader(), inputs_->asof(),
                             marketDataLoader_->quotes()[inputs_->asof()],
                             !inputs_->entireMarket());
        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeFixings(*fixingReport, marketDataLoader_->loader());
        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeDividends(*dividendReport, marketDataLoader_->loader());

        marketDataReports_["MARKETDATA"]["marketdata"] = mdReport;
        marketDataReports_["FIXINGS"]["fixings"] = fixingReport;
        marketDataReports_["DIVIDENDS"]["dividends"] = dividendReport;
    }

    // run requested analytics
    for (auto a : analytics_) {
        if (matches(analyticTypes, a.second->analyticTypes()) > 0) {
            LOG("run analytic with label '" << a.first << "'");
            a.second->runAnalytic(marketDataLoader_->loader(), analyticTypes);
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

std::map<std::string, Size> checkReportNames(const ore::analytics::Analytic::analytic_reports& rpts) {                                     
    std::map<std::string, Size> m;
    for (const auto& rep : rpts) {
        for (auto b : rep.second) {
            string reportName = b.first;
            auto it = m.find(reportName);
            if (it == m.end())
                m[reportName] = 1;
            else
                m[reportName] ++;
        }
    }
    for (auto r : m) {
        LOG("report name " << r.first << " occurs " << r.second << " times");
    }
    return m;
}

bool endsWith(const std::string& name, const std::string& suffix) {
    if (suffix.size() > name.size())
        return false;
    else
        return std::equal(suffix.rbegin(), suffix.rend(), name.rbegin());
}

void AnalyticsManager::toFile(const ore::analytics::Analytic::analytic_reports& rpts, const std::string& outputPath,
                              const std::map<std::string, std::string>& reportNames, const char sep,
                              const bool commentCharacter, char quoteChar, const string& nullString,
                              const std::set<std::string>& lowerHeaderReportNames) {
    std::map<std::string, Size> hits = checkReportNames(rpts);    
    for (const auto& rep : rpts) {
        string analytic = rep.first;
        for (auto b : rep.second) {
            string reportName = b.first;
            boost::shared_ptr<InMemoryReport> report = b.second;
            string fileName;
            auto it = hits.find(reportName);
            QL_REQUIRE(it != hits.end(), "something wrong here");
            if (it->second == 1) {
                // The report name is unique, check whether we want to rename it or use the standard name
                auto it2 = reportNames.find(reportName);
                fileName = it2 != reportNames.end() ? it2->second : reportName;
            }
            else {
                ALOG("Report " << reportName << " occurs " << it->second << " times, fix report naming");
                fileName = analytic + "_" + reportName + "_" + to_string(hits[fileName]);
            }

            // attach a suffix only if it does not have one already
            string suffix = "";
            if (!endsWith(fileName,".csv") && !endsWith(fileName, ".txt"))
                suffix = ".csv";
            std::string fullFileName = outputPath + "/" + fileName + suffix;

            report->toFile(fullFileName, sep, commentCharacter, quoteChar, nullString,
                           lowerHeaderReportNames.find(reportName) != lowerHeaderReportNames.end());
            LOG("report " << reportName << " written to " << fullFileName); 
        }
    }
}
}
}
