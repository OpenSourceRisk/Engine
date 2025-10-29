/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef ored_portfolio_i
#define ored_portfolio_i

%include cashflows.i
%include ored_xmlutils.i

%{
using ore::data::Portfolio;
using ore::data::Envelope;
using ore::data::MarketContext;
using ore::data::EngineData;
using ore::data::ReferenceDataManager;
using ore::data::LegBuilder;
using ore::data::EngineBuilder;
using ore::data::EngineFactory;
using OREForwardRateAgreement = ore::data::ForwardRateAgreement;
using ore::data::IborFallbackConfig;
using ore::data::Trade;
using ore::data::TradeFactory;
using ore::data::InstrumentWrapper;
using ore::data::Envelope;
using ORESwap = ore::data::Swap;
using ore::data::XMLUtils;
using ore::data::XMLSerializable;
using ore::data::RequiredFixings;
using ore::data::TradeActions;
using ore::data::TradeCashflowReportData;
using QuantLib::CashFlow;
using namespace std;
%}

%template(TradeVector) std::vector<ext::shared_ptr<Trade>>;
%template(StringStringMap) std::map<std::string, std::string>;
%template(StringTradeMap) std::map<std::string, ext::shared_ptr<Trade>>;

enum class MarketContext { irCalibration, fxCalibration, eqCalibration, pricing };

%shared_ptr(EngineData)
class EngineData : public XMLSerializable {
public:
    EngineData();
    bool hasProduct(const std::string& productName);
    const std::string& model(const std::string& productName) const;
    const std::map<std::string, std::string>& modelParameters(const std::string& productName) const;
    const std::string& engine(const std::string& productName) const;
    const std::map<std::string, std::string>& engineParameters(const std:: string& productName) const;
    const std::map<std::string, std::string>& globalParameters() const;
    std::vector<std::string> products() const;
    std::string& model(const std::string& productName);
    std::map<std::string, std::string>& modelParameters(const std::string& productName);
    std::string& engine(const std::string& productName);
    std::map<std::string, std::string>& engineParameters(const std::string& productName);
    std::map<std::string, std::string>& globalParameters();
    void clear();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(LegBuilder)
class LegBuilder {
  private:
    LegBuilder(const std::string& legType);
};

%shared_ptr(EngineBuilder)
class EngineBuilder {
  public:
    EngineBuilder(const std::string& model, const std::string& engine,
                  const std::set<std::string>& tradeTypes);
};

%shared_ptr(EngineFactory)
class EngineFactory {
  public:
    EngineFactory(const ext::shared_ptr<EngineData>& data,
                  const ext::shared_ptr<Market>& market,
                  const std::map<MarketContext, std::string>& configurations = std::map<MarketContext, std::string>(),
                  const ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                  const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig =
                     QuantLib::ext::make_shared<IborFallbackConfig>(IborFallbackConfig::defaultConfig()),
                  const std::vector<ext::shared_ptr<EngineBuilder>> extraEngineBuilders = {});
};

// TradeFactory just needed as a return type, no construction, no member functions.
%shared_ptr(TradeFactory)
class TradeFactory {
public:
};


%shared_ptr(Envelope)
class Envelope : public XMLSerializable {
public:
    Envelope();
    Envelope(const std::string& counterparty, const std::string& nettingSetId, const std::set<std::string>& portfolioIds = std::set<std::string>());;
    Envelope(const std::string& counterparty, const NettingSetDetails& nettingSetDetails = NettingSetDetails(),
             const std::set<std::string>& portfolioIds = std::set<std::string>());;
    Envelope(const std::string& counterparty, const std::map<std::string, std::string>& additionalFields);;
    Envelope(const std::string& counterparty, const std::string& nettingSetId, const std::map<std::string, std::string>& additionalFields,
             const std::set<std::string>& portfolioIds = std::set<std::string>());
    Envelope(const std::string& counterparty, const NettingSetDetails& nettingSetDetails,
             const std::map<std::string, std::string>& additionalFields, const std::set<std::string>& portfolioIds = std::set<std::string>());

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& counterparty() const;
    const std::string& nettingSetId() const;
    const NettingSetDetails nettingSetDetails() const;
    const std::set<std::string>& portfolioIds() const;
    const std::map<std::string, std::string> additionalFields() const;
    const std::map<std::string, QuantLib::ext::any>& fullAdditionalFields() const;
    string additionalField(const std::string& name, const bool mandatory = true,
                           const std::string& defaultValue = std::string()) const;
    QuantLib::ext::any additionalAnyField(const std::string& name, const bool mandatory = true,
                                  const QuantLib::ext::any& defaultValue = boost::none) const;
    void setAdditionalField(const std::string& key, const QuantLib::ext::any& value);
    bool initialized() const;
    bool hasNettingSetDetails() const;
};

// InstrumentWrapper pointer required as a return type only
%shared_ptr(InstrumentWrapper)
class InstrumentWrapper {
  private:
    InstrumentWrapper();
  public:
    Real NPV() const;
    ext::shared_ptr<QuantLib::Instrument> qlInstrument() const;
};

// Trade pointer required as a return type only
%shared_ptr(Trade)
class Trade : public XMLSerializable {                          
  private:
    Trade();
  public:
    const std::string& id();
    void setId(const std::string& id);
    const std::string& tradeType();
    const ext::shared_ptr<InstrumentWrapper>& instrument();
    std::vector<std::vector<ext::shared_ptr<QuantLib::CashFlow>>> legs();
    const Envelope& envelope() const;
    const QuantLib::Date& maturity();
    Real notional();
};

// Portfolio
%shared_ptr(Portfolio)
class Portfolio : public XMLSerializable {
  public:
    Portfolio(bool buildFailedTrades = true);
    std::size_t size() const;
    std::set<std::string> ids() const;
    void add(const ext::shared_ptr<Trade>& trade);
    bool has(const std::string& id);
    ext::shared_ptr<Trade> get(const std::string& id) const;
    const std::map<std::string, ext::shared_ptr<Trade>>& trades() const;
    bool remove(const std::string& tradeID);
    void fromFile(const std::string& fileName);
    void fromXMLString(const std::string& xmlString);    
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::string toXMLString();
    void build(const ext::shared_ptr<EngineFactory>& factory,
               const std::string& context = "unspecified",
               const bool emitStructuredError = true);
 };

//ORESwap
%shared_ptr(ORESwap)
class ORESwap : public Trade {
public:
    ORESwap(const std::string swapType = "Swap");
    ORESwap(const Envelope& env, const std::string swapType = "Swap");
    ORESwap(const Envelope& env, const std::vector<LegData>& legData, const std::string swapType = "Swap",
         const std::string settlement = "Physical");
    ORESwap(const Envelope& env, const LegData& leg0, const LegData& leg1, const std::string swapType = "Swap",
         const std::string settlement = "Physical");

    void build(const ext::shared_ptr<EngineFactory>&) override;
    virtual void setIsdaTaxonomyFields();
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;

    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    const std::string& settlement() const { return settlement_; }
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void fromXMLString(const std::string& xmlString);
    std::string toXMLString();
    const std::vector<LegData>& legData() const { return legData_; }
    const std::map<std::string,QuantLib::ext::any>& additionalData() const override;
};

%shared_ptr(OREForwardRateAgreement)
class OREForwardRateAgreement : public ORESwap {
public:
    OREForwardRateAgreement();
    OREForwardRateAgreement(Envelope& env, std::string longShort, std::string currency, std::string startDate, std::string endDate,
                         std::string index, double strike, double amount);

    void build(const ext::shared_ptr<EngineFactory>& engineFactory) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void fromXMLString(const std::string& xmlString);
    std::string toXMLString();
    const std::string& index() const { return index_; }

    const std::map<std::string,QuantLib::ext::any>& additionalData() const override;
};

#endif
