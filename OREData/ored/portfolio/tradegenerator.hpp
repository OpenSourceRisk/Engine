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


#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/parsers.hpp>
#include <qle/cashflows/equitycouponpricer.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>
#include <string>
#include <map>

using ore::data::XMLUtils;

namespace ore {
namespace data {

using namespace ore::data;
using std::map;
using std::string;
class TradeGenerator : public Portfolio {
public:
    TradeGenerator(std::string curveconfigFile = "", std::string counterpartyId = "", std::string nettingSetId = "");
    virtual ~TradeGenerator() {}
    void addCurveConfigs(std::string curveConfigFile);
    void addReferenceData(std::string refDataFile);
    void setNettingSet(std::string nettingSetId);
    void setCounterpartyId(std::string counterpartyId);
    bool validateDate(std::string date);
    void buildSwap(string indexId, Real notional, string maturity, Real rate, bool firstLegPays,
                   std::map<std::string, std::string> mapPairs = {});
    void buildSwap(string indexId, Real notional, string maturity, string recIndexId, Real spread, bool firstLegPays,
                   std::map<std::string, std::string> mapPairs = {});
    void buildFxForward(string payCcy, Real payNotional, string recCcy, Real recNotional, string expiryDate,
                        bool isLong, std::map<std::string, std::string> mapPairs = {});
    void buildFxOption(string payCcy, Real payNotional, string recCcy, Real recNotional, string expiryDate, bool isLong,
                       bool isCall, std::map<std::string, std::string> mapPairs = {});
    void buildCapFloor(string indexName, Real capFloorRate, Real notional, string maturity, bool isLong, bool isCap,
                       std::map<std::string, std::string> mapPairs = {});
    void buildEquityOption(string equityCurveId, Real quantity, string maturity, Real strike,
                           std::map<std::string, std::string> mapPairs = {});
    void buildEquityForward(string equityCurveId, Real quantity, string maturity, Real strike,
                            std::map<std::string, std::string> mapPairs = {});
    void buildEquitySwap(string equityCurveId, string returnType, Real quantity, string indexId, Real notional,
                         string maturity, bool firstLegPays, std::map<std::string, std::string> mapPairs = {});
    void buildEquitySwap(string equityCurveId, string returnType, Real quantity, Real rate, Real notional,
                         string maturity, bool firstLegPays, std::map<std::string, std::string> mapPairs = {});
    void buildCommoditySwap(string commodityId, string maturity, Real quantity, Real fixedPrice, string indexId,
                            string floatType, bool firstLegPays);
    void buildCommodityOption(string commodityId, Real quantity, string maturity, Real strike, string priceType,
                              bool isLong, bool isCall);
    void buildCommodityForward(string commodityId, Real quantity, string maturity, Real strike,
                               bool isLong);
    void buildInflationSwap(string inflationIndex, Real notional, string maturity, string floatIndex, Real baseRate,
                            Real cpiRate, bool firstLegPays);
    Date today_;
    map<string, QuantLib::ext::shared_ptr<Convention>> conventions_;
    CurveConfigurations curveConfigs_;
    BasicReferenceDataManager referenceData_;
    string nettingSetId_;
    string counterpartyId_;

private:
    string frequencyToTenor(const QuantLib::Frequency& freq);
    LegData buildCPILeg(QuantLib::ext::shared_ptr<Convention> conv, Real notional, string maturity, string currency,
                        Real baseRate, Real cpiRate, bool isPayer, std::map<std::string, std::string> mapPairs = {});
    LegData buildOisLeg(QuantLib::ext::shared_ptr<Convention> conv, Real notional, string maturity, bool isPayer,
                        std::map<std::string, std::string> mapPairs = {});
    LegData buildIborLeg(QuantLib::ext::shared_ptr<Convention> conv, Real notional, string maturity, bool isPayer,
                         std::map<std::string, std::string> mapPairs = {});
    map<string, string> fetchEquityRefData(string equityId);
    Envelope makeEnvelope();
    void addConventions();
};
} // namespace data
} // namespace ore