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

/*! \file orea/app/reportwriters/capitalreportwriter.hpp
  \brief A Class to write ORE Capital outputs to reports
  \ingroup app
 */

#pragma once

#include <orea/app/reportwriter.hpp>
#include <orea/engine/sacvasensitivityrecord.hpp>

namespace ore {
namespace data {
class Report;
} // namespace data
} // namespace ore

namespace ore {
namespace analytics {

class BaCvaCalculator;
class CvaSensitivityCubeStream;
struct CvaSensitivityRecord;
class SaccrTradeData;
class Crif;

//! Write ORE Capital outputs to reports
/*! \ingroup app
 */
class CapitalReportWriter : public ReportWriter {
public:
    /*! Constructor.
        \param nullString used to represent string values that are not applicable.
    */
    CapitalReportWriter(const std::string& nullString = "#NA") : ReportWriter(nullString) {}

    virtual void writeBaCvaReport(const QuantLib::ext::shared_ptr<BaCvaCalculator>& baCvaCalculator,
                                  ore::data::Report& reportOut);

    virtual void writeCvaSensiReport(const QuantLib::ext::shared_ptr<CvaSensitivityCubeStream>& ss,
                                     ore::data::Report& reportOut);

    virtual void writeCvaSensiReport(const std::vector<CvaSensitivityRecord>& records, ore::data::Report& reportOut);

    virtual void writeSaCvaSensiReport(const SaCvaNetSensitivities& sensis, ore::data::Report& reportOut);

    void writeSaccrTradeDetailReport(ore::data::Report& report,
                                     const QuantLib::ext::shared_ptr<SaccrTradeData>& tradeData) const;

    void writeCapitalCrifReport(ore::data::Report& report, const QuantLib::ext::shared_ptr<Crif>& crif,
                                const std::string& baseCurrency, const char& csvQuoteChar = '\0') const;
};

} // namespace analytics
} // namespace ore
