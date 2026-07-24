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

/*! \file orea/app/reportwriters/simmreportwriter.hpp
  \brief A Class to write ORE SIMM outputs to reports
  \ingroup app
 */

#pragma once

#include <orea/app/reportwriter.hpp>
#include <ql/shared_ptr.hpp>

namespace ore {
namespace data {
class NettingSetDetails;
class Report;
} // namespace data
} // namespace ore

namespace ore {
namespace analytics {

class SimmConfiguration;
class SimmResults;
struct CrifRecord;
class Crif;

//! Write ORE SIMM outputs to reports
/*! \ingroup app
 */
class SimmReportWriter : public ReportWriter{
public:
    /*! Constructor.
        \param nullString used to represent string values that are not applicable.
    */
    SimmReportWriter(const std::string& nullString = "#NA")  : ReportWriter(nullString) {}

    
    /*! Write out the SIMM results contained in the \p resultsMap and \p additionalMargin.
            The parameter \p resultsMap is a map containing the SIMM results containers for a
            set of portfolios. The key is the portfolio ID and the value is the SIMM results
            container for that portfolio. Similarly, the parameter \p additionalMargin contains
            the additional margin element for each portfolio.
        */
    virtual void
    writeSIMMReport(const std::map<SimmConfiguration::SimmSide,
                                   std::map<NettingSetDetails, std::pair<CrifRecord::Regulation, SimmResults>>>&
                        finalSimmResultsMap,
                    const QuantLib::ext::shared_ptr<ore::data::Report> report, const bool hasNettingSetDetails = false,
                    const std::string& simmResultCcy = "", const std::string& simmCalcCcyCall = "",
                    const std::string& simmCalcCcyPost = "", const std::string& reportCcy = "",
                    QuantLib::Real fxSpot = 1.0, QuantLib::Real outputThreshold = 0.005);

    virtual void writeSIMMReport(
        const std::map<SimmConfiguration::SimmSide,
                       std::map<NettingSetDetails, std::map<std::set<CrifRecord::Regulation>, SimmResults>>>&
            simmResultsMap,
        const QuantLib::ext::shared_ptr<ore::data::Report> report, const bool hasNettingSetDetails = false,
        const std::string& simmResultCcy = "", const std::string& simmCalcCcyCall = "",
        const std::string& simmCalcCcyPost = "", const std::string& reportCcy = "", const bool isFinalSimm = true,
        QuantLib::Real fxSpot = 1.0, QuantLib::Real outputThreshold = 0.005);

    //! Write the SIMM data report i.e. the netted CRIF records used in a SIMM calculation
    virtual void writeSIMMData(const Crif& simmData, const QuantLib::ext::shared_ptr<ore::data::Report>& dataReport,
                               const bool hasNettingSetDetails = false);
                               
    //! Write out CRIF records to a report
    virtual void writeCrifReport(const QuantLib::ext::shared_ptr<ore::data::Report>& report,
                                 const QuantLib::ext::shared_ptr<Crif>& crifRecords);
};

} // namespace analytics
} // namespace ore
