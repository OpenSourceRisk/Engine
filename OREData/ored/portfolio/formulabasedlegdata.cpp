/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/utilities/log.hpp>

#include <ored/portfolio/builders/formulabasedcoupon.hpp>
#include <ored/portfolio/formulabasedindexbuilder.hpp>
#include <ored/portfolio/formulabasedlegdata.hpp>
#include <ored/utilities/formulaparser.hpp>

#include <qle/cashflows/couponpricer.hpp>
#include <qle/cashflows/formulabasedcoupon.hpp>
#include <qle/cashflows/couponpricer.hpp>

#include <ored/utilities/indexnametranslator.hpp>
#include <ored/portfolio/builders/capfloorediborleg.hpp>
#include <ored/portfolio/builders/cms.hpp>
#include <ored/portfolio/legdata.hpp>

#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

XMLNode* FormulaBasedLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChild(doc, node, "Index", formulaBasedIndex_);
    XMLUtils::addChild(doc, node, "IsInArrears", isInArrears_);
    XMLUtils::addChild(doc, node, "FixingDays", fixingDays_);
    XMLUtils::addChild(doc, node, "FixingCalendar", fixingCalendar_);
    return node;
}

void FormulaBasedLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    formulaBasedIndex_ = XMLUtils::getChildValue(node, "Index", true);
    fixingDays_ = XMLUtils::getChildValueAsInt(node, "FixingDays", true);
    // These are all optional
    XMLNode* arrNode = XMLUtils::getChildNode(node, "IsInArrears");
    if (arrNode)
        isInArrears_ = XMLUtils::getChildValueAsBool(node, "IsInArrears", true);
    else
        isInArrears_ = false;                                          // default to fixing-in-advance
    fixingCalendar_ = XMLUtils::getChildValue(node, "FixingCalendar"); // defaults to empty string
    initIndices();
}

void FormulaBasedLegData::initIndices() {
    std::vector<std::string> variables;
    QuantExt::CompiledFormula compiledFormula = parseFormula(formulaBasedIndex_, variables);
    indices_.insert(variables.begin(), variables.end());
}

namespace {
QuantLib::ext::shared_ptr<QuantLib::FloatingRateCouponPricer>
getFormulaBasedCouponPricer(const QuantLib::ext::shared_ptr<QuantExt::FormulaBasedIndex>& formulaBasedIndex,
                            const Currency& paymentCurrency, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                            const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexMaps) {
    auto builder =
        QuantLib::ext::dynamic_pointer_cast<FormulaBasedCouponPricerBuilder>(engineFactory->builder("FormulaBasedCoupon"));
    QL_REQUIRE(builder != nullptr, "makeFormulaBasedLeg(): no builder found for FormulaBasedCoupon");

    std::map<std::string, QuantLib::ext::shared_ptr<IborCouponPricer>> iborPricers;
    std::map<std::string, QuantLib::ext::shared_ptr<CmsCouponPricer>> cmsPricers;
    for (auto const& i : formulaBasedIndex->indices()) {
        // add ibor pricer for index
        if (auto ibor = QuantLib::ext::dynamic_pointer_cast<QuantLib::IborIndex>(i)) {
            auto iborBuilder = QuantLib::ext::dynamic_pointer_cast<CapFlooredIborLegEngineBuilder>(
                engineFactory->builder("CapFlooredIborLeg"));
            QL_REQUIRE(iborBuilder != nullptr, "makeFormulaBasedLeg(): No builder found for CapFlooredIborLeg");
            auto pricerKey = IndexNameTranslator::instance().oreName(ibor->name());
            auto iborPricer = QuantLib::ext::dynamic_pointer_cast<IborCouponPricer>(iborBuilder->engine(pricerKey));
            QL_REQUIRE(iborPricer != nullptr, "makeFormulaBasedLeg(): expected ibor coupon pricer");
            iborPricers[i->name()] = iborPricer;
        }
        // add cms pricer for index
        if (auto cms = QuantLib::ext::dynamic_pointer_cast<QuantLib::SwapIndex>(i)) {
            auto cmsBuilder = QuantLib::ext::dynamic_pointer_cast<CmsCouponPricerBuilder>(engineFactory->builder("CMS"));
            QL_REQUIRE(cmsBuilder, "makeFormulaBasedLeg(): No builder found for CmsLeg");
            auto pricerKey = IndexNameTranslator::instance().oreName(cms->iborIndex()->name());
            auto cmsPricer = QuantLib::ext::dynamic_pointer_cast<CmsCouponPricer>(cmsBuilder->engine(pricerKey));
            QL_REQUIRE(cmsPricer != nullptr, "makeFormulaBasedLeg(): expected cms coupon pricer");
            cmsPricers[cms->iborIndex()->name()] = cmsPricer;
        }
    }

    return builder->engine(paymentCurrency.code(), iborPricers, cmsPricers, indexMaps);
}
} // namespace

Leg makeFormulaBasedLeg(const LegData& data, const QuantLib::ext::shared_ptr<QuantExt::FormulaBasedIndex>& formulaBasedIndex,
                        const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                        const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexMaps,
                        const QuantLib::Date& openEndDateReplacement) {
    auto formulaBasedData = QuantLib::ext::dynamic_pointer_cast<FormulaBasedLegData>(data.concreteLegData());
    QL_REQUIRE(formulaBasedData, "Wrong LegType, expected FormulaBased, got " << data.legType());
    Currency paymentCurrency = parseCurrency(data.currency());
    Schedule schedule = makeSchedule(data.schedule(), openEndDateReplacement);
    if (schedule.size() < 2)
        return Leg();
    Calendar paymentCalendar;
    if (data.paymentCalendar().empty())
        paymentCalendar = schedule.calendar();
    else
        paymentCalendar = parseCalendar(data.paymentCalendar());
    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());
    // day counter is optional in leg data in general, but here we need it
    QL_REQUIRE(data.dayCounter() != "", "makeFormulaBasedLeg(): day counter must be given");
    DayCounter dc = parseDayCounter(data.dayCounter());
    // bdc is optional too, but all the other legs just do this, too ?!
    // FIXME, to be solved for all leg types, here we do as in the other legs
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());
    auto formulaBasedLegData = QuantLib::ext::dynamic_pointer_cast<FormulaBasedLegData>(data.concreteLegData());
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);

    ore::data::applyAmortization(notionals, data, schedule);
    // amortization type annuity is not allowed, check this
    for (auto const& amort : data.amortizationData()) {
        if (!amort.initialized())
            continue;
        AmortizationType amortizationType = parseAmortizationType(amort.type());
        QL_REQUIRE(amortizationType != AmortizationType::Annuity,
                   "AmortizationType " << amort.type() << " not supported for Formula based legs");
    }

    QuantExt::FormulaBasedLeg formulaBasedLeg =
        QuantExt::FormulaBasedLeg(paymentCurrency, schedule, formulaBasedIndex)
            .withNotionals(notionals)
            .withPaymentCalendar(paymentCalendar)
            .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
            .withPaymentDayCounter(dc)
            .withPaymentAdjustment(bdc)
            .withFixingDays(formulaBasedData->fixingDays())
            .inArrears(formulaBasedData->isInArrears());

    auto couponPricer = getFormulaBasedCouponPricer(formulaBasedIndex, paymentCurrency, engineFactory, indexMaps);

    // make sure leg is built before pricers are set...
    Leg tmp = formulaBasedLeg;
    QuantExt::setCouponPricer(tmp, couponPricer);
    return tmp;
}

} // namespace data
} // namespace ore
