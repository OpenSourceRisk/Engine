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

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/trade.hpp>

using namespace QuantLib;
using namespace ore::data;
using std::ostringstream;
using std::pair;
using std::vector;

namespace testsuite {

//! Utilities to set up simple test trades
/*!
  \ingroup tests
*/

string toString(Date d);

QuantLib::ext::shared_ptr<Trade> buildSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term, Real rate,
                                   Real spread, string fixedFreq, string fixedDC, string floatFreq, string floatDC,
                                   string index, Calendar cal = TARGET(),
                                   QuantLib::Natural fixingDays = 2, bool spotStartLag = false);

QuantLib::ext::shared_ptr<Trade> buildEuropeanSwaption(string id, string longShort, string ccy, bool isPayer, Real notional,
                                               int start, Size term, Real rate, Real spread, string fixedFreq,
                                               string fixedDC, string floatFreq, string floatDC, string index,
                                               string cashPhysical = "Cash", Real premium = 0.0, string premiumCcy = "",
                                               string premiumDate = "");

QuantLib::ext::shared_ptr<Trade> buildBermudanSwaption(string id, string longShort, string ccy, bool isPayer, Real notional,
                                               Size exercises, int start, Size term, Real rate, Real spread,
                                               string fixedFreq, string fixedDC, string floatFreq, string floatDC,
                                               string index, string cashPhysical = "Cash", Real premium = 0.0,
                                               string premiumCcy = "", string premiumDate = "");

QuantLib::ext::shared_ptr<Trade> buildFxOption(string id, string longShort, string putCall, Size expiry, string boughtCcy,
                                       Real boughtAmount, string soldCcy, Real soldAmount, Real premium = 0.0,
                                       string premiumCcy = "", string premiumDate = "");

QuantLib::ext::shared_ptr<Trade> buildEquityOption(string id, string longShort, string putCall, Size expiry, string equityName,
                                           string currency, Real strike, Real quantity, Real premium = 0.0,
                                           string premiumCcy = "", string premiumDate = "");

QuantLib::ext::shared_ptr<Trade> buildEquityForward(string id, string longShort, Size expiry, string equityName,
                                            string currency, Real strike, Real quantity);

QuantLib::ext::shared_ptr<Trade> buildCap(string id, string ccy, string longShort, Real capRate, Real notional, int start,
                                  Size term, string floatFreq, string floatDC, string index,
                                  Calendar cal = TARGET(), QuantLib::Natural fixingDays = 2,
                                  bool spotStartLag = false);

QuantLib::ext::shared_ptr<Trade> buildFloor(string id, string ccy, string longShort, Real floorRate, Real notional, int start,
                                    Size term, string floatFreq, string floatDC, string index,
                                    Calendar cal = TARGET(), QuantLib::Natural fixingDays = 2,
                                    bool spotStartLag = false);

QuantLib::ext::shared_ptr<Trade> buildCapFloor(string id, string ccy, string longShort, vector<Real> capRates,
                                       vector<Real> floorRates, Real notional, int start, Size term, string floatFreq,
                                       string floatDC, string index,
                                       Calendar cal = TARGET(), QuantLib::Natural fixingDays = 2,
                                       bool spotStartLag = false);

QuantLib::ext::shared_ptr<Trade> buildCrossCcyBasisSwap(
        string id, string recCcy, Real recNotional, string payCcy, Real payNotional, int start, Size term,
        Real recLegSpread, Real payLegSpread, string recFreq, string recDC, string recIndex, Calendar recCalendar,
        string payFreq, string payDC, string payIndex, Calendar payCalendar, QuantLib::Natural spotDays = 2,
        bool spotStartLag = false, bool notionalInitialExchange = false, bool notionalFinalExchange = false,
        bool notionalAmortizingExchange = false, bool isRecLegFXResettable = false, bool isPayLegFXResettable = false);
    
QuantLib::ext::shared_ptr<Trade> buildZeroBond(string id, string ccy, Real notional, Size term, string suffix = "1");

QuantLib::ext::shared_ptr<Trade> buildCreditDefaultSwap(string id, string ccy, string issuerId, string creditCurveId,
                                                bool isPayer, Real notional, int start, Size term, Real rate,
                                                Real spread, string fixedFreq, string fixedDC);

QuantLib::ext::shared_ptr<Trade> buildSyntheticCDO(string id, string name, vector<string> names, string longShort,
                                           string ccy, vector<string> ccys, bool isPayer,
                                           vector<Real> notionals, Real notional, int start, Size term,
                                           Real rate, Real spread, string fixedFreq, string fixedDC);

QuantLib::ext::shared_ptr<Trade> buildCmsCapFloor(string id, string ccy, string indexId, bool isPayer, Real notional,
                                          int start, Size term, Real capRate, Real floorRate, Real spread,
                                          string freq, string dc);

QuantLib::ext::shared_ptr<Trade> buildCPIInflationSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term,
                                               Real spread, string floatFreq, string floatDC, string index,
                                               string cpiFreq, string cpiDC, string cpiIndex, Real baseRate,
                                               string observationLag, bool interpolated, Real cpiRate);

QuantLib::ext::shared_ptr<Trade> buildYYInflationSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term,
                                              Real spread, string floatFreq, string floatDC, string index,
                                              string yyFreq, string yyDC, string yyIndex, string observationLag,
                                              Size fixDays);

QuantLib::ext::shared_ptr<Trade> buildYYInflationCapFloor(string id, string ccy, Real notional, bool isCap,
                                                  bool isLong, Real capFloorRate, int start, Size term,
                                                  string yyFreq, string yyDC, string yyIndex,
                                                  string observationLag, Size fixDays);

QuantLib::ext::shared_ptr<Trade> buildCommodityForward(const std::string& id, const std::string& position, Size term,
                                               const std::string& commodityName, const std::string& currency,
                                               Real strike, Real quantity);

QuantLib::ext::shared_ptr<Trade> buildCommodityOption(const std::string& id, const std::string& longShort,
                                              const std::string& putCall, QuantLib::Size term,
                                              const std::string& commodityName, const std::string& currency,
                                              QuantLib::Real strike, QuantLib::Real quantity,
                                              QuantLib::Real premium = 0.0, const std::string& premiumCcy = "",
                                              const std::string& premiumDate = "");

} // namespace testsuite
