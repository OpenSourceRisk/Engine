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

#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/cashflows/equitycouponpricer.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/durationadjustedcmscoupon.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/typedcashflow.hpp>
#include <qle/instruments/cashflowresults.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>

namespace ore {
namespace data {
using ore::data::XMLUtils;
using namespace QuantExt;
using namespace QuantExt;

void populateReportDataFromAdditionalResults(
    std::vector<TradeCashflowReportData>& result, std::map<Size, Size>& cashflowNumber,
    const std::map<std::string, boost::any>& addResults, const Real multiplier, const std::string& baseCurrency,
    const std::string& npvCurrency, QuantLib::ext::shared_ptr<ore::data::Market> market, 
    const Handle<YieldTermStructure>& specificDiscountCurve, const std::string& configuration, const bool includePastCashflows) {

    Date asof = Settings::instance().evaluationDate();

    // ensures all cashFlowResults from composite trades are being accounted for
    auto lower = addResults.lower_bound("cashFlowResults");
    auto upper = addResults.lower_bound("cashFlowResults_a"); // Upper bound due to alphabetical order

    for (auto cashFlowResults = lower; cashFlowResults != upper; ++cashFlowResults) {

        QL_REQUIRE(cashFlowResults->second.type() == typeid(std::vector<CashFlowResults>),
                   "internal error: cashflowResults type does not match CashFlowResults: '"
                       << cashFlowResults->second.type().name() << "'");
        std::vector<CashFlowResults> cfResults = boost::any_cast<std::vector<CashFlowResults>>(cashFlowResults->second);

        for (auto const& cf : cfResults) {

            Real effectiveAmount = Null<Real>();
            Real discountFactor = Null<Real>();
            Real presentValue = Null<Real>();
            Real presentValueBase = Null<Real>();
            Real fxRateLocalBase = Null<Real>();
            Real floorStrike = Null<Real>();
            Real capStrike = Null<Real>();
            Real floorVolatility = Null<Real>();
            Real capVolatility = Null<Real>();
            Real effectiveFloorVolatility = Null<Real>();
            Real effectiveCapVolatility = Null<Real>();

            string ccy;
            if (!cf.currency.empty()) {
                ccy = cf.currency;
            } else {
                ccy = npvCurrency;
            }

            if (cf.amount != Null<Real>())
                effectiveAmount = cf.amount * multiplier;
            if (cf.discountFactor != Null<Real>())
                discountFactor = cf.discountFactor;
            else if (!ccy.empty() && cf.payDate != Null<Date>() && market) {
                auto discountCurve =
                    specificDiscountCurve.empty() ? market->discountCurve(ccy, configuration) : specificDiscountCurve;
                discountFactor = cf.payDate < asof ? 0.0 : discountCurve->discount(cf.payDate);
            }
            if (cf.presentValue != Null<Real>()) {
                presentValue = cf.presentValue * multiplier;
            } else if (effectiveAmount != Null<Real>() && discountFactor != Null<Real>()) {
                presentValue = effectiveAmount * discountFactor;
            }
            if (cf.fxRateLocalBase != Null<Real>()) {
                fxRateLocalBase = cf.fxRateLocalBase;
            } else if (!ccy.empty() && market) {
                try {
                    fxRateLocalBase = market->fxRate(ccy + baseCurrency, configuration)->value();
                } catch (...) {
                }
            }
            if (cf.presentValueBase != Null<Real>()) {
                presentValueBase = cf.presentValueBase;
            } else if (presentValue != Null<Real>() && fxRateLocalBase != Null<Real>()) {
                presentValueBase = presentValue * fxRateLocalBase;
            }
            if (cf.floorStrike != Null<Real>())
                floorStrike = cf.floorStrike;
            if (cf.capStrike != Null<Real>())
                capStrike = cf.capStrike;
            if (cf.floorVolatility != Null<Real>())
                floorVolatility = cf.floorVolatility;
            if (cf.capVolatility != Null<Real>())
                capVolatility = cf.capVolatility;
            if (cf.effectiveFloorVolatility != Null<Real>())
                floorVolatility = cf.effectiveFloorVolatility;
            if (cf.effectiveCapVolatility != Null<Real>())
                capVolatility = cf.effectiveCapVolatility;

            // to be consistent with the leg-based cf report we should do this:
            // if (!includePastCashflows && cf.payDate <= asof)
            //     continue;
            // however, this changes a lot of results, so we output all cfs for the time being

            result.emplace_back();
            result.back().cashflowNo = ++cashflowNumber[cf.legNumber];
            result.back().legNo = cf.legNumber;
            result.back().payDate = cf.payDate;
            result.back().flowType = cf.type;
            result.back().amount = effectiveAmount;
            result.back().currency = ccy;
            result.back().coupon = cf.rate;
            result.back().accrual = cf.accrualPeriod;
            result.back().accrualStartDate = cf.accrualStartDate;
            result.back().accrualEndDate = cf.accrualEndDate;
            result.back().accruedAmount = cf.accruedAmount * (cf.accruedAmount == Null<Real>() ? 1.0 : multiplier);
            result.back().fixingDate = cf.fixingDate;
            result.back().fixingValue = cf.fixingValue;
            result.back().notional = cf.notional * (cf.notional == Null<Real>() ? 1.0 : multiplier);
            result.back().discountFactor = discountFactor;
            result.back().presentValue = presentValue;
            result.back().fxRateLocalBase = fxRateLocalBase;
            result.back().presentValueBase = presentValueBase;
            result.back().baseCurrency = baseCurrency;
            result.back().floorStrike = floorStrike;
            result.back().capStrike = capStrike;
            result.back().floorVolatility = floorVolatility;
            result.back().capVolatility = capVolatility;
            result.back().effectiveFloorVolatility = effectiveFloorVolatility;
            result.back().effectiveCapVolatility = effectiveCapVolatility;
        }
    }
}

void Trade::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Trade");
    tradeType_ = XMLUtils::getChildValue(node, "TradeType", true);
    if (XMLNode* envNode = XMLUtils::getChildNode(node, "Envelope")) {
        envelope_.fromXML(envNode);
    }
    tradeActions_.clear();
    XMLNode* taNode = XMLUtils::getChildNode(node, "TradeActions");
    if (taNode)
        tradeActions_.fromXML(taNode);
}

XMLNode* Trade::toXML(XMLDocument& doc) const {
    // Crete Trade Node with Id attribute.
    XMLNode* node = doc.allocNode("Trade");
    QL_REQUIRE(node, "Failed to create trade node");
    XMLUtils::addAttribute(doc, node, "id", id_);
    XMLUtils::addChild(doc, node, "TradeType", tradeType_);
    XMLUtils::appendNode(node, envelope_.toXML(doc));
    if (!tradeActions_.empty())
        XMLUtils::appendNode(node, tradeActions_.toXML(doc));
    return node;
}

Date Trade::addPremiums(std::vector<QuantLib::ext::shared_ptr<Instrument>>& addInstruments, std::vector<Real>& addMultipliers,
                        const Real tradeMultiplier, const PremiumData& premiumData, const Real premiumMultiplier,
                        const Currency& tradeCurrency, const string& discountCurve, const QuantLib::ext::shared_ptr<EngineFactory>& factory,
                        const string& configuration) {

    Date latestPremiumPayDate = Date::minDate();

    for (auto const& d : premiumData.premiumData()) {
        QL_REQUIRE(d.amount != Null<Real>(), "Trade contains invalid premium data.");

        Currency premiumCurrency = parseCurrencyWithMinors(d.ccy);
        Real premiumAmount = convertMinorToMajorCurrency(d.ccy, d.amount);
        Currency payCurrency = d.payCurrency.empty() ? premiumCurrency : parseCurrencyWithMinors(d.payCurrency);
        QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex;
        std::optional<QuantLib::Date> fixingDate;
        if (payCurrency != premiumCurrency) {
            QL_REQUIRE(!d.fxIndex.empty(), "Trade contains premium data with premium currency " << premiumCurrency
                                                  << " and cash settlement payment currency " << payCurrency
                                                  << ", but no FX index is provided for conversion.");
            auto ind = parseFxIndex(d.fxIndex);
            fxIndex = buildFxIndex(d.fxIndex, payCurrency.code(), premiumCurrency.code(), factory->market(),
                                   configuration, true);
            if (!d.fixingDate.empty()) {
                fixingDate = parseDate(d.fixingDate);
            }
        }
        auto fee = QuantLib::ext::make_shared<QuantExt::Payment>(premiumAmount, premiumCurrency, d.payDate, payCurrency,
                                                                 fxIndex, fixingDate);
        addMultipliers.push_back(premiumMultiplier);

        std::string premiumSettlementCurrency = payCurrency.code();

        Handle<YieldTermStructure> yts = discountCurve.empty()
            ? factory->market()->discountCurve(premiumSettlementCurrency, configuration)
            : indexOrYieldCurve(factory->market(), discountCurve, configuration);
        Handle<Quote> fx;
        DLOG("Premium Discounting currency is " << premiumSettlementCurrency
                                                  << ", trade currency is " << tradeCurrency.code()
                                                  << ", configuration is " << configuration);
        
        // If the premium settlement currency is different from the trade currency, we need to get the FX rate
        // for the premium settlement currency to the trade npvcurrency.                                                  
        if (tradeCurrency.code() != premiumSettlementCurrency) {
            fx = factory->market()->fxRate(premiumSettlementCurrency + tradeCurrency.code(), configuration);
        }
        QuantLib::ext::shared_ptr<PricingEngine> discountingEngine(new QuantExt::PaymentDiscountingEngine(yts, fx));
        fee->setPricingEngine(discountingEngine);

        // 1) Add to additional instruments for pricing
        addInstruments.push_back(fee);

        // 2) Add a trade leg for cash flow reporting, divide the amount by the multiplier, because the leg entries
        //    are multiplied with the trade multiplier in the cashflow report (and if used elsewhere)
        if (premiumCurrency != payCurrency) {
            auto fxFixingDate = fixingDate ? fixingDate.value() : fxIndex->fixingDate(fee->cashFlow()->date());
            legs_.push_back(Leg(1, QuantLib::ext::make_shared<QuantExt::FXLinkedTypedCashFlow>(
                                       fee->cashFlow()->date(), fxFixingDate,
                                       fee->cashFlow()->amount() * premiumMultiplier / tradeMultiplier, fxIndex,
                                       QuantExt::TypedCashFlow::Type::Premium)));
        } else {
            legs_.push_back(Leg(1, QuantLib::ext::make_shared<QuantExt::TypedCashFlow>(
                                       fee->cashFlow()->amount() * premiumMultiplier / tradeMultiplier,
                                       fee->cashFlow()->date(), QuantExt::TypedCashFlow::Type::Premium)));
        }
        legCurrencies_.push_back(fee->currency().code());
        // premium * premiumMultiplier reflects the correct pay direction, set payer to false therefore
        legPayers_.push_back(false);

        legCashflowInclusion_[legs_.size() - 1] = Trade::LegCashflowInclusion::Always;

        // update latest premium pay date
        latestPremiumPayDate = std::max(latestPremiumPayDate, d.payDate);

        DLOG("added fee " << d.amount << " " << d.ccy << " payable on " << d.payDate << " to trade");
    }

    return latestPremiumPayDate;
}

void Trade::validate() const {
    QL_REQUIRE(id_ != "", "Trade id has not been set.");
    QL_REQUIRE(tradeType_ != "", "Trade id has not been set.");
    QL_REQUIRE(instrument_ || legs_.size() > 0,
               "Trade " << id_ << " requires either QuantLib instruments or legs to be created.");
    QL_REQUIRE(npvCurrency_ != "", "NPV currency has not been set for trade " << id_ << ".");
    // QL_REQUIRE(notional_ != Null<Real>(), "Notional has not been set for trade " << id_ << ".");
    // QL_REQUIRE(notionalCurrency_ != "", "Notional currency has not been set for trade " << id_ << ".");
    QL_REQUIRE(maturity_ != Null<Date>(), "Maturity not set for trade " << id_ << ".");    
    QL_REQUIRE(envelope_.initialized(), "Envelope not set for trade " << id_ << ".");
    if (legs_.size() > 0) {
        QL_REQUIRE(legs_.size() == legPayers_.size(),
                   "Inconsistent number of pay/receive indicators for legs in trade " << id_ << ".");
        QL_REQUIRE(legs_.size() == legCurrencies_.size(),
                   "Inconsistent number of leg currencies for legs in trade " << id_ << ".");
    }
}

void Trade::setEnvelope(const Envelope& envelope) {
    envelope_ = envelope;
}

void Trade::setAdditionalData(const std::map<std::string, boost::any>& additionalData) {
    additionalData_ = additionalData;
}

void Trade::reset() {
    // save accumulated timings from wrapper to trade before resetting
    if (instrument_ != nullptr) {
        savedNumberOfPricings_ += instrument_->getNumberOfPricings();
        savedCumulativePricingTime_ += instrument_->getCumulativePricingTime();
    }
    // reset members
    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>();
    legs_.clear();
    legCurrencies_.clear();
    legPayers_.clear();
    npvCurrency_.clear();
    notional_ = Null<Real>();
    notionalCurrency_.clear();
    legCashflowInclusion_.clear();
    maturity_ = Date();
    maturityType_.clear();
    issuer_.clear();
    requiredFixings_.clear();
    sensitivityTemplate_.clear();
    sensitivityTemplateSet_ = false;
    productModelEngine_.clear();
    additionalData_.clear();
}
    
const std::map<std::string, boost::any>& Trade::additionalData() const { return additionalData_; }

void Trade::setLegBasedAdditionalData(const Size i, Size resultLegId) const {
    if (legs_.size() < i + 1)
        return;
    Date asof = Settings::instance().evaluationDate();
    string legID = std::to_string(resultLegId == Null<Size>() ? i + 1 : resultLegId);
    for (Size j = 0; j < legs_[i].size(); ++j) {
        QuantLib::ext::shared_ptr<CashFlow> flow = legs_[i][j];
        QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(flow);
        // pick flow with the earliest future accrual period end date on this leg
        if (coupon->accrualEndDate() > asof) {
            Real flowAmount = 0.0;
            try {
                flowAmount = flow->amount();
            } catch (std::exception& e) {
                ALOG("flow amount could not be determined for trade " << id() << ", set to zero: " << e.what());
            }
            additionalData_["amount[" + legID + "]"] = flowAmount;
            additionalData_["paymentDate[" + legID + "]"] = ore::data::to_string(flow->date());
            //QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(flow);
            if (coupon) {
                Real currentNotional = 0;
                try {
                    currentNotional = coupon->nominal();
                } catch (std::exception& e) {
                    ALOG("current notional could not be determined for trade " << id()
                                                                               << ", set to zero: " << e.what());
                }
                additionalData_["currentNotional_boop![" + legID + "]"] = currentNotional;

                Real rate = 0;
                try {
                    rate = coupon->rate();
                } catch (std::exception& e) {
                    ALOG("coupon rate could not be determined for trade " << id() << ", set to zero: " << e.what());
                }
                additionalData_["rate[" + legID + "]"] = rate;

                if (auto frc = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(flow)) {
                    additionalData_["index[" + legID + "]"] = frc->index()->name();
                    additionalData_["spread[" + legID + "]"] = frc->spread();
                }

                if (auto eqc = QuantLib::ext::dynamic_pointer_cast<QuantExt::EquityCoupon>(flow)) {
                    auto arc = eqc->pricer()->additionalResultCache();
                    additionalData_["currentPeriodStartPrice[" + legID + "]"] = arc.currentPeriodStartPrice;
                    additionalData_["endEquityFixing[" + legID + "]"] = arc.endFixing;
                    if (arc.startFixing != Null<Real>())
                        additionalData_["startEquityFixing[" + legID + "]"] = arc.startFixing;
                    if (arc.dividendFactor != Null<Real>())
                        additionalData_["dividendFactor[" + legID + "]"] = arc.dividendFactor;
                    if (arc.startFixingTotal != Null<Real>())
                        additionalData_["startEquityFixingTotal[" + legID + "]"] =
                            arc.startFixingTotal;
                    if (arc.endFixingTotal != Null<Real>())
                        additionalData_["endEquityFixingTotal[" + legID + "]"] = arc.endFixingTotal;
                    if (arc.currentPeriodStartFxFixing != Null<Real>())
                        additionalData_["currentPeriodStartFxFixing[" + legID + "]"] = arc.currentPeriodStartFxFixing;
                    if (arc.currentPeriodEndFxFixing != Null<Real>())
                        additionalData_["currentPeriodEndFxFixing[" + legID + "]"] = arc.currentPeriodEndFxFixing;
                    if (arc.pastDividends != Null<Real>())
                        additionalData_["pastDividends[" + legID + "]"] = arc.pastDividends;
                    if (arc.forecastDividends != Null<Real>())
                        additionalData_["forecastDividends[" + legID + "]"] = arc.forecastDividends;
                }

                if (auto cpic = QuantLib::ext::dynamic_pointer_cast<QuantExt::CPICoupon>(flow)) {
                    Real baseCPI;
                    baseCPI = cpic->baseCPI();
                    if (baseCPI == Null<Real>()) {
                        try {
                            baseCPI =
                                QuantLib::CPI::laggedFixing(cpic->cpiIndex(), cpic->baseDate() + cpic->observationLag(),
                                                            cpic->observationLag(), cpic->observationInterpolation());
                        } catch (std::exception&) {
                            ALOG("CPICoupon baseCPI could not be interpolated for additional results for trade " << id()
                                                                                                             << ".")
                        }
                    }

                    additionalData_["baseCPI[" + legID + "]"] = baseCPI;
                } else if (auto cpicf = QuantLib::ext::dynamic_pointer_cast<QuantLib::CPICashFlow>(flow)) {
                    Real baseCPI;
                    baseCPI = cpicf->baseFixing();
                    if (baseCPI == Null<Real>()) {
                        try {
                            baseCPI = QuantLib::CPI::laggedFixing(cpicf->cpiIndex(),
                                                                  cpicf->baseDate() + cpicf->observationLag(),
                                                                  cpicf->observationLag(), cpicf->interpolation());
                        } catch (std::exception&) {
                            ALOG("CPICashFlow baseCPI could not be interpolated for additional results for trade " << id()
                                                                                                               << ".")
                        }
                    }

                    additionalData_["baseCPI[" + legID + "]"] = baseCPI;
                }
            }
            break;
        }
    }
    if (legs_[i].size() > 0) {
        QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(legs_[i][0]);
        if (coupon) {
            Real originalNotional = 0.0;
            try {
                originalNotional = coupon->nominal();
            } catch (std::exception& e) {
                ALOG("original nominal could not be determined for trade " << id() << ", set to zero: " << e.what());
            }
            additionalData_["originalNotional[" + legID + "]"] = originalNotional;
            if (auto eqc = QuantLib::ext::dynamic_pointer_cast<QuantExt::EquityCoupon>(coupon)) {
                Real quantity = eqc->quantity();
                if (quantity == Null<Real>()) {
                    if (eqc->legInitialNotional() != Null<Real>() && eqc->initialPrice() != Null<Real>()) {
                        quantity = eqc->legInitialNotional() / eqc->initialPrice();
                    }
                }

                additionalData_[(eqc->notionalReset() ? "quantity[" : "initialQuantity[") + legID + "]"] = quantity;

                Real currentPrice = Null<Real>();
                if (eqc->equityCurve()->isValidFixingDate(asof)) {
                    currentPrice = eqc->equityCurve()->equitySpot()->value();
                }
                if (currentPrice != Null<Real>() && originalNotional != Null<Real>() && !eqc->notionalReset()) {
                    additionalData_["currentQuantity" + legID + "]"] = originalNotional / currentPrice;
                }
            }
        }
    }
    for (Size j = 0; j < legs_[i].size(); ++j) {
        QuantLib::ext::shared_ptr<CashFlow> flow = legs_[i][j];
        if (flow->date() > asof) {
            Size k = 0;
            for (auto const& [fixingDate, index, multiplier] :
                 QuantExt::getIndexedCouponOrCashFlowFixingDetails(flow)) {
                auto label = "[" + legID + "][" + std::to_string(j) + "][" + std::to_string(k) + "]";
                additionalData_["indexingFixingDate" + label] = fixingDate;
                additionalData_["indexingIndex" + label] =
                    index == nullptr ? "na" : IndexNameTranslator::instance().oreName(index->name());
                additionalData_["indexingMultiplier" + label] = multiplier;
            }
        }
    }
}

void Trade::setSensitivityTemplate(const EngineBuilder& builder) {
    sensitivityTemplate_ = builder.engineParameter("SensitivityTemplate", {}, false, std::string());
    sensitivityTemplateSet_ = true;
}

void Trade::setSensitivityTemplate(const std::string& id) {
    sensitivityTemplate_ = id;
    sensitivityTemplateSet_ = true;
}

const std::string& Trade::sensitivityTemplate() const {
    if (!sensitivityTemplateSet_) {
        StructuredTradeWarningMessage(
            id(), tradeType(), "No valid sensitivty template.",
            "Either build() was not called, or the trade builder did not set the sensitivity template.")
            .log();
    }
    return sensitivityTemplate_;
}

const std::set<std::tuple<std::set<std::string>, std::string, std::string>>& Trade::productModelEngine() const {
    return productModelEngine_;
}

void Trade::addProductModelEngine(const EngineBuilder& builder) {
    productModelEngine_.insert(std::make_tuple(builder.tradeTypes(), builder.model(), builder.engine()));
    updateProductModelEngineAdditionalData();
}

void Trade::addProductModelEngine(
    const std::set<std::tuple<std::set<std::string>, std::string, std::string>>& productModelEngine) {
    productModelEngine_.insert(productModelEngine.begin(), productModelEngine.end());
    updateProductModelEngineAdditionalData();
}

void Trade::updateProductModelEngineAdditionalData() {
    Size counter = 0;
    for (auto const& [p, m, e] : productModelEngine_) {
        std::string suffix = productModelEngine_.size() > 1 ? "[" + std::to_string(counter) + "]" : std::string();
        if (p.size() == 1) {
            additionalData_["PricingConfigProductType" + suffix] = *p.begin();
        } else {
            additionalData_["PricingConfigProductType" + suffix] = std::vector<std::string>(p.begin(), p.end());
        }
        additionalData_["PricingConfigModel" + suffix] = m;
        additionalData_["PricingConfigEngine" + suffix] = e;
        ++counter;
    }
}

std::vector<TradeCashflowReportData> Trade::cashflows(const std::string& baseCurrency,
    const QuantLib::ext::shared_ptr<ore::data::Market>& market,
    const std::string& configuration,
    const bool includePastCashflows) const {
    std::vector<TradeCashflowReportData> result;

    Date asof = Settings::instance().evaluationDate();

    string specificDiscountStr = envelope().additionalField("discount_curve", false);
    Handle<YieldTermStructure> specificDiscountCurve;
    if (!specificDiscountStr.empty() && market)
        specificDiscountCurve = indexOrYieldCurve(market, specificDiscountStr, configuration);

    Real multiplier = instrument()->multiplier() * instrument()->multiplier2();

    // add cashflows from (ql-) additional results in instrument and additional instruments

    std::map<Size, Size> cashflowNumber;

    populateReportDataFromAdditionalResults(result, cashflowNumber, instrument()->additionalResults(),
                                            multiplier, baseCurrency, npvCurrency(),
                                            market, specificDiscountCurve, configuration, includePastCashflows);

    for (std::size_t i = 0; i < instrument()->additionalInstruments().size(); ++i) {
        populateReportDataFromAdditionalResults(
            result, cashflowNumber, instrument()->additionalInstruments()[i]->additionalResults(),
            instrument()->additionalMultipliers()[i], baseCurrency, npvCurrency(),
            market, specificDiscountCurve, configuration, includePastCashflows);
    }

    // determine offset for leg numbering to avoid conflicting leg numbers from add results and leg-based results

    Size legNoOffset = 0;
    if (auto l = std::max_element(
            result.begin(), result.end(),
            [](const TradeCashflowReportData& d1, const TradeCashflowReportData& d2) { return d1.legNo < d2.legNo; });
        l != result.end()) {
        legNoOffset = l->legNo;
    }

    // add cashflows from trade legs, if no cashflows were added so far or if a leg is marked as mandatory for cashflows

    bool haveEngineCashflows = !result.empty();
    for (size_t i = 0; i < legs().size(); i++) {
        Trade::LegCashflowInclusion cashflowInclusion = Trade::LegCashflowInclusion::IfNoEngineCashflows;
        if (auto incl = legCashflowInclusion().find(i); incl != legCashflowInclusion().end()) {
            cashflowInclusion = incl->second;
        }

        if (cashflowInclusion == Trade::LegCashflowInclusion::Never ||
            (cashflowInclusion == Trade::LegCashflowInclusion::IfNoEngineCashflows && haveEngineCashflows))
            continue;

        const QuantLib::Leg& leg = legs()[i];
        bool payer = legPayers()[i];

        string ccy = legCurrencies()[i];

        Handle<YieldTermStructure> discountCurve = specificDiscountCurve;
        if (discountCurve.empty() && market) {
            discountCurve = market->discountCurve(ccy, configuration);
        }

        for (size_t j = 0; j < leg.size(); j++) {
            QuantLib::ext::shared_ptr<QuantLib::CashFlow> ptrFlow = leg[j];
            Date payDate = ptrFlow->date();
            if (!ptrFlow->hasOccurred(asof) || includePastCashflows) {
                Real amount = ptrFlow->amount();
                string flowType = "";
                if (payer)
                    amount *= -1.0;
                std::string ccy = legCurrencies()[i];

                QuantLib::ext::shared_ptr<QuantLib::Coupon> ptrCoupon =
                    QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantExt::CommodityCashFlow> ptrCommCf =
                    QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantExt::TypedCashFlow> ptrTypedCf =
                    QuantLib::ext::dynamic_pointer_cast<QuantExt::TypedCashFlow>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantExt::FXLinkedTypedCashFlow> ptrFxlTypedCf =
                    QuantLib::ext::dynamic_pointer_cast<QuantExt::FXLinkedTypedCashFlow>(ptrFlow);

                Real coupon;
                Real accrual;
                Real notional;
                Date accrualStartDate, accrualEndDate;
                Real accruedAmount;

                if (ptrCoupon) {
                    coupon = ptrCoupon->rate();
                    accrual = ptrCoupon->accrualPeriod();
                    notional = ptrCoupon->nominal();
                    accrualStartDate = ptrCoupon->accrualStartDate();
                    accrualEndDate = ptrCoupon->accrualEndDate();
                    accruedAmount = ptrCoupon->accruedAmount(asof);
                    if (payer)
                        accruedAmount *= -1.0;
                    flowType = "Interest";
                } else if (ptrCommCf) {
                    coupon = Null<Real>();
                    accrual = Null<Real>();
                    notional = ptrCommCf->periodQuantity(); // this is measured in units, e.g. barrels for oil
                    accrualStartDate = accrualEndDate = Null<Date>();
                    accruedAmount = Null<Real>();
                    flowType = "Notional (units)";
                } else if (ptrTypedCf) {
                    coupon = Null<Real>();
                    accrual = Null<Real>();
                    notional = Null<Real>();
                    accrualStartDate = accrualEndDate = Null<Date>();
                    accruedAmount = Null<Real>();
                    flowType = ore::data::to_string(ptrTypedCf->type());
                } else if (ptrFxlTypedCf) {
                    coupon = Null<Real>();
                    accrual = Null<Real>();
                    notional = Null<Real>();
                    accrualStartDate = accrualEndDate = Null<Date>();
                    accruedAmount = Null<Real>();
                    flowType = ore::data::to_string(ptrFxlTypedCf->type());
                } else {
                    coupon = Null<Real>();
                    accrual = Null<Real>();
                    notional = Null<Real>();
                    accrualStartDate = accrualEndDate = Null<Date>();
                    accruedAmount = Null<Real>();
                    flowType = "Notional";
                }

                if (auto cpn = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(ptrFlow)) {
                    ptrFlow = unpackIndexedCoupon(cpn);
                }

                QuantLib::ext::shared_ptr<QuantLib::FloatingRateCoupon> ptrFloat =
                    QuantLib::ext::dynamic_pointer_cast<QuantLib::FloatingRateCoupon>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantLib::InflationCoupon> ptrInfl =
                    QuantLib::ext::dynamic_pointer_cast<QuantLib::InflationCoupon>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantLib::IndexedCashFlow> ptrIndCf =
                    QuantLib::ext::dynamic_pointer_cast<QuantLib::IndexedCashFlow>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantExt::FXLinkedCashFlow> ptrFxlCf =
                    QuantLib::ext::dynamic_pointer_cast<QuantExt::FXLinkedCashFlow>(ptrFlow);
                QuantLib::ext::shared_ptr<QuantExt::EquityCoupon> ptrEqCp =
                    QuantLib::ext::dynamic_pointer_cast<QuantExt::EquityCoupon>(ptrFlow);

                Date fixingDate;
                Real fixingValue = Null<Real>();
                if (ptrFloat) {
                    fixingDate = ptrFloat->fixingDate();
                    if (fixingDate > asof)
                        flowType = "InterestProjected";

                    try {
                        fixingValue = ptrFloat->index()->fixing(fixingDate);
                    } catch (...) {
                    }

                    if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantLib::IborCoupon>(ptrFloat)) {
                        fixingValue = (c->rate() - c->spread()) / c->gearing();
                    }

                    if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantLib::CappedFlooredIborCoupon>(ptrFloat)) {
                        fixingValue =
                            (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
                    }

                    if (auto sc =
                            QuantLib::ext::dynamic_pointer_cast<QuantLib::StrippedCappedFlooredCoupon>(ptrFloat)) {
                        if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantLib::CappedFlooredIborCoupon>(
                                sc->underlying())) {
                            fixingValue =
                                (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
                        }
                    }

                    // for (capped-floored) BMA / ON / subperiod coupons the fixing value is the
                    // compounded / averaged rate, not a single index fixing

                    if (auto on = QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(ptrFloat)) {
                        fixingValue = (on->rate() - on->spread()) / on->gearing();
                    } else if (auto on =
                                   QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(ptrFloat)) {
                        fixingValue = (on->rate() - on->effectiveSpread()) / on->gearing();
                    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(ptrFloat)) {
                        fixingValue = (c->rate() - c->spread()) / c->gearing();
                    } else if (auto c =
                                   QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredAverageONIndexedCoupon>(
                                       ptrFloat)) {
                        fixingValue =
                            (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
                    } else if (auto c =
                                   QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(
                                       ptrFloat)) {
                        fixingValue =
                            (c->underlying()->rate() - c->underlying()->effectiveSpread()) / c->underlying()->gearing();
                    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantExt::CappedFlooredAverageBMACoupon>(
                                   ptrFloat)) {
                        fixingValue =
                            (c->underlying()->rate() - c->underlying()->spread()) / c->underlying()->gearing();
                    } else if (auto sp = QuantLib::ext::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(ptrFloat)) {
                        fixingValue = (sp->rate() - sp->spread()) / sp->gearing();
                    }
                } else if (ptrInfl) {
                    fixingDate = ptrInfl->fixingDate();
                    fixingValue = ptrInfl->indexFixing();
                    flowType = "Inflation";
                } else if (ptrIndCf) {
                    fixingDate = ptrIndCf->fixingDate();
                    fixingValue = ptrIndCf->indexFixing();
                    flowType = "Index";
                } else if (ptrFxlCf) {
                    fixingDate = ptrFxlCf->fxFixingDate();
                    fixingValue = ptrFxlCf->fxRate();
                } else if (ptrEqCp) {
                    fixingDate = ptrEqCp->fixingEndDate();
                    fixingValue = ptrEqCp->equityCurve()->fixing(fixingDate);
                } else if (ptrCommCf) {
                    fixingDate = ptrCommCf->lastPricingDate();
                    fixingValue = ptrCommCf->fixing();
                } else {
                    fixingDate = Null<Date>();
                    fixingValue = Null<Real>();
                }

                Real effectiveAmount = Null<Real>();
                Real discountFactor = Null<Real>();
                Real presentValue = Null<Real>();
                Real presentValueBase = Null<Real>();
                Real fxRateLocalCcy = Null<Real>();
                Real fxRateLocalBase = Null<Real>();
                Real fxRateCcyBase = Null<Real>();
                Real floorStrike = Null<Real>();
                Real capStrike = Null<Real>();
                Real floorVolatility = Null<Real>();
                Real capVolatility = Null<Real>();
                Real effectiveFloorVolatility = Null<Real>();
                Real effectiveCapVolatility = Null<Real>();

                if (amount != Null<Real>())
                    effectiveAmount = amount * multiplier;

                if (market) {
                    auto dcurve = specificDiscountCurve.empty() ? discountCurve : specificDiscountCurve;
                    discountFactor = ptrFlow->hasOccurred(asof) ? 0.0 : dcurve->discount(payDate);
                    if (effectiveAmount != Null<Real>())
                        presentValue = discountFactor * effectiveAmount;
                    try {
                        fxRateCcyBase = market->fxRate(npvCurrency_ + baseCurrency, configuration)->value();
                        fxRateLocalCcy = market->fxRate(ccy + npvCurrency_, configuration)->value();
                        fxRateLocalBase = fxRateCcyBase  * fxRateLocalCcy;
                        presentValueBase = presentValue * fxRateLocalBase;
                    } catch (...) {
                    }

                    // scan for known capped / floored coupons and extract cap / floor strike and fixing
                    // date

                    // unpack stripped cap/floor coupon
                    QuantLib::ext::shared_ptr<CashFlow> c = ptrFlow;
                    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(ptrFlow)) {
                        c = tmp->underlying();
                    }
                    Date volFixingDate;
                    std::string qlIndexName; // index used to retrieve vol
                    bool usesCapVol = false, usesSwaptionVol = false;
                    Period swaptionTenor;
                    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CappedFlooredCoupon>(c)) {
                        floorStrike = tmp->effectiveFloor();
                        capStrike = tmp->effectiveCap();
                        volFixingDate = tmp->fixingDate();
                        qlIndexName = tmp->index()->name();
                        if (auto cms = QuantLib::ext::dynamic_pointer_cast<CmsCoupon>(tmp->underlying())) {
                            swaptionTenor = cms->swapIndex()->tenor();
                            qlIndexName = cms->swapIndex()->iborIndex()->name();
                            usesSwaptionVol = true;
                        } else if (auto cms = QuantLib::ext::dynamic_pointer_cast<DurationAdjustedCmsCoupon>(
                                       tmp->underlying())) {
                            swaptionTenor = cms->swapIndex()->tenor();
                            qlIndexName = cms->swapIndex()->iborIndex()->name();
                            usesSwaptionVol = true;
                        } else if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(tmp->underlying())) {
                            qlIndexName = ibor->index()->name();
                            usesCapVol = true;
                        }
                    } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(c)) {
                        floorStrike = tmp->effectiveFloor();
                        capStrike = tmp->effectiveCap();
                        volFixingDate = tmp->underlying()->fixingDates().front();
                        qlIndexName = tmp->index()->name();
                        usesCapVol = true;
                        if (floorStrike != Null<Real>())
                            effectiveFloorVolatility = tmp->effectiveFloorletVolatility();
                        if (capStrike != Null<Real>())
                            effectiveCapVolatility = tmp->effectiveCapletVolatility();
                    } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageONIndexedCoupon>(c)) {
                        floorStrike = tmp->effectiveFloor();
                        capStrike = tmp->effectiveCap();
                        volFixingDate = tmp->underlying()->fixingDates().front();
                        qlIndexName = tmp->index()->name();
                        usesCapVol = true;
                        if (floorStrike != Null<Real>())
                            effectiveFloorVolatility = tmp->effectiveFloorletVolatility();
                        if (capStrike != Null<Real>())
                            effectiveCapVolatility = tmp->effectiveCapletVolatility();
                    } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageBMACoupon>(c)) {
                        floorStrike = tmp->effectiveFloor();
                        capStrike = tmp->effectiveCap();
                        volFixingDate = tmp->underlying()->fixingDates().front();
                        qlIndexName = tmp->index()->name();
                        usesCapVol = true;
                        if (floorStrike != Null<Real>())
                            effectiveFloorVolatility = tmp->effectiveFloorletVolatility();
                        if (capStrike != Null<Real>())
                            effectiveCapVolatility = tmp->effectiveCapletVolatility();
                    }

                    // get market volaility for cap / floor

                    if (volFixingDate != Date() && fixingDate > market->asofDate()) {
                        volFixingDate = std::max(volFixingDate, market->asofDate() + 1);
                        if (floorStrike != Null<Real>()) {
                            if (usesSwaptionVol) {
                                floorVolatility =
                                    market
                                        ->swaptionVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                      configuration)
                                        ->volatility(volFixingDate, swaptionTenor, floorStrike);
                            } else if (usesCapVol && floorVolatility == Null<Real>()) {
                                floorVolatility =
                                    market
                                        ->capFloorVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                      configuration)
                                        ->volatility(volFixingDate, floorStrike);
                            }
                        }
                        if (capStrike != Null<Real>()) {
                            if (usesSwaptionVol) {
                                capVolatility = market
                                                    ->swaptionVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                                  configuration)
                                                    ->volatility(volFixingDate, swaptionTenor, capStrike);
                            } else if (usesCapVol && capVolatility == Null<Real>()) {
                                capVolatility = market
                                                    ->capFloorVol(IndexNameTranslator::instance().oreName(qlIndexName),
                                                                  configuration)
                                                    ->volatility(volFixingDate, capStrike);
                            }
                        }
                    }
                }

                result.emplace_back();

                result.back().cashflowNo = j + 1;
                result.back().legNo = i + legNoOffset;
                result.back().payDate = payDate;
                result.back().flowType = flowType;
                result.back().amount = effectiveAmount;
                result.back().currency = ccy;
                result.back().coupon = coupon;
                result.back().accrual = accrual;
                result.back().accrualStartDate = accrualStartDate;
                result.back().accrualEndDate = accrualEndDate;
                result.back().accruedAmount = accruedAmount * (accruedAmount == Null<Real>() ? 1.0 : multiplier);
                result.back().fixingDate = fixingDate;
                result.back().fixingValue = fixingValue;
                result.back().notional = notional * (notional == Null<Real>() ? 1.0 : multiplier);
                result.back().discountFactor = discountFactor;
                result.back().presentValue = presentValue;
                result.back().fxRateLocalBase = fxRateLocalBase;
                result.back().presentValueBase = presentValueBase;
                result.back().baseCurrency = baseCurrency;
                result.back().floorStrike = floorStrike;
                result.back().capStrike = capStrike;
                result.back().floorVolatility = floorVolatility;
                result.back().capVolatility = capVolatility;
                result.back().effectiveFloorVolatility = effectiveFloorVolatility;
                result.back().effectiveCapVolatility = effectiveCapVolatility;
            }
        }
    }
    return result;

}

} // namespace data
} // namespace ore
