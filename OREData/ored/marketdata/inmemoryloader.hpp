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
    InMemoryLoader() {}

    std::vector<QuantLib::ext::shared_ptr<MarketDatum>> loadQuotes(const QuantLib::Date& d) const override;
    QuantLib::ext::shared_ptr<MarketDatum> get(const string& name, const QuantLib::Date& d) const override;
    std::set<QuantLib::ext::shared_ptr<MarketDatum>> get(const std::set<std::string>& names,
                                                 const QuantLib::Date& asof) const override;
    std::set<QuantLib::ext::shared_ptr<MarketDatum>> get(const Wildcard& wildcard, const QuantLib::Date& asof) const override;
    std::set<Fixing> loadFixings() const override { return fixings_; }
    std::set<QuantExt::Dividend> loadDividends() const override { return dividends_; }
    bool hasQuotes(const QuantLib::Date& d) const override;

    // add a market datum
    virtual void add(QuantLib::Date date, const string& name, QuantLib::Real value);

    // add a fixing
    virtual void addFixing(QuantLib::Date date, const string& name, QuantLib::Real value);

    // add a dividend
    virtual void addDividend(const QuantExt::Dividend& dividend);

    // clear data
    void reset();

protected:
    std::map<QuantLib::Date, std::set<QuantLib::ext::shared_ptr<MarketDatum>, SharedPtrMarketDatumComparator>> data_;
    std::set<Fixing> fixings_;
    std::set<QuantExt::Dividend> dividends_;
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

} // namespace data
} // namespace ore
