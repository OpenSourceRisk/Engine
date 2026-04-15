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

#include <ored/portfolio/equitydeltahedgedoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void EquityAutoDeltaHedgedOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Equity");
    additionalData_["isdaBaseProduct"] = string("Option");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    additionalData_["isdaTransaction"] = string("");
    additionalData_["hedgingVolatility"] = hedgingVol_;
    additionalData_["forwardRate"] = forwardRate_;
    additionalData_["observationStartDate"] = ore::data::to_string(observationStartDate_);

    QL_REQUIRE(!underlyings_.empty(),
               "EquityAutoDeltaHedgedOption: no underlyings specified for trade " << id());

    Date today = Settings::instance().evaluationDate();
    bool hedgingStarted = (today >= observationStartDate_);

    const QuantLib::ext::shared_ptr<Market>& market = engineFactory->market();
    string config = engineFactory->configuration(MarketContext::pricing);

    // Composite instrument: accumulates the option and hedge contributions
    auto composite = QuantLib::ext::make_shared<CompositeInstrument>();

    // Additional instruments for premiums and realized hedge P&L
    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;

    notional_ = 0.0;
    maturity_ = Date::minDate();

    // Equity Amount = Sum_i [ DeltaHedge_Final^i + Premium^i - Option_Final^i ] x N^i
    //
    // DeltaHedge is built up discretely:
    //   DeltaHedge_t = DeltaHedge_{t-1} + delta_{t-1} * (Fwd_{t,Final} - Fwd_{t-1,Final})
    //   DeltaHedge_Start = 0, delta_t = N(d1) at hedging vol
    //
    // We split the hedge P&L into:
    //   (a) Realized past hedge P&L (observationStartDate to today): computed from historical
    //       fixings and hedging vol deltas, accumulated as a known cash amount.
    //   (b) Expected future hedge P&L (today to expiry): equals C(sigma_h, today), the
    //       BS option value at hedging vol, since continuous delta hedging with vol sigma_h
    //       replicates the option payoff priced at sigma_h.
    //
    // PV(EqAmount) = Sum_i N^i * [ realizedHedgePnL^i * DF(T)
    //                             + C_hedge^i(today)           -- future hedge
    //                             + Premium^i * DF(T)
    //                             - C_market^i(today) ]        -- option payoff PV

    CumulativeNormalDistribution N_cdf;

    for (size_t i = 0; i < underlyings_.size(); ++i) {
        const auto& u = underlyings_[i];
        string assetName = u.equityUnderlying.name();
        Currency ccy = parseCurrency(u.currency);

        QL_REQUIRE(u.optionData.exerciseDates().size() == 1,
                   "EquityAutoDeltaHedgedOption: need exactly one exercise date for underlying " << i
                                                                                                << " in trade " << id());

        Date expiryDate = parseDate(u.optionData.exerciseDates().front());
        maturity_ = std::max(maturity_, expiryDate);

        QL_REQUIRE(observationStartDate_ < expiryDate,
                   "EquityAutoDeltaHedgedOption: observation start date " << observationStartDate_
                   << " must be before expiry date " << expiryDate << " for underlying " << i
                   << " in trade " << id());

        Real optionQuantity = u.quantity;
        Real K = u.strike.value();
        notional_ += K * optionQuantity;

        if (i == 0)
            npvCurrency_ = notionalCurrency_ = ccy.code();

        // option payoff and exercise
        Option::Type type = parseOptionType(u.optionData.callPut());
        Real phi = (type == Option::Call) ? 1.0 : -1.0;
        auto payoff = QuantLib::ext::make_shared<PlainVanillaPayoff>(type, K);
        auto exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);

        Handle<Quote> spot = market->equitySpot(assetName, config);
        Handle<YieldTermStructure> divCurve = market->equityDividendCurve(assetName, config);
        Handle<YieldTermStructure> fcstCurve = market->equityForecastCurve(assetName, config);
        Handle<YieldTermStructure> discountCurve = market->discountCurve(ccy.code(), config);

        // Get the equity index for historical fixings and forward computation
        Handle<QuantExt::EquityIndex2> eqIndex = market->equityCurve(assetName, config);
        Calendar fixingCal = eqIndex->fixingCalendar();

        // --- Realized past hedge P&L ---
        // Accumulate DeltaHedge from observationStartDate_ to today using historical fixings.
        // Fwd_{t,Final} = S_t * Exp(forwardRate_ * Act(t,Final)/365).
        // delta_t = N(phi * d1) where d1 = [ln(Fwd/K) + 0.5*sigma_h^2*(T-t)] / (sigma_h*sqrt(T-t))
        Real realizedHedgePnL = 0.0;

        if (hedgingStarted) {
            DayCounter dc = Actual365Fixed();
            Date obsDate = fixingCal.adjust(observationStartDate_, Following);

            // Previous forward: F_{t-1, Final} = S_{t-1} * Exp(forwardRate_ * tau)
            Real prevFwd = Null<Real>();
            // On the Observation Start Date: Delta_{Start-1} = Delta_Start.
            // prevDelta is initialised to 0 but is overwritten with Delta_Start on the first
            // iteration before it is used in any accumulation (since prevFwd starts as Null).
            Real prevDelta = 0.0;

            for (Date d = obsDate; d <= today; d = fixingCal.advance(d, 1, Days)) {
                Real spotD = eqIndex->fixing(d);

                // Contractual forward: Fwd_{t,Final} = S_t * Exp(forwardRate_ * tau)
                Time tToExpiry = dc.yearFraction(d, expiryDate);
                Real fwdD = spotD * std::exp(forwardRate_ * tToExpiry);

                if (prevFwd != Null<Real>()) {
                    // DeltaHedge_t += delta_{t-1} * (Fwd_t - Fwd_{t-1})
                    realizedHedgePnL += prevDelta * (fwdD - prevFwd);
                }

                // Compute delta_t = N(phi * d1) at hedging vol for the next step
                if (tToExpiry > 0.0) {
                    Real stdDev = hedgingVol_ * std::sqrt(tToExpiry);
                    Real d1 = (std::log(fwdD / K) + 0.5 * stdDev * stdDev) / stdDev;
                    prevDelta = N_cdf(phi * d1);
                } else {
                    prevDelta = 0.0;
                }

                prevFwd = fwdD;
            }
        }

        // --- Market-vol option: represents PV of E[Option_Final^i] ---
        auto marketOption = QuantLib::ext::make_shared<VanillaOption>(payoff, exercise);
        Handle<BlackVolTermStructure> marketVol = market->equityVol(assetName, config);
        auto marketProcess = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            spot, divCurve, fcstCurve, marketVol);
        marketOption->setPricingEngine(
            QuantLib::ext::make_shared<QuantLib::AnalyticEuropeanEngine>(marketProcess, discountCurve));

        // Subtract market option value: -N * C_market
        composite->subtract(marketOption, optionQuantity);

        // --- Future hedge P&L: BS option at hedging vol from today to expiry ---
        // The contractual delta uses Fwd = S_t * Exp(Some_Rates * tau), so the hedge GBM process
        // must use flat yield curves at the forward rate for both rate and dividend
        // so that BS forward = S * Exp((r - q) * tau) = S * Exp(forwardRate_ * tau).
        // The market discount curve is used separately for present-value discounting.
        if (hedgingStarted && today < expiryDate) {
            auto hedgeOption = QuantLib::ext::make_shared<VanillaOption>(payoff, exercise);
            Handle<BlackVolTermStructure> hedgeVol(QuantLib::ext::make_shared<BlackConstantVol>(
                today, NullCalendar(), hedgingVol_, Actual365Fixed()));
            hedgeVol->enableExtrapolation();
            // Flat yield curves: rate = forwardRate_, dividend = 0, so BS forward = S * Exp(forwardRate_ * tau)
            Handle<YieldTermStructure> fwdRate(QuantLib::ext::make_shared<FlatForward>(
                today, forwardRate_, Actual365Fixed()));
            Handle<YieldTermStructure> zeroRate(QuantLib::ext::make_shared<FlatForward>(
                today, 0.0, Actual365Fixed()));
            auto hedgeProcess = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
                spot, zeroRate, fwdRate, hedgeVol);
            hedgeOption->setPricingEngine(
                QuantLib::ext::make_shared<QuantLib::AnalyticEuropeanEngine>(hedgeProcess, discountCurve));

            // Add future hedge option value: +N * C_hedge(today)
            composite->add(hedgeOption, optionQuantity);
        }

        // --- Realized past hedge P&L as a payment at expiry ---
        // The realized hedge P&L is a known amount, discounted from expiry to today
        if (hedgingStarted && std::fabs(realizedHedgePnL) > 1e-14) {
            auto hedgePnLPayment = QuantLib::ext::make_shared<QuantExt::Payment>(realizedHedgePnL, ccy, expiryDate);
            hedgePnLPayment->setPricingEngine(
                QuantLib::ext::make_shared<QuantExt::PaymentDiscountingEngine>(discountCurve));
            additionalInstruments.push_back(hedgePnLPayment);
            additionalMultipliers.push_back(optionQuantity);
        }

        // --- Premium: settled at the expiry date as part of the equity amount ---
        auto premData = u.optionData.premiumData().premiumData();
        QL_REQUIRE(premData.size() == 1, "EquityAutoDeltaHedgedOption: expected exactly one premium per underlying, got "
                                             << premData.size() << " for underlying " << i << " in trade " << id());
        auto premiumData = premData.front();
        QL_REQUIRE(premiumData.amount != Null<Real>(), "Invalid premium data for underlying " << i);
        Real premAmount = convertMinorToMajorCurrency(premiumData.ccy, premiumData.amount);
        Currency premCcy = parseCurrencyWithMinors(premiumData.ccy);
        auto payment = QuantLib::ext::make_shared<QuantExt::Payment>(premAmount, premCcy, expiryDate);
        Handle<Quote> fxRate;
        if (premCcy != ccy)
            fxRate = market->fxRate(premCcy.code() + ccy.code(), config);
        payment->setPricingEngine(
            QuantLib::ext::make_shared<QuantExt::PaymentDiscountingEngine>(discountCurve, fxRate));
        additionalInstruments.push_back(payment);
        additionalMultipliers.push_back(optionQuantity);
    }

    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(
        new VanillaInstrument(composite, 1.0, additionalInstruments, additionalMultipliers));
    
    setSensitivityTemplate(std::string());
}

void EquityAutoDeltaHedgedOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);

    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityAutoDeltaHedgedOptionData");
    QL_REQUIRE(eqNode, "No EquityAutoDeltaHedgedOptionData node for trade " << id());

    hedgingVol_ = XMLUtils::getChildValueAsDouble(eqNode, "Volatility", true);
    forwardRate_ = XMLUtils::getChildValueAsDouble(eqNode, "ForwardRate", true);

    string obsStartStr = XMLUtils::getChildValue(eqNode, "ObservationStartDate", true);
    observationStartDate_ = parseDate(obsStartStr);

    XMLNode* underlyingsNode = XMLUtils::getChildNode(eqNode, "Underlyings");
    QL_REQUIRE(underlyingsNode, "No Underlyings node in EquityAutoDeltaHedgedOptionData for trade " << id());

    vector<XMLNode*> underlyingNodes = XMLUtils::getChildrenNodes(underlyingsNode, "Underlying");
    QL_REQUIRE(!underlyingNodes.empty(), "No Underlying nodes in Underlyings for trade " << id());

    underlyings_.clear();
    for (auto* uNode : underlyingNodes) {
        UnderlyingOptionData u;

        XMLNode* optNode = XMLUtils::getChildNode(uNode, "OptionData");
        QL_REQUIRE(optNode, "No OptionData in Underlying element for trade " << id());
        u.optionData.fromXML(optNode);

        // Parse the equity underlying name
        string nameStr = XMLUtils::getChildValue(uNode, "Name", true);
        u.equityUnderlying = EquityUnderlying(nameStr);
        u.currency = XMLUtils::getChildValue(uNode, "Currency", true);
        u.strike.fromXML(uNode, true, false);
        u.strikeCurrency = XMLUtils::getChildValue(uNode, "StrikeCurrency", false);
        u.quantity = XMLUtils::getChildValueAsDouble(uNode, "Quantity", true);
        underlyings_.push_back(u);
    }
}

XMLNode* EquityAutoDeltaHedgedOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityAutoDeltaHedgedOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::addChild(doc, eqNode, "Volatility", hedgingVol_);
    XMLUtils::addChild(doc, eqNode, "ForwardRate", forwardRate_);

    XMLNode* underlyingsNode = doc.allocNode("Underlyings");
    XMLUtils::appendNode(eqNode, underlyingsNode);

    for (const auto& u : underlyings_) {
        XMLNode* uNode = doc.allocNode("Underlying");
        XMLUtils::appendNode(underlyingsNode, uNode);

        XMLUtils::appendNode(uNode, u.optionData.toXML(doc));
        XMLUtils::addChild(doc, uNode, "Name", u.equityUnderlying.name());
        XMLUtils::addChild(doc, uNode, "Currency", u.currency);
        XMLUtils::appendNode(uNode, u.strike.toXML(doc));
        if (!u.strikeCurrency.empty())
            XMLUtils::addChild(doc, uNode, "StrikeCurrency", u.strikeCurrency);
        XMLUtils::addChild(doc, uNode, "Quantity", u.quantity);
    }

    XMLUtils::addChild(doc, eqNode, "ObservationStartDate", ore::data::to_string(observationStartDate_));

    return node;
}

std::map<AssetClass, std::set<std::string>>
EquityAutoDeltaHedgedOption::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::set<std::string> names;
    for (const auto& u : underlyings_)
        names.insert(u.equityUnderlying.name());
    return {{AssetClass::EQ, names}};
}

} // namespace data
} // namespace ore
