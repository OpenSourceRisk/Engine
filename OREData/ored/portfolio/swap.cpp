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
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/legbuilders.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/cashflows/simplecashflow.hpp>

#include <ql/time/calendars/target.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/currencyswap.hpp>

#include <ql/instruments/swap.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace QuantExt;
using std::make_pair;

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

        // Initialise the set of index name, leg index pairs
        for (const auto& index : legData_[i].indices()) {
            nameIndexPairs_.insert(make_pair(index, i));
        }
    }

    boost::shared_ptr<EngineBuilder> builder =
        isXCCY ? engineFactory->builder("CrossCurrencySwap") : engineFactory->builder("Swap");
    auto configuration = builder->configuration(MarketContext::pricing);

    for (Size i = 0; i < numLegs; ++i) {
        legPayers_[i] = legData_[i].isPayer();

        boost::shared_ptr<FxIndex> fxIndex;
        if (legData_[i].fxIndex() != "") {
            // We have an FX Index to setup
            fxIndex = buildFxIndex(legData_[i].fxIndex(), legData_[i].currency(),
                legData_[i].foreignCurrency(), market, configuration,
                legData_[i].fixingCalendar(), legData_[i].fixingDays());            
        }

        // build the leg

        auto legBuilder = engineFactory->legBuilder(legData_[i].legType());
        legs_[i] = legBuilder->buildLeg(legData_[i], engineFactory, configuration);

        // handle fx resetting Ibor leg

        if (legData_[i].legType() == "Floating" && !legData_[i].isNotResetXCCY()) {
                // this is the reseting leg...
                QL_REQUIRE(fxIndex != nullptr, "fx resetting leg requires fx index");

                // We will add the Libor component of the leg to additionalLegs_ so that we can 
                // capture its fixing dates later. Remove it from nameIndexPairs_ for efficiency.
                string floatIndex = boost::dynamic_pointer_cast<FloatingLegData>(legData_[i].concreteLegData())->index();
                auto nameIndex = make_pair(floatIndex, i);
                QL_REQUIRE(nameIndexPairs_.count(nameIndex) == 1, "Expected floating index '" << floatIndex <<
                    "' on swap's " << io::ordinal(i) << " leg.");
                nameIndexPairs_.erase(nameIndex);

                // We will add the FX linked component of the leg to additionalLegs_ also so erase it here. Do this because 
                // the first coupon on the FX linked floating leg is a simple floating coupon. If we just process it as 
                // normal, we would associate the floating coupon's fixing date with the FX index and ask for an FX fixing 
                // that we do not need. additionalLegs_ for the FX linked floating leg will contain all but the first coupon.
                nameIndex = make_pair(legData_[i].fxIndex(), i);
                QL_REQUIRE(nameIndexPairs_.count(nameIndex) == 1, "Expected FX index '" << legData_[i].fxIndex() <<
                    "' on swap's " << io::ordinal(i) << " leg.");
                nameIndexPairs_.erase(nameIndex);

                // If the domestic notional value is not specified, i.e. there are no notionals specified in the leg
                // data, then all coupons including the first will be FX linked. If the first coupon's FX fixing date
                // is in the past, a FX fixing will be used to determine the first domestic notional. If the first 
                // coupon's FX fixing date is in the future, the first coupon's domestic notional will be determined
                // by the FX forward rate on that future fixing date.
                Size j = 0;
                if (legData_[i].notionals().size() == 0) {
                    DLOG("Building FX Resettable with unspecified domestic notional");
                } else {
                    // First coupon a plain floating rate coupon i.e. it is not FX linked because the initial notional is known.
                    // But, we need to add it to additionalLegs_ so that we don't miss the first coupon's ibor fixing
                    LOG("Building FX Resettable with first domestic notional specified explicitly");
                    additionalLegs_[floatIndex].push_back(legs_[i][0]);
                    j = 1;
                }

                // Make the necessary FX linked floating rate coupons
                for (; j < legs_[i].size(); ++j) {
                    boost::shared_ptr<FloatingRateCoupon> coupon =
                        boost::dynamic_pointer_cast<FloatingRateCoupon>(legs_[i][j]);
                    additionalLegs_[floatIndex].push_back(coupon);

                    Date fixingDate = fxIndex->fixingCalendar().advance(coupon->accrualStartDate(),
                                                                        -static_cast<Integer>(fxIndex->fixingDays()), Days);
                    boost::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fxLinkedCoupon =
                        boost::make_shared<FloatingRateFXLinkedNotionalCoupon>(fixingDate, legData_[i].foreignAmount(),
                                                                               fxIndex, coupon);
                    // set the same pricer
                    fxLinkedCoupon->setPricer(coupon->pricer());
                    legs_[i][j] = fxLinkedCoupon;

                    // Add the FX linked floating coupon to the additionalLegs_
                    additionalLegs_[legData_[i].fxIndex()].push_back(fxLinkedCoupon);
                }
        }

        DLOG("Swap::build(): currency[" << i << "] = " << currencies[i]);

        // If we have an FX resetting leg, add the notional amount at the start and end of each coupon period.
        if (!legData_[i].isNotResetXCCY()) {
            
            DLOG("Building Resetting XCCY Notional leg");
            Real foreignNotional = legData_[i].foreignAmount();
            
            Leg resettingLeg;
            for (Size j = 0; j < legs_[i].size(); j++) {
                
                boost::shared_ptr<Coupon> c = boost::dynamic_pointer_cast<Coupon>(legs_[i][j]);
                QL_REQUIRE(c, "Expected each cashflow in FX resetting leg to be of type Coupon");

                // Build a pair of notional flows, one at the start and one at the end of the accrual period.
                // They both have the same FX fixing date => same amount in this leg's currency.
                boost::shared_ptr<CashFlow> outCf;
                boost::shared_ptr<CashFlow> inCf;
                if (j == 0) {
                    // Two possibilities for first coupon:
                    // 1. we have not been given a domestic notional so it is an FX linked coupon
                    // 2. we have been given an explicit domestic notional so it is a simple cashflow
                    if (legData_[i].notionals().size() == 0) {
                        Date fixingDate = fxIndex->fixingDate(c->accrualStartDate());
                        if (legData_[i].notionalInitialExchange()) {
                            outCf = boost::make_shared<FXLinkedCashFlow>(c->accrualStartDate(), fixingDate,
                                -foreignNotional, fxIndex);
                        }
                        inCf = boost::make_shared<FXLinkedCashFlow>(c->accrualEndDate(), fixingDate,
                            foreignNotional, fxIndex);
                    } else {
                        if (legData_[i].notionalInitialExchange()) {
                            outCf = boost::make_shared<SimpleCashFlow>(-c->nominal(), c->accrualStartDate());
                        }
                        inCf = boost::make_shared<SimpleCashFlow>(c->nominal(), c->accrualEndDate());
                    }
                } else {
                    Date fixingDate = fxIndex->fixingDate(c->accrualStartDate());
                    outCf = boost::make_shared<FXLinkedCashFlow>(c->accrualStartDate(), fixingDate,
                        -foreignNotional, fxIndex);
                    // we don't want a final one, unless there is notional exchange
                    if (j < legs_[i].size() - 1 || legData_[i].notionalFinalExchange()) {
                        inCf = boost::make_shared<FXLinkedCashFlow>(c->accrualEndDate(), fixingDate,
                            foreignNotional, fxIndex);
                    }
                }

                // Add the cashflows to the notional leg if they have been populated
                if (outCf)
                    resettingLeg.push_back(outCf);
                if (inCf)
                    resettingLeg.push_back(inCf);
            }

            legs_.push_back(resettingLeg);
            legPayers_.push_back(legPayers_[i]);
            currencies.push_back(currencies[i]);

            // Add this leg to nameIndexPairs_ also
            nameIndexPairs_.insert(make_pair(legData_[i].fxIndex(), legs_.size() - 1));

            if (legData_[i].notionalAmortizingExchange()) {
                QL_FAIL("Cannot have an amoritizing notional with FX reset");
            }
        }

        // check for notional exchanges on non FX reseting trades
        else if ((legData_[i].notionalInitialExchange() || legData_[i].notionalFinalExchange() ||
                  legData_[i].notionalAmortizingExchange()) &&
                 (legData_[i].legType() != "CPI")) {

            legs_.push_back(makeNotionalLeg(legs_[i], legData_[i].notionalInitialExchange(),
                                            legData_[i].notionalFinalExchange(),
                                            legData_[i].notionalAmortizingExchange()));
            legPayers_.push_back(legPayers_[i]);
            currencies.push_back(currencies[i]);
        }
    } // for legs

    // NPV currency and Current notional taken from the first leg that appears in the XML
    // unless the first leg is a Resettable XCCY, then use the second leg
    // For a XCCY Resettable the currentNotional may fail due missing FX fixing so we avoid
    // using this leg if possible
    // For a equity swap with resetting notional may fail due to missing equity fixing so avoid
    bool isEquityNotionalReset = false;
    if (legData_[0].legType() == "Equity") {
        boost::shared_ptr<EquityLegData> eld = boost::dynamic_pointer_cast<EquityLegData>(
            legData_[0].concreteLegData());
        isEquityNotionalReset = eld->notionalReset();
    }

    if (legData_.size() > 1 && (!legData_[0].isNotResetXCCY() || isEquityNotionalReset)) {
        npvCurrency_ = legData_[1].currency();
        notional_ = currentNotional(legs_[1]);
    } else {
        npvCurrency_ = legData_[0].currency();
        notional_ = currentNotional(legs_[0]);
    }    
    DLOG("Notional is " << notional_ << " " << npvCurrency_);
    Currency npvCcy = parseCurrency(npvCurrency_);

    if (isXCCY) {
        boost::shared_ptr<QuantExt::CurrencySwap> swap(new QuantExt::CurrencySwap(legs_, legPayers_, currencies));
        boost::shared_ptr<CrossCurrencySwapEngineBuilderBase> swapBuilder =
            boost::dynamic_pointer_cast<CrossCurrencySwapEngineBuilderBase>(builder);
        QL_REQUIRE(swapBuilder, "No Builder found for CrossCurrencySwap " << id());
        swap->setPricingEngine(swapBuilder->engine(currencies, npvCcy));
        // take the first legs currency as the npv currency (arbitrary choice)
        instrument_.reset(new VanillaInstrument(swap));
    } else {
        boost::shared_ptr<QuantLib::Swap> swap(new QuantLib::Swap(legs_, legPayers_));
        boost::shared_ptr<SwapEngineBuilderBase> swapBuilder =
            boost::dynamic_pointer_cast<SwapEngineBuilderBase>(builder);
        QL_REQUIRE(swapBuilder, "No Builder found for Swap " << id());
        swap->setPricingEngine(swapBuilder->engine(npvCcy));
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
        QL_REQUIRE(legs_[i].size() > 0, "Leg " << i + 1 << " of " << legs_.size() << " is empty.");
        Date d = legs_[i].back()->date();
        if (d > maturity_)
            maturity_ = d;
    }
}

map<string, set<Date>> Swap::fixings(const Date& settlementDate) const {
    
    map<string, set<Date>> result;
    
    for (const auto& p : nameIndexPairs_) {
        // Get the set of fixing dates on the leg and update the results
        set<Date> dates = fixingDates(legs_[p.second], settlementDate);
        if (!dates.empty()) result[p.first].insert(dates.begin(), dates.end());
    }

    // Deal with any additional legs
    for (const auto& kv : additionalLegs_) {
        // Get the set of fixing dates on the leg and update the results
        set<Date> dates = fixingDates(kv.second, settlementDate);
        if (!dates.empty()) result[kv.first].insert(dates.begin(), dates.end());
    }

    return result;
}

void Swap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    legData_.clear();
    XMLNode* swapNode = XMLUtils::getChildNode(node, "SwapData");
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); i++) {
        auto ld = createLegData();
        ld->fromXML(nodes[i]);
        legData_.push_back(*boost::static_pointer_cast<LegData>(ld));
    }
}

boost::shared_ptr<LegData> Swap::createLegData() const { return boost::make_shared<LegData>(); }

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
