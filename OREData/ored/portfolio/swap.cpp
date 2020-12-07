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
#include <ored/portfolio/legbuilders.hpp>
#include <ored/portfolio/legdata.hpp>
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

    // allow minor currencies in case first leg is equity
    Currency currency = parseCurrencyWithMinors(legData_[0].currency());
    string ccy_str = currency.code();

    Size numLegs = legData_.size();
    legPayers_ = vector<bool>(numLegs);
    std::vector<QuantLib::Currency> currencies(numLegs);
    legs_.resize(numLegs);

    bool isXCCY = false;
    bool isResetting = false;

    for (Size i = 0; i < numLegs; ++i) {
        // allow minor currencies for Equity legs as some exchanges trade in these, e.g LSE in pence - GBX or GBp
        // minor currencies on other legs will fail here
        if (legData_[i].legType() == "Equity")
            currencies[i] = parseCurrencyWithMinors(legData_[i].currency());
        else
            currencies[i] = parseCurrency(legData_[i].currency());
        if (currencies[i] != currency)
            isXCCY = true;
    }

    boost::shared_ptr<EngineBuilder> builder =
        isXCCY ? engineFactory->builder("CrossCurrencySwap") : engineFactory->builder("Swap");
    auto configuration = builder->configuration(MarketContext::pricing);

    for (Size i = 0; i < numLegs; ++i) {
        legPayers_[i] = legData_[i].isPayer();

        boost::shared_ptr<FxIndex> fxIndex;
        if (legData_[i].fxIndex() != "") {
            // We have an FX Index to setup
            fxIndex = buildFxIndex(legData_[i].fxIndex(), legData_[i].currency(), legData_[i].foreignCurrency(), market,
                                   configuration, legData_[i].fixingCalendar(), legData_[i].fixingDays(), true);
        }

        // build the leg

        auto legBuilder = engineFactory->legBuilder(legData_[i].legType());
        legs_[i] = legBuilder->buildLeg(legData_[i], engineFactory, requiredFixings_, configuration);

        // handle fx resetting Ibor leg

        if (legData_[i].legType() == "Floating" && !legData_[i].isNotResetXCCY()) {
            // this is the reseting leg...
            QL_REQUIRE(fxIndex != nullptr, "fx resetting leg requires fx index");

            // If the domestic notional value is not specified, i.e. there are no notionals specified in the leg
            // data, then all coupons including the first will be FX linked. If the first coupon's FX fixing date
            // is in the past, a FX fixing will be used to determine the first domestic notional. If the first
            // coupon's FX fixing date is in the future, the first coupon's domestic notional will be determined
            // by the FX forward rate on that future fixing date.
            Size j = 0;
            if (legData_[i].notionals().size() == 0) {
                DLOG("Building FX Resettable with unspecified domestic notional");
            } else {
                // First coupon a plain floating rate coupon i.e. it is not FX linked because the initial notional is
                // known. But, we need to add it to additionalLegs_ so that we don't miss the first coupon's ibor fixing
                LOG("Building FX Resettable with first domestic notional specified explicitly");
                j = 1;
            }

            // Make the necessary FX linked floating rate coupons
            for (; j < legs_[i].size(); ++j) {
                boost::shared_ptr<FloatingRateCoupon> coupon =
                    boost::dynamic_pointer_cast<FloatingRateCoupon>(legs_[i][j]);

                Date fixingDate = fxIndex->fixingCalendar().advance(coupon->accrualStartDate(),
                                                                    -static_cast<Integer>(fxIndex->fixingDays()), Days);
                boost::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fxLinkedCoupon =
                    boost::make_shared<FloatingRateFXLinkedNotionalCoupon>(fixingDate, legData_[i].foreignAmount(),
                                                                           fxIndex, coupon);
                // set the same pricer
                fxLinkedCoupon->setPricer(coupon->pricer());
                legs_[i][j] = fxLinkedCoupon;

                // Add the FX fixing to the required fixings
                requiredFixings_.addFixingDate(fixingDate, legData_[i].fxIndex(), fxLinkedCoupon->date());
            }
        }

        DLOG("Swap::build(): currency[" << i << "] = " << currencies[i]);

        // If we have an FX resetting leg, add the notional amount at the start and end of each coupon period.
        if (!legData_[i].isNotResetXCCY()) {

            DLOG("Building Resetting XCCY Notional leg");
            Real foreignNotional = legData_[i].foreignAmount();
            isResetting = true;

            Leg resettingLeg;
            for (Size j = 0; j < legs_[i].size(); j++) {

                boost::shared_ptr<Coupon> c = boost::dynamic_pointer_cast<Coupon>(legs_[i][j]);
                QL_REQUIRE(c, "Expected each cashflow in FX resetting leg to be of type Coupon");

                // Build a pair of notional flows, one at the start and one at the end of the accrual period.
                // They both have the same FX fixing date => same amount in this leg's currency.
                boost::shared_ptr<CashFlow> outCf;
                boost::shared_ptr<CashFlow> inCf;
                Date fixingDate;
                if (j == 0) {
                    // Two possibilities for first coupon:
                    // 1. we have not been given a domestic notional so it is an FX linked coupon
                    // 2. we have been given an explicit domestic notional so it is a simple cashflow
                    if (legData_[i].notionals().size() == 0) {
                        fixingDate = fxIndex->fixingDate(c->accrualStartDate());
                        if (legData_[i].notionalInitialExchange()) {
                            outCf = boost::make_shared<FXLinkedCashFlow>(c->accrualStartDate(), fixingDate,
                                                                         -foreignNotional, fxIndex);
                        }
                        inCf = boost::make_shared<FXLinkedCashFlow>(c->accrualEndDate(), fixingDate, foreignNotional,
                                                                    fxIndex);
                    } else {
                        if (legData_[i].notionalInitialExchange()) {
                            outCf = boost::make_shared<SimpleCashFlow>(-c->nominal(), c->accrualStartDate());
                        }
                        inCf = boost::make_shared<SimpleCashFlow>(c->nominal(), c->accrualEndDate());
                    }
                } else {
                    fixingDate = fxIndex->fixingDate(c->accrualStartDate());
                    outCf = boost::make_shared<FXLinkedCashFlow>(c->accrualStartDate(), fixingDate, -foreignNotional,
                                                                 fxIndex);
                    // we don't want a final one, unless there is notional exchange
                    if (j < legs_[i].size() - 1 || legData_[i].notionalFinalExchange()) {
                        inCf = boost::make_shared<FXLinkedCashFlow>(c->accrualEndDate(), fixingDate, foreignNotional,
                                                                    fxIndex);
                    }
                }

                // Add the cashflows to the notional leg if they have been populated
                if (outCf) {
                    resettingLeg.push_back(outCf);
                    if (fixingDate != Date())
                        requiredFixings_.addFixingDate(fixingDate, legData_[i].fxIndex(), outCf->date());
                }
                if (inCf) {
                    resettingLeg.push_back(inCf);
                    if (fixingDate != Date())
                        requiredFixings_.addFixingDate(fixingDate, legData_[i].fxIndex(), inCf->date());
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
        else if ((legData_[i].notionalInitialExchange() || legData_[i].notionalFinalExchange() ||
                  legData_[i].notionalAmortizingExchange()) &&
                 (legData_[i].legType() != "CPI")) {

            legs_.push_back(makeNotionalLeg(
                legs_[i], legData_[i].notionalInitialExchange(), legData_[i].notionalFinalExchange(),
                legData_[i].notionalAmortizingExchange(), parseBusinessDayConvention(legData_[i].paymentConvention()),
                parseCalendar(legData_[i].paymentCalendar())));
            legPayers_.push_back(legPayers_[i]);
            currencies.push_back(currencies[i]);
        }
    } // for legs

    // The npv currency, notional currency and current notional are taken from the first leg that
    // appears in the XML that has a notional. If no such leg exists the notional currency
    // and current notional are left empty and the npv currency is set to the first leg's currency

    Size notionalTakenFromLeg = 0;
    for (; notionalTakenFromLeg < legData_.size(); ++notionalTakenFromLeg) {
        const LegData& d = legData_[notionalTakenFromLeg];
        if (!d.notionals().empty())
            break;
    }

    if (notionalTakenFromLeg == legData_.size()) {
        ALOG("no suitable leg found to set notional, set to null and notionalCurrency to empty string");
        notional_ = Null<Real>();
        notionalCurrency_ = "";
        // parse for currrency in case first leg is Equity, we only want the major currency for NPV
        npvCurrency_ = parseCurrencyWithMinors(legData_.front().currency()).code();
    } else {
        if (legData_[notionalTakenFromLeg].schedule().hasData()) {
            Schedule schedule = makeSchedule(legData_[notionalTakenFromLeg].schedule());
            auto notional =
                buildScheduledVectorNormalised(legData_[notionalTakenFromLeg].notionals(),
                                               legData_[notionalTakenFromLeg].notionalDates(), schedule, 0.0);
            Date today = Settings::instance().evaluationDate();
            auto d = std::upper_bound(schedule.dates().begin(), schedule.dates().end(), today);
            // forward starting => take first notional
            // on or after last schedule date => zero notional
            // in between => notional of current period
            if (d == schedule.dates().begin())
                notional_ = notional.at(0);
            else if (d == schedule.dates().end())
                notional_ = 0.0;
            else
                notional_ = notional.at(std::distance(schedule.dates().begin(), d) - 1);
        } else {
            notional_ = legData_[notionalTakenFromLeg].notionals().at(0);
        }
        // parse for currrency in case leg is Equity, we only want the major currency for NPV and Notional
        notionalCurrency_ = parseCurrencyWithMinors(legData_[notionalTakenFromLeg].currency()).code();
        npvCurrency_ = parseCurrencyWithMinors(legData_[notionalTakenFromLeg].currency()).code();
        DLOG("Notional is " << notional_ << " " << notionalCurrency_);
    }

    Currency npvCcy = parseCurrency(npvCurrency_);
    DLOG("npv currency is " << npvCurrency_);

    if (isXCCY) {
        boost::shared_ptr<QuantExt::CurrencySwap> swap(
            new QuantExt::CurrencySwap(legs_, legPayers_, currencies, settlement_ == "Physical", isResetting));
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
    maturity_ = Date::minDate();
    for (auto const& l : legs_) {
        if (!l.empty())
            maturity_ = std::max(maturity_, l.back()->date());
    }
}

map<AssetClass, set<string>>
Swap::underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager) const {

    map<AssetClass, set<string>> result;
    for (const auto& ld : legData_) {
        for (auto ind : ld.indices()) {
            // only handle equity and commodity for now
            if (ind.substr(0, 5) != "COMM-" && ind.substr(0, 3) != "EQ-")
                continue;

            boost::shared_ptr<Index> index = parseIndex(ind);

            if (auto ei = boost::dynamic_pointer_cast<EquityIndex>(index)) {
                result[AssetClass::EQ].insert(ei->name());
            } else if (auto ci = boost::dynamic_pointer_cast<QuantExt::CommodityIndex>(index)) {
                result[AssetClass::COM].insert(ci->name());
            }
        }
    }

    return result;
}

void Swap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    legData_.clear();
    XMLNode* swapNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    // backwards compatibility
    if (swapNode == nullptr) {
        swapNode = XMLUtils::getChildNode(node, "SwapData");
    }
    QL_REQUIRE(swapNode, "Swap::fromXML(): expected '" << tradeType() << "Data'"
                                                       << (tradeType() == "Swap" ? "" : " or 'SwapData'"));

    settlement_ = XMLUtils::getChildValue(swapNode, "Settlement", false);
    if (settlement_ == "")
        settlement_ = "Physical";

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
    XMLNode* swapNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, swapNode);

    if (settlement_ == "Cash")
        XMLUtils::addChild(doc, swapNode, "Settlement", settlement_);
    for (Size i = 0; i < legData_.size(); i++)
        XMLUtils::appendNode(swapNode, legData_[i].toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
