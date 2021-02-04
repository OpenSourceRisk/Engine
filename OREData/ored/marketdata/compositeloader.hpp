/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file ored/marketdata/compositeloader.hpp
    \brief Loader that is a composite of two loaders
    \ingroup marketdata
*/

#pragma once

#include <boost/shared_ptr.hpp>
#include <ored/marketdata/loader.hpp>

namespace ore {
namespace data {

class CompositeLoader : public Loader {
public:
    CompositeLoader(const boost::shared_ptr<Loader>& a, const boost::shared_ptr<Loader>& b) : a_(a), b_(b) {
        QL_REQUIRE(a_ || b_, "CompositeLoader(): at least one loader must be not null");
    }

    const std::vector<boost::shared_ptr<MarketDatum>>& loadQuotes(const QuantLib::Date& d) const override {
        if (!b_)
            return a_->loadQuotes(d);
        if (!a_)
            return b_->loadQuotes(d);
        data_.clear();
        // loadQuotes() might throw if no quotes are avaiable in one loader, which is not an error here
        try {
            data_.insert(data_.end(), a_->loadQuotes(d).begin(), a_->loadQuotes(d).end());
        } catch (...) {
        }
        try {
            data_.insert(data_.end(), b_->loadQuotes(d).begin(), b_->loadQuotes(d).end());
        } catch (...) {
        }
        return data_;
    }

    const boost::shared_ptr<MarketDatum>& get(const std::string& name, const QuantLib::Date& d) const override {
        if (a_ && a_->has(name, d))
            return a_->get(name, d);
        if (b_ && b_->has(name, d))
            return b_->get(name, d);
        QL_FAIL("No MarketDatum for name " << name << " and date " << d);
    }

    bool has(const std::string& name, const QuantLib::Date& d) const override {
        return (a_ && a_->has(name, d)) || (b_ && b_->has(name, d));
    }

    const std::vector<Fixing>& loadFixings() const override {
        if (!b_)
            return a_->loadFixings();
        if (!a_)
            return b_->loadFixings();
        fixings_.clear();
        fixings_.insert(fixings_.end(), a_->loadFixings().begin(), a_->loadFixings().end());
        fixings_.insert(fixings_.end(), b_->loadFixings().begin(), b_->loadFixings().end());
        return fixings_;
    }

    const std::vector<Fixing>& loadDividends() const override {
        if (!b_)
            return a_->loadDividends();
        if (!a_)
            return b_->loadDividends();
        dividends_.clear();
        dividends_.insert(dividends_.end(), a_->loadDividends().begin(), a_->loadDividends().end());
        dividends_.insert(dividends_.end(), b_->loadDividends().begin(), b_->loadDividends().end());
        return dividends_;
    }

private:
    const boost::shared_ptr<Loader> a_, b_;
    mutable std::vector<boost::shared_ptr<MarketDatum>> data_;
    mutable std::vector<Fixing> fixings_, dividends_;
};
} // namespace data
} // namespace ore
