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
  Inspectors can be called to then retrive quotes and fixings.

  \ingroup marketdata
 */
class CSVLoader : public Loader {
public:
    //! Constructor
    CSVLoader( //! Quote file name
        const string& marketFilename,
        //! Fixing file name
        const string& fixingFilename,
        //! Enable/disable implying today's fixings
        bool implyTodaysFixings = false);

    //! \name Inspectors
    //@{
    //! Load market quotes
    const std::vector<boost::shared_ptr<MarketDatum>>& loadQuotes(const QuantLib::Date&) const;

    //! Get a particular quote by its unique name
    const boost::shared_ptr<MarketDatum>& get(const std::string& name, const QuantLib::Date&) const;

    //! Load fixings
    const std::vector<Fixing>& loadFixings() const { return fixings_; }
    //@}

private:
    void loadFile(const string&, bool);

    bool implyTodaysFixings_;
    std::map<QuantLib::Date, std::vector<boost::shared_ptr<MarketDatum>>> data_;
    std::vector<Fixing> fixings_;
};
}
}
