/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ored/utilities/bondindexbuilder.hpp>
#include <ored/utilities/marketdata.hpp>

namespace ore {
namespace data {

BondIndexBuilder::BondIndexBuilder(const std::string& securityId, const bool dirty, const bool relative,
                                   const Calendar& fixingCalendar, const bool conditionalOnSurvival,
                                   const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, Real bidAskAdjustment,
                                   const bool bondIssueDateFallback)
    : dirty_(dirty) {
    auto tmp = BondFactory::instance().build(engineFactory, engineFactory->referenceData(), securityId);
    bondData_ = tmp.bondData;
    trade_ = tmp.trade;
    bond_ = tmp.bond;
    buildIndex(relative, fixingCalendar, conditionalOnSurvival, engineFactory, bidAskAdjustment, bondIssueDateFallback);
}

BondIndexBuilder::BondIndexBuilder(const BondData& bondData, const bool dirty, const bool relative,
                                   const Calendar& fixingCalendar, const bool conditionalOnSurvival,
                                   const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                   QuantLib::Real bidAskAdjustment, const bool bondIssueDateFallback)
    : dirty_(dirty) {
    bondData_ = bondData;
    bondData_.populateFromBondReferenceData(engineFactory->referenceData());
    trade_ = QuantLib::ext::make_shared<Bond>(Envelope(), bondData);
    trade_->build(engineFactory);
    bond_ = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(trade_->instrument()->qlInstrument());
    QL_REQUIRE(bond_, "BondIndexBuilder: bond_ can not be cast to QuantLib::Bond, internal error.");
    buildIndex(relative, fixingCalendar, conditionalOnSurvival, engineFactory, bidAskAdjustment, bondIssueDateFallback);
}

void BondIndexBuilder::buildIndex(const bool relative, const Calendar& fixingCalendar, const bool conditionalOnSurvival,
                                  const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, Real bidAskAdjustment,
                                  const bool bondIssueDateFallback) {

    fixings_ = trade_->requiredFixings();

    // get the curves
    string securityId = bondData_.securityId();

    Handle<YieldTermStructure> discountCurve = engineFactory->market()->yieldCurve(
        bondData_.referenceCurveId(), engineFactory->configuration(MarketContext::pricing));

    Handle<DefaultProbabilityTermStructure> defaultCurve;
    if (!bondData_.creditCurveId().empty())
        defaultCurve = securitySpecificCreditCurve(engineFactory->market(), securityId, bondData_.creditCurveId(),
                                                   engineFactory->configuration(MarketContext::pricing))
                           ->curve();

    Handle<YieldTermStructure> incomeCurve;
    if (!bondData_.incomeCurveId().empty())
        incomeCurve = engineFactory->market()->yieldCurve(bondData_.incomeCurveId(),
                                                          engineFactory->configuration(MarketContext::pricing));

    Handle<Quote> recovery;
    try {
        recovery =
            engineFactory->market()->recoveryRate(securityId, engineFactory->configuration(MarketContext::pricing));
    } catch (...) {
        WLOG("security specific recovery rate not found for security ID "
             << securityId << ", falling back on the recovery rate for credit curve Id "
             << bondData_.creditCurveId());
        if (!bondData_.creditCurveId().empty())
            recovery = engineFactory->market()->recoveryRate(bondData_.creditCurveId(),
                                                             engineFactory->configuration(MarketContext::pricing));
    }

    Handle<Quote> spread(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    try {
        spread =
            engineFactory->market()->securitySpread(securityId, engineFactory->configuration(MarketContext::pricing));
    } catch (...) {
    }

    // build and return the index
    bondIndex_ = QuantLib::ext::make_shared<QuantExt::BondIndex>(
        securityId, dirty_, relative, fixingCalendar, bond_, discountCurve, defaultCurve, recovery, spread, incomeCurve,
        conditionalOnSurvival, parseDate(bondData_.issueDate()), bondData_.priceQuoteMethod(),
        bondData_.priceQuoteBaseValue(), bondData_.isInflationLinked(), bidAskAdjustment, bondIssueDateFallback,
        bondData_.quotedDirtyPrices());
}

QuantLib::ext::shared_ptr<QuantExt::BondIndex> BondIndexBuilder::bondIndex() const { return bondIndex_; }

void BondIndexBuilder::addRequiredFixings(RequiredFixings& requiredFixings, Leg leg) {
    requiredFixings.addData(fixings_.filteredFixingDates());
    if (dirty_) {
        if (leg.empty())
            return;
        RequiredFixings legFixings;
        auto fixingGetter = QuantLib::ext::make_shared<FixingDateGetter>(legFixings);
        fixingGetter->setRequireFixingStartDates(true);
        addToRequiredFixings(leg, fixingGetter);

        set<Date> fixingDates;

        auto fixingMap = legFixings.fixingDatesIndices();
        if (fixingMap.size() > 0) {
            std::map<std::string, std::set<Date>> indexFixings;
            for (const auto& [_, dates] : fixingMap) {
                for (const auto& [d, mandatory] : dates) {
                    auto tmp = fixings_.filteredFixingDates(d);
                    requiredFixings.addData(tmp);
                }
            }
        }
    }
}

Real BondIndexBuilder::priceAdjustment(Real price) {
    if (price == Null<Real>())
        return price;

    Real adj = bondData_.priceQuoteMethod() == QuantExt::BondIndex::PriceQuoteMethod::CurrencyPerUnit
                   ? 1.0 / bondData_.priceQuoteBaseValue()
                   : 1.0;
    return price * adj;
}

} // namespace data
} // namespace ore
