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

/*! \file orea/app/reportwriters/xvareportwriter.hpp
  \brief A Class to write ORE XVA outputs to reports
  \ingroup app
 */

#pragma once

#include <orea/app/reportwriter.hpp>
#include <orea/aggregation/nettedexposurecalculator.hpp>

namespace ore {
namespace data {
class Report;
class Portfolio;
} // namespace data
} // namespace ore

namespace ore {
namespace analytics {

class PostProcess;
class SensitivityStream;
class AggregationScenarioData;
class XvaExplainResults;

//! Write ORE XVA outputs to reports
/*! \ingroup app
 */
class XvaReportWriter : public ReportWriter {
public:
    /*! Constructor.
        \param nullString used to represent string values that are not applicable.
    */
    XvaReportWriter(const std::string& nullString = "#NA") : ReportWriter(nullString) {}

    
    virtual void writeTradeExposures(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                     const std::string& tradeId);

    virtual void writeTradeExposures(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess);

    virtual void writeNettingSetExposures(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                          const std::string& nettingSetId);

    virtual void writeNettingSetExposures(ore::data::Report& report,
                                          QuantLib::ext::shared_ptr<PostProcess> postProcess);

    virtual void writeNettingSetCvaSensitivities(ore::data::Report& report,
                                                 QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                                 const std::string& nettingSetId);

    virtual void writeNettingSetCvaSensitivities(ore::data::Report& report,
                                                 QuantLib::ext::shared_ptr<PostProcess> postProcess);

    virtual void writeNettingSetColva(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess,
                                      const std::string& nettingSetId);

    virtual void writeNettingSetColva(ore::data::Report& report, QuantLib::ext::shared_ptr<PostProcess> postProcess);

    virtual void writeXVA(ore::data::Report& report, const string& allocationMethod,
                          QuantLib::ext::shared_ptr<Portfolio> portfolio,
                          QuantLib::ext::shared_ptr<PostProcess> postProcess);

    virtual void writeAggregationScenarioData(ore::data::Report& report, const AggregationScenarioData& data);

    virtual void writeXvaSensitivityReport(Report& report, const QuantLib::ext::shared_ptr<SensitivityStream>& ssTrades,
                                           const QuantLib::ext::shared_ptr<SensitivityStream>& ssNettingSets,
                                           const std::map<std::string, std::string>& tradeNettingSetMap,
                                           Real outputThreshold = 0.0, Size outputPrecision = 2);

    virtual void writeTimeAveragedNettedExposure(
        ore::data::Report& report,
        const std::map<std::string, std::vector<NettedExposureCalculator::TimeAveragedExposure>>&);

    virtual void writeXvaExplainReport(ore::data::Report& report, const XvaExplainResults& xvaData);
    virtual void writeXvaExplainSummary(ore::data::Report& report, const XvaExplainResults& xvaData);
        
    virtual void writePcaReport(const std::string& ccy, const Array& eigenValue, const Matrix& eigenVector,
                                const Size& principalComponent, ore::data::Report& reportOut);

    virtual void writeMeanReversionReport(const Matrix& v, const Matrix& kappa, ore::data::Report& reportOut);
};

} // namespace analytics
} // namespace ore
