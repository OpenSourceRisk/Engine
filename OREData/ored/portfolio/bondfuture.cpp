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

#include <ored/portfolio/bondfuture.hpp>
#include <ored/portfolio/forwardbond.hpp>
#include <ored/portfolio/legdata.hpp>

#include <ql/currencies/america.hpp>
#include <ql/currencies/asia.hpp>
#include <ql/time/calendars/china.hpp>
#include <ql/time/calendars/unitedstates.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void populateFromBondFutureReferenceData(std::string& contractMonths, std::string& deliverableGrade,
                                         std::string& lastTrading, std::string& lastDelivery,
                                         std::vector<std::string>& secList,
                                         const QuantLib::ext::shared_ptr<BondFutureReferenceDatum>& bondFutureRefData) {
    DLOG("populating data bondfuture from reference data");
    // checked before
    // QL_REQUIRE(bondFutureRefData, "populateFromBondFutureReferenceData(): empty bondfuture reference datum given");
    if (contractMonths.empty()) {
        contractMonths = bondFutureRefData->bondFutureData().contractMonths;
        TLOG("overwrite contractMonths with '" << contractMonths << "'");
    }
    if (deliverableGrade.empty()) {
        deliverableGrade = bondFutureRefData->bondFutureData().deliverableGrade;
        TLOG("overwrite deliverableGrade with '" << deliverableGrade << "'");
    }
    if (lastTrading.empty()) {
        lastTrading = bondFutureRefData->bondFutureData().lastTrading;
        TLOG("overwrite lastTrading with '" << lastTrading << "'");
    }
    if (lastDelivery.empty()) {
        lastDelivery = bondFutureRefData->bondFutureData().lastDelivery;
        TLOG("overwrite lastDelivery with '" << lastDelivery << "'");
    }
    if (secList.empty()) {
        secList = bondFutureRefData->bondFutureData().secList;
        TLOG("overwrite secList ");
    }
    DLOG("populating bondfuture data from reference data done.");
}

FutureType selectTypeUS(std::string value) {

    //                       Deliverable Maturities	    CME Globex  Bloomberg
    // 2-Year T-Note         1 3/4 to 2 years	        ZT          TU
    // 3-Year T-Note         9/12 to 3 years	        Z3N         3Y
    // 5-Year T-Note         4 1/6 to 5 1/4 years	    ZF          FV
    // 10-Year T-Note        6 1/2 to 8 years	        ZN          TY
    // Ultra 10-Year T-Note	 9 5/12 to 10 Years	        TN          UXY
    // T-Bond                15 years up to 25 years	ZB          US
    // 20-Year T-Bond        19 2/12 to 19 11/12 years	TWE         TWE
    // Ultra T-Bond	         25 years to 30 years	    UB          WN
    // source: https://www.cmegroup.com/trading/interest-rates/basics-of-us-treasury-futures.html

    boost::to_upper(value);

    if (value == "UB" || value == "WN")
        return FutureType::LongTenor;
    else if (value == "ZB" || value == "US")
        return FutureType::LongTenor;
    else if (value == "TWE")
        return FutureType::LongTenor;
    else if (value == "TN" || value == "UXY")
        return FutureType::LongTenor;
    else if (value == "ZN" || value == "TY")
        return FutureType::LongTenor;
    else if (value == "ZF" || value == "FV")
        return FutureType::ShortTenor;
    else if (value == "Z3N" || value == "3Y")
        return FutureType::ShortTenor;
    else if (value == "ZT" || value == "TU")
        return FutureType::ShortTenor;
    else
        QL_FAIL("FutureType " << value << " unkown");
}

std::pair<QuantLib::Date, QuantLib::Date> deduceDates(QuantLib::Currency ccy, std::string deliverableGrade,
                                                      QuantLib::Month contractMonth, int year) {
    QuantLib::Date expiry;
    QuantLib::Date settlement;

    if (ccy == QuantLib::USDCurrency()) {
        // source : "Understanding Treasury Futures", CME Group, November 2017
        QuantLib::Calendar cal = QuantLib::UnitedStates(QuantLib::UnitedStates::GovernmentBond);
        QuantLib::Date lastBusiness = cal.endOfMonth(QuantLib::Date(1, contractMonth, year));

        if (selectTypeUS(deliverableGrade) == FutureType::ShortTenor) {
            // expiry: Last Trading Day: Last business day of the delivery month
            // settlement: Last Delivery Day: Third business day following the Last Trading Day
            expiry = lastBusiness;
            settlement = cal.advance(lastBusiness, +3 * QuantLib::Days);
        } else {
            // settlement: Last Delivery Day: Last business day of the delivery month
            // expiry: Last Trading Day: Seventh business day preceding the last business day of the delivery month
            expiry = cal.advance(lastBusiness, -7 * QuantLib::Days);
            settlement = lastBusiness;
        }
    } else if (ccy == QuantLib::CNYCurrency()) {
        // source: http://www.cffex.com.cn/en_new/10t.html
        // source: http://www.cffex.com.cn/en_new/2ts.html
        // source: http://www.cffex.com.cn/en_new/5tf.html
        QuantLib::Calendar cal = QuantLib::China(QuantLib::China::IB);
        QuantLib::Date secondFriday = QuantLib::Date::nthWeekday(2, QuantLib::Friday, contractMonth, year);

        // expiry: Last Trading Day:	Second Friday of the Contract’s expiry month
        // settlement: Last Delivery Day: Third trading day after the last trading day
        expiry = cal.adjust(secondFriday,
                            QuantLib::Following); // TODO: verify: adjust to business day, is following correct???
        settlement = cal.advance(expiry, +3 * QuantLib::Days);

    } else
        QL_FAIL("deduceDates: Currencies other than USD or CNY is not implemented. " << ccy.code() << " provided.");

    return std::make_pair(expiry, settlement);
}

void BondFuture::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("BondFuture::build() called for trade " << id());

    // hierarchy: first explicit, second refdata, thirdt deduction, fourth error



    // identify CTD bond

    // build using forward bond
}

BondData BondFuture::identifyCtdBond(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    auto market = engineFactory->market();

    // get settlement price for future
    double sp;
    try {
        // TODO implement futureprice method
    } catch (const std::exception& e) {
        // MESSAGE: failed to retrieve future price quote
    }

    // loop secList retrieve bondprice (bp) and conversion factor (cf)
    // find lowest value for bp_i - sp x cf_i

    double lowestValue = 1e6; // arbitray high number
    string ctdSec = string();

    for (const auto& sec : secList_) {

        try {
            // TODO implement bondPrice method
            double bp = 1.0; // market->bondPrice(sec)->value();
            double cf = market->conversionFactor(sec)->value();
            // if conversion factor not given, deduce from method...
            double value = bp - sp * cf;

            // store result
            if (value < lowestValue) {
                lowestValue = value;
                ctdSec = sec;
            }

        } catch (const std::exception& e) {
            // MESSAGE: failed to retrieve bond price quote or conversion factor for SECID
        }
    }
    // Check if results
    QL_REQUIRE(!ctdSec.empty(), "No CTD bond found.");

    // retrieve ctd bonddata ...
    BondData ctdBondData(ctdSec, notional_);
    ctdBondData.populateFromBondReferenceData(engineFactory->referenceData());

    return ctdBondData;

} // BondFuture::ctdBond

void BondFuture::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* bondFutureNode = XMLUtils::getChildNode(node, "BondFutureData");
    QL_REQUIRE(bondFutureNode, "No BondFutureData Node");

    //mandatory first tier information
    contractName_ = XMLUtils::getChildValue(bondFutureNode, "ContractName", true);
    notional_ = XMLUtils::getChildValueAsDouble(bondFutureNode, "Notional", true);
    notionalCurrency_ = XMLUtils::getChildValueAsDouble(bondFutureNode, "Currency", true);

    secList_.clear();
    XMLUtils::checkNode(bondFutureNode, "DeliveryBasket");
    for (XMLNode* child = XMLUtils::getChildNode(bondFutureNode, "SecurityId"); child;
         child = XMLUtils::getNextSibling(child)) {
        secList_.push_back(XMLUtils::getChildValue(child, "SecurityId", true));
    }

    //second tier information
    contractMonths_ = XMLUtils::getChildValue(bondFutureNode, "ContractMonths", false, "");
    deliverableGrade_ = XMLUtils::getChildValue(bondFutureNode, "DeliverableGrade", false, "");

    //thirdt tier information
    lastTrading_ = XMLUtils::getChildValue(bondFutureNode, "LastTradingDate", false, "");
    lastDelivery_ = XMLUtils::getChildValue(bondFutureNode, "LastDeliveryDate", false, "");
}

void BondFuture::populateFromBondFutureReferenceData(
    const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData) {
    QL_REQUIRE(!contractName_.empty(), "BondFuture::populateFromBondReferenceData(): no contract name given");

    if (!referenceData || !referenceData->hasData(BondFutureReferenceDatum::TYPE, contractName_)) {
        DLOG("could not get BondFutureReferenceDatum for name " << contractName_ << " leave data in trade unchanged");

    } else {
        auto bondFutureRefData = QuantLib::ext::dynamic_pointer_cast<BondFutureReferenceDatum>(
            referenceData->getData(BondFutureReferenceDatum::TYPE, contractName_));
        QL_REQUIRE(bondFutureRefData, "could not cast to BondReferenceDatum, this is unexpected");
        DLOG("Got BondFutureReferenceDatum for name " << contractName_ << " overwrite empty elements in trade");
        ore::data::populateFromBondFutureReferenceData(contractMonths_, deliverableGrade_, lastTrading_, lastDelivery_,
                                                       secList_, bondFutureRefData);
    }
}

} // namespace data
} // namespace ore
