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

/*! \file portfolio/portfolio.hpp
    \brief Portfolio class
    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/tradefactory.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>
#include <vector>

namespace ore {
namespace data {

//! Serializable portfolio
/*!
  \ingroup portfolio
*/
class Portfolio {
public:
    //! Default constructor
    Portfolio() {}

    //! Add a trade to the portfoliio
    void add(const boost::shared_ptr<Trade>& trade);

    //! Check if a trade id is already in the porfolio
    bool has(const string &id);

    //! Clear the portfolio
    void clear() { trades_.clear(); }

    //! Reset all trade data
    void reset();

    //! Portfolio size
    QuantLib::Size size() const { return trades_.size(); }

    //! Load using a default or user supplied TradeFactory, existing trades are kept
    void load(const std::string& fileName,
              const boost::shared_ptr<TradeFactory>& tf = boost::make_shared<TradeFactory>());

    //! Load from an XML string using a default or user supplied TradeFactory, existing trades are kept
    void loadFromXMLString(const std::string& xmlString,
        const boost::shared_ptr<TradeFactory>& tf = boost::make_shared<TradeFactory>());

    //! Save portfolio to an XML file
    void save(const std::string& fileName) const;

    //! Remove specified trade from the portfolio
    bool remove(const std::string& tradeID);

    //! Call build on all trades in the portfolio
    void build(const boost::shared_ptr<EngineFactory>&);

    //! Calculates the maturity of the portfolio
    QuantLib::Date maturity() const;

    //! Return trade list
    const std::vector<boost::shared_ptr<Trade>>& trades() const { return trades_; }

    //! Build a vector of tradeIds
    std::vector<std::string> ids() const;

    //! Build a map from trade Ids to NettingSet
    std::map<std::string, std::string> nettingSetMap() const;

    //! Compute set of portfolios
    std::set<std::string> portfolioIds() const;

private:
    //! Load from XMLDocument
    void load(XMLDocument& doc, const boost::shared_ptr<TradeFactory>& tf);

    std::vector<boost::shared_ptr<Trade>> trades_;
};
} // namespace data
} // namespace ore
