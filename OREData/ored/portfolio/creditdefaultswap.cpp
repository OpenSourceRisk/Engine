/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/creditdefaultswap.hpp>
#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void CreditDefaultSwap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("CreditDefaultSwap::build() called for trade " << id());

    const boost::shared_ptr<Market> market = engineFactory->market();
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("CreditDefaultSwap");

    auto legData = swap_.leg(); // copy
    const auto& notionals = swap_.leg().notionals();

    QL_REQUIRE(legData.legType() == "Fixed", "CreditDefaultSwap requires Fixed leg");
    Schedule schedule = makeSchedule(legData.schedule());
    QL_REQUIRE(schedule.size() > 1, "CreditDefaultSwap requires at least two dates in the schedule");

    BusinessDayConvention payConvention = Following;
    if (!legData.paymentConvention().empty()) {
        payConvention = parseBusinessDayConvention(legData.paymentConvention());
    }

    Protection::Side prot = legData.isPayer() ? Protection::Side::Buyer : Protection::Side::Seller;
    notional_ = notionals.front();

    DayCounter dc = Actual360();
    if (!legData.dayCounter().empty()) {
        dc = parseDayCounter(legData.dayCounter());
    }

    // In general for CDS and CDS index trades, the standard day counter is Actual/360 and the final
    // period coupon accrual includes the maturity date.
    DayCounter lastPeriodDayCounter;
    auto strLpdc = legData.lastPeriodDayCounter();
    if (!strLpdc.empty()) {
        lastPeriodDayCounter = parseDayCounter(strLpdc);
    } else {
        Actual360 standardDayCounter;
        if (dc == standardDayCounter) {
            lastPeriodDayCounter = dc == standardDayCounter ? Actual360(true) : dc;
            legData.lastPeriodDayCounter() = "A360 (Incl Last)";
        } else {
            lastPeriodDayCounter = dc;
        }
    }

    // Build the coupon leg
    auto legBuilder = engineFactory->legBuilder(legData.legType());
    auto couponLeg =
        legBuilder->buildLeg(legData, engineFactory, requiredFixings_, builder->configuration(MarketContext::pricing));
    // for the accrual rebate calculation we may need historical coupons that are already paid
    requiredFixings_.unsetPayDates();

    /* If we have an indexed leg we don't allow for an upfront fee, since we would need to index that
       as well, btu the ql instrument / engine cdoes not support this currently */
    QL_REQUIRE(legData.indexing().empty() || swap_.upfrontFee() == Null<Real>(),
               "CreditDefaultSwap with indexed coupon leg does not allow for an upfront fee");

    /* The rate is really only used to compute the fair spread in the add results and we support that only
       for fixed coupons with a sinlge rate, otherwise we set this rate to zero */
    Real fixedRate = 0.0;
    if (auto fixedData = boost::dynamic_pointer_cast<FixedLegData>(legData.concreteLegData())) {
        if (fixedData->rates().size() == 1)
            fixedRate = fixedData->rates().front();
    }

    boost::shared_ptr<QuantLib::CreditDefaultSwap> cds;

    if (swap_.upfrontFee() == Null<Real>()) {
        cds = boost::make_shared<QuantLib::CreditDefaultSwap>(
            prot, notional_, couponLeg, fixedRate, schedule, payConvention, dc, swap_.settlesAccrual(),
            swap_.protectionPaymentTime(), swap_.protectionStart(), boost::shared_ptr<Claim>(), lastPeriodDayCounter,
            swap_.rebatesAccrual(), swap_.tradeDate(), swap_.cashSettlementDays());
    } else {
        cds = boost::make_shared<QuantLib::CreditDefaultSwap>(
            prot, notional_, couponLeg, swap_.upfrontFee(), fixedRate, schedule, payConvention, dc,
            swap_.settlesAccrual(), swap_.protectionPaymentTime(), swap_.protectionStart(), swap_.upfrontDate(),
            boost::shared_ptr<Claim>(), lastPeriodDayCounter, swap_.rebatesAccrual(), swap_.tradeDate(),
            swap_.cashSettlementDays());
    }

    boost::shared_ptr<CreditDefaultSwapEngineBuilder> cdsBuilder =
        boost::dynamic_pointer_cast<CreditDefaultSwapEngineBuilder>(builder);

    npvCurrency_ = legData.currency();

    QL_REQUIRE(cdsBuilder, "No Builder found for CreditDefaultSwap: " << id());
    cds->setPricingEngine(cdsBuilder->engine(parseCurrency(npvCurrency_), swap_.creditCurveId(), swap_.recoveryRate()));

    instrument_.reset(new VanillaInstrument(cds));

    maturity_ = cds->coupons().back()->date();

    legs_ = {cds->coupons()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {legData.isPayer()};
    notionalCurrency_ = legData.currency();

    if (swap_.protectionStart() != Date())
        additionalData_["startDate"] = to_string(swap_.protectionStart());
    else
        additionalData_["startDate"] = to_string(schedule.dates().front());
}

QuantLib::Real CreditDefaultSwap::notional() const {
    Date asof = Settings::instance().evaluationDate();
    // get the current notional from CDS premium leg
    for (Size i = 0; i < legs_[0].size(); ++i) {
        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(legs_[0][i]);
        if (coupon->date() > asof)
            return coupon->nominal();
    }

    // if no coupons are given, return the initial notional by default
    return notional_;
}

void CreditDefaultSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* cdsNode = XMLUtils::getChildNode(node, "CreditDefaultSwapData");
    QL_REQUIRE(cdsNode, "No CreditDefaultSwapData Node");
    swap_.fromXML(cdsNode);
}

XMLNode* CreditDefaultSwap::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLUtils::appendNode(node, swap_.toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
