/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef ored_portfolio_core_i
#define ored_portfolio_core_i

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
using ore::data::IborFallbackConfig;
using ore::data::Trade;
using ore::data::TradeFactory;
using ore::data::InstrumentWrapper;
using ore::data::XMLSerializable;
using QuantLib::CashFlow;
%}

%include std_string.i

%template(TradeVector) std::vector<ext::shared_ptr<Trade>>;
%template(StringStringMap) std::map<std::string, std::string>;
%template(StringTradeMap) std::map<std::string, ext::shared_ptr<Trade>>;

%{
template<typename T>
std::vector<T> VECTOR_SWIG_TO_ORE(const std::vector<ext::shared_ptr<T>>& v) {
    std::vector<T> ret;
    for (const ext::shared_ptr<T>& t : v)
        ret.push_back(*t);
    return ret;
}
%}

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
    void setModel(const std::string& productName, const std::string& model) { model_[productName] = model; }
    void setModelParameters(const std::string& productName, const std::map<std::string, std::string>& params) { modelParams_[productName] = params; }
    void setEngine(const std::string& productName, const std::string& engine) { engine_[productName] = engine; }
    void setEngineParameters(const std::string& productName, const std::map<std::string, std::string>& params) { engineParams_[productName] = params; }
    void setGlobalParameter(const std::string& name, const std::string& param) { globalParams_[name] = param; }
    void clear();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
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

%shared_ptr(TradeFactory)
class TradeFactory {
public:
};

%shared_ptr(Envelope)
class Envelope : public XMLSerializable {
public:
    Envelope();
    Envelope(const std::string& counterparty, const std::string& nettingSetId, const std::set<std::string>& portfolioIds = std::set<std::string>());
    Envelope(const std::string& counterparty, const NettingSetDetails& nettingSetDetails = NettingSetDetails(),
             const std::set<std::string>& portfolioIds = std::set<std::string>());
    Envelope(const std::string& counterparty, const std::map<std::string, std::string>& additionalFields);
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
    std::string additionalField(const std::string& name, const bool mandatory = true,
                  const std::string& defaultValue = std::string()) const;
    QuantLib::ext::any additionalAnyField(const std::string& name, const bool mandatory = true,
                                  const QuantLib::ext::any& defaultValue = boost::none) const;
    void setAdditionalField(const std::string& key, const QuantLib::ext::any& value);
    bool initialized() const;
    bool hasNettingSetDetails() const;
};

%shared_ptr(InstrumentWrapper)
class InstrumentWrapper {
  private:
    InstrumentWrapper();
  public:
    Real NPV() const;
    ext::shared_ptr<QuantLib::Instrument> qlInstrument() const;
};

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

#endif
