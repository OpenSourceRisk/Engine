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

/*! \file orea/simm/simmtradedata.hpp
    \brief Class for holding a subset of trade data relevant for ISDA SIMM and CRIF generation
*/

#pragma once

#include <orea/simm/simmconfiguration.hpp>

#include <ored/marketdata/market.hpp>
#include <ored/portfolio/nettingsetdetails.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/referencedata.hpp>

#include <map>
#include <set>

using ore::data::NettingSetDetails;
using ore::data::parseBool;
using ore::data::parseCurrency;
using ore::data::Portfolio;
using std::map;
using std::vector;
using std::pair;
using std::string;

namespace ore {
namespace analytics {
class SimmBucketMapper;

/*! A simple container class for holding trade IDs along with their corresponding
    portfolio id, counterparty id, SIMM/Schedule product class. There is also the
    option to provide extra trade attributes that can be requested during CRIF generation.
*/
class SimmTradeData {
public:
    //! Class to hold additional trade attributes that may be needed during CRIF generation.
    class TradeAttributes {
    public:
        TradeAttributes() {}
        virtual ~TradeAttributes() {}
      
        const string& getTradeType() const { return tradeType_; }
        const CrifRecord::ProductClass& getSimmProductClass() const { return simmProductClass_; }
        const CrifRecord::ProductClass& getScheduleProductClass() const { return scheduleProductClass_; }
        const std::set<CrifRecord::Regulation>& getSimmCollectRegulations() const { return simmCollectRegulations_; }
        const std::set<CrifRecord::Regulation>& getSimmPostRegulations() const { return simmPostRegulations_; }
        // We add these to support populating a CRIF for IM Schedule calcs 
        double getNotional() const { return notional_; }
        const string& getNotionalCurrency() const { return notionalCurrency_; }
        QuantLib::Real getPresentValue() const { return presentValue_; }
        const string& getPresentValueCurrency() const { return presentValueCurrency_; }
        const string& getEndDate() const { return endDate_; }
        double getPresentValueUSD() const { return presentValueUSD_; }
        double getNotionalUSD() const { return notionalUSD_; }

        void setTradeType(const string tradeType);
        void setSimmProductClass(const CrifRecord::ProductClass& pc);
        void setScheduleProductClass(const CrifRecord::ProductClass& pc);
        // We add these to support populating a CRIF for IM Schedule calcs 
        void setNotional(double d);
        void setNotionalCurrency(const string& s);
        void setPresentValue(double d);
        void setPresentValueCurrency(const string& s);
        void setEndDate(const string& s);
        void setPresentValueUSD(double d);
        void setNotionalUSD(double d);

        /*! Set relevant extended attributes for each trade type, relevant for IM Schedule */
        virtual void
        setExtendedAttributes(const QuantLib::ext::shared_ptr<ore::data::Trade> trade,
                              const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                              const QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapper>& bucketMapper,
                              const bool emitStructuredError = true);

    protected:
        string tradeType_;
        CrifRecord::ProductClass simmProductClass_;
        CrifRecord::ProductClass scheduleProductClass_;
        std::set<CrifRecord::Regulation> simmCollectRegulations_;
        std::set<CrifRecord::Regulation> simmPostRegulations_;
        // We add these to support populating a CRIF for IM Schedule calculation
        double notional_ = 0.0;
        string notionalCurrency_;
        double presentValue_ = 0.0;
        string presentValueCurrency_;
        string endDate_;
        double presentValueUSD_ = 0.0;
        double notionalUSD_ = 0.0;
    };

    //! Default constructor giving an empty string default portfolio and counterparty ID
    SimmTradeData() : hasNettingSetDetails_(false), referenceData_(nullptr) {}

    /*! Construct from portfolio object. The portfolio ID is taken to be the
        netting set ID. Market is passed for IM Schedule related FX conversions.
    */
    SimmTradeData(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                  const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                  const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
                  const QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapper>& bucketMapper = nullptr,
                  const QuantLib::ext::shared_ptr<Portfolio>& auxiliaryPortfolio = nullptr)
        : referenceData_(referenceData), bucketMapper_(bucketMapper), portfolio_(portfolio), market_(market),
          auxiliaryPortfolio_(auxiliaryPortfolio) {}

    //! Constructor with a specific default portfolio and counterparty ID
    SimmTradeData(const string& defaultPortfolioId, const string& defaultCounterpartyId)
        : defaultPortfolioId_(defaultPortfolioId), defaultCounterpartyId_(defaultCounterpartyId),
          hasNettingSetDetails_(false), referenceData_(nullptr) {}

    virtual ~SimmTradeData() {}

    void init();
  
    //! Return the set of all trade IDs in the container
    std::set<pair<string, QuantLib::Size>> get() const;

    //! Return the set of all trade IDs in the container with the given \p portfolioId
   std::set<pair<string, QuantLib::Size>> get(const NettingSetDetails& nettingSetDetails) const;

    //! Return the set of all trade IDs in the container with the given \p nettingSetDetails
   std::set<pair<string, QuantLib::Size>> get(const string& portfolioId) const {
        return get(NettingSetDetails(portfolioId));
    }

    //! Return the set of portfolio IDs in the container
   std::set<string> portfolioIds() const;
   std::set<NettingSetDetails> nettingSetDetails() const;

    /*! Return the portfolio ID for the given \p tradeId

        \warning Throws an error if the \p tradeId is not in the container.
                 Can check before adding using <code>has</code>.
    */
    const string& portfolioId(const string& tradeId) const;
    const NettingSetDetails& nettingSetDetails(const string& tradeId) const;

    //! Return the set of counterparty IDs in the container
   std::set<string> counterpartyIds() const;

    /*! Return the counterparty ID for the given \p tradeId

        \warning Throws an error if the \p tradeId is not in the container.
                 Can check before adding using <code>has</code>.
    */
    const string& counterpartyId(const string& tradeId) const;

    //! Return true if there is already an entry for \p tradeId
    bool has(const string& tradeId) const;

    //! Return true if there is no trade data
    bool empty() const;

    //! Clear the trade data
    virtual void clear();

    //! Return true if the \p tradeId has additional attributes
    bool hasAttributes(const string& tradeId) const;

    /*! Get the additional \p attributes for a given \p tradeId.

        \warning This method throws if there are no additional attributes for the given
                 \p tradeId so use \c hasAttributes method before the call if in doubt
    */
    const QuantLib::ext::shared_ptr<TradeAttributes>& getAttributes(const string& tradeId) const;

    //! Indicate whether the trades are using netting set details instead of just netting set ID
    const bool hasNettingSetDetails() const { return hasNettingSetDetails_; }

    const std::set<string>& simmTradeIds() const { return simmTradeIds_; }

protected:
    /*! Set the additional \p attributes for a given \p tradeId. If attributes already
        exists for \p tradeId, they are overwritten.
    */
    void setAttributes(const string& tradeId, const QuantLib::ext::shared_ptr<TradeAttributes>& attributes);

    virtual void processPortfolio(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
				  const QuantLib::ext::shared_ptr<ore::data::Market>& market,
				  const QuantLib::ext::shared_ptr<Portfolio>& auxiliaryPortfolio = nullptr);

    /*! Add a \p tradeId with associated \p portfolioId and \p counterpartyId to the container

        \warning Throws an error if the \p tradeId is already in the
                 container. Can check before adding using <code>has</code>.
    */
    void add(const string& tradeId, const string& portfolioId, const string& counterpartyId);

    void add(const string& tradeId, const NettingSetDetails& nettingSetDetails, const string& counterpartyId);

    /*! Add a \p tradeId to the container

        \remark The tradeId is given the default portfolio ID and counterparty ID

        \warning Throws an error if the \p tradeId is already in the
                 container. Can check before adding using <code>has</code>.
    */
    void add(const string& tradeId);

    //! The default portfolio ID assigned to trades without one
    string defaultPortfolioId_;
    NettingSetDetails defaultNettingSetDetails_;

    //! The default counterparty ID assigned to trades without one
    string defaultCounterpartyId_;

    //! Map from trade ID to portfolio IDs (the netting set ID is the portfolio ID, but there are other netting set
    //! details used to define netting set uniqueness)
    map<string, NettingSetDetails> nettingSetDetails_;

    //! Map from trade ID to counterparty IDs
    map<string, string> counterpartyIds_;

    //! Vector of SIMM tradeIds
    std::set<string> simmTradeIds_;

    //! Indicate whether the trades are using netting set details instead of just netting set ID
    bool hasNettingSetDetails_;

    /*! Map from trade id to additional attributes for that trade id.
        This container should be empty for most trade IDs and contain exceptional elements
        that are required during CRIF generation for certain trades.
    */
    map<string, QuantLib::ext::shared_ptr<TradeAttributes>> tradeAttributes_;

    QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager> referenceData_;

    QuantLib::ext::shared_ptr<ore::analytics::SimmBucketMapper> bucketMapper_;

    // Used to fill TradeAttributes via init() and virtual processPortfolio()
    bool initialised_ = false;
    QuantLib::ext::shared_ptr<Portfolio> portfolio_;
    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    QuantLib::ext::shared_ptr<Portfolio> auxiliaryPortfolio_;
};

} // namespace analytics
} // namespace ore
