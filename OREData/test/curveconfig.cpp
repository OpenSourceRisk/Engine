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

#include <test/curveconfig.hpp>

#include <ored/configuration/basecorrelationcurveconfig.hpp>
#include <ored/configuration/capfloorvolcurveconfig.hpp>
#include <ored/configuration/cdsvolcurveconfig.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/defaultcurveconfig.hpp>
#include <ored/configuration/equitycurveconfig.hpp>
#include <ored/configuration/equityvolcurveconfig.hpp>
#include <ored/configuration/fxvolcurveconfig.hpp>
#include <ored/configuration/inflationcapfloorpricesurfaceconfig.hpp>
#include <ored/configuration/inflationcurveconfig.hpp>
#include <ored/configuration/swaptionvolcurveconfig.hpp>
#include <ored/configuration/yieldcurveconfig.hpp>
#include <ored/configuration/securityconfig.hpp>
#include <ored/configuration/fxspotconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>

#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore;
using namespace ore::data;

namespace testsuite {

void CurveConfigTest::testCurveConfigQuotes() {

    CurveConfigurations curveConfigs;

    vector<Period> tenors = {parsePeriod("1Y"), parsePeriod("2Y"), parsePeriod("3Y")};
    vector<Real> strikes = {-0.01,0,0.01};
    //add base correlations
    vector<Real> dp = {0.03, 0.06, 0.1, 0.2, 1};
    vector<Period> terms = {parsePeriod("1D")};
    boost::shared_ptr<BaseCorrelationCurveConfig> baseCorrelationConfig =
        boost::make_shared<BaseCorrelationCurveConfig>("CDXIG", "", dp, terms);
    curveConfigs.baseCorrelationCurveConfig("CDXIG") = baseCorrelationConfig;

    //add capfloors
    boost::shared_ptr<CapFloorVolatilityCurveConfig> capFloorVolatilityConfig =
        boost::make_shared<CapFloorVolatilityCurveConfig>("EUR_CF_N", "", CapFloorVolatilityCurveConfig::VolatilityType::Normal, 
            true, false, tenors, strikes, parseDayCounter("Actual/365 (Fixed)"), 0, parseCalendar("TARGET"), 
            parseBusinessDayConvention("MF"), "EUR-EURIBOR-6M", "Yield/GBP/GBP1D");

    curveConfigs.capFloorVolCurveConfig("EUR_CF_N") = capFloorVolatilityConfig;

    //add cds vols
    vector<string> expiries = {"1M","3M","6M"};
    boost::shared_ptr<CDSVolatilityCurveConfig> cdsVolatilityConfig =
        boost::make_shared<CDSVolatilityCurveConfig>("CDXIG", "", expiries);

    curveConfigs.cdsVolCurveConfig("CDXIG") = cdsVolatilityConfig;

    //add default curves
    vector<string> cdsQuotes = {
        "CDS/CREDIT_SPREAD/BANK/SR/USD/1Y",
        "CDS/CREDIT_SPREAD/BANK/SR/USD/2Y",
        "CDS/CREDIT_SPREAD/BANK/SR/USD/3Y"
    };
    boost::shared_ptr<DefaultCurveConfig> defaultConfig =
        boost::make_shared<DefaultCurveConfig>("BANK_SR_USD", "", "USD", DefaultCurveConfig::Type::SpreadCDS, 
            "Yield/USD/USD3M", "RECOVERY_RATE/RATE/BANK/SR/USD", parseDayCounter("A365"), "CDS-STANDARD-CONVENTIONS", cdsQuotes, true);

    curveConfigs.defaultCurveConfig("BANK_SR_USD") = defaultConfig;

    //add equity curves
    vector<string> fwdQuotes = {
        "EQUITY_DIVIDEND/RATE/SP5/USD/3M",
        "EQUITY_DIVIDEND/RATE/SP5/USD/20160915",
        "EQUITY_DIVIDEND/RATE/SP5/USD/1Y"
    };
    boost::shared_ptr<EquityCurveConfig> equityCurveConfig =
        boost::make_shared<EquityCurveConfig>("SP5", "", "USD1D", "USD", EquityCurveConfig::Type::DividendYield, 
            "EQUITY/PRICE/SP5/USD", fwdQuotes, "A365", "Zero", "Linear", true);

    curveConfigs.equityCurveConfig("SP5") = equityCurveConfig;
   
    //add equity vol curves
    boost::shared_ptr<EquityVolatilityCurveConfig> equityVolCurveConfig =
        boost::make_shared<EquityVolatilityCurveConfig>("SP5", "", "USD", EquityVolatilityCurveConfig::Dimension::ATM,
        expiries, vector<string>());

    curveConfigs.equityVolCurveConfig("SP5") = equityVolCurveConfig;

    vector<string> sstrikes = {"-0.01","0.01"};
    
    boost::shared_ptr<EquityVolatilityCurveConfig> equityVolCurveConfigSmile =
        boost::make_shared<EquityVolatilityCurveConfig>("SP5_Smile", "", "USD", EquityVolatilityCurveConfig::Dimension::Smile,
        expiries, sstrikes);

    curveConfigs.equityVolCurveConfig("SP5_Smile") = equityVolCurveConfigSmile;

    //add fx vol curves
    boost::shared_ptr<FXVolatilityCurveConfig> fxVolCurveConfig =
        boost::make_shared<FXVolatilityCurveConfig>("EURUSD", "", FXVolatilityCurveConfig::Dimension::ATM,
        tenors, "FX/EUR/USD", "Yield/USD/USD1D", "Yield/EUR/EUR1D");

    curveConfigs.fxVolCurveConfig("EURUSD") = fxVolCurveConfig;

    boost::shared_ptr<FXVolatilityCurveConfig> fxVolCurveConfigSmile =
        boost::make_shared<FXVolatilityCurveConfig>("EURUSD_Smile", "", FXVolatilityCurveConfig::Dimension::Smile,
        tenors, "FX/EUR/USD", "Yield/USD/USD1D", "Yield/EUR/EUR1D");

    curveConfigs.fxVolCurveConfig("EURUSD_Smile") = fxVolCurveConfigSmile;

    //add Inflation CapFloor Price Surface
    Period ob = parsePeriod("1D");
    boost::shared_ptr<InflationCapFloorPriceSurfaceConfig> inflationCapFloorConfig =
        boost::make_shared<InflationCapFloorPriceSurfaceConfig>("EUHICPXT_ZC_CF", "", InflationCapFloorPriceSurfaceConfig::Type::ZC,
        ob, parseCalendar("TARGET"), parseBusinessDayConvention("MF"), parseDayCounter("A365"), "EUHICPXT", 
        "Inflation/EUHICPXT/EUHICPXT_ZC_Swaps", "Yield/EUR/EUR1D", strikes, strikes, tenors);

    curveConfigs.inflationCapFloorPriceSurfaceConfig("EUHICPXT_ZC_CF") = inflationCapFloorConfig;

    //add Inflation 
        vector<string> swapQuotes = {
        "ZC_INFLATIONSWAP/RATE/EUHICPXT/1Y",
        "ZC_INFLATIONSWAP/RATE/EUHICPXT/2Y",
        "ZC_INFLATIONSWAP/RATE/EUHICPXT/3Y"
    };

    vector<string> seasonQuotes = {
        "SEASONALITY/RATE/MULT/EUHICPXT/JAN",
        "SEASONALITY/RATE/MULT/EUHICPXT/FEB",
        "SEASONALITY/RATE/MULT/EUHICPXT/MAR"
    };

    boost::shared_ptr<InflationCurveConfig> inflationCurveConfig =
        boost::make_shared<InflationCurveConfig>("EUHICPXT_ZC_Swaps", "", "Yield/EUR/EUR1D", InflationCurveConfig::Type::ZC,
        swapQuotes, "EUHICPXT_INFLATIONSWAP", true, parseCalendar("TARGET"), parseDayCounter("A365"), ob, QuantLib::NoFrequency, 
        0.01, 0.0001, QuantLib::Null<Date>(), QuantLib::NoFrequency, seasonQuotes);

    curveConfigs.inflationCurveConfig("EUHICPXT_ZC_Swaps") = inflationCurveConfig;

    //add swaption vol curves
    boost::shared_ptr<SwaptionVolatilityCurveConfig> swapVolCurveConfig =
        boost::make_shared<SwaptionVolatilityCurveConfig>("EUR_SW_N", "", SwaptionVolatilityCurveConfig::Dimension::ATM, 
        SwaptionVolatilityCurveConfig::VolatilityType::Normal, true, true, tenors, tenors, parseDayCounter("A365"), 
        parseCalendar("TARGET"), parseBusinessDayConvention("MF"), "EUR-CMS-1Y", "EUR-CMS-30Y", vector<Period>(), vector<Period>(),
        vector<Real>());

    curveConfigs.swaptionVolCurveConfig("EUR_SW_N") = swapVolCurveConfig;

    boost::shared_ptr<SwaptionVolatilityCurveConfig> swapVolCurveConfigSmile =
        boost::make_shared<SwaptionVolatilityCurveConfig>("EUR_SW_N_Smile", "", SwaptionVolatilityCurveConfig::Dimension::Smile, 
        SwaptionVolatilityCurveConfig::VolatilityType::Normal, true, true, vector<Period>(), vector<Period>(), parseDayCounter("A365"), 
        parseCalendar("TARGET"), parseBusinessDayConvention("MF"), "EUR-CMS-1Y", "EUR-CMS-30Y", tenors, tenors, 
        strikes);

    curveConfigs.swaptionVolCurveConfig("EUR_SW_N_Smile") = swapVolCurveConfigSmile;

    //add yield curves
    vector<boost::shared_ptr<YieldCurveSegment>> segments{boost::make_shared<SimpleYieldCurveSegment>(
        "Swap", "JPY-SWAP-CONVENTIONS", vector<string>(1, "IR_SWAP/RATE/JPY/2D/6M/2Y"))};
    boost::shared_ptr<YieldCurveConfig> yieldCurveConfig =
        boost::make_shared<YieldCurveConfig>("JPY6M", "JPY 6M curve", "JPY", "", segments);

    curveConfigs.yieldCurveConfig("JPY6M") = yieldCurveConfig;

    //add securities
    boost::shared_ptr<SecurityConfig> securityConfig =
        boost::make_shared<SecurityConfig>("SECURITY_1", "", "BOND/YIELD_SPREAD/SECURITY_1", "RECOVERY_RATE/RATE/SECURITY_1");

    curveConfigs.securityConfig("SECURITY_1") = securityConfig;

    //add fxSpots
    boost::shared_ptr<FXSpotConfig> fxSpotConfig =
        boost::make_shared<FXSpotConfig>("EURUSD", "");

    curveConfigs.fxSpotConfig("EURUSD") = fxSpotConfig;

    vector<string> expectedQuotes = {
        "CDS_INDEX/BASE_CORRELATION/CDXIG/1D/0.03",
        "CDS_INDEX/BASE_CORRELATION/CDXIG/1D/0.06",
        "CDS_INDEX/BASE_CORRELATION/CDXIG/1D/0.1",
        "CDS_INDEX/BASE_CORRELATION/CDXIG/1D/0.2",
        "CDS_INDEX/BASE_CORRELATION/CDXIG/1D/1",
        "CAPFLOOR/RATE_NVOL/EUR/1Y/6M/0/0/-0.01",
        "CAPFLOOR/RATE_NVOL/EUR/2Y/6M/0/0/-0.01",
        "CAPFLOOR/RATE_NVOL/EUR/3Y/6M/0/0/-0.01",
        "CAPFLOOR/RATE_NVOL/EUR/1Y/6M/0/0/0",
        "CAPFLOOR/RATE_NVOL/EUR/2Y/6M/0/0/0",
        "CAPFLOOR/RATE_NVOL/EUR/3Y/6M/0/0/0",
        "CAPFLOOR/RATE_NVOL/EUR/1Y/6M/0/0/0.01",
        "CAPFLOOR/RATE_NVOL/EUR/2Y/6M/0/0/0.01",
        "CAPFLOOR/RATE_NVOL/EUR/3Y/6M/0/0/0.01",
        "INDEX_CDS_OPTION/RATE_LNVOL/CDXIG/1M",
        "INDEX_CDS_OPTION/RATE_LNVOL/CDXIG/3M",
        "INDEX_CDS_OPTION/RATE_LNVOL/CDXIG/6M",
        "RECOVERY_RATE/RATE/BANK/SR/USD",
        "CDS/CREDIT_SPREAD/BANK/SR/USD/1Y",
        "CDS/CREDIT_SPREAD/BANK/SR/USD/2Y",
        "CDS/CREDIT_SPREAD/BANK/SR/USD/3Y",
        "EQUITY/PRICE/SP5/USD",
        "EQUITY_DIVIDEND/RATE/SP5/USD/3M",
        "EQUITY_DIVIDEND/RATE/SP5/USD/20160915",
        "EQUITY_DIVIDEND/RATE/SP5/USD/1Y",
        "EQUITY_OPTION/RATE_LNVOL/SP5/USD/1M/ATMF",
        "EQUITY_OPTION/RATE_LNVOL/SP5/USD/3M/ATMF",
        "EQUITY_OPTION/RATE_LNVOL/SP5/USD/6M/ATMF",
        "EQUITY_OPTION/RATE_LNVOL/SP5_Smile/USD/1M/-0.01",
        "EQUITY_OPTION/RATE_LNVOL/SP5_Smile/USD/3M/-0.01",
        "EQUITY_OPTION/RATE_LNVOL/SP5_Smile/USD/6M/-0.01",
        "EQUITY_OPTION/RATE_LNVOL/SP5_Smile/USD/1M/0.01",
        "EQUITY_OPTION/RATE_LNVOL/SP5_Smile/USD/3M/0.01",
        "EQUITY_OPTION/RATE_LNVOL/SP5_Smile/USD/6M/0.01",
        "FX_OPTION/RATE_LNVOL/EUR/USD/1Y/ATM",
        "FX_OPTION/RATE_LNVOL/EUR/USD/2Y/ATM",
        "FX_OPTION/RATE_LNVOL/EUR/USD/3Y/ATM",
        "FX_OPTION/RATE_LNVOL/EUR/USD/1Y/25RR",
        "FX_OPTION/RATE_LNVOL/EUR/USD/2Y/25RR",
        "FX_OPTION/RATE_LNVOL/EUR/USD/3Y/25RR",
        "FX_OPTION/RATE_LNVOL/EUR/USD/1Y/25BF",
        "FX_OPTION/RATE_LNVOL/EUR/USD/2Y/25BF",
        "FX_OPTION/RATE_LNVOL/EUR/USD/3Y/25BF",
        "ZC_INFLATIONSWAP/RATE/EUHICPXT/1Y",
        "ZC_INFLATIONSWAP/RATE/EUHICPXT/2Y",
        "ZC_INFLATIONSWAP/RATE/EUHICPXT/3Y",
        "SEASONALITY/RATE/MULT/EUHICPXT/JAN",
        "SEASONALITY/RATE/MULT/EUHICPXT/FEB",
        "SEASONALITY/RATE/MULT/EUHICPXT/MAR",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/1Y/F/-0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/2Y/F/-0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/3Y/F/-0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/1Y/F/0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/2Y/F/0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/3Y/F/0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/1Y/F/0",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/2Y/F/0",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/3Y/F/0",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/1Y/C/-0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/2Y/C/-0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/3Y/C/-0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/1Y/C/0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/2Y/C/0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/3Y/C/0.01",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/1Y/C/0",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/2Y/C/0",
        "ZC_INFLATIONCAPFLOOR/PRICE/EUHICPXT/3Y/C/0",
        "SWAPTION/RATE_NVOL/EUR/1Y/1Y/ATM",
        "SWAPTION/RATE_NVOL/EUR/1Y/2Y/ATM",
        "SWAPTION/RATE_NVOL/EUR/1Y/3Y/ATM",
        "SWAPTION/RATE_NVOL/EUR/2Y/1Y/ATM",
        "SWAPTION/RATE_NVOL/EUR/2Y/2Y/ATM",
        "SWAPTION/RATE_NVOL/EUR/2Y/3Y/ATM",
        "SWAPTION/RATE_NVOL/EUR/3Y/1Y/ATM",
        "SWAPTION/RATE_NVOL/EUR/3Y/2Y/ATM",
        "SWAPTION/RATE_NVOL/EUR/3Y/3Y/ATM",
        "SWAPTION/RATE_NVOL/EUR/1Y/1Y/Smile/-0.01",
        "SWAPTION/RATE_NVOL/EUR/1Y/2Y/Smile/-0.01",
        "SWAPTION/RATE_NVOL/EUR/1Y/3Y/Smile/-0.01",
        "SWAPTION/RATE_NVOL/EUR/2Y/1Y/Smile/-0.01",
        "SWAPTION/RATE_NVOL/EUR/2Y/2Y/Smile/-0.01",
        "SWAPTION/RATE_NVOL/EUR/2Y/3Y/Smile/-0.01",
        "SWAPTION/RATE_NVOL/EUR/3Y/1Y/Smile/-0.01",
        "SWAPTION/RATE_NVOL/EUR/3Y/2Y/Smile/-0.01",
        "SWAPTION/RATE_NVOL/EUR/3Y/3Y/Smile/-0.01",
        "SWAPTION/RATE_NVOL/EUR/1Y/1Y/Smile/0.01",
        "SWAPTION/RATE_NVOL/EUR/1Y/2Y/Smile/0.01",
        "SWAPTION/RATE_NVOL/EUR/1Y/3Y/Smile/0.01",
        "SWAPTION/RATE_NVOL/EUR/2Y/1Y/Smile/0.01",
        "SWAPTION/RATE_NVOL/EUR/2Y/2Y/Smile/0.01",
        "SWAPTION/RATE_NVOL/EUR/2Y/3Y/Smile/0.01",
        "SWAPTION/RATE_NVOL/EUR/3Y/1Y/Smile/0.01",
        "SWAPTION/RATE_NVOL/EUR/3Y/2Y/Smile/0.01",
        "SWAPTION/RATE_NVOL/EUR/3Y/3Y/Smile/0.01",
        "SWAPTION/RATE_NVOL/EUR/1Y/1Y/Smile/0",
        "SWAPTION/RATE_NVOL/EUR/1Y/2Y/Smile/0",
        "SWAPTION/RATE_NVOL/EUR/1Y/3Y/Smile/0",
        "SWAPTION/RATE_NVOL/EUR/2Y/1Y/Smile/0",
        "SWAPTION/RATE_NVOL/EUR/2Y/2Y/Smile/0",
        "SWAPTION/RATE_NVOL/EUR/2Y/3Y/Smile/0",
        "SWAPTION/RATE_NVOL/EUR/3Y/1Y/Smile/0",
        "SWAPTION/RATE_NVOL/EUR/3Y/2Y/Smile/0",
        "SWAPTION/RATE_NVOL/EUR/3Y/3Y/Smile/0",
        "IR_SWAP/RATE/JPY/2D/6M/2Y",
        "BOND/YIELD_SPREAD/SECURITY_1",
        "RECOVERY_RATE/RATE/SECURITY_1",
        "FX/RATE/EUR/USD"
    };

    std::set<string> quotes = curveConfigs.quotes();
    QL_REQUIRE(quotes.size() == expectedQuotes.size(), "size of generate quotes, " << quotes.size() << 
        ", does not match the expected size "<< expectedQuotes.size());
        
    for(auto q : quotes)
        QL_REQUIRE(std::find(expectedQuotes.begin(), expectedQuotes.end(), q) != expectedQuotes.end(), "quote " << q << " not found");
}

test_suite* CurveConfigTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("CurveConfigTest");
    suite->add(BOOST_TEST_CASE(&CurveConfigTest::testCurveConfigQuotes));
    return suite;
}
} // namespace testsuite
