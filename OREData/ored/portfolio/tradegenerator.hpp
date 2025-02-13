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
#include <ored/utilities/parsers.hpp>
#include <qle/cashflows/equitycouponpricer.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>

#include <map>

using ore::data::XMLUtils;

namespace ore {
namespace data {

    using namespace ore::data;
using std::map;
using std::string;
class TradeGenerator : public Portfolio {
public:
    TradeGenerator();
    void buildSwap(string conventionId, Real notional, Date maturity, Real rate,
                   std::map<std::string, std::string> mapPairs = {});

    void buildFxOption(string payCcy, Real payNotional, string recCcy, Real recNotional, Date expiryDate,
                       std::map<std::string, std::string> mapPairs = {});
    void buildCapFloor(string indexName, Real capFloorRate, Real notional, Date maturity, string capFloor = "Cap",
                       std::map<std::string, std::string> mapPairs = {});
    void addConventions(std::string conventionsFile);
    void addCurveConfigs(std::string curveConfigFile);
    string freqToTenor(string freq);
    void buildEquityOption(string equityCurveId, Real quantity, Date maturity, Real strike,
                           std::map<std::string, std::string> mapPairs = {});
    void buildEquityForward(string equityCurveId, Real quantity, Date maturity, Real strike,
                            std::map<std::string, std::string> mapPairs = {});
    void buildEquitySwap(string equityCurveId, string returnType, Real quantity, string conventionId, Real notional,
                         Date maturity, std::map<std::string, std::string> mapPairs = {});
    void buildEquitySwap(string equityCurveId, string returnType, Real quantity, Real rate, Real notional,
                         Date maturity, std::map<std::string, std::string> mapPairs = {});
    Date today_;
    LegData buildOisLeg(string conventionId, Real notional, Date maturity,
                     std::map<std::string, std::string> mapPairs = {});
    LegData buildIborLeg(string conventionId, Real notional, Date maturity,
                     std::map<std::string, std::string> mapPairs = {});

    ore::data::Conventions conventions_;
    ore::data::CurveConfigurations curveConfigs_;

private:
    void buildOisSwap(string conventionId, Real notional, Date maturity, Real rate,
                      QuantLib::ext::shared_ptr<OisConvention> oisConv, std::map<std::string, std::string> mapPairs);
    void buildIborSwap(string conventionId, Real notional, Date maturity, Real rate,
                       QuantLib::ext::shared_ptr<IRSwapConvention> iborConv, std::map<std::string, std::string> mapPairs);

};
} // namespace data
} // namespace ore