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

#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ql/cashflows/simplecashflow.hpp>

#include <ql/time/calendars/target.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/currencyswap.hpp>

#include <ql/instruments/swap.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void Swap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("Swap::build() called for trade " << id());

    QL_REQUIRE(legData_.size() >= 1, "Swap must have at least 1 leg");
    const boost::shared_ptr<Market> market = engineFactory->market();

    string ccy_str = legData_[0].currency();
    Currency currency = parseCurrency(ccy_str);

    Size numLegs = legData_.size();
    legPayers_ = vector<bool>(numLegs);
    std::vector<QuantLib::Currency> currencies(numLegs);
    legs_.resize(numLegs);

    bool isXCCY = false;

    for (Size i = 0; i < numLegs; ++i) {
        currencies[i] = parseCurrency(legData_[i].currency());
        if (legData_[i].currency() != ccy_str)
            isXCCY = true;
    }

    boost::shared_ptr<EngineBuilder> builder =
        isXCCY ? engineFactory->builder("CrossCurrencySwap") : engineFactory->builder("Swap");

    for (Size i = 0; i < numLegs; ++i) {
        legPayers_[i] = legData_[i].isPayer();

        boost::shared_ptr<FxIndex> fxIndex;
        bool invertFxIndex = false;
        if (legData_[i].fxIndex() != "") {
            // We have an FX Index to setup

            // 1. Parse the index we have with no term structures
            boost::shared_ptr<QuantExt::FxIndex> fxIndexBase = parseFxIndex(legData_[i].fxIndex());

            // get market data objects - we set up the index using source/target from legData_[i].fxIndex()
            string source = fxIndexBase->sourceCurrency().code();
            string target = fxIndexBase->targetCurrency().code();
            Handle<YieldTermStructure> sorTS = market->discountCurve(source);
            Handle<YieldTermStructure> tarTS = market->discountCurve(target);
            Handle<Quote> spot = market->fxSpot(source + target);
            fxIndex = boost::make_shared<FxIndex>(fxIndexBase->familyName(), legData_[i].fixingDays(),
                                                  fxIndexBase->sourceCurrency(), fxIndexBase->targetCurrency(),
                                                  fxIndexBase->fixingCalendar(), spot, sorTS, tarTS);
            QL_REQUIRE(fxIndex, "Resetting XCCY - fxIndex failed to build");

            // Now check the ccy and foreignCcy from the legdata, work out if we need to invert or not
            string domestic = legData_[i].currency();
            string foreign = legData_[i].foreignCurrency();
            if (domestic == target && foreign == source) {
                invertFxIndex = false;
            } else if (domestic == source && foreign == target) {
                invertFxIndex = true;
            } else {
                QL_FAIL("Cannot combine FX Index " << legData_[i].fxIndex() << " with reset ccy " << domestic
                                                   << " and reset foreignCurrency " << foreign);
            }
        }

        if (legData_[i].legType() == "Fixed") {
            legs_[i] = makeFixedLeg(legData_[i]);
        } else if (legData_[i].legType() == "Floating") {
            string indexName = legData_[i].floatingLegData().index();

            Handle<IborIndex> hIndex =
                engineFactory->market()->iborIndex(indexName, builder->configuration(MarketContext::pricing));
            QL_REQUIRE(!hIndex.empty(), "Could not find ibor index " << indexName << " in market.");
            boost::shared_ptr<IborIndex> index = hIndex.currentLink();

            // Do we have an overnight index?
            boost::shared_ptr<OvernightIndex> ois = boost::dynamic_pointer_cast<OvernightIndex>(index);
            if (ois) {
                legs_[i] = makeOISLeg(legData_[i], ois);
            } else if (!legData_[i].isNotResetXCCY()) {
                // this is the reseting leg...
                Real foreignNotional = legData_[i].foreignAmount();
                // build a temp leg
                Leg tempLeg = makeIborLeg(legData_[i], index, engineFactory);
                QL_REQUIRE(tempLeg.size() > 0, "At least one coupon needed for leg " << i);

                // and then copy everything into a vector of new FloatingRateFXLinkedNotionalCoupons
                legs_[i].resize(tempLeg.size());
                // First coupon is the same (no reset or FX link)
                legs_[i][0] = tempLeg[0];
                // The reset are FX Linked
                for (Size j = 1; j < tempLeg.size(); ++j) {
                    boost::shared_ptr<FloatingRateCoupon> coupon =
                        boost::dynamic_pointer_cast<FloatingRateCoupon>(tempLeg[j]);
                    boost::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fxLinkedCoupon(
                        new FloatingRateFXLinkedNotionalCoupon(
                            foreignNotional,
                            coupon->accrualStartDate(), // fx fixing at start of coupon
                            fxIndex, invertFxIndex, coupon->date(), coupon->accrualStartDate(),
                            coupon->accrualEndDate(), legData_[i].fixingDays(), coupon->index(), coupon->gearing(),
                            coupon->spread(), coupon->referencePeriodStart(), coupon->referencePeriodEnd(),
                            coupon->dayCounter(), coupon->isInArrears()));

                    // set the same pricer
                    fxLinkedCoupon->setPricer(coupon->pricer());

                    legs_[i][j] = fxLinkedCoupon;
                }
            } else {
                legs_[i] = makeIborLeg(legData_[i], index, engineFactory);
            }
        } else if (legData_[i].legType() == "CPI") {
            string inflationIndexName = legData_[i].cpiLegData().index();
            bool inflationIndexInterpolated = legData_[i].cpiLegData().interpolated();
            boost::shared_ptr<ZeroInflationIndex> index =
                *market->zeroInflationIndex(inflationIndexName, inflationIndexInterpolated);
            QL_REQUIRE(index, "zero inflation index not found for index " << legData_[i].cpiLegData().index());
            legs_[i] = makeCPILeg(legData_[i], index);
            // legTypes[i] = Inflation;
            // legTypes_[i] = "INFLATION";
        } else if (legData_[i].legType() == "YY") {
            string inflationIndexName = legData_[i].yoyLegData().index();
            bool inflationIndexInterpolated = legData_[i].yoyLegData().interpolated();
            boost::shared_ptr<YoYInflationIndex> index =
                *market->yoyInflationIndex(inflationIndexName, inflationIndexInterpolated);
            legs_[i] = makeYoYLeg(legData_[i], index);
            // legTypes[i] = Inflation;
            // legTypes_[i] = "INFLATION_YOY";
        } else if (legData_[i].legType() == "Cashflow") {
            legs_[i] = makeSimpleLeg(legData_[i]);
        } else if (legData_[i].legType() == "CMS") {
            string swapIndexName = legData_[i].cmsLegData().swapIndex();

            Handle<SwapIndex> hIndex =
                engineFactory->market()->swapIndex(swapIndexName, builder->configuration(MarketContext::pricing));
            QL_REQUIRE(!hIndex.empty(), "Could not find swap index " << swapIndexName << " in market.");

            boost::shared_ptr<SwapIndex> index = hIndex.currentLink();
            legs_[i] = makeCMSLeg(legData_[i], index, engineFactory);

        } else {
            QL_FAIL("Unknown leg type " << legData_[i].legType());
        }
        DLOG("Swap::build(): currency[" << i << "] = " << currencies[i]);

        // Add notional legs (if required)
        if (!legData_[i].isNotResetXCCY()) {
            DLOG("Building Resetting XCCY Notional leg");

            Real foreignNotional = legData_[i].foreignAmount();

            Leg resettingLeg;
            // Note we do not reset the first coupon.
            for (Size j = 0; j < legs_[i].size(); j++) {
                boost::shared_ptr<Coupon> c = boost::dynamic_pointer_cast<Coupon>(legs_[i][j]);
                QL_REQUIRE(c, "Resetting XCCY - expected Coupon"); // TODO: fixed fx resetable?

                // build a pair of notional flows, one at the start and one at the end of
                // the accrual period. Both with the same FX fixing date

                if (j == 0) {
                    // notional exchange?
                    if (legData_[i].notionalInitialExchange())
                        resettingLeg.push_back(
                            boost::shared_ptr<CashFlow>(new SimpleCashFlow(-(c->nominal()), c->accrualStartDate())));

                    // This is to offset the first fx change below
                    resettingLeg.push_back(
                        boost::shared_ptr<CashFlow>(new SimpleCashFlow(c->nominal(), c->accrualEndDate())));
                } else {
                    resettingLeg.push_back(boost::shared_ptr<CashFlow>(new FXLinkedCashFlow(
                        c->accrualStartDate(), c->accrualStartDate(), -foreignNotional, fxIndex, invertFxIndex)));

                    // we don't want a final one, unless there is notional exchange
                    if (j < legs_[i].size() - 1 || legData_[i].notionalFinalExchange())
                        resettingLeg.push_back(boost::shared_ptr<CashFlow>(new FXLinkedCashFlow(
                            c->accrualEndDate(), c->accrualStartDate(), foreignNotional, fxIndex, invertFxIndex)));
                }
            }
            legs_.push_back(resettingLeg);
            legPayers_.push_back(legPayers_[i]);
            currencies.push_back(currencies[i]);

            if (legData_[i].notionalAmortizingExchange()) {
                QL_FAIL("Cannot have an amoritizing notional with FX reset");
            }
        }
        // check for notional exchanges on non FX reseting trades
        else if (legData_[i].notionalInitialExchange() || legData_[i].notionalFinalExchange() ||
                 legData_[i].notionalAmortizingExchange()) {

            legs_.push_back(makeNotionalLeg(legs_[i], legData_[i].notionalInitialExchange(),
                                            legData_[i].notionalFinalExchange(),
                                            legData_[i].notionalAmortizingExchange()));
            legPayers_.push_back(legPayers_[i]);
            currencies.push_back(currencies[i]);
        }
    } // for legs

    // NPV currency is just taken from the first leg, so for XCCY this is just the first one
    // that appears in the XML
    npvCurrency_ = ccy_str;
    notional_ = currentNotional(legs_[0]); // match npvCurrency_

    if (isXCCY) {
        boost::shared_ptr<QuantExt::CurrencySwap> swap(new QuantExt::CurrencySwap(legs_, legPayers_, currencies));
        boost::shared_ptr<CrossCurrencySwapEngineBuilder> swapBuilder =
            boost::dynamic_pointer_cast<CrossCurrencySwapEngineBuilder>(builder);
        QL_REQUIRE(swapBuilder, "No Builder found for CrossCurrencySwap " << id());
        swap->setPricingEngine(swapBuilder->engine(currencies, currency));
        DLOG("Swap::build(): XCCY Swap NPV = " << swap->NPV());
        // take the first legs currency as the npv currency (arbitrary choice)
        instrument_.reset(new VanillaInstrument(swap));
    } else {
        boost::shared_ptr<QuantLib::Swap> swap(new QuantLib::Swap(legs_, legPayers_));
        boost::shared_ptr<SwapEngineBuilderBase> swapBuilder =
            boost::dynamic_pointer_cast<SwapEngineBuilderBase>(builder);
        QL_REQUIRE(swapBuilder, "No Builder found for Swap " << id());
        swap->setPricingEngine(swapBuilder->engine(currency));
        DLOG("Swap::build(): Swap NPV = " << swap->NPV());
        instrument_.reset(new VanillaInstrument(swap));
    }

    DLOG("Set instrument wrapper");

    // set Leg Currencies
    legCurrencies_ = vector<string>(currencies.size());
    for (Size i = 0; i < currencies.size(); i++)
        legCurrencies_[i] = currencies[i].code();

    // set maturity
    maturity_ = legs_[0].back()->date();
    for (Size i = 1; i < legs_.size(); i++) {
        QL_REQUIRE(legs_[i].size() > 0, "Leg " << i << " of " << legs_.size() << " is empty.");
        Date d = legs_[i].back()->date();
        if (d > maturity_)
            maturity_ = d;
    }
}

void Swap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    legData_.clear();
    XMLNode* swapNode = XMLUtils::getChildNode(node, "SwapData");
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); i++) {
        LegData ld;
        ld.fromXML(nodes[i]);
        legData_.push_back(ld);
    }
}

XMLNode* Swap::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swapNode = doc.allocNode("SwapData");
    XMLUtils::appendNode(node, swapNode);
    for (Size i = 0; i < legData_.size(); i++)
        XMLUtils::appendNode(swapNode, legData_[i].toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
