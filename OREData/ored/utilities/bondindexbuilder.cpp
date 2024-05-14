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

BondIndexBuilder::BondIndexBuilder(BondData securityData, const bool dirty, const bool relative,
                                   const Calendar& fixingCalendar, const bool conditionalOnSurvival,
                                   const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, Real bidAskAdjustment,
                                   const bool bondIssueDateFallback)
    : dirty_(dirty) {

    securityData.populateFromBondReferenceData(engineFactory->referenceData());
    bond_ = Bond(Envelope(), securityData);
    bond_.build(engineFactory);
    buildIndex(relative, fixingCalendar, conditionalOnSurvival, engineFactory, bidAskAdjustment, bondIssueDateFallback);
}

BondIndexBuilder::BondIndexBuilder(const Bond& bond, const bool dirty, const bool relative,
                                   const Calendar& fixingCalendar, const bool conditionalOnSurvival, 
                                   const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, Real bidAskAdjustment,
                                   const bool bondIssueDateFallback)
    : bond_(bond), dirty_(dirty) {
    buildIndex(relative, fixingCalendar, conditionalOnSurvival, engineFactory, bidAskAdjustment, bondIssueDateFallback);
}

BondIndexBuilder::BondIndexBuilder(const std::string& securityId, const bool dirty, const bool relative,
                                   const Calendar& fixingCalendar, const bool conditionalOnSurvival,
                                   const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, Real bidAskAdjustment,
                                   const bool bondIssueDateFallback)
    : dirty_(dirty) {
    BondData bondData(securityId, 100.0);
    bondData.populateFromBondReferenceData(engineFactory->referenceData());
    bond_ = Bond(Envelope(), bondData);
    bond_.build(engineFactory);
    buildIndex(relative, fixingCalendar, conditionalOnSurvival, engineFactory, bidAskAdjustment, bondIssueDateFallback);
}

void BondIndexBuilder::buildIndex(const bool relative, const Calendar& fixingCalendar, const bool conditionalOnSurvival,
                                  const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, Real bidAskAdjustment,
                                  const bool bondIssueDateFallback) {

    fixings_ = bond_.requiredFixings();

    auto qlBond = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(bond_.instrument()->qlInstrument());
    QL_REQUIRE(qlBond, "buildBondIndex(): could not cast to QuantLib::Bond, this is unexpected");

    // get the curves
    auto bondData = bond_.bondData();
    string securityId = bondData.securityId();

    Handle<YieldTermStructure> discountCurve = engineFactory->market()->yieldCurve(
        bondData.referenceCurveId(), engineFactory->configuration(MarketContext::pricing));

    Handle<DefaultProbabilityTermStructure> defaultCurve;
    if (!bondData.creditCurveId().empty())
        defaultCurve = securitySpecificCreditCurve(engineFactory->market(), securityId, bondData.creditCurveId(),
                                                   engineFactory->configuration(MarketContext::pricing))
                           ->curve();

    Handle<YieldTermStructure> incomeCurve;
    if (!bondData.incomeCurveId().empty())
        incomeCurve = engineFactory->market()->yieldCurve(bondData.incomeCurveId(),
                                                          engineFactory->configuration(MarketContext::pricing));

    Handle<Quote> recovery;
    try {
        recovery =
            engineFactory->market()->recoveryRate(securityId, engineFactory->configuration(MarketContext::pricing));
    } catch (...) {
        WLOG("security specific recovery rate not found for security ID "
             << securityId << ", falling back on the recovery rate for credit curve Id " << bondData.creditCurveId());
        if (!bondData.creditCurveId().empty())
            recovery = engineFactory->market()->recoveryRate(bondData.creditCurveId(),
                                                             engineFactory->configuration(MarketContext::pricing));
    }

    Handle<Quote> spread(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    try {
        spread =
            engineFactory->market()->securitySpread(securityId, engineFactory->configuration(MarketContext::pricing));
    } catch (...) {
    }

    if (!bondData.hasCreditRisk())
        defaultCurve = Handle<DefaultProbabilityTermStructure>();
   
    // build and return the index
    bondIndex_ = QuantLib::ext::make_shared<QuantExt::BondIndex>(securityId, dirty_, relative, fixingCalendar, qlBond,
        discountCurve, defaultCurve, recovery, spread, incomeCurve, conditionalOnSurvival, parseDate(bondData.issueDate()), bondData.priceQuoteMethod(),
        bondData.priceQuoteBaseValue(), bondData.isInflationLinked(), bidAskAdjustment, bondIssueDateFallback);
}

QuantLib::ext::shared_ptr<QuantExt::BondIndex> BondIndexBuilder::bondIndex() const { return bondIndex_; }

void BondIndexBuilder::addRequiredFixings(RequiredFixings& requiredFixings, Leg leg) {
    requiredFixings.addData(fixings_.filteredFixingDates());
    if (dirty_) {
        QL_REQUIRE(leg.size() > 0, "BondIndexBuild: Leg is required if dirty flag set to true");
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

    Real adj = bond_.bondData().priceQuoteMethod() == QuantExt::BondIndex::PriceQuoteMethod::CurrencyPerUnit
                   ? 1.0 / bond_.bondData().priceQuoteBaseValue()
                   : 1.0;
    return price * adj;
}

} // namespace data
} // namespace ore
