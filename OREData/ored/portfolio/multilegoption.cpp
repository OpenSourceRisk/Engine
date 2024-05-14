/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/multilegoption.hpp>
#include <ored/portfolio/multilegoption.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/log.hpp>

#include <qle/instruments/multilegoption.hpp>
#include <qle/pricingengines/mcmultilegoptionengine.hpp>

#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

using namespace QuantExt;

void MultiLegOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("Building MultiLegOption " << id());

    QL_REQUIRE(!underlyingData_.empty(), "MultiLegOption: no underlying given");

    Position::Type positionType = Position::Long;
    Settlement::Type settleType = Settlement::Cash;
    if (hasOption()) {
        Exercise::Type exerciseType = parseExerciseType(optionData_.style());
        QL_REQUIRE(exerciseType == Exercise::Bermudan || exerciseType == Exercise::European,
                   "MultiLegOption: exercise type must be bermudan or european");
        settleType = parseSettlementType(optionData_.settlement());
        positionType = parsePositionType(optionData_.longShort());
    }

    Date today = Settings::instance().evaluationDate();
    Real multiplier = positionType == Position::Long ? 1.0 : -1.0;

    auto builder =
        QuantLib::ext::dynamic_pointer_cast<MultiLegOptionEngineBuilderBase>(engineFactory->builder("MultiLegOption"));
    QL_REQUIRE(builder != nullptr, "wrong builder, expected multi leg option engine builder");

    // build underlying legs

    // TODO support notional exchanges, fx resets, more leg types
    vector<Leg> underlyingLegs;
    vector<bool> underlyingPayers;
    vector<Currency> underlyingCurrencies;
    legCurrencies_.clear();

    for (Size i = 0; i < underlyingData_.size(); ++i) {
        auto legBuilder = engineFactory->legBuilder(underlyingData_[i].legType());
        underlyingLegs.push_back(legBuilder->buildLeg(underlyingData_[i], engineFactory, requiredFixings_,
                                                      builder->configuration(MarketContext::pricing)));
        underlyingCurrencies.push_back(parseCurrency(underlyingData_[i].currency()));
        legCurrencies_.push_back(underlyingData_[i].currency());
        underlyingPayers.push_back(underlyingData_[i].isPayer());
        DLOG("Added leg of type " << underlyingData_[i].legType() << " in currency " << underlyingData_[i].currency()
                                  << " is payer " << underlyingData_[i].isPayer());
    }

    // build exercise option

    QuantLib::ext::shared_ptr<Exercise> exercise;
    vector<Date> exDates;
    if (hasOption()) {
        for (Size i = 0; i < optionData_.exerciseDates().size(); ++i) {
            Date exDate = parseDate(optionData_.exerciseDates()[i]);
            if (exDate > Settings::instance().evaluationDate())
                exDates.push_back(exDate);
        }
    }
    if (!exDates.empty()) {
        std::sort(exDates.begin(), exDates.end());
        exercise = QuantLib::ext::make_shared<BermudanExercise>(exDates);
        DLOG("Added exercise with " << exDates.size() << " alive exercise dates.");
    } else {
        DLOG("No exercise added, instrument is equal to the underlying");
    }

    // build instrument

    auto multiLegOption = QuantLib::ext::make_shared<QuantExt::MultiLegOption>(underlyingLegs, underlyingPayers,
                                                                       underlyingCurrencies, exercise, settleType);

    DLOG("QLE Instrument built.")

    // extract underlying fixing dates and indices (needed in the engine builder for model calibration below)
    // also add currencies from indices
    vector<Date> underlyingFixingDates;
    vector<QuantLib::ext::shared_ptr<InterestRateIndex>> underlyingIndices;
    vector<Currency> allCurrencies;
    for (auto const& c : underlyingCurrencies) {
        if (std::find(allCurrencies.begin(), allCurrencies.end(), c) == allCurrencies.end()) {
            allCurrencies.push_back(c);
        }
    }
    for (auto const& l : underlyingLegs) {
        for (auto const& c : l) {
            auto flr = QuantLib::ext::dynamic_pointer_cast<QuantLib::FloatingRateCoupon>(c);
            auto cfc = QuantLib::ext::dynamic_pointer_cast<QuantLib::CappedFlooredCoupon>(c);
            if (cfc != nullptr)
                flr = cfc->underlying();
            if (flr != nullptr) {
                if (flr->fixingDate() > today) {
                    underlyingFixingDates.push_back(flr->fixingDate());
                    underlyingIndices.push_back(flr->index());
                    if (std::find(allCurrencies.begin(), allCurrencies.end(), flr->index()->currency()) ==
                        allCurrencies.end())
                        allCurrencies.push_back(flr->index()->currency());
                }
            }
        }
    }

    DLOG("Extracted underlying currencies, indices and fixing dates.")

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    Date lastPremiumDate = addPremiums(additionalInstruments, additionalMultipliers, multiplier,
                                       optionData_.premiumData(), -multiplier, parseCurrency(legCurrencies_.front()),
                                       engineFactory, builder->configuration(MarketContext::pricing));

    // get engine and assign it

    std::sort(allCurrencies.begin(), allCurrencies.end(),
              [](const Currency& a, const Currency& b) { return a.code() < b.code(); });
    auto engine = builder->engine(id(), exDates, multiLegOption->maturityDate(), allCurrencies, underlyingFixingDates,
                                  underlyingIndices);
    multiLegOption->setPricingEngine(engine);
    setSensitivityTemplate(*builder);

    DLOG("Pricing engine set.")

    // build instrument
    // WARNING: we don't support an option wrapper here, i.e. the vanilla simulation will not work properly
    instrument_ =
        QuantLib::ext::make_shared<VanillaInstrument>(multiLegOption, multiplier, additionalInstruments, additionalMultipliers);

    // popular trade members

    legs_ = underlyingLegs;
    legPayers_ = underlyingPayers;
    notional_ = currentNotional(legs_.front());
    maturity_ = std::max(lastPremiumDate, multiLegOption->maturityDate());
    // npv currency is base currency of the pricing model
    auto moe = QuantLib::ext::dynamic_pointer_cast<McMultiLegOptionEngine>(engine);
    QL_REQUIRE(moe != nullptr, "MultiLegOption::build(): expected McMultiLegOptionEngine from engine builder");
    npvCurrency_ = moe->model()->irlgm1f(0)->currency().code();

    DLOG("Building MultiLegOption done");
} // build

void MultiLegOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    underlyingData_.clear();
    optionData_ = OptionData();
    hasOption_ = false;
    XMLNode* n0 = XMLUtils::getChildNode(node, "MultiLegOptionData");
    XMLNode* n1 = XMLUtils::getChildNode(n0, "OptionData");
    if (n1 != nullptr) {
        optionData_.fromXML(n1);
        hasOption_ = true;
    }
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(n0, "LegData");
    for (auto const n : nodes) {
        LegData ld;
        ld.fromXML(n);
        underlyingData_.push_back(ld);
    }
}

XMLNode* MultiLegOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* n0 = doc.allocNode("MultiLegOptionData");
    XMLUtils::appendNode(node, n0);
    if (hasOption_) {
        XMLUtils::appendNode(n0, optionData_.toXML(doc));
        for (auto& d : underlyingData_) {
            XMLUtils::appendNode(n0, d.toXML(doc));
        }
    }
    return node;
}

} // namespace data
} // namespace ore
