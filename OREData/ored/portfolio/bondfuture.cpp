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

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondfuture.hpp>
#include <ored/portfolio/builders/forwardbond.hpp>
#include <ored/portfolio/legdata.hpp>

#include <qle/instruments/forwardbond.hpp>

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
                                         const ext::shared_ptr<BondFutureReferenceDatum>& bondFutureRefData) {
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

void BondFuture::checkData(Date expiry, Date settlement) {

    Date asof = Settings::instance().evaluationDate();
    if (settlement < expiry)
        QL_FAIL("BondFuture::checkDate -- id " << id() << " settlement date " << io::iso_date(settlement)
                                               << " lies before expiry " << io::iso_date(expiry));
    if (expiry < asof)
        QL_FAIL("BondFuture::checkDate -- id " << id() << " is expired, asof " << io::iso_date(asof) << " vs. expiry "
                                               << io::iso_date(expiry));
    if (asof + 9 * Months > expiry)
        ALOG("BondFuture::checkDate -- id " << id() << " expiry may be not in standard cycle of next three quarters "
                                            << io::iso_date(expiry) << " vs asof " << io::iso_date(asof));
}

std::pair<Date, Date> deduceDates(Currency ccy, std::string deliverableGrade,
                                                      Month contractMonth) {
    Date expiry;
    Date settlement;

    Date asof = Settings::instance().evaluationDate();
    int year = asof.year();
    if (asof.month() > contractMonth)
        year++;

    if (ccy == USDCurrency()) {
        // source : "Understanding Treasury Futures", CME Group, November 2017
        Calendar cal = UnitedStates(UnitedStates::GovernmentBond);
        Date lastBusiness = cal.endOfMonth(Date(1, contractMonth, year));

        if (selectTypeUS(deliverableGrade) == FutureType::ShortTenor) {
            // expiry: Last Trading Day: Last business day of the delivery month
            // settlement: Last Delivery Day: Third business day following the Last Trading Day
            expiry = lastBusiness;
            settlement = cal.advance(lastBusiness, +3 * Days);
        } else {
            // settlement: Last Delivery Day: Last business day of the delivery month
            // expiry: Last Trading Day: Seventh business day preceding the last business day of the delivery month
            expiry = cal.advance(lastBusiness, -7 * Days);
            settlement = lastBusiness;
        }
    } else if (ccy == CNYCurrency()) {
        // source: http://www.cffex.com.cn/en_new/10t.html
        // source: http://www.cffex.com.cn/en_new/2ts.html
        // source: http://www.cffex.com.cn/en_new/5tf.html
        Calendar cal = China(China::IB);
        Date secondFriday = Date::nthWeekday(2, Friday, contractMonth, year);

        // expiry: Last Trading Day:	Second Friday of the Contract’s expiry month
        // settlement: Last Delivery Day: Third trading day after the last trading day
        expiry = cal.adjust(secondFriday,
                            Following); // TODO: verify: adjust to business day, is following correct???
        settlement = cal.advance(expiry, +3 * Days);

    } else
        QL_FAIL("deduceDates: Currencies other than USD or CNY is not implemented. " << ccy.code() << " provided.");

    std::cout << "asof " << io::iso_date(asof) << " expiry " << io::iso_date(expiry) << " settlement " << io::iso_date(settlement) << std::endl;

    return std::make_pair(expiry, settlement);
}

void BondFuture::build(const ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("BondFuture::build() called for trade " << id());

    // ISDA taxonomy https://www.isda.org/a/20EDE/q4-2011-credit-standardisation-legend.pdf
    // TODO: clarify ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("Other");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    ext::shared_ptr<EngineBuilder> builder_fwd = engineFactory->builder("ForwardBond");
    QL_REQUIRE(builder_fwd, "BondFuture::build(): internal error, forward bond engine builder is null");
    ext::shared_ptr<EngineBuilder> builder_bd = engineFactory->builder("Bond");
    QL_REQUIRE(builder_bd, "BondFuture::build(): internal error, bond engine builder is null");

    // if data not given explicitly, exploit bond future reference data
    populateFromBondFutureReferenceData(engineFactory->referenceData());

    Date expiry = parseDate(lastTrading_);
    Date settlement = parseDate(lastDelivery_);

    // if dates are not given explicitly or by referenceData, deduce dates from contract months
    if (expiry == Date() || settlement == Date()) {
        pair<Date, Date> exp_set =
            deduceDates(parseCurrency(currency_), deliverableGrade_, parseMonth(contractMonths_));
        expiry = exp_set.first;
        settlement = exp_set.second;
    }

    // check data
    checkData(expiry, settlement);

    // identify CTD bond
    //
    // TODO use identifyCtdBond method
    //
    string securityId = secList_.front();

    BondBuilder::Result underlying =
        BondFactory::instance().build(engineFactory, engineFactory->referenceData(), securityId);

    // hardcoded values for bondfuture vs forward bond
    double amount = 0.0; // strike amount is zero for bondfutures
    bool longInForward = true; // TODO Check
    double compensationPayment = 0.0; // no compensation payments for bondfutures
    Date compensationPaymentDate = Date(); // no compensation payments for bondfutures
    bool isPhysicallySettled = false; // TODO Check
    bool settlementDirty = true; // TODO Check

    // create quantext forward bond instrument
    ext::shared_ptr<Payoff> payoff =
        longInForward ? ext::make_shared<ForwardBondTypePayoff>(Position::Long, amount)
                      : ext::make_shared<ForwardBondTypePayoff>(Position::Short, amount);

    ext::shared_ptr<Instrument> fwdBond = ext::make_shared<ForwardBond>(
        underlying.bond, payoff, expiry, settlement, isPhysicallySettled, settlementDirty, compensationPayment,
        compensationPaymentDate, contractNotional_);

    ext::shared_ptr<FwdBondEngineBuilder> fwdBondBuilder =
        ext::dynamic_pointer_cast<FwdBondEngineBuilder>(builder_fwd);
    QL_REQUIRE(fwdBondBuilder, "BondFuture::build(): could not cast FwdBondEngineBuilder: " << id());

    fwdBond->setPricingEngine(fwdBondBuilder->engine(
        id(), parseCurrency(currency_), envelope().additionalField("discount_curve", false, std::string()),
        underlying.creditCurveId, securityId, underlying.bondTrade->bondData().referenceCurveId(),
        underlying.bondTrade->bondData().incomeCurveId(), settlementDirty));

    setSensitivityTemplate(*fwdBondBuilder);
    addProductModelEngine(*fwdBondBuilder);
    instrument_.reset(new VanillaInstrument(fwdBond, 1.0));

    maturity_ = settlement;
    maturityType_ = "Contract settled";
    npvCurrency_ = currency_;
    notional_ = contractNotional_;
    legs_ = vector<QuantLib::Leg>(1, underlying.bond->cashflows()); //FIX ME: get future cfs 
    legCurrencies_ = vector<string>(1, currency_);
    legPayers_ = vector<bool>(1, false); //TODO check if this is valid
}

string BondFuture::identifyCtdBond(const ext::shared_ptr<EngineFactory>& engineFactory) {

    auto market = engineFactory->market();

    // get settlement price for future
    double sp = -10.0;
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

    return ctdSec;

} // BondFuture::ctdBond

void BondFuture::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* bondFutureNode = XMLUtils::getChildNode(node, "BondFutureData");
    QL_REQUIRE(bondFutureNode, "No BondFutureData Node");

    // mandatory first tier information
    contractName_ = XMLUtils::getChildValue(bondFutureNode, "ContractName", true);
    contractNotional_ = XMLUtils::getChildValueAsDouble(bondFutureNode, "ContractNotional", true);
    currency_ = XMLUtils::getChildValue(bondFutureNode, "Currency", true);

    secList_.clear();
    XMLNode* basket = XMLUtils::getChildNode(bondFutureNode, "DeliveryBasket");
    QL_REQUIRE(basket, "No DeliveryBasket Node");
    for (XMLNode* child = XMLUtils::getChildNode(basket, "SecurityId"); child; child = XMLUtils::getNextSibling(child))
        secList_.push_back(XMLUtils::getNodeValue(child));

    // second tier information
    contractMonths_ = XMLUtils::getChildValue(bondFutureNode, "ContractMonths", false, "");
    deliverableGrade_ = XMLUtils::getChildValue(bondFutureNode, "DeliverableGrade", false, "");

    // thirdt tier information
    lastTrading_ = XMLUtils::getChildValue(bondFutureNode, "LastTradingDate", false, "");
    lastDelivery_ = XMLUtils::getChildValue(bondFutureNode, "LastDeliveryDate", false, "");
}

XMLNode* BondFuture::toXML(XMLDocument& doc) const {
    // TODO implement
    XMLNode* bondFutureNode = doc.allocNode("BondFutureData");
    return bondFutureNode;
}

void BondFuture::populateFromBondFutureReferenceData(
    const ext::shared_ptr<ReferenceDataManager>& referenceData) {
    QL_REQUIRE(!contractName_.empty(), "BondFuture::populateFromBondReferenceData(): no contract name given");

    if (!referenceData || !referenceData->hasData(BondFutureReferenceDatum::TYPE, contractName_)) {
        DLOG("could not get BondFutureReferenceDatum for name " << contractName_ << " leave data in trade unchanged");

    } else {
        auto bondFutureRefData = ext::dynamic_pointer_cast<BondFutureReferenceDatum>(
            referenceData->getData(BondFutureReferenceDatum::TYPE, contractName_));
        QL_REQUIRE(bondFutureRefData, "could not cast to BondReferenceDatum, this is unexpected");
        DLOG("Got BondFutureReferenceDatum for name " << contractName_ << " overwrite empty elements in trade");
        ore::data::populateFromBondFutureReferenceData(contractMonths_, deliverableGrade_, lastTrading_, lastDelivery_,
                                                       secList_, bondFutureRefData);
    }
}

} // namespace data
} // namespace ore
