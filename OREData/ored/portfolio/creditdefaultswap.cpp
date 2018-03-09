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

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void CreditDefaultSwap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("CreditDefaultSwap::build() called for trade " << id());

    const boost::shared_ptr<Market> market = engineFactory->market();
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("CreditDefaultSwap");

    QL_REQUIRE(swap_.leg().legType() == "Fixed", "CreditDefaultSwap requires Fixed leg");
    Schedule schedule = makeSchedule(swap_.leg().schedule());
    Calendar calendar = parseCalendar(swap_.leg().schedule().rules().front().calendar());
    BusinessDayConvention payConvention = parseBusinessDayConvention(swap_.leg().paymentConvention());
    Protection::Side prot = swap_.leg().isPayer() ? Protection::Side::Buyer : Protection::Side::Seller;
    QL_REQUIRE(swap_.leg().notionals().size() == 1, "CreditDefaultSwap requires single notional");
    notional_ = swap_.leg().notionals().front();
    DayCounter dc = parseDayCounter(swap_.leg().dayCounter());

    boost::shared_ptr<FixedLegData> fixedData =
        boost::dynamic_pointer_cast<FixedLegData>(swap_.leg().concreteLegData());
    QL_REQUIRE(fixedData, "Wrong LegType, expected Fixed, got " << swap_.leg().legType());

    QL_REQUIRE(fixedData->rates().size() == 1, "CreditDefaultSwap requires single rate");

    boost::shared_ptr<QuantExt::CreditDefaultSwap> cds;

    if (swap_.upfrontDate() == Null<Date>())
        cds = boost::make_shared<QuantExt::CreditDefaultSwap>(
            prot, swap_.leg().notionals().front(), fixedData->rates().front(), schedule, payConvention, dc,
            swap_.settlesAccrual(), swap_.paysAtDefaultTime(), swap_.protectionStart());
    else {
        QL_REQUIRE(swap_.upfrontFee() != Null<Real>(), "CreditDefaultSwap: upfront date given, but no upfront fee");
        cds = boost::make_shared<QuantExt::CreditDefaultSwap>(
            prot, notional_, swap_.upfrontFee(), fixedData->rates().front(), schedule, payConvention, dc,
            swap_.settlesAccrual(), swap_.paysAtDefaultTime(), swap_.protectionStart(), swap_.upfrontDate());
    }

    boost::shared_ptr<CreditDefaultSwapEngineBuilder> cdsBuilder =
        boost::dynamic_pointer_cast<CreditDefaultSwapEngineBuilder>(builder);

    npvCurrency_ = swap_.leg().currency();

    QL_REQUIRE(cdsBuilder, "No Builder found for CreditDefaultSwap: " << id());
    cds->setPricingEngine(cdsBuilder->engine(parseCurrency(npvCurrency_), swap_.creditCurveId()));

    instrument_.reset(new VanillaInstrument(cds));

    maturity_ = cds->coupons().back()->date();

    legs_ = {cds->coupons()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {swap_.leg().isPayer()};
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
