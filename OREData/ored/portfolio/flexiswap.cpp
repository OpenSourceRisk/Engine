/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/flexiswap.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/flexiswap.hpp>
#include <ored/portfolio/swaption.hpp>

#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>

#include <qle/instruments/currencyswap.hpp>
#include <qle/instruments/flexiswap.hpp>
#include <qle/instruments/flexiswapreplication.hpp>
#include <qle/instruments/rebatedexercise.hpp>

#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/swap.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void FlexiSwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("FlexiSwap::build() for id \"" << id() << "\" called.");

    // ISDA taxonomy

    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("Exotic");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    // checks

    QL_REQUIRE(!underlyingData_.empty(), "MultiLegOption: no underlying given");

    // get engine builder

    auto flexiSwapBuilder =
        QuantLib::ext::dynamic_pointer_cast<FlexiSwapEngineBuilder>(engineFactory->builder("FlexiSwap"));

    QL_REQUIRE(flexiSwapBuilder, "FlexiSwap::build(): FlexiSwapEngineBuilder is null");

    bool isXCcy = std::any_of(underlyingData_.begin(), underlyingData_.end(),
                              [this](const LegData& d) { return d.currency() != underlyingData_.front().currency(); });
    auto builder = QuantLib::ext::dynamic_pointer_cast<SwaptionEngineBuilder>(
        engineFactory->builder("BermudanSwaption" + (isXCcy ? std::string("_XCcy") : std::string(""))));
    auto configuration = builder->configuration(MarketContext::pricing);

    QL_REQUIRE(builder, "FlexiSwap::build(): SwaptionEngineBuilder is null");

    // build underlying legs

    legs_.resize(underlyingData_.size());
    legPayers_.resize(underlyingData_.size());
    legCurrencies_.resize(underlyingData_.size());

    std::vector<Leg> couponLegCopies(legs_.size());

    for (Size i = 0; i < underlyingData_.size(); ++i) {
        auto legBuilder = engineFactory->legBuilder(underlyingData_[i].legType());
        legs_[i] = legBuilder->buildLeg(underlyingData_[i], engineFactory, requiredFixings_,
                                        builder->configuration(MarketContext::pricing));
        couponLegCopies[i] = legBuilder->buildLeg(underlyingData_[i], engineFactory, requiredFixings_,
                                                  builder->configuration(MarketContext::pricing));
        legCurrencies_[i] = underlyingData_[i].currency();
        legPayers_[i] = underlyingData_[i].isPayer();

        auto leg =
            buildNotionalLeg(underlyingData_[i], legs_[i], requiredFixings_, engineFactory->market(), configuration);
        if (!leg.empty()) {
            legs_.push_back(leg);
            legPayers_.push_back(legPayers_[i]);
            legCurrencies_.push_back(legCurrencies_[i]);
        }
    }

    // set trade members

    std::tie(notionalTakenFromLeg_, notional_, npvCurrency_, notionalCurrency_) =
        getSwapNpvAndNotionalInfo(underlyingData_);

    Date startDate;
    std::tie(startDate, maturity_, maturityType_) = getSwapStartMaturity(legs_);
    additionalData_["startDate"] = to_string(startDate);

    // set single currency flag

    bool isSingleCurrency = std::all_of(underlyingData_.begin(), underlyingData_.end(), [&](const LegData& d) {
        return d.currency() == underlyingData_[0].currency();
    });

    // build lower notional bounds

    std::vector<std::vector<Real>> lowerNotionalBounds;
    for (Size i = 0; i < underlyingData_.size(); ++i) {
        Schedule schedule = makeSchedule(underlyingData_[i].schedule());
        std::vector<double> tmpLowerNotionalBounds;
        std::vector<std::string> tmpLowerNotionalBoundDates;
        if (auto b = lowerNotionalBounds_.find(underlyingData_[i].currency()); b != lowerNotionalBounds_.end()) {
            std::tie(tmpLowerNotionalBounds, tmpLowerNotionalBoundDates) = b->second;
        } else if (auto b = lowerNotionalBounds_.find(std::string());
                   b != lowerNotionalBounds_.end() && isSingleCurrency) {
            std::tie(tmpLowerNotionalBounds, tmpLowerNotionalBoundDates) = b->second;
        } else {
            QL_FAIL("FlexiSwap: lower notional bounds for currency '" << underlyingData_[i].currency()
                                                                      << "' not given.");
        }
        lowerNotionalBounds.push_back(
            buildScheduledVectorNormalised(tmpLowerNotionalBounds, tmpLowerNotionalBoundDates, schedule, 0.0));
    }

    // build swap part

    Currency npvCcy = parseCurrency(npvCurrency_);
    std::vector<Currency> legCcys;
    std::transform(legCurrencies_.begin(), legCurrencies_.end(), std::back_inserter(legCcys),
                   [](const std::string& ccy) { return parseCurrency(ccy); });

    ext::shared_ptr<Instrument> swap;
    if (isXCcy) {
        swap = ext::make_shared<QuantExt::CurrencySwap>(legs_, legPayers_, legCcys);
        auto swapBuilder =
            ext::dynamic_pointer_cast<CrossCurrencySwapEngineBuilder>(engineFactory->builder("CrossCurrencySwap"));
        swap->setPricingEngine(swapBuilder->engine(legCcys, npvCcy, false, {}));
    } else {
        swap = ext::make_shared<QuantLib::Swap>(legs_, legPayers_);
        auto swapBuilder = ext::dynamic_pointer_cast<SwapEngineBuilderBase>(engineFactory->builder("Swap"));
        swap->setPricingEngine(swapBuilder->engine(npvCcy, {}, {}, {}));
    }

    auto qlInstr = ext::make_shared<CompositeInstrument>();
    qlInstr->add(swap);

    // flexi swap replication, build ql instrument and instrument wrapper

    Date today = Settings::instance().evaluationDate();

    bool generateNotionalExchanges = std::any_of(underlyingData_.begin(), underlyingData_.end(),
                                                 [](const LegData& d) { return d.notionalAmortizingExchange(); });

    auto positionType = parsePositionType(optionLongShort_);

    std::vector<Currency> couponLegCurrencies(legCcys.begin(), std::next(legCcys.begin(), underlyingData_.size()));
    std::vector<bool> couponLegPayers(legPayers_.begin(), std::next(legPayers_.begin(), underlyingData_.size()));

    auto basket = generateFlexiSwapReplication(today, couponLegCopies, couponLegPayers, couponLegCurrencies,
                                               lowerNotionalBounds, positionType == Position::Type::Long,
                                               generateNotionalExchanges, generateNotionalExchanges);

    // determine qualifiers, calibration strikes ir, fx (latter is set to ATMF), exercise dates, maturities

    std::vector<std::string> differentCcys;
    std::vector<std::vector<Leg>> legsPerCcy;

    for (Size j = 0; j < couponLegCopies.size(); ++j) {
        Size idx = std::distance(differentCcys.begin(),
                                 std::find(differentCcys.begin(), differentCcys.end(), couponLegCurrencies[j].code()));
        if (idx == differentCcys.size()) {
            differentCcys.push_back(couponLegCurrencies[j].code());
            legsPerCcy.push_back({});
        }
        legsPerCcy[idx].push_back(couponLegCopies[j]);
    }

    std::set<Date> differentDates;
    Date maxMaturityDate = Date::minDate();
    for (auto const& b : basket) {
        if (auto ex = ext::dynamic_pointer_cast<BermudanExercise>(b->exercise())) {
            differentDates.insert(ex->dates().begin(), ex->dates().end());
        } else if (auto ex = ext::dynamic_pointer_cast<RebatedExercise>(b->exercise())) {
            differentDates.insert(ex->dates().begin(), ex->dates().end());
        } else {
            QL_FAIL(
                "FlexiSwap::build(): could not cast exercise to BermudanExercise or RebatedExercise. Internal error.");
        }
        maxMaturityDate = std::max(maxMaturityDate, b->maturityDate());
    }

    std::vector<Date> dates(differentDates.begin(), differentDates.end());
    std::vector<Date> maturities(dates.size(), maxMaturityDate);

    std::vector<std::string> qualifiers;
    std::vector<std::vector<Real>> strikes;

    for (Size j = 0; j < differentCcys.size(); ++j) {
        auto index = getInterestRateIndexFromLegs(legsPerCcy[j]);
        qualifiers.push_back(index.empty() ? differentCcys[j]
                                           : IndexNameTranslator::instance().oreName(index.front()->name()));
        strikes.push_back(getCalibrationStrikesFromLegs(legsPerCcy[j], dates));
    }

    // build global model if required

    bool useGlobalModel = parseBool(flexiSwapBuilder->modelParameter("GlobalModel", {}, true));
    SwaptionModel globalModel;

    if (useGlobalModel) {
        globalModel = builder->model(
            id() + "_0", qualifiers, dates, maturities, strikes,
            std::vector<std::vector<Real>>(differentCcys.size() - 1, std::vector<Real>(dates.size(), Null<Real>())),
            false);
    }

    // set pricing engine on basket constituents

    for (Size i = 0; i < basket.size(); ++i) {

        if (!useGlobalModel) {

            // local dates

            if (auto ex = ext::dynamic_pointer_cast<BermudanExercise>(basket[i]->exercise())) {
                dates = ex->dates();
            } else if (auto ex = ext::dynamic_pointer_cast<RebatedExercise>(basket[i]->exercise())) {
                dates = ex->dates();
            } else {
                QL_FAIL("FlexiSwap::build(): could not cast exercise to BermudanExercise or RebatedExercise. Internal "
                        "error.");
            }
            maturities = std::vector<Date> (dates.size(), basket[i]->maturityDate());

            // local strikes

            strikes.clear();
            for (Size j = 0; j < differentCcys.size(); ++j) {
                strikes.push_back(getCalibrationStrikesFromLegs(legsPerCcy[j], dates));
            }
        }

        // build engine

        basket[i]->setPricingEngine(builder->engine(
            id() + "_" + std::to_string(i), qualifiers, dates, maturities, strikes,
            std::vector<std::vector<Real>>(differentCcys.size() - 1, std::vector<Real>(dates.size(), Null<Real>())),
            false, std::string(), std::string(), globalModel));
        positionType == Position::Type::Long ? qlInstr->add(basket[i]) : qlInstr->subtract(basket[i]);
    }

    instrument_ = ext::make_shared<VanillaInstrument>(qlInstr);

    // log replication basket details

    DLOG("replicatingSwaptionNo,firstExerciseDate,notional,start,end");
    for (Size i = 0; i < basket.size(); ++i) {
        // description string, for logging
        auto cpnf = ext::dynamic_pointer_cast<Coupon>(basket[i]->legs().front().front());
        auto cpnl = ext::dynamic_pointer_cast<Coupon>(
            *std::next(basket[i]->legs().front().rbegin(), generateNotionalExchanges ? 1 : 0));
        DLOG(i << "," << QuantLib::io::iso_date(basket[i]->exercise()->dates().front()) << "," << cpnf->nominal() << ","
               << QuantLib::io::iso_date(cpnf->accrualStartDate()) << ","
               << QuantLib::io::iso_date(cpnl->accrualEndDate()));
    }

    // set sensi template and pme info

    setSensitivityTemplate(*builder);
    addProductModelEngine(*builder);
}

QuantLib::Real FlexiSwap::notional() const {
    if (notionalTakenFromLeg_ < legs_.size()) {
        Real n = currentNotional(legs_[notionalTakenFromLeg_]);
        if (fabs(n) > QL_EPSILON) {
            return n;
        }
    }
    DLOG("swap does not provide coupon notionals, using face value");
    return notional_;
}

void FlexiSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* swapNode = XMLUtils::getChildNode(node, "FlexiSwapData");
    QL_REQUIRE(swapNode, "FlexiSwap::fromXML(): FlexiSwapData not found");
    // optionality given by lower notional bounds
    for (auto n : XMLUtils::getChildrenNodes(swapNode, "LowerNotionalBounds")) {
        auto ccy = XMLUtils::getAttribute(n, "currency");
        std::vector<std::string> tmpDates;
        auto tmpBounds = XMLUtils::getChildrenValuesWithAttributes<Real>(n, std::string(), "Notional", "startDate",
                                                                         tmpDates, &parseReal);
        lowerNotionalBounds_[ccy] = std::make_pair(tmpBounds, tmpDates);
    }
    // long short flag
    optionLongShort_ = XMLUtils::getChildValue(swapNode, "OptionLongShort", true);
    // underlying legs
    underlyingData_.clear();
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); i++) {
        LegData ld;
        ld.fromXML(nodes[i]);
        underlyingData_.push_back(ld);
    }
}

XMLNode* FlexiSwap::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swapNode = doc.allocNode("FlexiSwapData");
    XMLUtils::appendNode(node, swapNode);
    // optionality given by lower notional bounds
    for (auto const& [ccy, l] : lowerNotionalBounds_) {
        auto n = doc.allocNode("LowerNotionalBounds");
        XMLUtils::addAttribute(doc, n, "currency", ccy);
        XMLUtils::addChildrenWithOptionalAttributes(doc, n, std::string(), "Notional", l.first, "startDate", l.second);
    }
    // long short option flag
    XMLUtils::addChild(doc, swapNode, "OptionLongShort", optionLongShort_);
    // underlying legs
    for (Size i = 0; i < underlyingData_.size(); i++)
        XMLUtils::appendNode(swapNode, underlyingData_[i].toXML(doc));
    return node;
}

std::map<AssetClass, std::set<std::string>>
FlexiSwap::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return getSwapUnderlyingIndices(referenceDataManager, underlyingData_,
                                    envelope().additionalField("security_spread", false));
}

} // namespace data
} // namespace ore
