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

boost::shared_ptr<Trade> buildSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term, Real rate,
                                   Real spread, string fixedFreq, string fixedDC, string floatFreq, string floatDC,
                                   string index);

boost::shared_ptr<Trade> buildEuropeanSwaption(string id, string longShort, string ccy, bool isPayer, Real notional,
                                               int start, Size term, Real rate, Real spread, string fixedFreq,
                                               string fixedDC, string floatFreq, string floatDC, string index,
                                               string cashPhysical = "Cash", Real premium = 0.0, string premiumCcy = "",
                                               string premiumDate = "");

boost::shared_ptr<Trade> buildBermudanSwaption(string id, string longShort, string ccy, bool isPayer, Real notional,
                                               Size exercises, int start, Size term, Real rate, Real spread,
                                               string fixedFreq, string fixedDC, string floatFreq, string floatDC,
                                               string index, string cashPhysical = "Cash", Real premium = 0.0,
                                               string premiumCcy = "", string premiumDate = "");

boost::shared_ptr<Trade> buildFxOption(string id, string longShort, string putCall, Size expiry, string boughtCcy,
                                       Real boughtAmount, string soldCcy, Real soldAmount, Real premium = 0.0,
                                       string premiumCcy = "", string premiumDate = "");

boost::shared_ptr<Trade> buildEquityOption(string id, string longShort, string putCall, Size expiry, string equityName,
                                           string currency, Real strike, Real quantity, Real premium = 0.0,
                                           string premiumCcy = "", string premiumDate = "");

boost::shared_ptr<Trade> buildEquityForward(string id, string longShort, Size expiry, string equityName,
                                            string currency, Real strike, Real quantity);

boost::shared_ptr<Trade> buildCap(string id, string ccy, string longShort, Real capRate, Real notional, int start,
                                  Size term, string floatFreq, string floatDC, string index);

boost::shared_ptr<Trade> buildFloor(string id, string ccy, string longShort, Real floorRate, Real notional, int start,
                                    Size term, string floatFreq, string floatDC, string index);

boost::shared_ptr<Trade> buildCapFloor(string id, string ccy, string longShort, vector<Real> capRates,
                                       vector<Real> floorRates, Real notional, int start, Size term, string floatFreq,
                                       string floatDC, string index);

boost::shared_ptr<Trade> buildZeroBond(string id, string ccy, Real notional, Size term);

boost::shared_ptr<Trade> buildCPIInflationSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term,
                                               Real spread, string floatFreq, string floatDC, string index,
                                               string cpiFreq, string cpiDC, string cpiIndex, Real baseRate,
                                               string observationLag, bool interpolated, Real cpiRate);

boost::shared_ptr<Trade> buildYYInflationSwap(string id, string ccy, bool isPayer, Real notional, int start, Size term,
                                              Real spread, string floatFreq, string floatDC, string index,
                                              string yyFreq, string yyDC, string yyIndex, string observationLag,
                                              Size fixDays);

boost::shared_ptr<Trade> buildCommodityForward(const std::string& id, const std::string& position, Size term,
                                               const std::string& commodityName, const std::string& currency,
                                               Real strike, Real quantity);

boost::shared_ptr<Trade> buildCommodityOption(const std::string& id, const std::string& longShort,
                                              const std::string& putCall, QuantLib::Size term,
                                              const std::string& commodityName, const std::string& currency,
                                              QuantLib::Real strike, QuantLib::Real quantity,
                                              QuantLib::Real premium = 0.0, const std::string& premiumCcy = "",
                                              const std::string& premiumDate = "");

} // namespace testsuite
