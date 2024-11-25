/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file orea/engine/sacvasensitivityloader.hpp
    \brief Class for loading CvaSensitivity records
*/

#pragma once

#include <map>
#include <set>
#include <tuple>

#include <orea/engine/sacvasensitivityrecord.hpp>
#include <orea/engine/cvasensitivitycubestream.hpp>
#include <ored/portfolio/counterpartymanager.hpp>
#include <ql/types.hpp>

namespace ore {
namespace analytics {

/*! A class for loading CvaSensitivity records. The records are aggregated and stored in a
    SaCvaNetSensitivities object so that they can later be used in a CVA capital charge calculation
*/
class SaCvaSensitivityLoader {
public:
    /*! Constructor
        We set the trade ID to an empty string if we are going to be netting at portfolio level.
        This is the default. To override this the flag \p keepTradeId may be set to true.
    */
    SaCvaSensitivityLoader() {}

    /*! Destructor */
    virtual ~SaCvaSensitivityLoader() {}

    //! Add a single CvaSensitivity record
    virtual void add(const SaCvaSensitivityRecord& cvaSensitivityRecord, bool aggregate);

    /*! Load SaCvaSensitivity records from a CvaSensitivity file.

    */
    void load(const std::string& fileName, char eol = '\n', char delim = '\t', char quoteChar = '\0');


    /*! Load record from CvaSensi
    */
    void loadRawSensi(const CvaSensitivityRecord& cvaSensi,
        const std::string& baseCurrency, const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager = nullptr);

    /*! Load SaCvaSensitity records from raw Cva Sensi stream
    */
    void loadFromRawSensis(const QuantLib::ext::shared_ptr<CvaSensitivityCubeStream> sensiStream, 
        const std::string& baseCurrency, const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager = nullptr);

    /*! Load SaCvaSensitity records from vector of Cva Sensis
    */
    void loadFromRawSensis(std::vector<CvaSensitivityRecord> cvaSensis,
        const std::string& baseCurrency, const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager = nullptr);

    //! Return the netted CvaSensitivity records for use in a CVA calculation
    const SaCvaNetSensitivities& netRecords() const { return netRecords_; }

    //! Give back the set of portfolio IDs that have been loaded
    const std::set<std::string>& nettingSetIds() const;

    //! Reset loader to initial state
    void clear();

    static std::map<QuantLib::Size, std::string> expectedHeaders;
protected:
    //! Netted CvaSensitivity records that can subsequently be used in a SIMM calculation
    SaCvaNetSensitivities netRecords_;

    //! Set of portfolio IDs that have been loaded
    std::set<std::string> nettingSetIds_;

    /*! Internal map from known index of CvaSensitivity record member to file column
        For example, give trade ID an index of 0 and find the column index of
        trade ID in the CvaSensitivity file e.g. n. The map entry would be [0, n]
    */
    std::map<QuantLib::Size, QuantLib::Size> columnIndex_;

    //! Process the elements of a header line of a CvaSensitivity file
    virtual void processHeader(const std::vector<std::string>& headers);

    /*! Process a line of a CvaSensitivity file and return true if valid line
        or false if an invalid line
    */
    virtual bool process(const std::vector<std::string>& entries, QuantLib::Size maxIndex, QuantLib::Size currentLine);
};

} // namespace analytics
} // namespace ore
