/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file orea/app/reportwriter.hpp
  \brief A Class to write ORE outputs to reports
  \ingroup app
 */

#pragma once

#include <ql/shared_ptr.hpp>
#include <map>
#include <orea/aggregation/postprocess.hpp>
#include <orea/app/parameters.hpp>
#include <orea/app/analytics/xvaexplainanalytic.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/cube/sensitivitycube.hpp>
#include <orea/engine/bacvacalculator.hpp>
#include <orea/engine/cvasensitivitycubestream.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <orea/simm/crifrecord.hpp>
#include <orea/simm/simmresults.hpp>
#include <orea/simm/crif.hpp>
#include <orea/simm/imschedulecalculator.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/marketdata/todaysmarketcalibrationinfo.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/report/report.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <string>

namespace ore {
namespace analytics {
//! Write ORE outputs to reports
/*! \ingroup app
 */
class ReportWriter {
public:
    /*! Constructor.
        \param nullString used to represent string values that are not applicable.
    */
    ReportWriter(const std::string& nullString = "#NA") : nullString_(nullString) {}

    virtual ~ReportWriter(){};

    virtual void writeNpv(ore::data::Report& report, const std::string& baseCurrency,
                          QuantLib::ext::shared_ptr<ore::data::Market> market, const std::string& configuration,
                          QuantLib::ext::shared_ptr<Portfolio> portfolio);

    virtual void writeCashflow(ore::data::Report& report, const std::string& baseCurrency,
                               QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio,
                               QuantLib::ext::shared_ptr<ore::data::Market> market,
                               const std::string& configuration,
                               const bool includePastCashflows = false);

    virtual void writeCashflowNpv(ore::data::Report& report,
                                  const ore::data::InMemoryReport& cashflowReport,
                                  QuantLib::ext::shared_ptr<ore::data::Market> market,
                                  const std::string& configuration,
                                  const std::string& baseCcy,
                                  const Date& horizon = Date::maxDate());

    virtual void writeCurves(ore::data::Report& report, const std::string& configID, const DateGrid& grid,
                             const TodaysMarketParameters& marketConfig, const QuantLib::ext::shared_ptr<Market>& market,
                             const bool continueOnError = false);

    virtual void writeTradeExposures(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                     const std::string& tradeId);

    virtual void writeNettingSetExposures(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                          const std::string& nettingSetId);

    virtual void writeNettingSetExposures(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess);
    
    virtual void writeNettingSetCvaSensitivities(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                          const std::string& nettingSetId);

    virtual void writeNettingSetColva(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                      const std::string& nettingSetId);

    virtual void writeXVA(ore::data::Report& report, const string& allocationMethod,
                          QuantLib::ext::shared_ptr<Portfolio> portfolio, QuantLib::ext::shared_ptr<PostProcess> postProcess);

    virtual void writeAggregationScenarioData(ore::data::Report& report, const AggregationScenarioData& data);

    virtual void writeScenarioReport(ore::data::Report& report,
                                     const std::vector<QuantLib::ext::shared_ptr<SensitivityCube>>& sensitivityCubes,
                                     QuantLib::Real outputThreshold = 0.0);

    virtual void writeSensitivityReport(ore::data::Report& report, const QuantLib::ext::shared_ptr<SensitivityStream>& ss,
                                        QuantLib::Real outputThreshold = 0.0, QuantLib::Size outputPrecision = 2);

    virtual void writeXvaSensitivityReport(
        Report& report, const QuantLib::ext::shared_ptr<SensitivityStream>& ssTrades,
        const QuantLib::ext::shared_ptr<SensitivityStream>& ssNettingSets,
        const std::map<std::string, std::string>& tradeNettingSetMap, Real outputThreshold = 0.0, Size outputPrecision = 2);

    virtual void writeSensitivityConfigReport(ore::data::Report& report,
                                              const std::map<RiskFactorKey, QuantLib::Real>& shiftSizes,
                                              const std::map<RiskFactorKey, QuantLib::Real>& baseValues,
                                              const std::map<RiskFactorKey, std::string>& keyToFactor);

    virtual void writeAdditionalResultsReport(ore::data::Report& report,
                                              QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio,
                                              QuantLib::ext::shared_ptr<Market> market,
                                              const std::string& configuration, const std::string& baseCurrency,
                                              const std::size_t precision = 6);

    virtual void writeMarketData(ore::data::Report& report, const QuantLib::ext::shared_ptr<ore::data::Loader>& loader, const QuantLib::Date& asof,
        const set<string>& quoteNames, bool returnAll);

    virtual void writeFixings(ore::data::Report& report, const QuantLib::ext::shared_ptr<ore::data::Loader>& loader);

    virtual void writeDividends(ore::data::Report& report, const QuantLib::ext::shared_ptr<ore::data::Loader>& loader);

    virtual void writePricingStats(ore::data::Report& report, const QuantLib::ext::shared_ptr<Portfolio>& portfolio);

    virtual void writeRunTimes(ore::data::Report& report, const Timer& timer);

    virtual void writeCube(ore::data::Report& report, const QuantLib::ext::shared_ptr<NPVCube>& cube,
                           const std::map<std::string, std::string>& nettingSetMap = std::map<std::string, std::string>());

    virtual void writeTimeAveragedNettedExposure(
        ore::data::Report& report,
        const std::map<std::string, std::vector<NettedExposureCalculator::TimeAveragedExposure>>&);

    const std::string& nullString() const { return nullString_; }

    /*! Write out the SIMM results contained in the \p resultsMap and \p additionalMargin.
        The parameter \p resultsMap is a map containing the SIMM results containers for a
        set of portfolios. The key is the portfolio ID and the value is the SIMM results
        container for that portfolio. Similarly, the parameter \p additionalMargin contains
        the additional margin element for each portfolio.
    */
    virtual void
    writeSIMMReport(const std::map<SimmConfiguration::SimmSide,
                       std::map<NettingSetDetails, std::pair<CrifRecord::Regulation, SimmResults>>>& finalSimmResultsMap,
                    const QuantLib::ext::shared_ptr<ore::data::Report> report, const bool hasNettingSetDetails = false,
                    const std::string& simmResultCcy = "", const std::string& simmCalcCcyCall = "",
                    const std::string& simmCalcCcyPost = "", const std::string& reportCcy = "",
                    QuantLib::Real fxSpot = 1.0, QuantLib::Real outputThreshold = 0.005);
    virtual void
    writeSIMMReport(const std::map<SimmConfiguration::SimmSide,
                       std::map<NettingSetDetails, std::map<std::set<CrifRecord::Regulation>, SimmResults>>>& simmResultsMap,
                    const QuantLib::ext::shared_ptr<ore::data::Report> report, const bool hasNettingSetDetails = false,
                    const std::string& simmResultCcy = "", const std::string& simmCalcCcyCall = "",
                    const std::string& simmCalcCcyPost = "", const std::string& reportCcy = "",
                    const bool isFinalSimm = true, QuantLib::Real fxSpot = 1.0, QuantLib::Real outputThreshold = 0.005);

    //! Write the SIMM data report i.e. the netted CRIF records used in a SIMM calculation
    virtual void writeSIMMData(const ore::analytics::Crif& simmData,
                               const QuantLib::ext::shared_ptr<ore::data::Report>& dataReport,
                               const bool hasNettingSetDetails = false);

    //! Write out CRIF records to a report
    virtual void writeCrifReport(const QuantLib::ext::shared_ptr<ore::data::Report>& report,
                                 const QuantLib::ext::shared_ptr<ore::analytics::Crif>& crifRecords);

    virtual void writeScenarioStatistics(const QuantLib::ext::shared_ptr<ore::analytics::ScenarioGenerator>& generator,
                                         const std::vector<ore::analytics::RiskFactorKey>& keys,
                                         QuantLib::Size numPaths, const std::vector<QuantLib::Date>& dates,
                                         ore::data::Report& report);

    virtual void writeScenarioDistributions(const QuantLib::ext::shared_ptr<ore::analytics::ScenarioGenerator>& generator,
                                            const std::vector<ore::analytics::RiskFactorKey>& keys,
                                            QuantLib::Size numPaths, const std::vector<QuantLib::Date>& dates,
                                            QuantLib::Size distSteps, ore::data::Report& report);

    virtual void
    writeHistoricalScenarioDetails(const QuantLib::ext::shared_ptr<ore::analytics::HistoricalScenarioGenerator>& generator,
                                   ore::data::Report& report);

    virtual void writeStockSplitReport(const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& baseScenario,
                                       const QuantLib::ext::shared_ptr<ore::analytics::HistoricalScenarioLoader>& hsloader,
                                       const QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors>& adjFactors,
                                       const QuantLib::ext::shared_ptr<ore::data::Report>& report);

    void writeHistoricalScenarios(const QuantLib::ext::shared_ptr<HistoricalScenarioLoader>& hsloader,
                                  const QuantLib::ext::shared_ptr<ore::data::Report>& report);

    void writeHistoricalScenarioDistributions(
        QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hsgen,
        const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
        const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
        QuantLib::ext::shared_ptr<ore::data::Report> histScenDetailsReport, QuantLib::ext::shared_ptr<ore::data::Report> statReport,
        QuantLib::ext::shared_ptr<ore::data::Report> distReport, QuantLib::Size distSteps = Null<Size>());

    virtual void writeIMScheduleSummaryReport(
        const std::map<SimmConfiguration::SimmSide,
                       std::map<NettingSetDetails, std::pair<CrifRecord::Regulation, IMScheduleResults>>>& finalResultsMap,
        const QuantLib::ext::shared_ptr<Report> report, const bool hasNettingSetDetails = false,
        const std::string& simmResultCcy = "", const std::string& reportCcy = "", QuantLib::Real fxSpot = 1.0,
        QuantLib::Real outputThreshold = 0.005);

    virtual void writeIMScheduleTradeReport(const std::map<std::string, std::vector<IMScheduleCalculator::IMScheduleTradeData>>& tradeResults,
                                            const QuantLib::ext::shared_ptr<ore::data::Report> report,
                                            const bool hasNettingSetDetails = false);
                                            
    virtual void writePnlReport(ore::data::Report& report,
	    const ext::shared_ptr<InMemoryReport>& t0NpvReport,
	    const ext::shared_ptr<InMemoryReport>& t0NpvLaggedReport,
        const ext::shared_ptr<InMemoryReport>& t1NpvLaggedReport,
        const ext::shared_ptr<InMemoryReport>& t1Npvt0PortReport,
	    const ext::shared_ptr<InMemoryReport>& t1NpvReport,
	    const ext::shared_ptr<InMemoryReport>& t0CashFlowReport,			
	    const Date& startDate, const Date& endDate,
	    const std::string& baseCurrency,
	    const ext::shared_ptr<ore::data::Market>& market, const std::string& configuration,
	    const ext::shared_ptr<Portfolio>& portfolio);

    virtual void writeXvaExplainReport(ore::data::Report& report, const ore::analytics::XvaExplainResults& xvaData);
    virtual void writeXvaExplainSummary(ore::data::Report& report, const ore::analytics::XvaExplainResults& xvaData);

    void writeXmlReport(ore::data::Report& report, std::string header, std::string xmlString);

    virtual void writeBaCvaReport(const QuantLib::ext::shared_ptr<BaCvaCalculator>& baCvaCalculator, ore::data::Report& reportOut);

    virtual void writeCvaSensiReport(const QuantLib::ext::shared_ptr<CvaSensitivityCubeStream>& ss, ore::data::Report& reportOut);

    virtual void writeCvaSensiReport(const std::vector<CvaSensitivityRecord>& records, ore::data::Report& reportOut);

    virtual void writeSaCvaSensiReport(const SaCvaNetSensitivities& sensis, ore::data::Report& reportOut);

    void writeSaccrTradeDetailReport(ore::data::Report& report,
                                     const QuantLib::ext::shared_ptr<ore::analytics::SaccrTradeData>& tradeData) const;

    void writeCapitalCrifReport(ore::data::Report& report, const QuantLib::ext::shared_ptr<ore::analytics::Crif>& crif,
                                const std::string& baseCurrency, const char& csvQuoteChar = '\0') const;

protected:
    std::string nullString_;
    void addMarketDatum(ore::data::Report& report, const ore::data::MarketDatum& md,
                        const QuantLib::Date& actualDate = Date());
};

} // namespace analytics
} // namespace ore
