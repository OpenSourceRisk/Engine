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

#pragma once

#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/marketdatumparser.hpp>

namespace ore {
namespace data {
using std::string;

class InMemoryLoader : public Loader {
public:
    //! Constructor
    InMemoryLoader() {}

    //! Load market quotes
    const std::vector<boost::shared_ptr<MarketDatum>>& loadQuotes(const QuantLib::Date& d) const override {
        QL_REQUIRE(data_.find(d) != data_.end(), "There are no quotes available for date " << d)
        return data_.find(d)->second;
    }

    //! Get a particular quote by its unique name
    const boost::shared_ptr<MarketDatum>& get(const string& name, const QuantLib::Date& d) const override {
        for (auto& md : loadQuotes(d)) {
            if (md->name() == name)
                return md;
        }
        QL_FAIL("No datum for " << name << " on date " << d);
    }

    //! Load fixings
    const std::vector<Fixing>& loadFixings() const override { return fixings_; }

    // add a market datum
    virtual void add(QuantLib::Date date, const string& name, QuantLib::Real value) {
        try {
            data_[date].push_back(parseMarketDatum(date, name, value));
            TLOG("Added MarketDatum " << data_[date].back()->name());
        }
        catch (std::exception& e) {
            WLOG("Failed to parse MarketDatum " << name << ": " << e.what());
        }
    }

    // add a fixing
    virtual void addFixing(QuantLib::Date date, const string& name, QuantLib::Real value) {
        // Don't check against today's date here
        fixings_.emplace_back(Fixing(date, name, value));
    }

protected:
    std::map<QuantLib::Date, std::vector<boost::shared_ptr<MarketDatum>>> data_;
    std::vector<Fixing> fixings_;
};

//! Utility function for loading market quotes and fixings from an in memory csv buffer
// This function throws on bad data
void loadDataFromBuffers(
    //! The loader that will be populated
    InMemoryLoader& loader,
    //! QuantLib::Date Key Value in a single std::string, separated by blanks, tabs, colons or commas 
    const std::vector<std::string>& marketData, 
    //! QuantLib::Date Index Fixing in a single std::string, separated by blanks, tabs, colons or commas 
    const std::vector<std::string>& fixingData, 
    //! Enable/disable implying today's fixings 
    bool implyTodaysFixings = false); 

}
}
