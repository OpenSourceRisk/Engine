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

#ifndef ored_tradegenerator_i
#define ored_tradegenerator_i
%include ored_portfolio.i
%include ored_referencedatamanager.i
%include ored_xmlutils.i
%include std_set.i

%{
using ore::data::Portfolio;
using ore::data::TradeGenerator;
using ore::data::LegData;
using ore::data::XMLUtils;
using ore::data::XMLSerializable;
%}

%shared_ptr(TradeGenerator)
class TradeGenerator : public Portfolio {
public:

    TradeGenerator(std::string counterpartyId = "", std::string nettingSetId = "");
    TradeGenerator(ext::shared_ptr<CurveConfigurations> curveConfig, ext::shared_ptr < BasicReferenceDataManager> refData,
                   std::string counterpartyId = "", std::string nettingSetId = "");

    void buildSwap(std::string indexId, QuantLib::Real notional, std::string maturity, QuantLib::Real rate, bool firstLegPays,
                   string start = string(), std::string tradeId = "", std::map<std::string, std::string> mapPairs = {});
    void buildSwap(std::string indexId, QuantLib::Real notional, std::string maturity, std::string recIndexId, QuantLib::Real spread, bool firstLegPays,
                   string start = string(), std::string tradeId = "", std::map<std::string, std::string> mapPairs = {});
    void buildCapFloor(std::string indexName, QuantLib::Real capFloorRate, QuantLib::Real notional, std::string maturity, bool isLong, bool isCap,
                   std::string tradeId = "", std::map<std::string, std::string> mapPairs = {});
    void buildInflationSwap(std::string inflationIndex, QuantLib::Real notional, std::string maturity, std::string floatIndex, QuantLib::Real baseRate, QuantLib::Real cpiRate, bool firstLegPays,
                   std::string tradeId = "");
    void buildFxForward(std::string payCcy, QuantLib::Real payNotional, std::string recCcy, QuantLib::Real recNotional, std::string expiryDate,
                        bool isLong, std::string tradeId = "", std::map<std::string, std::string> mapPairs = {});
    void buildFxOption(std::string payCcy, QuantLib::Real payNotional, std::string recCcy, QuantLib::Real recNotional, std::string expiryDate, bool isLong,
                       bool isCall, std::string tradeId = "", std::map<std::string, std::string> mapPairs = {});
    void addCurveConfigs(std::string curveConfigFile);
    void setNettingSet(std::string nettingSetId);
    void setCounterpartyId(std::string counterpartyId);
    void buildEquityOption(std::string equityCurveId, QuantLib::Real quantity, std::string maturity, QuantLib::Real strike,
                   std::string tradeId = "",std::map<std::string, std::string> mapPairs = {});
    void buildEquityForward(std::string equityCurveId, QuantLib::Real quantity, std::string maturity, QuantLib::Real strike,
                   std::string tradeId = "",std::map<std::string, std::string> mapPairs = {});
    void buildEquitySwap(std::string equityCurveId, std::string returnType, QuantLib::Real quantity, std::string indexId, QuantLib::Real notional,
                         std::string maturity, bool firstLegPays, std::string tradeId = "", std::map<std::string, std::string> mapPairs = {});
    void buildEquitySwap(std::string equityCurveId, std::string returnType, QuantLib::Real quantity, QuantLib::Real rate, QuantLib::Real notional,
                         std::string maturity, bool firstLegPays, std::string tradeId = "", std::map<std::string, std::string> mapPairs = {});
    void buildCommoditySwap(std::string commodityId, std::string maturity, QuantLib::Real quantity, QuantLib::Real fixedPrice, std::string indexId,
                            std::string floatType, bool firstLegPays, std::string tradeId = "");
    void buildCommodityOption(std::string commodityId, QuantLib::Real quantity, std::string maturity, QuantLib::Real strike, std::string priceType,
                              bool isLong, bool isCall);
    Date today_;
    void buildCommodityForward(std::string commodityId, QuantLib::Real quantity, std::string maturity, QuantLib::Real strike,
                               bool isLong, std::string tradeId = "");
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

#endif