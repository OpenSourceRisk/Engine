/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>
#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/builders/fxtouchoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxtouchoption.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/barrieroption.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <qle/indexes/fxindex.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

FxTouchOption::FxTouchOption(Envelope& env, OptionData option, BarrierData barrier, string foreignCurrency,
                             string domesticCurrency, string payoffCurrency, double payoffAmount, string startDate,
                             string calendar, string fxIndex)
    : ore::data::Trade("FxTouchOption", env),
      FxSingleAssetDerivative("", env, foreignCurrency, domesticCurrency), option_(option),
      barrier_(barrier), startDate_(startDate), calendar_(calendar), fxIndex_(fxIndex), payoffAmount_(payoffAmount),
      payoffCurrency_(payoffCurrency) {
    Barrier::Type barrierType = parseBarrierType(barrier_.type());
    switch (barrierType) {
    case Barrier::DownIn:
    case Barrier::UpIn:
        type_ = "One-Touch";
        break;
    case Barrier::DownOut:
    case Barrier::UpOut:
        type_ = "No-Touch";
        break;
    default:
        QL_FAIL("unknown barrier type");
    }
}

void FxTouchOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Foreign Exchange");
    additionalData_["isdaBaseProduct"] = string("Simple Exotic");
    additionalData_["isdaSubProduct"] = string("Barrier");
    additionalData_["isdaTransaction"] = string("");

    additionalData_["payoffAmount"] = payoffAmount_;
    additionalData_["payoffCurrency"] = payoffCurrency_;

    npvCurrency_ = payoffCurrency_;
    notional_ = payoffAmount_;
    notionalCurrency_ = payoffCurrency_;

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxOption");
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    QL_REQUIRE(barrier_.levels().size() == 1, "Double barriers not supported for FxTouchOptions");
    QL_REQUIRE(barrier_.style().empty() || barrier_.style() == "American", "Only american barrier style suppported");

    // Parse trade data
    Currency fgnCcy = parseCurrency(foreignCurrency_);
    Currency domCcy = parseCurrency(domesticCurrency_);
    Real level = barrier_.levels()[0].value();
    Date expiryDate = parseDate(option_.exerciseDates().front());
    
    Natural payLag = 0;
    BusinessDayConvention payConvention = Unadjusted;
    Calendar payCalendar = NullCalendar();
    Date payDate = expiryDate;
    const boost::optional<OptionPaymentData>& opd = option_.paymentData();
    if (opd) {
        if (opd->rulesBased()) {
            payLag = opd->lag();
            payConvention = opd->convention();
            payCalendar = opd->calendar();
            payDate = payCalendar.advance(expiryDate, opd->lag(), Days, opd->convention());
        } else {
            if (opd->dates().size() > 1)
                ore::data::StructuredTradeWarningMessage(id(), tradeType(), "Trade build",
                                                         "Found more than 1 payment date. The first one will be used.")
                    .log();
            payDate = opd->dates().front();
        }
    }
    QL_REQUIRE(payDate >= expiryDate, "Settlement date cannot be earlier than expiry date");

    Barrier::Type barrierType = parseBarrierType(barrier_.type());
    Option::Type type;
    if (barrierType == Barrier::DownIn || barrierType == Barrier::DownOut)
        type = Option::Type::Put;
    else
        type = Option::Type::Call;
    option_.setCallPut(to_string(type));
    bool payoffAtExpiry = option_.payoffAtExpiry();
    Real rebate = barrier_.rebate();
    Position::Type positionType = parsePositionType(option_.longShort());
    Date start = ore::data::parseDate(startDate_);

    QL_REQUIRE(rebate == 0, "Rebates not supported for FxTouchOptions");
    QL_REQUIRE(payoffAtExpiry == true || barrierType == Barrier::Type::DownIn || barrierType == Barrier::Type::UpIn,
               "Payoff at hit not supported for FxNoTouchOptions");
    if ((barrierType == Barrier::Type::DownIn || barrierType == Barrier::Type::UpIn) && payoffAtExpiry == false)
        QL_REQUIRE(
            !opd || (opd->rulesBased() && opd->relativeTo() == OptionPaymentData::RelativeTo::Exercise),
            "Option payment data must be rules-based and relative to Exercise for FxOneTouchOption with payoff at hit");

    // Handle PayoffCurrency, we might have to flip the trade here
    bool flipResults = false;
    if (payoffCurrency_ == foreignCurrency_) {
        // Invert the trade, switch dom and for and flip Put/Call
        level = 1.0 / level;
        std::swap(fgnCcy, domCcy);
        type = type == Option::Call ? Option::Put : Option::Call;
        switch (barrierType) {
        case Barrier::DownIn:
            barrierType = Barrier::UpIn;
            break;
        case Barrier::UpIn:
            barrierType = Barrier::DownIn;
            break;
        case Barrier::DownOut:
            barrierType = Barrier::UpOut;
            break;
        case Barrier::UpOut:
            barrierType = Barrier::DownOut;
            break;
        }
        flipResults = true;
    } else if (payoffCurrency_ != domesticCurrency_) {
        QL_FAIL("Invalid Payoff currency (" << payoffCurrency_ << ") for FxTouchOption " << foreignCurrency_
                                            << domesticCurrency_);
    }
    DLOG("Setting up FxTouchOption with level " << level << " foreign/bought " << fgnCcy << " domestic/sold "
                                                << domCcy);
    // from this point on it's important not to use domesticCurrency_, foreignCurrency_, barrier_.level(), etc
    // rather the local variables (fgnCcy, domCcy, level, etc) should be used as they may have been flipped.

    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex;
    if (!fxIndex_.empty())
        fxIndex = buildFxIndex(fxIndex_, domCcy.code(), fgnCcy.code(), engineFactory->market(),
                               engineFactory->configuration(MarketContext::pricing));
    Calendar cal = ore::data::parseCalendar(calendar_);

    auto buildBarrierOptionWrapperInstr = [this, type, level, engineFactory, domCcy, fgnCcy, flipResults, positionType,
                                           market, barrierType, rebate, fxIndex, cal,
                                           start](const Date& expiryDate, const Date& payDate) {
        QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(type, level, 1.0));
        Leg leg;

        leg.push_back(QuantLib::ext::shared_ptr<CashFlow>(new SimpleCashFlow(1.0, payDate)));
        // Hard code payoff at expiry to true - we ignore in pricing; QPR-10669
        bool payoffFlag = true;

        QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<AmericanExercise>(expiryDate, payoffFlag);

        QuantLib::ext::shared_ptr<Instrument> barrier = QuantLib::ext::make_shared<VanillaOption>(payoff, exercise);
        QuantLib::ext::shared_ptr<Instrument> underlying = QuantLib::ext::make_shared<Swap>(Leg(), leg);

        // set pricing engines
        QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
        QL_REQUIRE(builder, "No builder found for " << tradeType_);
        QuantLib::ext::shared_ptr<FxTouchOptionEngineBuilder> fxTouchOptBuilder =
            QuantLib::ext::dynamic_pointer_cast<FxTouchOptionEngineBuilder>(builder);
        barrier->setPricingEngine(fxTouchOptBuilder->engine(fgnCcy, domCcy, type_, payDate, flipResults));
        setSensitivityTemplate(*fxTouchOptBuilder);
        if (type_ == "One-Touch") {
            // if a one-touch option is triggered it becomes a simple forward cashflow
            // which we price as a swap
            builder = engineFactory->builder("Swap");
            QL_REQUIRE(builder, "No builder found for Swap");
            QuantLib::ext::shared_ptr<SwapEngineBuilderBase> swapBuilder =
                QuantLib::ext::dynamic_pointer_cast<SwapEngineBuilderBase>(builder);
            underlying->setPricingEngine(swapBuilder->engine(domCcy, std::string(), std::string()));
        }

        bool isLong = (positionType == Position::Long) ? true : false;

        std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
        std::vector<Real> additionalMultipliers;
        Date lastPremiumDate =
            addPremiums(additionalInstruments, additionalMultipliers, (isLong ? 1.0 : -1.0) * payoffAmount_,
                        option_.premiumData(), isLong ? -1.0 : 1.0, parseCurrency(payoffCurrency_), engineFactory,
                        builder->configuration(MarketContext::pricing));

        Handle<Quote> spot = market->fxRate(fgnCcy.code() + domCcy.code());

        auto barrierOptionWrapper = QuantLib::ext::make_shared<SingleBarrierOptionWrapper>(
            barrier, isLong, expiryDate, false, underlying, barrierType, spot, level, rebate, domCcy, start, fxIndex,
            cal, payoffAmount_, payoffAmount_, additionalInstruments, additionalMultipliers);
        
        maturity_ = std::max(lastPremiumDate, payDate);

        return barrierOptionWrapper;
    };

    auto barrierOptionWrapper = buildBarrierOptionWrapperInstr(expiryDate, payDate);

    // We make sure to add required fixings before checking for modifying the instrument's expiry date, to make sure the
    // portfolio-analyzer gets all the fixings needed for the instrument at the first evaluation.
    Calendar fixingCal = fxIndex ? fxIndex->fixingCalendar() : cal;
    if (start != Date()) {
        for (Date d = start; d <= expiryDate; d = fixingCal.advance(d, 1 * Days))
            requiredFixings_.addFixingDate(d, fxIndex_, payDate);
    }

    // Check if the barrier has been triggered already. If payoff-at-hit, and barrier was touched in the past, then
    // create instrument again, with expiry date and pay date corresponding to that past barrier exercise date
    if (auto rt = engineFactory->engineData()->globalParameters().find("RunType");
        rt != engineFactory->engineData()->globalParameters().end() && rt->second != "PortfolioAnalyser" &&
        barrierOptionWrapper->exercise()) {
        QL_REQUIRE(barrierOptionWrapper->exerciseDate() != Date(), "Option is exercised but exercise date was not defined");
        expiryDate = barrierOptionWrapper->exerciseDate();
        additionalData_["exerciseDate"] = expiryDate;

        if (!payoffAtExpiry && type_ == "One-Touch") {
            payDate = payCalendar.advance(expiryDate, payLag, Days, payConvention);
            barrierOptionWrapper = buildBarrierOptionWrapperInstr(expiryDate, payDate);
            additionalData_["settlementDate"] = payDate;
        }
    }

    instrument_ = barrierOptionWrapper;

    // maturity_ is set in buildBarrierOptionWrapperInstr()
}

bool FxTouchOption::checkBarrier(Real spot, Barrier::Type type, Real barrier) {
    switch (type) {
    case Barrier::DownIn:
    case Barrier::DownOut:
        return spot <= barrier;
    case Barrier::UpIn:
    case Barrier::UpOut:
        return spot >= barrier;
    default:
        QL_FAIL("unknown barrier type " << type);
    }
}

void FxTouchOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxTouchOptionData");
    QL_REQUIRE(fxNode, "No FxOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    barrier_.fromXML(XMLUtils::getChildNode(fxNode, "BarrierData"));
    Barrier::Type barrierType = parseBarrierType(barrier_.type());
    switch (barrierType) {
    case Barrier::DownIn:
    case Barrier::UpIn:
        type_ = "One-Touch";
        break;
    case Barrier::DownOut:
    case Barrier::UpOut:
        type_ = "No-Touch";
        break;
    default:
        QL_FAIL("unknown barrier type");
    }

    foreignCurrency_ = XMLUtils::getChildValue(fxNode, "ForeignCurrency", true);
    domesticCurrency_ = XMLUtils::getChildValue(fxNode, "DomesticCurrency", true);
    payoffCurrency_ = XMLUtils::getChildValue(fxNode, "PayoffCurrency", true);
    startDate_ = XMLUtils::getChildValue(fxNode, "StartDate", false);
    calendar_ = XMLUtils::getChildValue(fxNode, "Calendar", false);
    fxIndex_ = XMLUtils::getChildValue(fxNode, "FXIndex", false);
    payoffAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "PayoffAmount", true);
}

XMLNode* FxTouchOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxTouchOptionData");
    XMLUtils::appendNode(node, fxNode);
    XMLUtils::appendNode(fxNode, option_.toXML(doc));
    XMLUtils::appendNode(fxNode, barrier_.toXML(doc));
    XMLUtils::addChild(doc, fxNode, "ForeignCurrency", foreignCurrency_);
    XMLUtils::addChild(doc, fxNode, "DomesticCurrency", domesticCurrency_);
    XMLUtils::addChild(doc, fxNode, "PayoffCurrency", payoffCurrency_);
    XMLUtils::addChild(doc, fxNode, "PayoffAmount", payoffAmount_);
    if (startDate_ != "")
        XMLUtils::addChild(doc, fxNode, "StartDate", startDate_);
    if (fxIndex_ != "")
        XMLUtils::addChild(doc, fxNode, "FXIndex", fxIndex_);
    if (calendar_ != "")
        XMLUtils::addChild(doc, fxNode, "Calendar", calendar_);

    return node;
}

} // namespace data
} // namespace oreplus
