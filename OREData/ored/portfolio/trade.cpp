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
#include <ored/utilities/to_string.hpp>

#include <qle/cashflows/equitycouponpricer.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

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
                        const Currency& tradeCurrency, const QuantLib::ext::shared_ptr<EngineFactory>& factory,
                        const string& configuration) {

    Date latestPremiumPayDate = Date::minDate();

    for (auto const& d : premiumData.premiumData()) {
        QL_REQUIRE(d.amount != Null<Real>(), "Trade contains invalid premium data.");

        Currency premiumCurrency = parseCurrencyWithMinors(d.ccy);
        Real premiumAmount = convertMinorToMajorCurrency(d.ccy, d.amount);
        auto fee = QuantLib::ext::make_shared<QuantExt::Payment>(premiumAmount, premiumCurrency, d.payDate);

        addMultipliers.push_back(premiumMultiplier);

        Handle<YieldTermStructure> yts = factory->market()->discountCurve(d.ccy, configuration);
        Handle<Quote> fx;
        if (tradeCurrency.code() != d.ccy) {
            fx = factory->market()->fxRate(d.ccy + tradeCurrency.code(), configuration);
        }
        QuantLib::ext::shared_ptr<PricingEngine> discountingEngine(new QuantExt::PaymentDiscountingEngine(yts, fx));
        fee->setPricingEngine(discountingEngine);

        // 1) Add to additional instruments for pricing
        addInstruments.push_back(fee);

        // 2) Add a trade leg for cash flow reporting, divide the amount by the multiplier, because the leg entries
        //    are multiplied with the trade multiplier in the cashflow report (and if used elsewhere)
        legs_.push_back(
            Leg(1, QuantLib::ext::make_shared<SimpleCashFlow>(fee->cashFlow()->amount() * premiumMultiplier / tradeMultiplier,
                                                      fee->cashFlow()->date())));
        legCurrencies_.push_back(fee->currency().code());

        // premium * premiumMultiplier reflects the correct pay direction, set payer to false therefore
        legPayers_.push_back(false);

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
    maturity_ = Date();
    issuer_.clear();
    requiredFixings_.clear();
    sensitivityTemplate_.clear();
    sensitivityTemplateSet_ = false;
}
    
const std::map<std::string, boost::any>& Trade::additionalData() const { return additionalData_; }

void Trade::setLegBasedAdditionalData(const Size i, Size resultLegId) const {
    if (legs_.size() < i + 1)
        return;
    Date asof = Settings::instance().evaluationDate();
    string legID = std::to_string(resultLegId == Null<Size>() ? i + 1 : resultLegId);
    for (Size j = 0; j < legs_[i].size(); ++j) {
        QuantLib::ext::shared_ptr<CashFlow> flow = legs_[i][j];
        // pick flow with earliest future payment date on this leg
        if (flow->date() > asof) {
            Real flowAmount = 0.0;
            try {
                flowAmount = flow->amount();
            } catch (std::exception& e) {
                ALOG("flow amount could not be determined for trade " << id() << ", set to zero: " << e.what());
            }
            additionalData_["amount[" + legID + "]"] = flowAmount;
            additionalData_["paymentDate[" + legID + "]"] = ore::data::to_string(flow->date());
            QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(flow);
            if (coupon) {
                Real currentNotional = 0;
                try {
                    currentNotional = coupon->nominal();
                } catch (std::exception& e) {
                    ALOG("current notional could not be determined for trade " << id()
                                                                               << ", set to zero: " << e.what());
                }
                additionalData_["currentNotional[" + legID + "]"] = currentNotional;

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
                    additionalData_["initialPrice[" + legID + "]"] = arc.initialPrice;
                    additionalData_["endEquityFixing[" + legID + "]"] = arc.endFixing;
                    if (arc.startFixing != Null<Real>())
                        additionalData_["startEquityFixing[" + legID + "]"] = arc.startFixing;
                    if (arc.startFixingTotal != Null<Real>())
                        additionalData_["startEquityFixingTotal[" + legID + "]"] =
                            arc.startFixingTotal;
                    if (arc.endFixingTotal != Null<Real>())
                        additionalData_["endEquityFixingTotal[" + legID + "]"] = arc.endFixingTotal;
                    if (arc.startFxFixing != Null<Real>())
                        additionalData_["startFxFixing[" + legID + "]"] = arc.startFxFixing;
                    if (arc.endFxFixing != Null<Real>())
                        additionalData_["endFxFixing[" + legID + "]"] = arc.endFxFixing;
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
                        } catch (std::exception& e) {
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
                        } catch (std::exception& e) {
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
                additionalData_["initialQuantity[" + legID + "]"] = quantity;

                Real currentPrice = Null<Real>();
                if (eqc->equityCurve()->isValidFixingDate(asof)) {
                    currentPrice = eqc->equityCurve()->equitySpot()->value();
                }
                if (currentPrice != Null<Real>() && originalNotional != Null<Real>()) {
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

} // namespace data
} // namespace ore
