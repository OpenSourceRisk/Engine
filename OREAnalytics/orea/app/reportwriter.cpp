/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 Copyright (C) 2017 Aareal Bank AG

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

#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <ored/report/inmemoryreport.hpp>

#include <orea/simm/utilities.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>
#include <orea/cube/sensitivitycube.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/simm/crifrecord.hpp>
#include <orea/simm/simmresults.hpp>
#include <orea/simm/crif.hpp>
#include <orea/simm/imschedulecalculator.hpp>

#include <ored/utilities/marketdata.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/model/assetmodelbuilderbase.hpp>
#include <ored/scripting/models/assetmodel.hpp>
#include <ored/scripting/models/heston.hpp>

#include <qle/currencies/currencycomparator.hpp>
#include <qle/instruments/pathlevelresult.hpp>
#include <ored/portfolio/cashflowutils.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/range/adaptor/indexed.hpp>

#include <iomanip>
#include <ostream>
#include <regex>
#include <stdio.h>

using ore::data::to_string;
using ore::data::InMemoryReport;
using QuantLib::Date;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

typedef std::map<Currency, Matrix, CurrencyComparator> result_type_matrix;
typedef std::map<Currency, std::vector<Real>, CurrencyComparator> result_type_vector;
typedef std::map<Currency, Real, CurrencyComparator> result_type_scalar;

void ReportWriter::writeScenarioReport(ore::data::Report& report,
                                       const std::vector<QuantLib::ext::shared_ptr<SensitivityCube>>& sensitivityCubes,
                                       Real outputThreshold) {

    LOG("Writing Scenario report");

    report.addColumn("TradeId", string());
    report.addColumn("Factor", string());
    report.addColumn("Up/Down", string());
    report.addColumn("Base NPV", double(), 2);
    report.addColumn("ShiftSize_1", double(), 6);
    report.addColumn("ShiftSize_2", double(), 6);
    report.addColumn("Scenario NPV", double(), 2);
    report.addColumn("Difference", double(), 2);

    for (auto const& sensitivityCube : sensitivityCubes) {

        auto scenarioDescriptions = sensitivityCube->scenarioDescriptions();
        auto tradeIds = sensitivityCube->tradeIdx();
        auto npvCube = sensitivityCube->npvCube();

        for (const auto& [tradeId, i] : tradeIds) {

            Real baseNpv = npvCube->getT0(i);
            for (const auto& [j, scenarioNpv] : npvCube->getTradeNPVs(i)) {
                auto scenarioDescription = scenarioDescriptions[j];
                Real difference = scenarioNpv - baseNpv;
                Real shift1 = scenarioDescription.key1().keytype == RiskFactorKey::KeyType::None
                                  ? Null<Real>()
                                  : sensitivityCube->actualShiftSize(scenarioDescription.key1());
                Real shift2 = scenarioDescription.key2().keytype == RiskFactorKey::KeyType::None
                                  ? Null<Real>()
                                  : sensitivityCube->actualShiftSize(scenarioDescription.key2());
                if (fabs(difference) > outputThreshold) {
                    report.next();
                    report.add(tradeId);
                    report.add(prettyPrintInternalCurveName(scenarioDescription.factors()));
                    report.add(scenarioDescription.typeString());
                    report.add(baseNpv);
                    report.add(shift1);
                    report.add(shift2);
                    report.add(scenarioNpv);
                    report.add(difference);
                } else if (!std::isfinite(difference)) {
                    // TODO: is this needed?
                    ALOG("sensitivity scenario for trade " << tradeId << ", factor " << scenarioDescription.factors()
                                                           << " is not finite (" << difference << ")");
                }
            }
        }
    }

    report.end();
    LOG("Scenario report finished");
}

namespace {
template <class T>
void addMapResults(QuantLib::ext::any resultMap, const std::string& tradeId, const std::string& resultName, Report& report) {
    T map = QuantLib::ext::any_cast<T>(resultMap);
    for (auto it : map) {
        std::string name = resultName + "_" + it.first.code();
        QuantLib::ext::any tmp = it.second;
        auto p = parseBoostAny(tmp);
        report.next().add(tradeId).add(name).add(p.first).add(p.second);
    }
}

void addAnyResults(Report& report, const std::string& tradeId, const std::string& field, const QuantLib::ext::any& result,
                 const std::size_t precision) {
    auto p = parseBoostAny(result, precision);
    if (boost::starts_with(p.first, "vector")) {
        vector<std::string> tokens;
        string vect = p.second;
        vect.erase(remove(vect.begin(), vect.end(), '\"'), vect.end());
        if (vect.empty())
            return;
        boost::split(tokens, vect, boost::is_any_of(","));
        for (Size i = 0; i < tokens.size(); ++i) {
            boost::trim(tokens[i]);
            report.next().add(tradeId).add(field + "[" + std::to_string(i) + "]").add(p.first.substr(7)).add(tokens[i]);
        }
    } else {
        report.next().add(tradeId).add(field).add(p.first).add(p.second);
    }
}
} // namespace

void ReportWriter::writeAdditionalResultsReport(Report& report, QuantLib::ext::shared_ptr<Portfolio> portfolio,
                                                QuantLib::ext::shared_ptr<Market> market,
                                                const std::string& configuration, const std::string& baseCurrency,
                                                const std::size_t precision) {

    LOG("Writing AdditionalResults report");

    report.addColumn("TradeId", string())
        .addColumn("ResultId", string())
        .addColumn("ResultType", string())
        .addColumn("ResultValue", string());

    for (auto & [tId, trade] : portfolio->trades()) {
        try {
            // we first add any additional trade data.
            string tradeId = tId;
            string tradeType = trade->tradeType();
            Real notional2 = Null<Real>();
            string notional2Ccy = "";
            // Get the additional data for the current instrument.
            auto additionalData = trade->additionalData();
            for (const auto& kv : additionalData) {
                addAnyResults(report, tradeId, kv.first, kv.second, precision);
            }
            // if the 'notional[2]' has been provided convert it to base currency
            if (additionalData.count("notional[2]") != 0 && additionalData.count("notionalCurrency[2]") != 0) {
                notional2 = trade->additionalDatum<Real>("notional[2]");
                notional2Ccy = trade->additionalDatum<string>("notionalCurrency[2]");
            }

            auto additionalResults = trade->instrument()->additionalResults();
            if (additionalResults.count("notional[2]") != 0 && additionalResults.count("notionalCurrency[2]") != 0) {
                notional2 = trade->instrument()->qlInstrument()->result<Real>("notional[2]");
                notional2Ccy = trade->instrument()->qlInstrument()->result<string>("notionalCurrency[2]");
            }

            if (notional2 != Null<Real>() && notional2Ccy != "") {
                Real fx = 1.0;
                if (notional2Ccy != baseCurrency)
                    fx = market->fxRate(notional2Ccy + baseCurrency, configuration)->value();
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(8) << notional2 * fx;
                // report.next().add(tradeId).add("notionalInBaseCurrency[2]").add("double").add(oss.str());
            }

            // Just use the unadjusted trade ID in the additional results report for the main instrument.
            // If we have one or more additional instruments, use "_i" as suffix where i = 1, 2, 3, ... for each
            // additional instrument in turn and underscore as prefix to reduce risk of ID clash. We also add the
            // multiplier as an extra additional result if additional results exist.
            auto instruments = trade->instrument()->additionalInstruments();
            auto multipliers = trade->instrument()->additionalMultipliers();
            QL_REQUIRE(instruments.size() == multipliers.size(),
                       "Expected the number of "
                           << "additional instruments (" << instruments.size() << ") to equal the number of "
                           << "additional multipliers (" << multipliers.size() << ").");

            for (Size i = 0; i <= instruments.size(); ++i) {

                if (i > 0 && instruments[i - 1] == nullptr)
                    continue;

                std::map<std::string, QuantLib::ext::any> thisAddResults =
                    i == 0 ? additionalResults : instruments[i - 1]->additionalResults();

                // Trade ID suffix for additional instruments. Put underscores to reduce risk of clash with other IDs in
                // the portfolio (still a risk).
                tradeId = i == 0 ? trade->id() : ("_" + trade->id() + "_" + std::to_string(i));

                // Add the multiplier if there are additional results.
                // Check on 'instMultiplier' already existing is probably unnecessary.
                if (!thisAddResults.empty() && thisAddResults.count("instMultiplier") == 0) {
                    thisAddResults["instMultiplier"] =
                        i == 0 ? trade->instrument()->multiplier() * trade->instrument()->multiplier2()
                               : multipliers[i - 1];
                }

                // Write current instrument's additional results.
                for (const auto& kv : thisAddResults) {
                    // some results are stored as maps. We loop over these so that there is one result per line
                    if (kv.second.type() == typeid(result_type_matrix)) {
                        addMapResults<result_type_matrix>(kv.second, tradeId, kv.first, report);
                    } else if (kv.second.type() == typeid(result_type_vector)) {
                        addMapResults<result_type_vector>(kv.second, tradeId, kv.first, report);
                    } else if (kv.second.type() == typeid(result_type_scalar)) {
                        addMapResults<result_type_scalar>(kv.second, tradeId, kv.first, report);
                    } else {
                        addAnyResults(report, tradeId, kv.first, kv.second, precision);
                    }
                }
            }

            // add coupon additional results

            for (Size i = 0; i < trade->legs().size(); ++i) {
                for (Size j = 0; j < trade->legs()[i].size(); ++j) {
                    auto const& c = trade->legs()[i][j];
                    try {
                        for (auto const& kv : c->additionalResults()) {
                            addAnyResults(report, tradeId,
                                          kv.first + "[" + std::to_string(i) + "][" + std::to_string(j) + "]",
                                          kv.second, precision);
                        }
                    } catch (...) {
                    }
                }
            }
        } catch (const std::exception& e) {
            StructuredTradeErrorMessage(trade->id(), trade->tradeType(),
                                        "Error during trade pricing (additional results)", e.what())
                .log();
        }
    }

    report.end();

    LOG("AdditionalResults report written");
}

void ReportWriter::writeAdditionalResultsPathLevelReport(ore::data::Report& report,
                                                         const ext::shared_ptr<Portfolio>& portfolio,
                                                         const std::size_t precision) {
    LOG("Write additional results path level report");
    report.addColumn("TradeId", string())
        .addColumn("ResultId", string())
        .addColumn("Index", Size())
        .addColumn("Date", Date())
        .addColumn("Time", double(), precision)
        .addColumn("Path", Size())
        .addColumn("Value", double(), precision);
    for (auto const& [tId, trade] : portfolio->trades()) {
        try {
            for (auto const& [label, result] : trade->instrument()->additionalResults()) {
                if (result.type() == typeid(std::vector<PathLevelResult>)) {
                    for (auto const& p : ext::any_cast<const std::vector<PathLevelResult>&>(result)) {
                        for (Size i = 0; i < p.values.size(); ++i) {
                            report.next()
                                .add(tId)
                                .add(p.resultId)
                                .add(p.index)
                                .add(p.date)
                                .add(p.time)
                                .add(i)
                                .add(p.values[i]);
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            // any exception is reported in additional results report already
        }
    }
    report.end();
    LOG("Write additional results path breakdown report written");
}

void ReportWriter::addMarketDatum(Report& report, const ore::data::MarketDatum& md, const Date& actualDate) {
    const Date& d = actualDate == Null<Date>() ? md.asofDate() : actualDate;
    report.next().add(d).add(md.name()).add(md.quote()->value());
}

void ReportWriter::writeMarketData(Report& report, const QuantLib::ext::shared_ptr<Loader>& loader, const Date& asof,
                                   const set<string>& quoteNames, bool returnAll) {

    LOG("Writing MarketData report");

    report.addColumn("datumDate", Date()).addColumn("datumId", string()).addColumn("datumValue", double(), 10);

    if (returnAll) {
        for (const auto& md : loader->loadQuotes(asof)) {
            addMarketDatum(report, *md, loader->actualDate());
        }
        return;
    }

    set<string> names;
    set<string> regexStrs;
    partitionQuotes(quoteNames, names, regexStrs);

    vector<std::regex> regexes;
    regexes.reserve(regexStrs.size());
    for (const auto& regexStr : regexStrs) {
        regexes.push_back(std::regex(regexStr));
    }

    for (const auto& md : loader->loadQuotes(asof)) {
        const auto& mdName = md->name();

        if (names.find(mdName) != names.end()) {
            addMarketDatum(report, *md, loader->actualDate());
            continue;
        }

        // This could be slow
        for (const auto& regex : regexes) {
            if (std::regex_match(mdName, regex)) {
                addMarketDatum(report, *md, loader->actualDate());
                break;
            }
        }
    }

    report.end();
    LOG("MarketData report written");
}

void ReportWriter::writeFixings(Report& report, const QuantLib::ext::shared_ptr<Loader>& loader) {

    LOG("Writing Fixings report");

    report.addColumn("fixingDate", Date()).addColumn("fixingId", string()).addColumn("fixingValue", double(), 10);

    for (const auto& f : loader->loadFixings()) {
        report.next().add(f.date).add(f.name).add(f.fixing);
    }

    report.end();
    LOG("Fixings report written");
}

void ReportWriter::writeDividends(Report& report, const QuantLib::ext::shared_ptr<Loader>& loader) {

    LOG("Writing Dividends report");

    report.addColumn("dividendExDate", Date())
        .addColumn("equityId", string())
        .addColumn("dividendRate", double(), 10)
        .addColumn("dividendPaymentDate", Date());

    for (const auto& f : loader->loadDividends()) {
        report.next().add(f.exDate).add(f.name).add(f.rate).add(f.payDate);
    }

    report.end();
    LOG("Dividends report written");
}

void ReportWriter::writePricingStats(ore::data::Report& report, const QuantLib::ext::shared_ptr<Portfolio>& portfolio) {

    LOG("Writing Pricing stats report");

    report.addColumn("TradeId", string())
        .addColumn("TradeType", string())
        .addColumn("NumberOfPricings", Size())
        .addColumn("CumulativeTiming", Size())
        .addColumn("AverageTiming", Size());

    for (auto const& [tid, trade] : portfolio->trades()) {
        std::size_t num = trade->getNumberOfPricings();
        Size cumulative = trade->getCumulativePricingTime() / 1000;
        Size average = num > 0 ? cumulative / num : 0;
        report.next().add(tid).add(trade->tradeType()).add(num).add(cumulative).add(average);
    }

    report.end();
    LOG("Pricing stats report written");
}

void ReportWriter::writeRunTimes(ore::data::Report& report, const Timer& timer) {

    LOG("Writing runtimes report");

    report.addColumn("Key", string())
        .addColumn("Total", Size())
        .addColumn("Count", Size())
        .addColumn("Max", Size())
        .addColumn("Min", Size())
        .addColumn("Average", double(), 2);
    for (const auto& [key, stats] : timer.getTimes()) {
        Size totalTime = stats.totalTime / 1000;
        Size maxTime = stats.maxTime / 1000;
        Size minTime = stats.minTime / 1000;
        Size count = stats.count;
        Real averageTime = stats.avgTime() / 1000;
        report.next()
            .add(boost::algorithm::join(key, "|"))
            .add(totalTime)
            .add(count)
            .add(maxTime)
            .add(minTime)
            .add(averageTime);
    }

    report.end();
    LOG("Finished writing runtimes report")
}

void ReportWriter::writeCube(ore::data::Report& report, const QuantLib::ext::shared_ptr<NPVCube>& cube,
                             const std::map<std::string, std::string>& nettingSetMap) {
    LOG("Writing cube report");

    report.addColumn("Id", string())
        .addColumn("NettingSet", string())
        .addColumn("DateIndex", Size())
        .addColumn("Date", string())
        .addColumn("Sample", Size())
        .addColumn("Depth", Size())
        .addColumn("Value", double(), 4);

    const map<string, Size>& idsAndPos = cube->idsAndIndexes();
    vector<string> dateStrings(cube->numDates());
    for (Size i = 0; i < cube->numDates(); ++i) {
        std::ostringstream oss;
        oss << QuantLib::io::iso_date(cube->dates()[i]);
        dateStrings[i] = oss.str();
    }

    std::ostringstream oss;
    oss << QuantLib::io::iso_date(cube->asof());
    string asofString = oss.str();

    vector<string> ids(idsAndPos.size());
    vector<string> nettingSetIds(idsAndPos.size());
    for (const auto& [id, idCubePos] : idsAndPos) {
        ids[idCubePos] = id;
        auto it = nettingSetMap.find(id);
        if (it != nettingSetMap.end())
            nettingSetIds[idCubePos] = it->second;
        else
            nettingSetIds[idCubePos] = "";
    }

    // T0
    for (Size i = 0; i < ids.size(); ++i) {
        report.next();
        report.add(ids[i])
            .add(nettingSetIds[i])
            .add(static_cast<Size>(0))
            .add(asofString)
            .add(static_cast<Size>(0))
            .add(static_cast<Size>(0))
            .add(cube->getT0(i));
    }
    
    // Cube
    for (Size i = 0; i < ids.size(); i++) {
        for (Size j = 0; j < cube->numDates(); j++) {
            for (Size k = 0; k < cube->samples(); k++) {
                for (Size l = 0; l < cube->depth(); l++) {
                    report.next();
                    report.add(ids[i])
                        .add(nettingSetIds[i])
                        .add(j + 1)
                        .add(dateStrings[j])
                        .add(k+1)
                        .add(l)
                        .add(cube->get(i, j, k, l));
                }
            }
        }
    }

    report.end();

    LOG("Cube report written");
}

void ReportWriter::writeModelCalibrationReport(ore::data::Report& report, const ext::shared_ptr<Portfolio>& portfolio) {

    report.addColumn("TradeID", string())
        .addColumn("Index", string())
        .addColumn("Name", string())
        .addColumn("Value", string());

    for (const auto& [id, trade] : portfolio->trades()) {
        try {
            const auto& additionalResults = trade->instrument()->additionalResults();
            if (auto r = additionalResults.find("Heston.calibration"); r != additionalResults.end()) {
                DLOG("MultiAssetHeston calibration found in additional results for trade " << id);
                const std::vector<AssetModelCalibrationResults>& results =
                    QuantLib::ext::any_cast<const std::vector<AssetModelCalibrationResults>&>(r->second);
                for (const auto& result : results) {
                    if (result.constantParameters.size() > 0) {
                        Size n = result.constantParameters.size();
                        DLOG("constant paramters: " << n);
                        for (Size i = 0; i < n; ++i)
                            report.next()
                                .add(id)
                                .add(result.indexName)
                                .add(result.constantParameters[i].first)
                                .add(to_string(result.constantParameters[i].second));
                    } else if (result.piecewiseParameters.size() > 0) {
                        Size n = result.piecewiseParameters.size();
                        DLOG("piecewise paramters: " << n);
                        for (Size i = 0; i < n; ++i)
                            report.next()
                                .add(id)
                                .add(result.indexName)
                                .add(result.piecewiseParameters[i].first)
                                .add(result.piecewiseParameters[i].second);
                    }
                    report.next().add(id).add(result.indexName).add("Error").add(to_string(result.rmse));
                }
            }
        } catch (std::exception& e) {
            ALOG("error getting results for trade " << id << ": " << e.what());
        }
    }
    report.end();
}

void ReportWriter::writeModelCalibrationDetailReport(ore::data::Report& report, const ext::shared_ptr<Portfolio>& portfolio) {
    report.addColumn("TradeID", string())
        .addColumn("Index", string())
        .addColumn("Expiry", Period())
        .addColumn("Moneyness", double(), 4)
        .addColumn("MarketValue", double(), 4)
        .addColumn("ModelValue", double(), 4)
        .addColumn("MarketVol", double(), 4)
        .addColumn("ModelVol", double(), 4)
        .addColumn("VolDifference", double(), 4);

    for (const auto& [id, trade] : portfolio->trades()) {
        try {
            const auto& additionalResults = trade->instrument()->additionalResults();
            if (auto r = additionalResults.find("Heston.calibration"); r != additionalResults.end()) {
                DLOG("MultiAssetHeston calibration found in additional results for trade " << id);
                const std::vector<AssetModelCalibrationResults>& results =
                    QuantLib::ext::any_cast<const std::vector<AssetModelCalibrationResults>&>(r->second);
                for (const auto& result : results) {
                    for (const auto& helper : result.data) {
                        report.next()
                            .add(id)
                            .add(result.indexName)
                            .add(helper.expiry)
                            .add(helper.moneyness)
                            .add(helper.marketValue)
                            .add(helper.modelValue)
                            .add(helper.marketVol)
                            .add(helper.modelVol)
                            .add(helper.modelVol - helper.marketVol);
                    }
                }
            }
        } catch (std::exception& e) {
            ALOG("error getting results for trade " << id << ": " << e.what());
        }
    }
    report.end();
}

void ReportWriter::writeStockSplitReport(const QuantLib::ext::shared_ptr<Scenario>& baseScenario,
                                         const QuantLib::ext::shared_ptr<ore::analytics::HistoricalScenarioLoader>& hsloader,
                                         const QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors>& adjFactors,
                                         const QuantLib::ext::shared_ptr<ore::data::Report>& report) {

    report->addColumn("EquityId", string())
        .addColumn("Date", Date())
        .addColumn("Price", double(), 8)
        .addColumn("Factor", double(), 8)
        .addColumn("CumulatedFactor", double(), 8)
        .addColumn("AdjustedPrice", double(), 8);

    if (adjFactors) {
        std::set<std::string> names;
        for (auto const& k : baseScenario->keys()) {
            if (k.keytype == RiskFactorKey::KeyType::EquitySpot) {
                names.insert(k.name);
            }
        }

        std::vector<QuantLib::Date> hsdates = hsloader->dates();

        for (auto const& name : names) {

            std::set<QuantLib::Date> dates = adjFactors->dates(name);
            dates.insert(hsdates.begin(), hsdates.end());

            for (auto const& d : dates) {

                Real price = Null<Real>();
                if (std::find(hsdates.begin(), hsdates.end(), d) != hsdates.end()) {
                    auto scen = hsloader->getScenario(d);
                    RiskFactorKey rf(RiskFactorKey::KeyType::EquitySpot, name);
                    if (scen->has(rf))
                        price = scen->get(rf);
                }
                Real factor = adjFactors->getFactorContribution(name, d);
                Real cumFactor = adjFactors->getFactor(name, d);
                Real adjPrice = price == Null<Real>() ? Null<Real>() : price * cumFactor;

                report->next().add(name).add(d).add(price).add(factor).add(cumFactor).add(adjPrice);
            }
        }
    }
    report->end();
}

// Ease notation again
typedef CrifRecord::ProductClass ProductClass;
typedef SimmConfiguration::RiskClass RiskClass;
typedef SimmConfiguration::MarginType MarginType;
typedef SimmConfiguration::SimmSide SimmSide;
typedef IMScheduleCalculator::IMScheduleTradeData IMScheduleTradeData;

void ReportWriter::writeIMScheduleSummaryReport(
    const map<SimmSide, map<NettingSetDetails, pair<CrifRecord::Regulation, IMScheduleResults>>>& finalResultsMap,
    const QuantLib::ext::shared_ptr<Report> report, const bool hasNettingSetDetails, const string& simmResultCcy,
    const string& reportCcy, Real fxSpot, Real outputThreshold) {

    LOG("Writing IM Schedule results summary report.");

    // netting set headers
    report->addColumn("Portfolio", string());
    if (hasNettingSetDetails) {
        for (const string& field : NettingSetDetails::optionalFieldNames())
            report->addColumn(field, string());
    }

    report->addColumn("ProductClass", string())
        .addColumn("GrossIM", double(), 2)
        .addColumn("GrossCurrentRC", double(), 2)
        .addColumn("NetCurrentRC", double(), 2)
        .addColumn("NetToGrossRatio", double(), 6)
        .addColumn("Side", string())
        .addColumn("Regulation", string())
        .addColumn("ScheduleIM", double(), 2)
        .addColumn("Currency", string());
    if (!reportCcy.empty()) {
        report->addColumn("ScheduleIM(Report)", double(), 2).addColumn("ReportCurrency", string());
    }

    const vector<SimmSide> sides({SimmSide::Call, SimmSide::Post});
    for (const SimmSide side : sides) {
        const string& sideString = to_string(side);

        // Variable to hold sum of schedule IM over all portfolios
        Real sumSideScheduleIM = 0.0;
        Real sumSideScheduleIMReporting = 0.0;

        std::set<CrifRecord::Regulation> winningRegs;
        if (finalResultsMap.find(side) != finalResultsMap.end()) {
            for (const auto& nv : finalResultsMap.at(side)) {
                const NettingSetDetails& portfolioId = nv.first;
                const CrifRecord::Regulation& regulation = nv.second.first;
                const IMScheduleResults& results = nv.second.second;

                winningRegs.insert(regulation);

                QL_REQUIRE(results.currency() == simmResultCcy,
                           "writeIMScheduleSummaryReport(): IMSchedule results ("
                               << results.currency() << ") should be denominated in the SIMM result currency ("
                               << simmResultCcy << ").");

                // Loop over the results for this portfolio
                for (const auto& imScheduleResult : results.data()) {
                    ProductClass pc = imScheduleResult.first;
                    IMScheduleResult result = imScheduleResult.second;

                    Real im = pc == ProductClass::All ? result.scheduleIM : result.grossIM;

                    report->next();
                    const map<string, string> nettingSetMap = portfolioId.mapRepresentation();
                    for (const string& field : NettingSetDetails::fieldNames(hasNettingSetDetails)) {
                        report->add(nettingSetMap.at(field));
                    }
                    report->add(to_string(pc))
                        .add(result.grossIM)
                        .add(result.grossRC)
                        .add(result.netRC)
                        .add(result.NGR)
                        .add(sideString)
                        .add(ore::data::to_string(regulation))
                        .add(im)
                        .add(results.currency());

                    if (!reportCcy.empty()) {
                        Real scheduleIMReporting = im * fxSpot;
                        report->add(scheduleIMReporting).add(reportCcy);

                        if (pc == ProductClass::All)
                            sumSideScheduleIMReporting += scheduleIMReporting;
                    }

                    if (pc == ProductClass::All)
                        sumSideScheduleIM += result.scheduleIM;
                }
            }
        }

        // Write out a row for the aggregate IM over all portfolios
        // We only write out this row if either reporting ccy was provided or if currency of all the results is the same
        string finalWinningReg =
            winningRegs.size() > 1 || winningRegs.empty() ? "" : ore::data::to_string(*winningRegs.begin());

        // Write out common columns
        report->next();
        Size numNettingSetFields = NettingSetDetails::fieldNames(hasNettingSetDetails).size();
        for (Size t = 0; t < numNettingSetFields; t++)
            report->add("All");
        report->add(to_string(ProductClass::All))
            .add(Null<Real>())
            .add(Null<Real>())
            .add(Null<Real>())
            .add(Null<Real>())
            .add(sideString)
            .add(finalWinningReg)
            .add(sumSideScheduleIM)
            .add(simmResultCcy);

        // Write out schedule IM in reporting currency if we can
        if (!reportCcy.empty())
            report->add(sumSideScheduleIMReporting).add(reportCcy);
    }

    report->end();

    LOG("IM Schedule results summary report written.");
}

void ReportWriter::writeIMScheduleTradeReport(const map<string, vector<IMScheduleTradeData>>& tradeResults,
                                              const QuantLib::ext::shared_ptr<ore::data::Report> report,
                                              const bool hasNettingSetDetails) {

    LOG("Writing IM Schedule trade results report.");

    report->addColumn("TradeId", string());

    // netting set headers
    report->addColumn("Portfolio", string());
    if (hasNettingSetDetails) {
        for (const string& field : NettingSetDetails::optionalFieldNames())
            report->addColumn(field, string());
    }

    report->addColumn("ProductClass", string())
        .addColumn("EndDate", string())
        .addColumn("Maturity", double(), 5)
        .addColumn("Label", string())
        .addColumn("Multiplier", double(), 2)
        .addColumn("Notional", double(), 2)
        .addColumn("NotionalCurrency", string())
        .addColumn("PV", double(), 2)
        .addColumn("PVCurrency", string())
        .addColumn("Notional(Base)", double(), 2)
        .addColumn("PV(Base)", double(), 2)
        .addColumn("BaseCurrency", string())
        .addColumn("GrossIM(Base)", double(), 2)
        .addColumn("CollectRegulations", string())
        .addColumn("PostRegulations", string());

    // Variable to hold sum of schedule IM over all portfolios
    for (const auto& kv : tradeResults) {
        const string& tradeId = kv.first;

        for (const IMScheduleTradeData& tradeData : kv.second) {
            const NettingSetDetails& portfolioId = tradeData.nettingSetDetails;

            // Write row if IM not negligible relative to outputThreshold.
            report->next();
            report->add(tradeId);

            const map<string, string> nettingSetMap = portfolioId.mapRepresentation();
            for (const string& field : NettingSetDetails::fieldNames(hasNettingSetDetails)) {
                report->add(nettingSetMap.at(field));
            }
            string collectRegsString = escapeCommaSeparatedList(regulationsToString(tradeData.collectRegulations), '\0');
            string postRegsString = escapeCommaSeparatedList(regulationsToString(tradeData.postRegulations), '\0');
            
            report->add(to_string(tradeData.productClass))
                .add(to_string(tradeData.endDate))
                .add(tradeData.maturity)
                .add(tradeData.labelString)
                .add(tradeData.multiplier)
                .add(tradeData.notional)
                .add(tradeData.notionalCcy)
                .add(tradeData.presentValue)
                .add(tradeData.presentValueCcy)
                .add(tradeData.notionalCalc)
                .add(tradeData.presentValueCalc)
                .add(tradeData.calculationCcy)
                .add(tradeData.grossMarginCalc)
                .add(collectRegsString)
                .add(postRegsString);
        }
    }

    report->end();

    LOG("IM Schedule trade results report written.");
}

void ReportWriter::writePnlReport(ore::data::Report& report,
            const ext::shared_ptr<InMemoryReport>& t0NpvReport,
            const ext::shared_ptr<InMemoryReport>& t0m1p0NpvReport,
            const ext::shared_ptr<InMemoryReport>& t1m0p0NpvReport,
            const ext::shared_ptr<InMemoryReport>& t1m1p0NpvReport,
            const ext::shared_ptr<InMemoryReport>& t1m0p1NpvReport,
            const ext::shared_ptr<InMemoryReport>& t1m1p1NpvReport,
            const std::map<std::string, std::vector<ore::data::TradeCashflowReportData>>& t0TradeCashflows,
            const Date& startDate, const Date& endDate,
            const std::string& baseCurrency,
            const ext::shared_ptr<ore::data::Market>& market,
            const std::string& configuration,
            const ext::shared_ptr<Portfolio>& portfolio) {
  
    LOG("PnL report");

    report.addColumn("TradeId", string())
        .addColumn("TradeType", string())
        .addColumn("Maturity", Date())
        .addColumn("MaturityTime", double(), 6)
        .addColumn("StartDate", Date())
        .addColumn("EndDate", Date())
        .addColumn("NPV(t0_m0_p0)", double(), 6) // renamed from NPV(t0)
        .addColumn("NPV(t0_m1_p0)", double(), 6) // renamed from NPV(asof=t0;mkt=t1)
        .addColumn("NPV(t1_m0_p0)", double(), 6) // renamed from NPV(asof=t1;mkt=t0)
        .addColumn("NPV(t1_m1_p0)", double(), 6) // renamed from NPV(t1;portfolio=t0)
        .addColumn("NPV(t1_m0_p1)", double(), 6) // new column
        .addColumn("NPV(t1_m1_p1)", double(), 6) // renamed from NPV(t1)
        .addColumn("Day1PnL", double(), 6)
        .addColumn("TradeChangePnL", double(), 6)
        .addColumn("PeriodCashFlow", double(), 6)
        .addColumn("New", double(), 6)
        .addColumn("Matured", double(), 6)
        .addColumn("Terminated", double(), 6)
        .addColumn("Amendments", double(), 6)
        .addColumn("Theta", double(), 6)
        .addColumn("HypotheticalCleanPnL", double(), 6)
        .addColumn("CleanPnL", double(), 6)
        .addColumn("DirtyPnL", double(), 6)
        .addColumn("Currency", string());

    Size tradeIdColumn = 0;
    Size tradeTypeColumn = 1;
    Size maturityDateColumn = 2;
    Size maturityTimeColumn = 3;
    Size npvBaseColumn = 6;
    Size baseCcyColumn = 7;
    
    // t0 NPV = NPV(t0;m0;p0)
    QL_REQUIRE(t0NpvReport->rows() == t0m1p0NpvReport->rows(), "different number of rows in npv reports");
    QL_REQUIRE(t0NpvReport->rows() == t1m0p0NpvReport->rows(), "different number of rows in npv reports");

    QL_REQUIRE(t0NpvReport->header(tradeIdColumn) == "TradeId", "incorrect trade id column " << tradeIdColumn);
    QL_REQUIRE(t0NpvReport->header(tradeTypeColumn) == "TradeType", "incorrect trade type column " << tradeTypeColumn);
    QL_REQUIRE(t0NpvReport->header(maturityDateColumn) == "Maturity", "incorrect maturity date column " << maturityDateColumn);
    QL_REQUIRE(t0NpvReport->header(maturityTimeColumn) == "MaturityTime", "incorrect maturity time column " << maturityTimeColumn);
    QL_REQUIRE(t0NpvReport->header(npvBaseColumn) == "NPV(Base)", "incorrect npv base column " << npvBaseColumn);
    QL_REQUIRE(t0NpvReport->header(baseCcyColumn) == "BaseCurrency", "incorrect base currency column " << baseCcyColumn);

    // t0 portfolio as of t0 using the t1 market = NPV(t0;m1;p0)
    QL_REQUIRE(t0m1p0NpvReport->header(tradeIdColumn) == "TradeId", "incorrect trade id column " << tradeIdColumn);
    QL_REQUIRE(t0m1p0NpvReport->header(tradeTypeColumn) == "TradeType", "incorrect trade type column " << tradeTypeColumn);
    QL_REQUIRE(t0m1p0NpvReport->header(maturityDateColumn) == "Maturity", "incorrect maturity date column " << maturityDateColumn);
    QL_REQUIRE(t0m1p0NpvReport->header(maturityTimeColumn) == "MaturityTime", "incorrect maturity time column " << maturityTimeColumn);
    QL_REQUIRE(t0m1p0NpvReport->header(npvBaseColumn) == "NPV(Base)", "incorrect npv base column " << npvBaseColumn);
    QL_REQUIRE(t0m1p0NpvReport->header(baseCcyColumn) == "BaseCurrency", "incorrect base currency column " << baseCcyColumn);

    // t0 portfolio as of t1 using the t0 market = NPV(t1;m0;p0)
    QL_REQUIRE(t1m0p0NpvReport->header(tradeIdColumn) == "TradeId", "incorrect trade id column " << tradeIdColumn);
    QL_REQUIRE(t1m0p0NpvReport->header(tradeTypeColumn) == "TradeType", "incorrect trade type column " << tradeTypeColumn);
    QL_REQUIRE(t1m0p0NpvReport->header(maturityDateColumn) == "Maturity", "incorrect maturity date column " << maturityDateColumn);
    QL_REQUIRE(t1m0p0NpvReport->header(maturityTimeColumn) == "MaturityTime", "incorrect maturity time column " << maturityTimeColumn);
    QL_REQUIRE(t1m0p0NpvReport->header(npvBaseColumn) == "NPV(Base)", "incorrect npv base column " << npvBaseColumn);
    QL_REQUIRE(t1m0p0NpvReport->header(baseCcyColumn) == "BaseCurrency", "incorrect base currency column " << baseCcyColumn);

    // t1 portfolio as of t1 using the t0 market = NPV(t1;m0;p1)
    QL_REQUIRE(t1m0p1NpvReport->header(tradeIdColumn) == "TradeId", "incorrect trade id column " << tradeIdColumn);
    QL_REQUIRE(t1m0p1NpvReport->header(tradeTypeColumn) == "TradeType", "incorrect trade type column " << tradeTypeColumn);
    QL_REQUIRE(t1m0p1NpvReport->header(maturityDateColumn) == "Maturity", "incorrect maturity date column " << maturityDateColumn);
    QL_REQUIRE(t1m0p1NpvReport->header(maturityTimeColumn) == "MaturityTime", "incorrect maturity time column " << maturityTimeColumn);
    QL_REQUIRE(t1m0p1NpvReport->header(npvBaseColumn) == "NPV(Base)", "incorrect npv base column " << npvBaseColumn);
    QL_REQUIRE(t1m0p1NpvReport->header(baseCcyColumn) == "BaseCurrency", "incorrect base currency column " << baseCcyColumn);

    // t1 portfolio as of t1 using the t1 market = NPV(t1;m1;p1)
    QL_REQUIRE(t1m1p1NpvReport->header(tradeIdColumn) == "TradeId", "incorrect trade id column " << tradeIdColumn);
    QL_REQUIRE(t1m1p1NpvReport->header(tradeTypeColumn) == "TradeType", "incorrect trade type column " << tradeTypeColumn);
    QL_REQUIRE(t1m1p1NpvReport->header(maturityDateColumn) == "Maturity", "incorrect maturity date column " << maturityDateColumn);
    QL_REQUIRE(t1m1p1NpvReport->header(maturityTimeColumn) == "MaturityTime", "incorrect maturity time column " << maturityTimeColumn);
    QL_REQUIRE(t1m1p1NpvReport->header(npvBaseColumn) == "NPV(Base)", "incorrect npv base column " << npvBaseColumn);
    QL_REQUIRE(t1m1p1NpvReport->header(baseCcyColumn) == "BaseCurrency", "incorrect base currency column " << baseCcyColumn);

    map<string, Size> t1IdMap;
    for (Size j = 0; j < t1m1p1NpvReport->rows(); ++j) {
    string tradeId = boost::get<string>(t1m1p1NpvReport->data(tradeIdColumn, j));
        t1IdMap[tradeId] = j;
    }

    std::set<string> t0Ids;
    for (Size i = 0; i < t0NpvReport->rows(); ++i) {
        string tradeId = "unknown", tradeType = "unknown";
        try {
            tradeId = boost::get<string>(t0NpvReport->data(tradeIdColumn, i));
            t0Ids.insert(tradeId);
            string tradeId2 = boost::get<string>(t0m1p0NpvReport->data(tradeIdColumn, i));
            string tradeId3 = boost::get<string>(t1m0p0NpvReport->data(tradeIdColumn, i));
            QL_REQUIRE(tradeId == tradeId2 && tradeId == tradeId3,
                       "inconsistent ordering of npv reports, got non-matching trade ids "
                           << tradeId << ", " << tradeId2 << "," << tradeId3);
            tradeType = boost::get<string>(t0NpvReport->data(tradeTypeColumn, i));
            Date maturityDate = boost::get<Date>(t0NpvReport->data(maturityDateColumn, i));
            Real maturityTime = boost::get<Real>(t0NpvReport->data(maturityTimeColumn, i));
            string ccy = boost::get<string>(t0NpvReport->data(baseCcyColumn, i));
            QL_REQUIRE(ccy == baseCurrency,
                       "inconsistent npv ccy (" << ccy << ") and base ccy (" << baseCurrency << ") for trade " << tradeId);
            Real t0Npv = boost::get<Real>(t0NpvReport->data(npvBaseColumn, i));
            Real t0m1p0Npv = boost::get<Real>(t0m1p0NpvReport->data(npvBaseColumn, i));
            Real t1m0p0Npv = boost::get<Real>(t1m0p0NpvReport->data(npvBaseColumn, i));
            Real t1m1p0Npv = boost::get<Real>(t1m1p0NpvReport->data(npvBaseColumn, i));

            auto it = t1IdMap.find(tradeId);
            Real t1m0p1Npv = it == t1IdMap.end() ? 0.0 : boost::get<Real>(t1m0p1NpvReport->data(npvBaseColumn, it->second));
            Real t1m1p1Npv = it == t1IdMap.end() ? 0.0 : boost::get<Real>(t1m1p1NpvReport->data(npvBaseColumn, it->second));
            
            Real tradeChangePnl = t1m1p1Npv - t1m1p0Npv;
            Real hypotheticalCleanPnl = t0m1p0Npv - t0Npv;
            auto cfIt = t0TradeCashflows.find(tradeId);
            Real periodFlow = cfIt == t0TradeCashflows.end() ? 0.0 : getAggregateTradeFlows(startDate, endDate,
                                                                            cfIt->second, market, configuration, baseCurrency);
            Real matured =
                (maturityDate <= endDate && close_enough(t1m1p0Npv, 0.0) && close_enough(t1m1p1Npv, 0.0)) ? t0Npv : 0.0;
            Real terminated = (close_enough(t1m1p1Npv, 0.0) && !close_enough(t1m1p0Npv, 0.0)) ? t0Npv : 0.0;
            Real amendments = (close_enough(matured, 0.0) && close_enough(terminated, 0.0)) ? t1m1p1Npv - t1m1p0Npv : 0.0;
            Real theta = t1m0p0Npv - t0Npv + periodFlow;
            Real dirtyPnl = t1m1p1Npv - t0Npv;
            Real cleanPnl = dirtyPnl + periodFlow;
            DLOG("PnL report, writing line " << i << " tradeId " << tradeId);
        
            report.next()
                .add(tradeId)
                .add(tradeType)
                .add(maturityDate)
                .add(maturityTime)
                .add(startDate)
                .add(endDate)
                .add(t0Npv)
                .add(t0m1p0Npv)
                .add(t1m0p0Npv)
                .add(t1m1p0Npv)
                .add(t1m0p1Npv)
                .add(t1m1p1Npv)
                .add(0.0)
                .add(tradeChangePnl)
                .add(periodFlow)
                .add(0.0)
                .add(matured)
                .add(terminated)
                .add(amendments)
                .add(theta)
                .add(hypotheticalCleanPnl)
                .add(cleanPnl)
                .add(dirtyPnl)
                .add(ccy);

        } catch (std::exception& e) {
            StructuredTradeErrorMessage(tradeId, tradeType, "Error writing pnl report", e.what()).log();
        }
    }

    for (const auto& tId : t1IdMap) {
        auto it = t0Ids.find(tId.first);
        if (it == t0Ids.end()) {
            string tradeId = "unknown", tradeType = "unknown";
            try {
                tradeId = tId.first;
                Size loc = tId.second;
                tradeType = boost::get<string>(t1m1p1NpvReport->data(tradeTypeColumn, loc));
                Date maturityDate = boost::get<Date>(t1m1p1NpvReport->data(maturityDateColumn, loc));
                Real maturityTime = boost::get<Real>(t1m1p1NpvReport->data(maturityTimeColumn, loc));
                string ccy = boost::get<string>(t1m1p1NpvReport->data(baseCcyColumn, loc));
                QL_REQUIRE(ccy == baseCurrency, "inconsistent npv ccy (" << ccy << ") and base ccy (" << baseCurrency
                                                                         << ") for " << tradeId);

                Real t1m0p1Npv = boost::get<Real>(t1m0p1NpvReport->data(npvBaseColumn, loc));
                Real t1m1p1Npv = boost::get<Real>(t1m1p1NpvReport->data(npvBaseColumn, loc));
                Real day1Pnl = t1m1p1Npv - t1m0p1Npv;
                DLOG("PnL report, writing line " << loc << " tradeId " << tradeId);

                report.next()
                    .add(tradeId)
                    .add(tradeType)
                    .add(maturityDate)
                    .add(maturityTime)
                    .add(startDate)
                    .add(endDate)
                    .add(0.0)
                    .add(0.0)
                    .add(0.0)
                    .add(0.0)
                    .add(t1m0p1Npv)
                    .add(t1m1p1Npv)
                    .add(day1Pnl)
                    .add(0.0)
                    .add(0.0)
                    .add(t1m1p1Npv)
                    .add(0.0)
                    .add(0.0)
                    .add(0.0)
                    .add(0.0)
                    .add(0.0)
                    .add(t1m1p1Npv)
                    .add(t1m1p1Npv)
                    .add(ccy);
            } catch (std::exception& e) {
                StructuredTradeErrorMessage(tradeId, tradeType, "Error writing pnl report", e.what()).log();
            }
        }
    }

    report.end();

    LOG("PnL report written.");
}

void ReportWriter::writeXmlReport(ore::data::Report& report, std::string header, std::string xml) {
    report.addColumn(header, string());
    report.next().add(xml);
    report.end();
}

}
}
