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
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/simmcurrencies.hpp>
#include <ql/cashflows/simplecashflow.hpp>

#include <ql/indexes/inflation/euhicp.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>
#include <ql/time/calendars/target.hpp>
#include <qle/cashflows/equitycouponpricer.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/currencyswap.hpp>
#include <qle/instruments/swapoptimized.hpp>
#include <qle/utilities/fairrate.hpp>

#include <ql/instruments/swap.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace QuantExt;
using std::make_pair;

namespace ore {
namespace data {

void Swap::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("Swap::build() called for trade " << id());

    setIsdaTaxonomyFields();

    QL_REQUIRE(legData_.size() >= 1, "Swap must have at least 1 leg");
    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();

    // allow minor currencies in case first leg is equity
    Currency currency = parseCurrencyWithMinors(legData_[0].currency());
    string ccy_str = currency.code();

    Size numLegs = legData_.size();
    legPayers_ = vector<bool>(numLegs);
    std::vector<QuantLib::Currency> currencies(numLegs);
    std::vector<QuantLib::Currency> currenciesWithIndexing;
    std::set<std::string> eqNames;
    legs_.resize(numLegs);

    isXCCY_ = false;
    isResetting_ = false;

    for (Size i = 0; i < numLegs; ++i) {
        // allow minor currencies for Equity legs as some exchanges trade in these, e.g LSE in pence - GBX or GBp
        // minor currencies on other legs will fail here
        if (legData_[i].legType() == LegType::Equity)
            currencies[i] = parseCurrencyWithMinors(legData_[i].currency());
        else
            currencies[i] = parseCurrency(legData_[i].currency());

        if (currencies[i] != currency)
            isXCCY_ = true;
        isResetting_ = isResetting_ || (!legData_[i].isNotResetXCCY());
    }

    /* collect currencies from fx indexing and eq names from eq indexing
       note: we do not add ccys from eq here, this requires accessing the market and is left to engine builders */

    auto addUnique = [](vector<Currency>& currencies, Currency ccy) {
        if (std::find(currencies.begin(), currencies.end(), ccy) ==
            currencies.end()) {
            currencies.push_back(ccy);
        }
    };

    for (Size i = 0; i < numLegs; ++i) {
        addUnique(currenciesWithIndexing, currencies[i]);
        vector<Indexing> indexings = legData_[i].indexing();
        if (!indexings.empty() && indexings.front().hasData()) {
            Indexing indexing = indexings.front();
            if (boost::starts_with(indexing.index(), "FX-")) {
                auto index = parseFxIndex(indexing.index());
                addUnique(currenciesWithIndexing, index->targetCurrency());
                addUnique(currenciesWithIndexing, index->sourceCurrency());
            } else if(boost::starts_with(indexing.index(), "EQ-")) {
                auto index = parseEquityIndex(indexing.index());
                eqNames.insert(index->name());
            }
        }
    }

    // identify equity underlyings

    for (Size i = 0; i < numLegs; ++i) {
        if (legData_[i].legType() == LegType::Equity) {
            eqNames.insert(QuantLib::ext::dynamic_pointer_cast<EquityLegData>(legData_[i].concreteLegData())->eqName());
        }
    }

    /* determine whether we have a xccy and whether to use the special xbs curves */
    isXCCY_ = isXCCY_ || currenciesWithIndexing.size() > 1;

    bool useXbsCurves = isSimmEligibleXccySwap(legData_, settlement_);

    std::tie(notionalTakenFromLeg_, notional_, npvCurrency_, notionalCurrency_) = getSwapNpvAndNotionalInfo(legData_);

    Currency npvCcy = parseCurrency(npvCurrency_);
    DLOG("npv currency is " << npvCurrency_);

    QuantLib::ext::shared_ptr<EngineBuilder> builder =
        isXCCY_ ? engineFactory->builder("CrossCurrencySwap") : engineFactory->builder("Swap");
    auto configuration = builder->configuration(MarketContext::pricing);

    for (Size i = 0; i < numLegs; ++i) {
        legPayers_[i] = legData_[i].isPayer();
        auto legBuilder = engineFactory->legBuilder(legData_[i].legType());
        std::set<std::tuple<std::set<std::string>, std::string, std::string>> productModelEngines;
        legs_[i] = legBuilder->buildLeg(legData_[i], engineFactory, requiredFixings_, configuration, Null<Date>(),
                                        useXbsCurves, true, &productModelEngines);
        addProductModelEngine(productModelEngines);
        DLOG("Swap::build(): currency[" << i << "] = " << currencies[i]);

        // add notional leg, if applicable
        auto leg = buildNotionalLeg(legData_[i], legs_[i], requiredFixings_, engineFactory->market(), configuration);
        applyIndexing(leg, legData_[i], engineFactory, requiredFixings_, Null<Date>(), useXbsCurves);
        if (!leg.empty()) {
            legs_.push_back(leg);
            legPayers_.push_back(legPayers_[i]);
            currencies.push_back(currencies[i]);
        }
    } // for legs

    if (isXCCY_) {
        QuantLib::ext::shared_ptr<QuantExt::CurrencySwap> swap(
            new QuantExt::CurrencySwap(legs_, legPayers_, currencies, settlement_ == "Physical", isResetting_));
        QuantLib::ext::shared_ptr<CrossCurrencySwapEngineBuilderBase> swapBuilder =
            QuantLib::ext::dynamic_pointer_cast<CrossCurrencySwapEngineBuilderBase>(builder);
        QL_REQUIRE(swapBuilder,
                   "internal error: engine builder for product type CrossCurrencySwap can not be cast, trade " << id());
        swap->setPricingEngine(
            swapBuilder->engine(currenciesWithIndexing, npvCcy, useXbsCurves, eqNames));
        setSensitivityTemplate(*swapBuilder);
        addProductModelEngine(*swapBuilder);
        // take the first legs currency as the npv currency (arbitrary choice)
        instrument_.reset(new VanillaInstrument(swap));
    } else {
        QuantLib::ext::shared_ptr<SwapEngineBuilderBase> swapBuilder =
            QuantLib::ext::dynamic_pointer_cast<SwapEngineBuilderBase>(builder);
        QL_REQUIRE(swapBuilder, "internal error: engine builder for product type Swap can not be cast, trade " << id());
        QuantLib::ext::shared_ptr<QuantLib::Instrument> swap;
        if(swapBuilder->optimizedInstrument()) {
            auto market = builder->market();
            auto config = builder->configuration(MarketContext::pricing);
            swap = QuantLib::ext::make_shared<SwapOptimized>(legs_, legPayers_,
                                                             market->discountCurve(npvCcy.code(), config));
        } else {
            swap = QuantLib::ext::make_shared<QuantLib::Swap>(legs_, legPayers_);
            swap->setPricingEngine(swapBuilder->engine(npvCcy, envelope().additionalField("discount_curve", false),
                                                       envelope().additionalField("security_spread", false), eqNames));
        }
        setSensitivityTemplate(*swapBuilder);
        addProductModelEngine(*swapBuilder);
        instrument_.reset(new VanillaInstrument(swap));
    }

    // set Leg Currencies
    legCurrencies_ = vector<string>(currencies.size());
    for (Size i = 0; i < currencies.size(); i++)
        legCurrencies_[i] = currencies[i].code();

    // set start and maturity dates
    Date startDate;
    std::tie(startDate, maturity_, maturityType_) = getSwapStartMaturity(legs_);
    additionalData_["startDate"] = to_string(startDate);

    // Store discount curves and FX spots for fair rate calculation
    discountCurves_.resize(numLegs);
    fxSpots_.resize(numLegs);

    if (isXCCY_) {
        // Cross-currency swap: per-leg curves and FX spots
        for (Size i = 0; i < numLegs; ++i) {
            string ccyCode = currencies[i].code();
            if (useXbsCurves) {
                discountCurves_[i] = (xccyYieldCurve)(market, ccyCode, configuration);
            } else {
                discountCurves_[i] = market->discountCurve(ccyCode, configuration);
            }
            // FX spot: convert leg currency to NPV currency
            if (ccyCode == npvCcy.code()) {
                fxSpots_[i] = 1.0;
            } else {
                string pair = ccyCode + npvCcy.code();
                Handle<Quote> fx = market->fxRate(pair, configuration);
                if (fx.empty()) {
                    DLOG("Swap::build(): missing FX spot for pair " << pair << " in configuration " << configuration
                                                                     << ", using 1.0 as placeholder for fair-rate reporting.");
                    fxSpots_[i] = 1.0;
                } else {
                    fxSpots_[i] = fx->value();
                }
            }
        }
    } else {
        // Single-currency swap: same curve for all legs, FX spots = 1.0
        std::string discountCurveName = envelope().additionalField("discount_curve", false);
        std::string securitySpread = envelope().additionalField("security_spread", false);

        Handle<YieldTermStructure> yts =
            discountCurveName.empty() ? market->discountCurve(npvCcy.code(), configuration)
                                      : indexOrYieldCurve(market, discountCurveName, configuration);

        if (!securitySpread.empty()) {
            yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
                yts, market->securitySpread(securitySpread, configuration)));
        }

        for (Size i = 0; i < numLegs; ++i) {
            discountCurves_[i] = yts;
            fxSpots_[i] = 1.0;
        }
    }
}

void Swap::setIsdaTaxonomyFields() {
    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string(isXCCY_ ? "Cross Currency" : "IR Swap");
    additionalData_["isdaSubProduct"] = isdaSubProductSwap(id(), legData_);
    additionalData_["isdaTransaction"] = string("");
}

const std::map<std::string,QuantLib::ext::any>& Swap::additionalData() const {
    Size numLegs = legData_.size();
    // use the build time as of date to determine current notionals
    QuantLib::ext::shared_ptr<QuantLib::Swap> swap = QuantLib::ext::dynamic_pointer_cast<QuantLib::Swap>(instrument_->qlInstrument());
    QuantLib::ext::shared_ptr<QuantExt::CurrencySwap> cswap = QuantLib::ext::dynamic_pointer_cast<QuantExt::CurrencySwap>(instrument_->qlInstrument());
    std::map<std::string, Real> legNpv; // by currency
    for (Size i = 0; i < numLegs; ++i) {
        string legID = to_string(i+1);
        additionalData_["legType[" + legID + "]"] = ore::data::to_string(legData_[i].legType());
        additionalData_["isPayer[" + legID + "]"] = legData_[i].isPayer();
        additionalData_["notionalCurrency[" + legID + "]"] = legData_[i].currency();
        if (!isXCCY_) {
            if (swap) {
                additionalData_["legNPV[" + legID + "]"] = swap->legNPV(i);
                if (legData_[i].legType() == LegType::Fixed) {
                    additionalData_["PV01[" + legID + "]"] = std::abs(swap->legBPS(i));
                }
            } else
                ALOG("single currency swap underlying instrument not set, skip leg npv reporting");
        }
        else {
            if (cswap) {
                // The currency swap has more legs than the swap wrapper (additional notional legs), so aggregate by currency
                Real legNpv = 0;
                Real legNpvInCcy = 0;
                for (Size j = 0; j < cswap->legs().size(); ++j) {
                    if (cswap->legCurrency(j).code() == legData_[i].currency()) {
                        legNpv += cswap->legNPV(j);
                        legNpvInCcy += cswap->inCcyLegNPV(j);
                    }
                }
                additionalData_["legNPV[" + legID + "]"] = legNpv;
                additionalData_["legNPVCCY[" + legID + "]"] = legNpvInCcy;
            }
            else
                ALOG("cross currency swap underlying instrument not set, skip leg npv reporting");
        }
        setLegBasedAdditionalData(i);
    }

    // Compute fair rate using the new utility function
    if (!legs_.empty() && !discountCurves_.empty() && discountCurves_.size() == numLegs) {
        try {
            auto [atmForward, spreadCorrection] = QuantExt::fairRate(
                std::vector<Leg>(legs_.begin(), legs_.begin() + numLegs),
                std::vector<bool>(legPayers_.begin(), legPayers_.begin() + numLegs),
                discountCurves_,
                fxSpots_);
            additionalData_["atmForward"] = atmForward;
            additionalData_["spreadCorrection"] = spreadCorrection;
        } catch (const std::exception& e) {
            DLOG("Could not compute atmForward using fairRate utility: " << e.what());
        }
    }
    return additionalData_;
}

QuantLib::Real Swap::notional() const {
    // try to get the notional from the additional results of the instrument
    try {
        return instrument_->qlInstrument(true)->result<Real>("currentNotional");
    } catch (const std::exception& e) {
        DLOG("swap engine does not provide current notional: " << e.what() << ", using fallback");
        // Try getting current notional from coupons
        if (notionalTakenFromLeg_ < legs_.size()) {
            Real n = currentNotional(legs_[notionalTakenFromLeg_]);
            if (fabs(n) > QL_EPSILON) {
                return n;
            }
        }
        // else return the face value
        DLOG("swap does not provide coupon notionals, using face value");
        return notional_;
    }
}

std::string Swap::notionalCurrency() const {
    // try to get the notional ccy from the additional results of the instrument
    try {
        return instrument_->qlInstrument(true)->result<std::string>("notionalCurrency");
    } catch (const std::exception& e) {
        if (strcmp(e.what(), "notionalCurrency not provided"))
            DLOG("swap engine does not provide notional ccy: " << e.what() << ", using fallback");
        return notionalCurrency_;
    }
}

map<AssetClass, set<string>>
Swap::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return getSwapUnderlyingIndices(referenceDataManager, legData_,
                                    envelope().additionalField("security_spread", false));
}

std::string isdaSubProductSwap(const std::string& tradeId, const vector<LegData>& legData) {

    Size nFixed = 0;
    Size nFloating = 0;
    for (Size i = 0; i < legData.size(); ++i) {
        const LegType& type = legData[i].legType();
        if (type == LegType::Fixed ||
            type == LegType::ZeroCouponFixed ||
            type == LegType::Cashflow ||
            type == LegType::CommodityFixed)
            nFixed++;
        else if (type ==  LegType::Floating ||
                 type ==  LegType::CPI ||
                 type ==  LegType::YY ||
                 type ==  LegType::CMS ||
                 type ==  LegType::DigitalCMS ||
                 type ==  LegType::CMSSpread ||
                 type ==  LegType::DigitalCMSSpread ||
                 type ==  LegType::CMB ||
                 type ==  LegType::Equity ||
                 type ==  LegType::DurationAdjustedCMS ||
                 type ==  LegType::FormulaBased ||
                 type == LegType::CommodityFloating ||
                 type == LegType::EquityMargin ||
                 type == LegType::RangeAccrual)
            nFloating++;
        else {
            ALOG("leg type " << type << " not mapped for trade " << tradeId);
        }
    }

    if (nFixed == 0)
        return string("Basis");
    else if (nFloating >= 1)
        return string("Fixed Float");
    else
        return string("Fixed Fixed");
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
        auto ld = ext::make_shared<LegData>();
        ld->fromXML(nodes[i]);
        legData_.push_back(*QuantLib::ext::static_pointer_cast<LegData>(ld));
    }
}

XMLNode* Swap::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swapNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, swapNode);

    if (settlement_ == "Cash")
        XMLUtils::addChild(doc, swapNode, "Settlement", settlement_);
    for (Size i = 0; i < legData_.size(); i++)
        XMLUtils::appendNode(swapNode, legData_[i].toXML(doc));
    return node;
}

std::tuple<Size, Real, std::string, std::string> getSwapNpvAndNotionalInfo(const std::vector<LegData>& legData) {

    // The npv currency, notional currency and current notional are taken from the first leg that
    // appears in the XML that has a notional. If no such leg exists the notional currency
    // and current notional are left empty and the npv currency is set to the first leg's currency

    Size notionalTakenFromLeg = 0;
    for (; notionalTakenFromLeg < legData.size(); ++notionalTakenFromLeg) {
        const LegData& d = legData[notionalTakenFromLeg];
        if (!d.notionals().empty())
            break;
    }

    Real notional;
    std::string npvCurrency;
    std::string notionalCurrency;

    if (notionalTakenFromLeg == legData.size()) {
        ALOG("no suitable leg found to set notional, set to null and notionalCurrency to empty string");
        notional = Null<Real>();
        notionalCurrency = "";
        // parse for currency in case first leg is Equity, we only want the major currency for NPV
        npvCurrency = parseCurrencyWithMinors(legData.front().currency()).code();
    } else {
        if (legData[notionalTakenFromLeg].schedule().hasData()) {
            Schedule schedule = makeSchedule(legData[notionalTakenFromLeg].schedule());
            auto notionalSchedule =
                buildScheduledVectorNormalised(legData[notionalTakenFromLeg].notionals(),
                                               legData[notionalTakenFromLeg].notionalDates(), schedule, 0.0);
            Date today = Settings::instance().evaluationDate();
            auto d = std::upper_bound(schedule.dates().begin(), schedule.dates().end(), today);
            // forward starting => take first notional
            // on or after last schedule date => zero notional
            // in between => notional of current period
            if (d == schedule.dates().begin())
                notional = notionalSchedule.at(0);
            else if (d == schedule.dates().end())
                notional = 0.0;
            else
                notional = notionalSchedule.at(std::distance(schedule.dates().begin(), d) - 1);
        } else {
            notional = legData[notionalTakenFromLeg].notionals().at(0);
        }
        // parse for currency in case leg is Equity, we only want the major currency for NPV and Notional
        notionalCurrency = parseCurrencyWithMinors(legData[notionalTakenFromLeg].currency()).code();
        npvCurrency = parseCurrencyWithMinors(legData[notionalTakenFromLeg].currency()).code();
    }

    DLOG("Notional is taken from leg " << notionalTakenFromLeg << ", notional is " << notional << ", npvCurrency is "
                                       << npvCurrency << ", notionalCurrency is " << notionalCurrency);
    return std::make_tuple(notionalTakenFromLeg, notional, npvCurrency, notionalCurrency);
}

std::map<AssetClass, std::set<std::string>>
getSwapUnderlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager,
                         const std::vector<LegData>& legData, const std::string& security) {

    map<AssetClass, set<string>> result;
    for (const auto& ld : legData) {
        for (auto ind : ld.indices()) {
            // only handle equity and commodity for now
            if (ind.substr(0, 5) != "COMM-" && ind.substr(0, 3) != "EQ-")
                continue;

            QuantLib::ext::shared_ptr<Index> index = parseIndex(ind);

            if (auto ei = QuantLib::ext::dynamic_pointer_cast<EquityIndex2>(index)) {
                result[AssetClass::EQ].insert(ei->name());
            } else if (auto ci = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityIndex>(index)) {
                result[AssetClass::COM].insert(ci->name());
            }
        }
    }

    if (!security.empty())
        result[AssetClass::BOND] = {security};

    return result;
}

std::tuple<Date, Date, std::string> getSwapStartMaturity(const std::vector<Leg>& legs) {
    Date maturity = Date::minDate();
    Date startDate = Date::maxDate();
    std::string maturityType;
    for (auto const& l : legs) {
        if (!l.empty()) {
            maturity = std::max(maturity, l.back()->date());
            if (maturity == l.back()->date())
                maturityType = "Leg End Date";
            startDate = std::min(startDate, l.front()->date());
            QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(l.front());
            if (coupon)
                startDate = std::min(startDate, coupon->accrualStartDate());
        }
    }
    return std::make_tuple(startDate, maturity, maturityType);
}

//! Return true if the floating leg has at least one cap or floor
bool floatingLegHasCapFloors(const QuantLib::ext::shared_ptr<FloatingLegData>& floatingLegData) {
    QL_REQUIRE(floatingLegData, "internal error: expected floating leg data for floating leg, contact dev.");
    return !floatingLegData->caps().empty() || !floatingLegData->floors().empty();
}

bool legIsSimmEligableXccySwap(const LegData& ld) {
    if (ld.legType() != LegType::Fixed && ld.legType() != LegType::Floating && ld.legType() != LegType::Cashflow)
        return false;
    if (!ld.indexing().empty()) {
        return false;
    }
    if (ld.legType() == LegType::Floating &&
        floatingLegHasCapFloors(QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(ld.concreteLegData()))) {
        return false;
    }
    return true;
}

bool isSimmEligibleXccySwap(const std::vector<LegData>& legData, const std::string& settlement) {
    if (settlement != "Physical") {
        return false;
    }

    std::map<string, bool> legPayerReceiver;
    std::set<string> standardSimCurrencies;
    for (Size i = 0; i < legData.size(); i++) {
        const LegData& ld = legData[i];
        if (!legIsSimmEligableXccySwap(ld))
            return false;
        // check that all legs with the same currency are in the same direction
        const string& ccy = ld.currency();
        auto payerReceiverIt = legPayerReceiver.find(ccy);
        if (payerReceiverIt == legPayerReceiver.end()) {
            legPayerReceiver[ccy] = ld.isPayer();
        } else if (payerReceiverIt->second != ld.isPayer()) {
            return false;
        }
        standardSimCurrencies.insert(ore::data::isUnidadeCurrency(ccy) ? ore::data::simmStandardCurrency(ccy) : ccy);
    }
    // Dont allow three raw currencies even if two of them are the same standard SIMM currency
    return legPayerReceiver.size() == 2 && standardSimCurrencies.size() == 2;
}

} // namespace data
} // namespace ore
