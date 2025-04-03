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
using ore::data::Envelope;
using QuantLib::CashFlow;
using namespace std;
%}

%template(TradeVector) std::vector<ext::shared_ptr<Trade>>;
%template(StringStringMap) std::map<std::string, std::string>;
%template(StringTradeMap) std::map<std::string, ext::shared_ptr<Trade>>;

enum class MarketContext { irCalibration, fxCalibration, eqCalibration, pricing };

%shared_ptr(EngineData)
class EngineData {
  public:
    EngineData();
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
                  const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                  const std::vector<ext::shared_ptr<EngineBuilder>> extraEngineBuilders = {},
                  const bool allowOverwrite = false);
};

// TradeFactory just needed as a return type, no construction, no member functions.
%shared_ptr(TradeFactory)
class TradeFactory {
public:
};

// Envelope is not wrapped into a pointer, because we pass it around in ORE as reference
class Envelope {
public:
    const std::string& counterparty() const;
    const std::string& nettingSetId() const;
    const std::map<std::string, std::string> additionalFields() const;
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
class Trade {
  private:
    Trade();
  public:
    const std::string& id();
    const std::string& tradeType();
    const ext::shared_ptr<InstrumentWrapper>& instrument();
    std::vector<std::vector<ext::shared_ptr<QuantLib::CashFlow>>> legs();
    const Envelope& envelope() const;
    const QuantLib::Date& maturity();
    Real notional();
};


// Portfolio
%shared_ptr(Portfolio)
class Portfolio {
  public:
    Portfolio(bool buildFailedTrades = true);
    std::size_t size() const;
    std::set<std::string> ids() const;
    ext::shared_ptr<Trade> get(const std::string& id) const;
    const std::map<std::string, ext::shared_ptr<Trade>>& trades() const;
    bool remove(const std::string& tradeID);
    void fromFile(const std::string& fileName);
    void fromXMLString(const std::string& xmlString);
    std::string toXMLString();
    void build(const ext::shared_ptr<EngineFactory>& factory,
               const std::string& context = "unspecified",
               const bool emitStructuredError = true);
 };

#endif
