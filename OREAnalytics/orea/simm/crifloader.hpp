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
#include <orea/simm/crif.hpp>
#include <orea/simm/simmconfiguration.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/report/report.hpp>
#include <tuple>

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
    CrifLoader(const QuantLib::ext::shared_ptr<SimmConfiguration>& configuration,
               const std::vector<std::set<std::string>>& additionalHeaders = {},
               bool updateMapper = false, bool aggregateTrades = true)
        : configuration_(configuration), additionalHeaders_(additionalHeaders), updateMapper_(updateMapper),
          aggregateTrades_(aggregateTrades) {}

    /*! Destructor */
    virtual ~CrifLoader() {}

    virtual Crif loadCrif() {
        auto crif = loadCrifImpl();
        if (updateMapper_ && configuration_->bucketMapper() != nullptr) {
            configuration_->bucketMapper()->updateFromCrif(crif);
        }
        return crif;
    }

    //! SIMM configuration getter
    const QuantLib::ext::shared_ptr<SimmConfiguration>& simmConfiguration() { return configuration_; }

protected:
    virtual Crif loadCrifImpl() = 0;

    void addRecordToCrif(Crif& crif, CrifRecord&& recordToAdd) const;

    //! Check if the record is a valid Simm Crif Record
    void validateSimmRecord(const CrifRecord& cr) const;
    //! Override currency codes 
    void currencyOverrides(CrifRecord& crifRecord) const;
    //! update bucket mappings
    void updateMapping(const CrifRecord& cr) const;

    //! Simm configuration that is used during loading of CRIF records
    QuantLib::ext::shared_ptr<SimmConfiguration> configuration_;

    //! Defines accepted column headers, beyond required and optional headers, see crifloader.cpp
    std::vector<std::set<std::string>> additionalHeaders_;

    /*! If true, the SIMM configuration's bucket mapper is updated during the
        CRIF loading with the mapping from SIMM qualifier to SIMM bucket. This is
        useful when consuming CRIF files from elsewhere in that it allows for
        using the mapping that is already present in the external file.
    */
    bool updateMapper_;

    /*! If true, aggregate over trade ids */
    bool aggregateTrades_;

    //! Map giving required CRIF file headers and their allowable alternatives
    static std::map<QuantLib::Size, std::set<std::string>> requiredHeaders;

    //! Map giving optional CRIF file headers and their allowable alternatives
    static std::map<QuantLib::Size, std::set<std::string>> optionalHeaders;
};

class StringStreamCrifLoader : public CrifLoader {
public:
    StringStreamCrifLoader(const QuantLib::ext::shared_ptr<SimmConfiguration>& configuration,
                           const std::vector<std::set<std::string>>& additionalHeaders = {}, bool updateMapper = false,
                           bool aggregateTrades = true, char eol = '\n', char delim = '\t', char quoteChar = '\0',
                           char escapeChar = '\\', const std::string& nullString = "#N/A");

protected:
    Crif loadCrifImpl() override { return loadFromStream(stream()); }

    //! Core CRIF loader from generic istream
    Crif loadFromStream(std::stringstream&& stream);

    virtual std::stringstream stream() const = 0;
    /*! Internal map from known index of CRIF record member to file column
        For example, give trade ID an index of 0 and find the column index of
        trade ID in the CRIF file e.g. n. The map entry would be [0, n]
    */
    std::map<QuantLib::Size, QuantLib::Size> columnIndex_;


    std::map<QuantLib::Size, std::set<std::string>> additionalHeadersIndexMap_;

    //! Process the elements of a header line of a CRIF file
    void processHeader(const std::vector<std::string>& headers);

    /*! Process a line of a CRIF file and return true if valid line
        or false if an invalid line
    */
    bool process(const std::vector<std::string>& entries, QuantLib::Size maxIndex, QuantLib::Size currentLine, Crif& result);
    char eol_;
    char delim_;
    char quoteChar_;
    char escapeChar_;
    std::string nullString_;
};

class CsvFileCrifLoader : public StringStreamCrifLoader {
public:
    CsvFileCrifLoader(const std::string& filename, const QuantLib::ext::shared_ptr<SimmConfiguration>& configuration,
                      const std::vector<std::set<std::string>>& additionalHeaders = {},
                      bool updateMapper = false, bool aggregateTrades = true, char eol = '\n', char delim = '\t',
                      char quoteChar = '\0', char escapeChar = '\\', const std::string& nullString = "#N/A")
        : StringStreamCrifLoader(configuration, additionalHeaders, updateMapper, aggregateTrades, eol, delim, quoteChar,
                                 escapeChar, nullString),
          filename_(filename) {}

protected:
    std::string filename_;
    std::stringstream stream() const override;
};

class CsvBufferCrifLoader : public StringStreamCrifLoader {
public:
    CsvBufferCrifLoader(const std::string& buffer, const QuantLib::ext::shared_ptr<SimmConfiguration>& configuration,
                        const std::vector<std::set<std::string>>& additionalHeaders = {},
                        bool updateMapper = false, bool aggregateTrades = true, char eol = '\n', char delim = '\t',
                        char quoteChar = '\0', char escapeChar = '\\', const std::string& nullString = "#N/A")
        : StringStreamCrifLoader(configuration, additionalHeaders, updateMapper, aggregateTrades, eol, delim, quoteChar,
                                 escapeChar, nullString),
          buffer_(buffer) {}

protected:
    std::string buffer_;
    std::stringstream stream() const override;
};

} // namespace analytics
} // namespace ore
