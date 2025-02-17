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

void populateFromBondFutureReferenceData(string& currency, string& contractMonth, string& deliverableGrade,
                                         string& rootDate, string& expiryBasis, string& settlementBasis,
                                         string& expiryLag, string& settlementLag, string& lastTrading,
                                         string& lastDelivery, vector<string>& secList,
                                         const ext::shared_ptr<BondFutureReferenceDatum>& bondFutureRefData) {
    DLOG("populating data bondfuture from reference data");
    // checked before
    // QL_REQUIRE(bondFutureRefData, "populateFromBondFutureReferenceData(): empty bondfuture reference datum given");
    if (currency.empty()) {
        currency = bondFutureRefData->bondFutureData().currency;
        TLOG("overwrite currency with '" << currency << "'");
    }
    if (contractMonth.empty()) {
        contractMonth = bondFutureRefData->bondFutureData().contractMonth;
        TLOG("overwrite contractMonth with '" << contractMonth << "'");
    }
    if (deliverableGrade.empty()) {
        deliverableGrade = bondFutureRefData->bondFutureData().deliverableGrade;
        TLOG("overwrite deliverableGrade with '" << deliverableGrade << "'");
    }
    if (rootDate.empty()) {
        rootDate = bondFutureRefData->bondFutureData().rootDate;
        TLOG("overwrite rootDate with '" << rootDate << "'");
    }
    if (expiryBasis.empty()) {
        expiryBasis = bondFutureRefData->bondFutureData().expiryBasis;
        TLOG("overwrite expiryBasis with '" << expiryBasis << "'");
    }
    if (settlementBasis.empty()) {
        settlementBasis = bondFutureRefData->bondFutureData().settlementBasis;
        TLOG("overwrite settlementBasis with '" << settlementBasis << "'");
    }
    if (expiryLag.empty()) {
        expiryLag = bondFutureRefData->bondFutureData().expiryLag;
        TLOG("overwrite expiryLag with '" << expiryLag << "'");
    }
    if (settlementLag.empty()) {
        settlementLag = bondFutureRefData->bondFutureData().settlementLag;
        TLOG("overwrite settlementLag with '" << settlementLag << "'");
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

void BondFuture::build(const ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("BondFuture::build() called for trade " << id());

    // ISDA taxonomy https://www.isda.org/a/20EDE/q4-2011-credit-standardisation-legend.pdf
    // TODO: clarify ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("Other");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("ForwardBond");
    QL_REQUIRE(builder, "BondFuture::build(): internal error, forward bond engine builder is null");

    // if data not given explicitly, exploit bond future reference data
    populateFromBondFutureReferenceData(engineFactory->referenceData());
    checkData();

    // if dates are not given explicitly or by referenceData, deduce dates from contractMonth/deliverableGrade
    Date expiry = parseDate(lastTrading_);
    Date settlement = parseDate(lastDelivery_);
    if (expiry == Date() || settlement == Date())
        deduceDates(expiry, settlement);
    checkDates(expiry, settlement);

    // identify CTD bond
    //
    // TODO use identifyCtdBond method
    //
    string securityId = secList_.front();

    BondBuilder::Result underlying =
        BondFactory::instance().build(engineFactory, engineFactory->referenceData(), securityId);

    // hardcoded values for bondfuture vs forward bond
    double amount = 0.0;                   // strike amount is zero for bondfutures
    double compensationPayment = 0.0;      // no compensation payments for bondfutures
    Date compensationPaymentDate = Date(); // no compensation payments for bondfutures
    bool isPhysicallySettled = true;       // TODO Check
    bool settlementDirty = true;           // TODO Check

    // create quantext forward bond instrument
    ext::shared_ptr<Payoff> payoff = parsePositionType(longShort_) == Position::Long
                                         ? ext::make_shared<ForwardBondTypePayoff>(Position::Long, amount)
                                         : ext::make_shared<ForwardBondTypePayoff>(Position::Short, amount);

    ext::shared_ptr<Instrument> fwdBond =
        ext::make_shared<ForwardBond>(underlying.bond, payoff, expiry, settlement, isPhysicallySettled, settlementDirty,
                                      compensationPayment, compensationPaymentDate, contractNotional_);

    ext::shared_ptr<FwdBondEngineBuilder> fwdBondBuilder = ext::dynamic_pointer_cast<FwdBondEngineBuilder>(builder);
    QL_REQUIRE(fwdBondBuilder, "BondFuture::build(): could not cast FwdBondEngineBuilder: " << id());

    fwdBond->setPricingEngine(fwdBondBuilder->engine(
        id(), parseCurrency(currency_), envelope().additionalField("discount_curve", false, string()),
        underlying.creditCurveId, securityId, underlying.bondTrade->bondData().referenceCurveId(),
        underlying.bondTrade->bondData().incomeCurveId(), settlementDirty));

    setSensitivityTemplate(*fwdBondBuilder);
    addProductModelEngine(*fwdBondBuilder);
    instrument_.reset(new VanillaInstrument(fwdBond, 1.0));

    maturity_ = settlement;
    maturityType_ = "Contract settled";
    npvCurrency_ = currency_;
    notional_ = contractNotional_;
    legs_ = vector<Leg>(1, underlying.bond->cashflows()); // presenting the cashflows of the underlying
    legCurrencies_ = vector<string>(1, currency_);
    legPayers_ = vector<bool>(1, false);
}

void BondFuture::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* bondFutureNode = XMLUtils::getChildNode(node, "BondFutureData");
    QL_REQUIRE(bondFutureNode, "No BondFutureData Node");

    contractName_ = XMLUtils::getChildValue(bondFutureNode, "ContractName", true);
    contractNotional_ = XMLUtils::getChildValueAsDouble(bondFutureNode, "ContractNotional", true);
    longShort_ = XMLUtils::getChildValue(bondFutureNode, "LongShort", true);

    secList_.clear();
    XMLNode* basket = XMLUtils::getChildNode(bondFutureNode, "DeliveryBasket");
    if (basket) {
        for (XMLNode* child = XMLUtils::getChildNode(basket, "SecurityId"); child;
             child = XMLUtils::getNextSibling(child))
            secList_.push_back(XMLUtils::getNodeValue(child));
    }
    currency_ = XMLUtils::getChildValue(bondFutureNode, "Currency", false);
    contractMonth_ = XMLUtils::getChildValue(bondFutureNode, "ContractMonth", false);
    deliverableGrade_ = XMLUtils::getChildValue(bondFutureNode, "DeliverableGrade", false);
    rootDate_ = XMLUtils::getChildValue(bondFutureNode, "RootDate", false);
    expiryBasis_ = XMLUtils::getChildValue(bondFutureNode, "ExpiryBasis", false);
    settlementBasis_ = XMLUtils::getChildValue(bondFutureNode, "SettlementBasis", false);
    expiryLag_ = XMLUtils::getChildValue(bondFutureNode, "ExpiryLag", false);
    settlementLag_ = XMLUtils::getChildValue(bondFutureNode, "SettlementLag", false);

    lastTrading_ = XMLUtils::getChildValue(bondFutureNode, "LastTradingDate", false);
    lastDelivery_ = XMLUtils::getChildValue(bondFutureNode, "LastDeliveryDate", false);
}

XMLNode* BondFuture::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("BondFutureData");

    XMLUtils::addChild(doc, node, "ContractName", contractName_);
    XMLUtils::addChild(doc, node, "ContractNotional", contractNotional_);
    XMLUtils::addChild(doc, node, "LongShort", longShort_);

    XMLUtils::addChild(doc, node, "Currency", currency_);
    XMLUtils::addChild(doc, node, "ContractMonth", contractMonth_);
    XMLUtils::addChild(doc, node, "DeliverableGrade", deliverableGrade_);
    XMLUtils::addChild(doc, node, "RootDate", rootDate_);
    XMLUtils::addChild(doc, node, "ExpiryBasis", expiryBasis_);
    XMLUtils::addChild(doc, node, "SettlementBasis", settlementBasis_);
    XMLUtils::addChild(doc, node, "ExpiryLag", expiryLag_);
    XMLUtils::addChild(doc, node, "SettlementLag", settlementLag_);

    XMLUtils::addChild(doc, node, "LastTradingDate", lastTrading_);
    XMLUtils::addChild(doc, node, "LastDeliveryDate", lastDelivery_);

    XMLNode* dbNode = XMLUtils::addChild(doc, node, "DeliveryBasket");
    for (auto& sec : secList_)
        XMLUtils::addChild(doc, dbNode, "SecurityId", sec);

    return node;
}

void BondFuture::checkData() {

    QL_REQUIRE(!contractName_.empty(), "BondFuture invalid: no contractName given");
    // contractNotional_, longShort_ already checked in fromXML

    vector<string> missingElements;
    if (currency_.empty())
        missingElements.push_back("Currency");

    // we can deduce lastTrading_/lastDelivery_ given DeliverableGrade/ContractMonth
    // expiryLag_, settlementLag_ covered in method where it is used
    if (lastTrading_.empty() && lastDelivery_.empty()) {
        if (deliverableGrade_.empty())
            missingElements.push_back("DeliverableGrade");
        if (contractMonth_.empty())
            missingElements.push_back("ContractMonth");
        if (rootDate_.empty())
            missingElements.push_back("RootDate");
        if (expiryBasis_.empty())
            missingElements.push_back("ExpiryBasis");
        if (settlementBasis_.empty())
            missingElements.push_back("SettlementBasis");
    }

    if (secList_.size() < 1)
        missingElements.push_back("DeliveryBasket");

    QL_REQUIRE(missingElements.empty(), "BondFuture invalid: missing " + boost::algorithm::join(missingElements, ", ") +
                                                " - check if reference data is set up for '"
                                            << contractName_ << "'");
}

void BondFuture::deduceDates(Date& expiry, Date& settlement) {
    DLOG("BondFuture::deduceDates called.");

    /*

    //This is the hardcode rule version
    Currency ccy_ql = parseCurrency(currency_);
    Month contractMonth_ql = parseMonth(contractMonth_);

    Date asof = Settings::instance().evaluationDate();
    int year = asof.year();
    if (asof.month() > contractMonth_ql)
        year++;

    if (ccy_ql == USDCurrency()) {
        // source : "Understanding Treasury Futures", CME Group, November 2017
        Calendar cal = UnitedStates(UnitedStates::GovernmentBond);
        Date lastBusiness = cal.endOfMonth(Date(1, contractMonth_ql, year));

        if (selectTypeUS(deliverableGrade_) == FutureType::ShortTenor) {
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
    } else if (ccy_ql == CNYCurrency()) {
        // source: http://www.cffex.com.cn/en_new/10t.html
        // source: http://www.cffex.com.cn/en_new/2ts.html
        // source: http://www.cffex.com.cn/en_new/5tf.html
        Calendar cal = China(China::IB);
        Date secondFriday = Date::nthWeekday(2, Friday, contractMonth_ql, year);

        // expiry: Last Trading Day:	Second Friday of the Contract’s expiry month
        // settlement: Last Delivery Day: Third trading day after the last trading day
        expiry = cal.adjust(secondFriday,
                            Following); // TODO: verify: adjust to business day, is following correct???
        settlement = cal.advance(expiry, +3 * Days);

    } else
        QL_FAIL("deduceDates: Currencies other than USD or CNY is not implemented. " << ccy_ql.code() << " provided.");
    */

    Month contractMonth_ql = parseMonth(contractMonth_);
    Date asof = Settings::instance().evaluationDate();
    int year = asof.year();
    if (asof.month() > contractMonth_ql)
        year++;

    Calendar cal = parseCalendar(currency_);

    // calc root date
    Date rootDate;
    vector<string> tokens;
    boost::split(tokens, rootDate_, boost::is_any_of(","));
    string day = boost::to_upper_copy(tokens[0]);
    if (day == "FIRST")
        rootDate = cal.adjust(Date(1, contractMonth_ql, year), QuantLib::Following);
    else if (day == "END")
        rootDate = cal.endOfMonth(Date(1, contractMonth_ql, year), QuantLib::Preceding);
    else { // nth weekday case expected (example format 'Fri,2' for second Friday)
        QL_REQUIRE(tokens.size() == 2, "RootDate " << rootDate_ << " unexpected");
        int n = parseInteger(tokens[1]);
        Weekday day = parseWeekday(tokens[0]);
        rootDate = Date::nthWeekday(n, day, contractMonth_ql, year);
    }

    // now calc expiry // settlement from root date
    string expiryBasis_up = boost::to_upper_copy(expiryBasis_);
    string settlementBasis_up = boost::to_upper_copy(settlementBasis_);
    Period expiryLag_ql = 0 * Days;
    if (!expiryLag_.empty())
        expiryLag_ql = parsePeriod(expiryLag_);
    Period settlementLag_ql = 0 * Days;
    if (!settlementLag_.empty())
        settlementLag_ql = parsePeriod(settlementLag_);
    BusinessDayConvention bdc_expiry = QuantLib::Following;
    if (expiryLag_ql < 0 * Days)
        bdc_expiry = QuantLib::Preceding;
    BusinessDayConvention bdc_settle = QuantLib::Following;
    if (settlementLag_ql < 0 * Days)
        bdc_settle = QuantLib::Preceding;

    if (expiryBasis_up == "ROOT" && settlementBasis_up == "EXPIRY") {
        expiry = cal.advance(rootDate, expiryLag_ql, bdc_expiry);
        settlement = cal.advance(expiry, settlementLag_ql, bdc_settle);
    } else if (settlementBasis_up == "ROOT" && expiryBasis_up == "SETTLEMENT") {
        settlement = cal.advance(rootDate, settlementLag_ql, bdc_settle);
        expiry = cal.advance(settlement, expiryLag_ql, bdc_expiry);
    } else if (expiryBasis_up == "ROOT" && settlementBasis_up == "ROOT") {
        expiry = cal.advance(rootDate, expiryLag_ql, bdc_expiry);
        settlement = cal.advance(rootDate, settlementLag_ql, bdc_settle);
    } else {
        QL_FAIL("expected either expiry or settlement or both to start with root");
    }

}

FutureType BondFuture::selectTypeUS(const string& value) {

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

    string val_up = boost::to_upper_copy(value);

    if (val_up == "UB" || val_up == "WN")
        return FutureType::LongTenor;
    else if (val_up == "ZB" || val_up == "US")
        return FutureType::LongTenor;
    else if (val_up == "TWE")
        return FutureType::LongTenor;
    else if (val_up == "TN" || val_up == "UXY")
        return FutureType::LongTenor;
    else if (val_up == "ZN" || val_up == "TY")
        return FutureType::LongTenor;
    else if (val_up == "ZF" || val_up == "FV")
        return FutureType::ShortTenor;
    else if (val_up == "Z3N" || val_up == "3Y")
        return FutureType::ShortTenor;
    else if (val_up == "ZT" || val_up == "TU")
        return FutureType::ShortTenor;
    else
        QL_FAIL("FutureType " << val_up << " unkown");
}

void BondFuture::checkDates(const Date& expiry, const Date& settlement) {

    Date asof = Settings::instance().evaluationDate();
    if (settlement < expiry)
        QL_FAIL("BondFuture::checkDates for " << id() << " settlement date " << io::iso_date(settlement)
                                              << " lies before expiry " << io::iso_date(expiry));
    if (expiry < asof)
        QL_FAIL("BondFuture::checkDates for " << id() << " is expired, asof " << io::iso_date(asof) << " vs. expiry "
                                              << io::iso_date(expiry));
    // refine + 9 * Months ...
    if (asof + 9 * Months > expiry)
        ALOG("BondFuture::checkDates for " << id() << " expiry may be not in standard cycle of next three quarters "
                                           << io::iso_date(expiry) << " vs asof " << io::iso_date(asof));
    DLOG("BondFuture::checkDates for " << id() << " ExpiryDate " << io::iso_date(expiry) << " SettlementDate "
                                       << io::iso_date(settlement));
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

void BondFuture::populateFromBondFutureReferenceData(const ext::shared_ptr<ReferenceDataManager>& referenceData) {
    QL_REQUIRE(!contractName_.empty(), "BondFuture::populateFromBondReferenceData(): no contract name given");

    if (!referenceData || !referenceData->hasData(BondFutureReferenceDatum::TYPE, contractName_)) {
        DLOG("could not get BondFutureReferenceDatum for name " << contractName_ << " leave data in trade unchanged");

    } else {
        auto bondFutureRefData = ext::dynamic_pointer_cast<BondFutureReferenceDatum>(
            referenceData->getData(BondFutureReferenceDatum::TYPE, contractName_));
        QL_REQUIRE(bondFutureRefData, "could not cast to BondReferenceDatum, this is unexpected");
        DLOG("Got BondFutureReferenceDatum for name " << contractName_ << " overwrite empty elements in trade");
        ore::data::populateFromBondFutureReferenceData(currency_, contractMonth_, deliverableGrade_, rootDate_,
                                                       expiryBasis_, settlementBasis_, expiryLag_, settlementLag_,
                                                       lastTrading_, lastDelivery_, secList_, bondFutureRefData);
    }
}

} // namespace data
} // namespace ore
