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

class ReferenceDataManager;

//! Serializable portfolio
/*!
  \ingroup portfolio
*/
class Portfolio {
public:
    //! Default constructor
    Portfolio() {}

    //! Add a trade to the portfoliio
    void add(const boost::shared_ptr<Trade>& trade, const bool checkForDuplicateIds = true);

    //! Check if a trade id is already in the porfolio
    bool has(const string& id);

    /*! Get a Trade with the given \p id from the portfolio

        \remark returns a `nullptr` if no trade found with the given \p id
    */
    boost::shared_ptr<Trade> get(const std::string& id) const;

    //! Clear the portfolio
    void clear() { trades_.clear(); }

    //! Reset all trade data
    void reset();

    //! Portfolio size
    QuantLib::Size size() const { return trades_.size(); }

    //! Load using a default or user supplied TradeFactory, existing trades are kept
    void load(const std::string& fileName,
              const boost::shared_ptr<TradeFactory>& tf = boost::make_shared<TradeFactory>(),
              const bool checkForDuplicateIds = true);

    //! Load from an XML string using a default or user supplied TradeFactory, existing trades are kept
    void loadFromXMLString(const std::string& xmlString,
                           const boost::shared_ptr<TradeFactory>& tf = boost::make_shared<TradeFactory>(),
                           const bool checkForDuplicateIds = true);

    //! Load from XML Node
    void fromXML(XMLNode* node, const boost::shared_ptr<TradeFactory>& tf = boost::make_shared<TradeFactory>(),
                 const bool checkForDuplicateIds = true);

    //! Save portfolio to an XML file
    void save(const std::string& fileName) const;

    //! Remove specified trade from the portfolio
    bool remove(const std::string& tradeID);

    //! Remove matured trades from portfolio for a given date, each removal is logged with an Alert
    void removeMatured(const QuantLib::Date& asof);

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

    //! Build a vector of unique counterparties
    std::vector<std::string> counterparties() const;

    //! Build a map from counterparty to NettingSet
    std::map<std::string, std::set<std::string>> counterpartyNettingSets() const;

    //! Compute set of portfolios
    std::set<std::string> portfolioIds() const;

    /*! Return the fixings that will be requested in order to price every Trade in this Portfolio given
        the \p settlementDate. The map key is the ORE name of the index and the map value is the set of fixing dates.

        \warning This method will return an empty map if the Portfolio has not been built.
    */
    std::map<std::string, std::set<QuantLib::Date>>
    fixings(const QuantLib::Date& settlementDate = QuantLib::Date()) const;

    /*! Returns the names of the underlying instruments for each asset class */
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr);
    std::set<std::string>
    underlyingIndices(AssetClass assetClass,
                      const boost::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr);

private:
    std::vector<boost::shared_ptr<Trade>> trades_;
    std::map<AssetClass, std::set<std::string>> underlyingIndicesCache_;
};
} // namespace data
} // namespace ore
