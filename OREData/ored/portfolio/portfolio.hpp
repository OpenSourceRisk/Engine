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
class Portfolio : public XMLSerializable {
public:
    //! Default constructor
    explicit Portfolio(bool buildFailedTrades = true) : buildFailedTrades_(buildFailedTrades) {}

    //! Add a trade to the portfolio
    void add(const boost::shared_ptr<Trade>& trade);

    //! Check if a trade id is already in the portfolio
    bool has(const string& id);

    /*! Get a Trade with the given \p id from the portfolio

        \remark returns a `nullptr` if no trade found with the given \p id
    */
    boost::shared_ptr<Trade> get(const std::string& id) const;

    //! Clear the portfolio
    void clear();

    //! Reset all trade data
    void reset();

    //! Portfolio size
    QuantLib::Size size() const { return trades_.size(); }

    //! XMLSerializable interface
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;

    //! Remove specified trade from the portfolio
    bool remove(const std::string& tradeID);

    //! Remove matured trades from portfolio for a given date, each removal is logged with an Alert
    void removeMatured(const QuantLib::Date& asof);

    //! Call build on all trades in the portfolio, the context is included in error messages
    void build(const boost::shared_ptr<EngineFactory>&, const std::string& context = "unspecified",
               const bool emitStructuredError = true);

    //! Calculates the maturity of the portfolio
    QuantLib::Date maturity() const;

    //! Return the map tradeId -> trade
    const std::map<std::string, boost::shared_ptr<Trade>>& trades() const;

    //! Build a set of tradeIds
    std::set<std::string> ids() const;

    //! Build a map from trade Ids to NettingSet
    std::map<std::string, std::string> nettingSetMap() const;

    //! Build a set of all counterparties in the portfolio
    std::set<std::string> counterparties() const;

    //! Build a map from counterparty to NettingSet
    std::map<std::string, std::set<std::string>> counterpartyNettingSets() const;

    //! Compute set of portfolios
    std::set<std::string> portfolioIds() const;

    //! Check if at least one trade in the portfolio uses the NettingSetDetails node, and not just NettingSetId
    bool hasNettingSetDetails() const;

    //! Does this portfolio build failed trades?
    bool buildFailedTrades() const { return buildFailedTrades_; }

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
    bool buildFailedTrades_;
    std::map<std::string, boost::shared_ptr<Trade>> trades_;
    std::map<AssetClass, std::set<std::string>> underlyingIndicesCache_;
};

std::pair<boost::shared_ptr<Trade>, bool> buildTrade(boost::shared_ptr<Trade>& trade,
                                                     const boost::shared_ptr<EngineFactory>& engineFactory,
                                                     const std::string& context, const bool buildFailedTrades,
                                                     const bool emitStructuredError);

} // namespace data
} // namespace ore
