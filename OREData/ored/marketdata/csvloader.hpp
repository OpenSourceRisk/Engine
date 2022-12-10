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

/*! \file ored/marketdata/csvloader.hpp
    \brief Market Datum Loader Implementation
    \ingroup marketdata
*/

#pragma once

#include <map>
#include <ored/marketdata/loader.hpp>

namespace ore {
namespace data {

//! Utility class for loading market quotes and fixings from a file
/*!
  Data is loaded with the call to the constructor.
  Inspectors can be called to then retrieve quotes and fixings.

  \ingroup marketdata
 */
class CSVLoader : public Loader {
public:
    //! Constructor
    CSVLoader() {}

    CSVLoader( //! Quote file name
        const string& marketFilename,
        //! Fixing file name
        const string& fixingFilename,
        //! Enable/disable implying today's fixings
        bool implyTodaysFixings = false);

    CSVLoader( //! Quote file name
        const vector<string>& marketFiles,
        //! Fixing file name
        const vector<string>& fixingFiles,
        //! Enable/disable implying today's fixings
        bool implyTodaysFixings = false);

    CSVLoader( //! Quote file name
        const string& marketFilename,
        //! Fixing file name
        const string& fixingFilename,
        //! Dividend file name
        const string& dividendFilename,
        //! Enable/disable implying today's fixings
        bool implyTodaysFixings = false);

    CSVLoader( //! Quote file name
        const vector<string>& marketFiles,
        //! Fixing file name
        const vector<string>& fixingFiles,
        //! Dividend file name
        const vector<string>& dividendFiles,
        //! Enable/disable implying today's fixings
        bool implyTodaysFixings = false);

    //! \name Inspectors
    //@{
    //! Load market quotes
    std::vector<boost::shared_ptr<MarketDatum>> loadQuotes(const QuantLib::Date&) const override;

    //! Get a particular quote by its unique name
    using Loader::get;

    //! get quotes matching a wildcard
    std::set<boost::shared_ptr<MarketDatum>> get(const Wildcard& wildcard, const QuantLib::Date& asof) const override;

    //! Load fixings
    std::set<Fixing> loadFixings() const override { return fixings_; }
    //! Load dividends
    std::set<Fixing> loadDividends() const override { return dividends_; }
    //@}

private:
    enum class DataType { Market, Fixing, Dividend };
    void loadFile(const string&, DataType);

    bool implyTodaysFixings_;
    std::map<QuantLib::Date, std::set<boost::shared_ptr<MarketDatum>, SharedPtrMarketDatumComparator>> data_;
    std::set<Fixing> fixings_;
    std::set<Fixing> dividends_;
};
} // namespace data
} // namespace ore
