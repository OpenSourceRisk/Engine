/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/simm/crifloader.hpp
    \brief Class for loading CRIF records
*/

#pragma once

#include <map>
#include <tuple>

#include <orea/simm/crifrecord.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/report/report.hpp>

#include <ql/types.hpp>

namespace ore {
namespace analytics {

/*! A class for loading CRIF records. The records are aggregated and stored in a
    SimmNetSensitivities object so that they can later be used in a SIMM calculation
*/
class CrifLoader {
public:
    /*! Constructor
        We set the trade ID to an empty string if we are going to be netting at portfolio level.
        This is the default. To override this the flag \p keepTradeId may be set to true.
    */
    CrifLoader(const boost::shared_ptr<SimmConfiguration>& configuration,
               std::map<QuantLib::Size, std::set<std::string>> additionalHeaders = std::map<QuantLib::Size, std::set<std::string>>(),
               bool updateMapper = false, bool aggregateTrades = true)
        : configuration_(configuration), additionalHeaders_(additionalHeaders),
          updateMapper_(updateMapper), aggregateTrades_(aggregateTrades) {}

    /*! Destructor */
    virtual ~CrifLoader() {}

    //! Add a single CRIF record
    virtual void add(CrifRecord crifRecord, const bool onDiffAmountCcy = false);

    /*! Load CRIF records from a CRIF file.

        The Risk Data Standards document outlines what character should be used to separate columns,
        \p delim, and end lines, \p eol. These parameters have the correct defaults here but can be changed.

        The \p quoteChar allows one specify a character that can be used to enclose strings in the CRIF file.
        If this character is not `\0`, then an attempt is made to strip this character from the each column.
    */
    void loadFromFile(const std::string& fileName, char eol = '\n', char delim = '\t', char quoteChar = '\0',
                      char escapeChar = '\\');
    //! Load CRIF records in string format
    void loadFromString(const std::string& csvBuffer, char eol = '\n', char delim = '\t', char quoteChar = '\0',
                        char escapeChar = '\\');
    //! Core CRIF loader from generic istream
    void loadFromStream(std::istream& stream, char eol = '\n', char delim = '\t', char quoteChar = '\0',
                        char escapeChar = '\\');

    //! Return the netted CRIF records for use in a SIMM calculation
    const SimmNetSensitivities netRecords(const bool includeSimmParams = false) const;

    //! Return the SIMM parameters for use in calculating additional margin in SIMM
    const SimmNetSensitivities& simmParameters() const { return simmParameters_; }

    const bool hasCrifRecords() const { return !crifRecords_.empty(); }
    const bool hasSimmParameters() const { return !simmParameters_.empty(); }

    void setCrifRecords(const SimmNetSensitivities& crifRecords) { crifRecords_ = crifRecords; }
    void setSimmParameters(const SimmNetSensitivities& simmParameters) { simmParameters_ = simmParameters; }

    //! Give back the set of portfolio IDs that have been loaded
    const std::set<std::string>& portfolioIds() const;
    const std::set<ore::data::NettingSetDetails>& nettingSetDetails() const;

    //! Reset loader to initial state
    void clear();

    //! Map giving required CRIF file headers and their allowable alternatives
    static std::map<QuantLib::Size, std::set<std::string>> requiredHeaders;

    //! Map giving optional CRIF file headers and their allowable alternatives
    static std::map<QuantLib::Size, std::set<std::string>> optionalHeaders;
    
    //! Check if netting set details are used anywhere, instead of just the netting set ID
    bool hasNettingSetDetails() const;

    //! Aggregate all existing records
    void aggregate();

    //! For each CRIF record in netRecords_, checks if amountCurrency and amount are 
    //! defined and uses these to populate the record's amountUsd
    void fillAmountUsd(const boost::shared_ptr<ore::data::Market> market);

    //! SIMM configuration getter
    const boost::shared_ptr<SimmConfiguration>& simmConfiguration() { return configuration_; }

protected:
    //! Simm configuration that is used during loading of CRIF records
    boost::shared_ptr<SimmConfiguration> configuration_;

    //! Defines accepted column headers, beyond required and optional headers, see crifloader.cpp
    std::map<QuantLib::Size, std::set<std::string>> additionalHeaders_;
    
    /*! If true, the SIMM configuration's bucket mapper is updated during the
        CRIF loading with the mapping from SIMM qualifier to SIMM bucket. This is
        useful when consuming CRIF files from elsewhere in that it allows for
        using the mapping that is already present in the external file.
    */
    bool updateMapper_;

    /*! If true, aggregate over trade ids */
    bool aggregateTrades_;

    //! Netted CRIF records that can subsequently be used in a SIMM calculation
    SimmNetSensitivities crifRecords_;

    //! SIMM parameters for additional margin, provided in the same format as CRIF records
    SimmNetSensitivities simmParameters_;

    //! Set of portfolio IDs that have been loaded
    std::set<std::string> portfolioIds_;
    std::set<ore::data::NettingSetDetails> nettingSetDetails_;

    /*! Internal map from known index of CRIF record member to file column
        For example, give trade ID an index of 0 and find the column index of
        trade ID in the CRIF file e.g. n. The map entry would be [0, n]
    */
    std::map<QuantLib::Size, QuantLib::Size> columnIndex_;

    //! Process the elements of a header line of a CRIF file
    virtual void processHeader(const std::vector<std::string>& headers);

    /*! Process a line of a CRIF file and return true if valid line
        or false if an invalid line
    */
    virtual bool process(const std::vector<std::string>& entries, QuantLib::Size maxIndex, QuantLib::Size currentLine);
};

} // namespace analytics
} // namespace ore
