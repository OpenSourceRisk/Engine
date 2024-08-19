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

/*! \file portfolio/referencedata.hpp
    \brief Reference data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/referencedatafactory.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/underlying.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/patterns/singleton.hpp>
#include <ql/time/date.hpp>
#include <ql/time/period.hpp>
#include <set>
#include <tuple>

namespace ore {
namespace data {

//! Base class for reference data
/*! Each reference datum object is a subclass of this base and has it's own accessor functions.
 *  Instances of ReferenceDatum can be gotten from the ReferenceDataManager below, and then cast up as required.
 *  Each instance should be uniquely identified by it's type (which defines it's subclass, e.g. "Bond" for
 *  BondReferenceDatum) and it's id, which is a string. Here it can be any string but in applications there can be
 *  a naming scheme like ISIN for Bonds.
 */
class ReferenceDatum : public XMLSerializable {
public:
    //! Default Constructor
    ReferenceDatum() : validFrom_(QuantLib::Date::minDate()) {}
    //! Base class constructor
    ReferenceDatum(const std::string& type, const std::string& id)
        : type_(type), id_(id), validFrom_(QuantLib::Date::minDate()) {}
    //! Base class constructor
    ReferenceDatum(const std::string& type, const std::string& id, const QuantLib::Date& validFrom)
        : type_(type), id_(id), validFrom_(validFrom) {}

    //! setters
    void setType(const string& type) { type_ = type; }
    void setId(const string& id) { id_ = id; }
    void setValidFrom(const QuantLib::Date& validFrom) { validFrom_ = validFrom; }

    //! getters
    const std::string& type() const { return type_; }
    const std::string& id() const { return id_; }
    const QuantLib::Date& validFrom() const { return validFrom_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

private:
    std::string type_;
    std::string id_;
    QuantLib::Date validFrom_;
};

/*
<ReferenceDatum id="US12345678">
  <Type>Bond</Type>
  <BondReferenceData>
    <IssuerId>...</IssuerId>
    <SettlementDays>...</SettlementDays>
    <Calendar>...</Calendar>
    <IssueDate>...</IssueDate>
    <CreditCurveId>...</CreditCurveId>
    <ReferenceCurveId>...</ReferenceCurveId>
    <IncomCurveId>...</IncomeCurveId>
    <LegData>...</LegData>
  </BondReferenceData>
</ReferenceDatum>
*/
class LegData;
class BondReferenceDatum : public ReferenceDatum {
public:
    static constexpr const char* TYPE = "Bond";

    struct BondData : XMLSerializable {
        string issuerId;
        string settlementDays;
        string calendar;
        string issueDate;
        string creditCurveId;
        string creditGroup;
        string referenceCurveId;
        string incomeCurveId;
        string volatilityCurveId;
	string priceQuoteMethod;
        string priceQuoteBaseValue;
        std::vector<LegData> legData;
        string subType;
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    };

    BondReferenceDatum() { setType(TYPE); }

    BondReferenceDatum(const string& id) : ReferenceDatum(TYPE, id) {}

    BondReferenceDatum(const string& id, const QuantLib::Date& validFrom) : ReferenceDatum(TYPE, id, validFrom) {}

    BondReferenceDatum(const string& id, const BondData& bondData) : ReferenceDatum(TYPE, id), bondData_(bondData) {}

    BondReferenceDatum(const string& id, const QuantLib::Date& validFrom, const BondData& bondData)
        : ReferenceDatum(TYPE, id, validFrom), bondData_(bondData) {}

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    const BondData& bondData() const { return bondData_; }
    void setBondData(const BondData& bondData) { bondData_ = bondData; }

private:
    BondData bondData_;
};

/*! Hold reference data on a constituent of a credit index.

    Gives the name and the weight of the credit index constituent. A weight of zero indicates that there has been a
    credit event relating to the constituent. In this case, the weight of the constituent prior to the credit event
    is supplied along with the recovery rate, i.e. final auction price, default date, event determination date, 
    auction date and auction cash settlement date. Not all of these fields are required by every engine in the event 
    of default.
*/
class CreditIndexConstituent : public XMLSerializable {
public:
    CreditIndexConstituent();

    CreditIndexConstituent(const std::string& name,
                           QuantLib::Real weight,
                           QuantLib::Real priorWeight = QuantLib::Null<QuantLib::Real>(),
                           QuantLib::Real recovery = QuantLib::Null<QuantLib::Real>(),
                           const QuantLib::Date& auctionDate = QuantLib::Date(),
                           const QuantLib::Date& auctionSettlementDate = QuantLib::Date(),
                           const QuantLib::Date& defaultDate = QuantLib::Date(),
                           const QuantLib::Date& eventDeterminationDate = QuantLib::Date());

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    const std::string& name() const;
    QuantLib::Real weight() const;
    QuantLib::Real priorWeight() const;
    QuantLib::Real recovery() const;
    const QuantLib::Date& auctionDate() const;
    const QuantLib::Date& auctionSettlementDate() const;
    const QuantLib::Date& defaultDate() const;
    const QuantLib::Date& eventDeterminationDate() const;

private:
    std::string name_;
    QuantLib::Real weight_;
    QuantLib::Real priorWeight_;
    QuantLib::Real recovery_;
    QuantLib::Date auctionDate_;
    QuantLib::Date auctionSettlementDate_;
    QuantLib::Date defaultDate_;
    QuantLib::Date eventDeterminationDate_;
};

//! Compare CreditIndexConstituent instances using their name
bool operator<(const CreditIndexConstituent& lhs, const CreditIndexConstituent& rhs);

//! Credit index reference data, contains a set of index constituents.
class CreditIndexReferenceDatum : public ReferenceDatum {
public:
    static constexpr const char* TYPE = "CreditIndex";

    CreditIndexReferenceDatum();

    CreditIndexReferenceDatum(const std::string& name);

    CreditIndexReferenceDatum(const string& id, const QuantLib::Date& validFrom);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    //! Add a constituent. The constituent is not added if already present.
    void add(const CreditIndexConstituent& c);

    //! Get all of the underlying constituents.
    const std::set<CreditIndexConstituent>& constituents() const;

    const std::string& indexFamily() const { return indexFamily_; }

    void setIndexFamily(const std::string& indexFamily) { indexFamily_ = indexFamily; }

private:
    std::set<CreditIndexConstituent> constituents_;
    std::string indexFamily_;
};


/*
<ReferenceDatum id="SP500">
  <Type>EquityIndex</Type>
  <EquityIndexReferenceData>
      <Underlying>
        <Name>Apple</Name>
        <Weight>0.03</Weight>
      </Underlying>
      ...
  </EquityIndexReferenceData>
</ReferenceDatum>
*/
//! Base class for indices - lets see if we can keep this, they might diverge too much...
class IndexReferenceDatum : public ReferenceDatum {
protected:
    IndexReferenceDatum() {}
    IndexReferenceDatum(const string& type, const string& id) : ReferenceDatum(type, id) {}
    IndexReferenceDatum(const string& type, const string& id, const QuantLib::Date& validFrom)
        : ReferenceDatum(type, id, validFrom) {}

public:
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    // Get all underlyings (names and weights)
    const map<string, double> underlyings() const { return data_; }
    // Set all underlying (or reset)
    void setUnderlyings(const std::map<string, double>& data) { data_ = data; }
    // add a new underlying
    void addUnderlying(const string& name, double weight) { data_[name]+=weight; }

private:
    std::map<std::string, double> data_;
};

//! EquityIndex Reference data, contains the names and weights of an equity index
class EquityIndexReferenceDatum : public IndexReferenceDatum {
public:
    static constexpr const char* TYPE = "EquityIndex";

    EquityIndexReferenceDatum() {}
    EquityIndexReferenceDatum(const string& name) : IndexReferenceDatum(TYPE, name) {}
    EquityIndexReferenceDatum(const string& name, const QuantLib::Date& validFrom)
        : IndexReferenceDatum(TYPE, name, validFrom) {}
};


//! EquityIndex Reference data, contains the names and weights of an equity index
class CommodityIndexReferenceDatum : public IndexReferenceDatum {
public:
    static constexpr const char* TYPE = "Commodity";

    CommodityIndexReferenceDatum() {}
    CommodityIndexReferenceDatum(const string& name) : IndexReferenceDatum(TYPE, name) {}
    CommodityIndexReferenceDatum(const string& name, const QuantLib::Date& validFrom)
        : IndexReferenceDatum(TYPE, name, validFrom) {}
};

/*
<ReferenceDatum id="RIC:.SPXEURHedgedMonthly">
  <Type>CurrencyHedgedEquityIndex</Type>
  <CurrencyHedgedEquityIndexReferenceDatum>
      <UnderlyingIndex>RIC:.SPX</UnderlyingIndex>
      <HedgeCurrency>EUR</HedgeCurrency>
      <RebalancingStrategy>EndOfMonth</RebalancingStrategy>
      <ReferenceDateOffset>1</ReferenceDateOffset>
      <HedgeAdjustment>None|Daily</HedgeAdjustment>
      <HedgeCalendar>EUR,USD</HedgeCalendar>
      <FxIndex>ECB-EUR-USD</FxIndex>
      <IndexWeightsAtLastRebalancingDate>
        <Underlying>
            <Name>Apple</Name>
            <Weight>0.1</Weight>
        </Underlying>
        ...
      </IndexWeightsAtLastRebalancingDate>
  </CurrencyHedgedEquityIndexReferenceDatum>
</ReferenceDatum>
*/
class CurrencyHedgedEquityIndexReferenceDatum : public ReferenceDatum {
public:
    static constexpr const char* TYPE = "CurrencyHedgedEquityIndex";

    struct RebalancingDate {
        enum Strategy { EndOfMonth };
    };

    struct HedgeAdjustment {
        enum Rule { None, Daily };
    };

    CurrencyHedgedEquityIndexReferenceDatum()
        : underlyingIndexName_(""), rebalancingStrategy_(RebalancingDate::Strategy::EndOfMonth),
          referenceDateOffset_(0), hedgeAdjustmentRule_(HedgeAdjustment::Rule::None), hedgeCalendar_(WeekendsOnly()) {
        setType(TYPE);
    }

    CurrencyHedgedEquityIndexReferenceDatum(const string& name)
        : ReferenceDatum(TYPE, name), underlyingIndexName_(""),
          rebalancingStrategy_(RebalancingDate::Strategy::EndOfMonth), referenceDateOffset_(0),
          hedgeAdjustmentRule_(HedgeAdjustment::Rule::None), hedgeCalendar_(WeekendsOnly()) {}

    CurrencyHedgedEquityIndexReferenceDatum(const string& name, const QuantLib::Date& validFrom)
        : ReferenceDatum(TYPE, name, validFrom), underlyingIndexName_(""), 
          rebalancingStrategy_(RebalancingDate::Strategy::EndOfMonth), referenceDateOffset_(0),
          hedgeAdjustmentRule_(HedgeAdjustment::Rule::None), hedgeCalendar_(WeekendsOnly()) {}

    const std::string& underlyingIndexName() const { return underlyingIndexName_; }
    int referenceDateOffset() const { return referenceDateOffset_; }
    RebalancingDate::Strategy rebalancingStrategy() const { return rebalancingStrategy_; }
    HedgeAdjustment::Rule hedgeAdjustmentRule() const { return hedgeAdjustmentRule_; }
    QuantLib::Calendar hedgeCalendar() const { return hedgeCalendar_; }
    const std::map<std::string, std::string>& fxIndexes() const { return fxIndexes_; }
    //! Returns the currency weights at the last rebalancing date
    const std::map<string, double>& currencyWeights() const { return data_; }

    Date referenceDate(const Date& asof);
    Date rebalancingDate(const Date& asof);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

private:
    std::string underlyingIndexName_;
    RebalancingDate::Strategy rebalancingStrategy_;
    int referenceDateOffset_;
    HedgeAdjustment::Rule hedgeAdjustmentRule_;

    QuantLib::Calendar hedgeCalendar_;
    std::map<std::string, std::string> fxIndexes_;
    map<string, double> data_;
};

/*
<ReferenceDatum id="MSFDSJP">
 <Type>PortfolioBasket</Type>
 <PortfolioBasketReferenceData>
  <Components>
   <Trade>
    <TradeType>EquityPosition</TradeType>
     <Envelope>
      <CounterParty>{{netting_set_id}}</CounterParty>
       <NettingSetId>{{netting_set_id}}</NettingSetId>
       <AdditionalFields>
        <valuation_date>2023-11-07</valuation_date>
        <im_model>SIMM</im_model>
        <post_regulations>SEC</post_regulations>
        <collect_regulations>SEC</collect_regulations>
       </AdditionalFields>
      </Envelope>
      <EquityPositionData>
       <Quantity>7006.0</Quantity>
        <Underlying>
         <Type>Equity</Type>
         <Name>CR.N</Name>
         <IdentifierType>RIC</IdentifierType>
        </Underlying>
       </EquityPositionData>
     </Trade>
      <Trade id="CashSWAP_USD.CASH">
       <TradeType>Swap</TradeType>
        <Envelope>
          <CounterParty>{{netting_set_id}}</CounterParty>
           <NettingSetId>{{netting_set_id}}</NettingSetId>
           <AdditionalFields>
            <valuation_date>2023-11-07</valuation_date>
            <im_model>SIMM</im_model>
            <post_regulations>SEC</post_regulations>
            <collect_regulations>SEC</collect_regulations>
           </AdditionalFields>
          </Envelope>
          <SwapData>
           <LegData>
           <Payer>true</Payer>
           <LegType>Cashflow</LegType>
           <Currency>USD</Currency>
           <CashflowData>
            <Cashflow>
             <Amount date="2023-11-08">28641475.824680243</Amount>
            </Cashflow>
           </CashflowData>
       </LegData>
      </SwapData>
     </Trade>
   </Components>
  </PortfolioBasketReferenceData>
</ReferenceDatum>
*/
class PortfolioBasketReferenceDatum : public ReferenceDatum {
public:
    static constexpr const char* TYPE = "PortfolioBasket";


    PortfolioBasketReferenceDatum() {
        setType(TYPE);
    }

    PortfolioBasketReferenceDatum(const string& id) : ReferenceDatum(TYPE, id) {}

    PortfolioBasketReferenceDatum(const string& id, const QuantLib::Date& validFrom)
        : ReferenceDatum(TYPE, id, validFrom) {}

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    const vector<QuantLib::ext::shared_ptr<Trade>>& getTrades() const { return tradecomponents_; }

private:
    vector<QuantLib::ext::shared_ptr<Trade>> tradecomponents_;
};


//! CreditIndex Reference data, contains the names and weights of a credit index
class CreditReferenceDatum : public ReferenceDatum {
public:
    static constexpr const char* TYPE = "Credit";

    struct CreditData {
        string name;
        string group;
        string successor;
        string predecessor;
        QuantLib::Date successorImplementationDate;
        QuantLib::Date predecessorImplementationDate;
        string entityType;
    };
    CreditReferenceDatum() {}

    CreditReferenceDatum(const string& id) : ReferenceDatum(TYPE, id) {}

    CreditReferenceDatum(const string& id, const QuantLib::Date& validFrom) : ReferenceDatum(TYPE, id, validFrom) {}

    CreditReferenceDatum(const string& id, const CreditData& creditData)
        : ReferenceDatum(TYPE, id), creditData_(creditData) {}

    CreditReferenceDatum(const string& id, const QuantLib::Date& validFrom, const CreditData& creditData)
        : ReferenceDatum(TYPE, id, validFrom), creditData_(creditData) {}

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    const CreditData& creditData() const { return creditData_; }
    void setCreditData(const CreditData& creditData) { creditData_ = creditData; }

private:
    CreditData creditData_;
};


//! Equity Reference data
class EquityReferenceDatum : public ReferenceDatum {
public:
    static constexpr const char* TYPE = "Equity";

    struct EquityData {
        std::string equityId;
        std::string equityName;
        std::string currency;
        QuantLib::Size scalingFactor;
        std::string exchangeCode;
        bool isIndex;
        QuantLib::Date equityStartDate;
        std::string proxyIdentifier;
        std::string simmBucket;
        std::string crifQualifier;
        std::string proxyVolatilityId;
    };

    EquityReferenceDatum() { setType(TYPE); }

    EquityReferenceDatum(const std::string& id) : ore::data::ReferenceDatum(TYPE, id) {}

    EquityReferenceDatum(const std::string& id, const QuantLib::Date& validFrom)
        : ore::data::ReferenceDatum(TYPE, id, validFrom) {}

    EquityReferenceDatum(const std::string& id, const EquityData& equityData) : ReferenceDatum(TYPE, id), equityData_(equityData) {}

    EquityReferenceDatum(const std::string& id, const QuantLib::Date& validFrom, const EquityData& equityData)
        : ReferenceDatum(TYPE, id, validFrom), equityData_(equityData) {}


    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;
   
    const EquityData& equityData() const { return equityData_; }
    void setEquityData(const EquityData& equityData) { equityData_ = equityData; }

protected:
    EquityData equityData_;
};

//! Bond Basket Reference Data
class BondBasketReferenceDatum : public ReferenceDatum {
public:
    static constexpr const char* TYPE = "BondBasket";

    BondBasketReferenceDatum() { setType(TYPE); }

    BondBasketReferenceDatum(const std::string& id) : ore::data::ReferenceDatum(TYPE, id) {}

    BondBasketReferenceDatum(const std::string& id, const QuantLib::Date& validFrom) : ore::data::ReferenceDatum(TYPE, id, validFrom) {}

    BondBasketReferenceDatum(const std::string& id, const std::vector<BondUnderlying>& underlyingData)
        : ReferenceDatum(TYPE, id), underlyingData_(underlyingData) {}
    
    BondBasketReferenceDatum(const std::string& id,
                             const QuantLib::Date& validFrom, const std::vector<BondUnderlying>& underlyingData)
        : ReferenceDatum(TYPE, id, validFrom), underlyingData_(underlyingData) {}

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    const std::vector<BondUnderlying>& underlyingData() const { return underlyingData_; }

private:
    std::vector<BondUnderlying> underlyingData_;
};
    
//! Interface for Reference Data lookups
/*! The ReferenceDataManager is a repository of ReferenceDatum objects.
 *
 *  This is an interface, there is a concrete impl below that is BasicReferenceDataManager which is filebased.
 *  It is also possible to have a transactional implementation that will fetch reference data from a DB or via
 *  a restful call, possibly with a cache.
 *
 *  An instance of this class is made available to Trades as they are built, this code be done with a singleton
 *  but it is clearer to pass a pointer to an instance when the Trade is built itself, as there is no access method
 *  for Trade::build() we instead pass a pointer to the Trade constructors, this is then populated when TradeBuilders
 *  are created for ORE+ trades, this also allows us to have custom ORE+ versions of Trades that overload the build()
 *  method to use this data in place of extracting it from XML.
 *
 *  The actual Trade objects will take a copy of any reference data they need, this way they own all the required data
 *  and a call to "Trade::toXML()" will write out the "full" trade. For example we might load a CDS Index trade using
 *  reference data from which the basket is created, but if we call "toXML()" on that CDS Index the trade the whole
 *  basket will be written out.
 */
class ReferenceDataManager {
public:
    virtual ~ReferenceDataManager() {}
    virtual bool hasData(const string& type, const string& id,
                         const QuantLib::Date& asof = QuantLib::Null<QuantLib::Date>()) = 0;
    virtual QuantLib::ext::shared_ptr<ReferenceDatum>
    getData(const string& type, const string& id, const QuantLib::Date& asof = QuantLib::Null<QuantLib::Date>()) = 0;
    virtual void add(const QuantLib::ext::shared_ptr<ReferenceDatum>& referenceDatum) = 0;
};

//! Basic Concrete impl that loads an big XML and keeps data in memory
class BasicReferenceDataManager : public ReferenceDataManager, public XMLSerializable {
public:
    BasicReferenceDataManager() {}
    BasicReferenceDataManager(const string& filename) { fromFile(filename); }

    // Load extra data and append to this manger
    void appendData(const string& filename) { fromFile(filename); }

    QuantLib::ext::shared_ptr<ReferenceDatum> buildReferenceDatum(const string& refDataType);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    // clear this ReferenceData manager, note that we can load multiple files
    void clear() { data_.clear(); }

    bool hasData(const string& type, const string& id,
                 const QuantLib::Date& asof = QuantLib::Null<QuantLib::Date>()) override;
    QuantLib::ext::shared_ptr<ReferenceDatum> getData(const string& type, const string& id,
                                              const QuantLib::Date& asof = QuantLib::Null<QuantLib::Date>()) override;
    void add(const QuantLib::ext::shared_ptr<ReferenceDatum>& referenceDatum) override;
    // adds a datum from an xml node and returns it (or nullptr if nothing was added due to an error)
    QuantLib::ext::shared_ptr<ReferenceDatum> addFromXMLNode(XMLNode* node, const std::string& id = std::string(),
                                                     const QuantLib::Date& validFrom = QuantLib::Null<QuantLib::Date>());

protected:
    std::tuple<QuantLib::Date, QuantLib::ext::shared_ptr<ReferenceDatum>> latestValidFrom(const string& type, const string& id,
                                                                                  const QuantLib::Date& asof) const;
    void check(const string& type, const string& id, const QuantLib::Date& asof) const;
    map<std::pair<string, string>, std::map<QuantLib::Date, QuantLib::ext::shared_ptr<ReferenceDatum>>> data_;
    std::set<std::tuple<string, string, QuantLib::Date>> duplicates_;
    map<std::pair<string, string>, std::map<QuantLib::Date, string>> buildErrors_;
};

} // namespace data
} // namespace ore
