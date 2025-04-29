/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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
using namespace std;
using std::map;

class TradeGenerator : public Portfolio {
public:
    
    TradeGenerator(const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfig, const QuantLib::ext::shared_ptr < BasicReferenceDataManager>& refData, string counterpartyId = "",
                   string nettingSetId = "")
        : curveConfigs_(curveConfig), referenceData_(refData), counterpartyId_(counterpartyId),
          nettingSetId_(nettingSetId) {
        setup();
    }
    TradeGenerator(string counterpartyId = "", string nettingSetId = "") { setup(); }
    virtual ~TradeGenerator() {}
    void setup();
    void addCurveConfigs(string curveConfigFile);
    void addReferenceData(string refDataFile);
    void setNettingSet(string nettingSetId);
    void setCounterpartyId(string counterpartyId);
    bool validateDate(string date);
    void buildSwap(string indexId, Real notional, string maturity, Real rate, bool firstLegPays, string tradeId = "",
                   std::map<string, string> mapPairs = {});
    void buildSwap(string indexId, Real notional, string maturity, string recIndexId, Real spread, bool firstLegPays,
                   string tradeId = "", std::map<string, string> mapPairs = {});
    void buildFxForward(string payCcy, Real payNotional, string recCcy, Real recNotional, string expiryDate,
                        bool isLong, string tradeId = "", std::map<string, string> mapPairs = {});
    void buildFxOption(string payCcy, Real payNotional, string recCcy, Real recNotional, string expiryDate, bool isLong,
                       bool isCall, string tradeId = "", std::map<string, string> mapPairs = {});
    void buildCapFloor(string indexName, Real capFloorRate, Real notional, string maturity, bool isLong, bool isCap,
                       string tradeId = "", std::map<string, string> mapPairs = {});
    void buildEquityOption(string equityCurveId, Real quantity, string maturity, Real strike, string tradeId = "",
                           std::map<string, string> mapPairs = {});
    void buildEquityForward(string equityCurveId, Real quantity, string maturity, Real strike, string tradeId = "",
                            std::map<string, string> mapPairs = {});
    void buildEquitySwap(string equityCurveId, string returnType, Real quantity, string indexId, Real notional,
                         string maturity, bool firstLegPays, string tradeId = "", std::map<string, string> mapPairs = {});
    void buildEquitySwap(string equityCurveId, string returnType, Real quantity, Real rate, Real notional,
                         string maturity, bool firstLegPays, string tradeId = "",
                         std::map<string, string> mapPairs = {});
    void buildCommoditySwap(string commodityId, string maturity, Real quantity, Real fixedPrice, string indexId,
                            string floatType, bool firstLegPays, string tradeId = "");
    void buildCommodityOption(string commodityId, Real quantity, string maturity, Real strike, string priceType,
                              bool isLong, bool isCall, string tradeId = "");
    void buildCommodityForward(string commodityId, Real quantity, string maturity, Real strike, bool isLong,
                               string tradeId = "");
    void buildInflationSwap(string inflationIndex, Real notional, string maturity, string floatIndex, Real baseRate,
                            Real cpiRate, bool firstLegPays, string tradeId = "");
    Date today_;
    map<string, QuantLib::ext::shared_ptr<Convention>> conventions_;
    QuantLib::ext::shared_ptr <CurveConfigurations> curveConfigs_;
    QuantLib::ext::shared_ptr <BasicReferenceDataManager> referenceData_;
    QuantLib::ext::shared_ptr<TodaysMarketParameters> market_;
    string counterpartyId_;
    string nettingSetId_;

private:
    string getEndDate(string maturity, string startDate, Calendar cal = QuantLib::NullCalendar());
    LegData buildLeg(boost::shared_ptr<Convention> conv, Real notional, string maturity, bool isPayer);
    string frequencyToTenor(const QuantLib::Frequency& freq);
    LegData buildCPILeg(QuantLib::ext::shared_ptr<Convention> conv, Real notional, string maturity, string currency,
                        Real baseRate, Real cpiRate, bool isPayer, std::map<string, string> mapPairs = {});
    LegData buildOisLeg(QuantLib::ext::shared_ptr<Convention> conv, Real notional, string maturity, bool isPayer,
                        std::map<string, string> mapPairs = {});
    LegData buildIborLeg(QuantLib::ext::shared_ptr<Convention> conv, Real notional, string maturity, bool isPayer,
                         std::map<string, string> mapPairs = {});
    map<string, string> fetchEquityRefData(string equityId);
    Envelope makeEnvelope();
    void addConventions();
};
} // namespace data
} // namespace ore