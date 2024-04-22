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

#include <ored/portfolio/bondoption.hpp>
#include <ored/portfolio/builders/bondoption.hpp>

#include <boost/lexical_cast.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondutils.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <ql/instruments/callabilityschedule.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void BondOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("Building Bond Option: " << id());
    
    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("Option");
    additionalData_["isdaSubProduct"] = string("Debt Option");
    additionalData_["isdaTransaction"] = string("");  

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("BondOption");
    bondData_ = originalBondData_;
    bondData_.populateFromBondReferenceData(engineFactory->referenceData());

    Calendar calendar = parseCalendar(bondData_.calendar());
    QuantLib::ext::shared_ptr<QuantExt::BondOption> bondoption;

    // FIXME this won't work for zero bonds (but their implementation is incomplete anyhow, see bond.cpp)
    underlying_ = QuantLib::ext::make_shared<ore::data::Bond>(Envelope(), bondData_);

    underlying_->build(engineFactory);

    legs_ = underlying_->legs();
    legCurrencies_ = underlying_->legCurrencies();
    legPayers_ = std::vector<bool>(legs_.size(), false); // always receive (long option view)
    npvCurrency_ = underlying_->bondData().currency();
    notional_ = underlying_->notional() * bondData_.bondNotional();
    notionalCurrency_ = underlying_->bondData().currency();
    maturity_ = std::max(optionData_.premiumData().latestPremiumDate(), underlying_->maturity());

    auto qlBondInstr = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(underlying_->instrument()->qlInstrument());
    QL_REQUIRE(qlBondInstr, "BondOption::build(): could not cast to QuantLib::Bond");
    for (auto const p : underlying_->legPayers()) {
        QL_REQUIRE(!p, "BondOption::build(): underlying leg must be receiver");
    }

    boost::variant<QuantLib::Bond::Price, QuantLib::InterestRate> callabilityPrice;
    if (strike_.type() == TradeStrike::Type::Price) {
        if (priceType_ == "Dirty") {
            callabilityPrice = QuantLib::Bond::Price(strike_.value(), QuantLib::Bond::Price::Dirty);
        } else if (priceType_ == "Clean") {
            callabilityPrice = QuantLib::Bond::Price(strike_.value(), QuantLib::Bond::Price::Clean);
        } else {
            QL_FAIL("BondOption::build(): price type \"" << priceType_ << "\" not recognised.");
        }
    } else {
        // strike is quoted as a yield
        DayCounter dayCounter = Actual365Fixed();
        Frequency freq = Annual;
        // attempt to get the daycounter and frequency from the bond
        if (bondData_.coupons().size() > 0) {
            auto cn = bondData_.coupons().front();
            const string& dc = cn.dayCounter();
            if (!dc.empty()) 
                dayCounter = parseDayCounter(dc);
            if (cn.schedule().rules().size() > 0)
                freq = parsePeriod(cn.schedule().rules().front().tenor()).frequency();
        }
        callabilityPrice = QuantLib::InterestRate(strike_.value(), dayCounter, strike_.compounding(), freq);
    }    

    Callability::Type callabilityType =
        parseOptionType(optionData_.callPut()) == Option::Call ? Callability::Call : Callability::Put;

    QL_REQUIRE(optionData_.exerciseDates().size() == 1,
               "BondOption::build(): exactly one option date required, found " << optionData_.exerciseDates().size());
    Date exerciseDate = parseDate(optionData_.exerciseDates().back());
    QuantLib::ext::shared_ptr<Callability> callability;
    callability.reset(new Callability(callabilityPrice, callabilityType, exerciseDate));
    CallabilitySchedule callabilitySchedule = std::vector<QuantLib::ext::shared_ptr<Callability>>(1, callability);

    bondoption.reset(new QuantExt::BondOption(qlBondInstr, callabilitySchedule, knocksOut_));

    Currency currency = parseCurrency(underlying_->bondData().currency());

    QuantLib::ext::shared_ptr<BondOptionEngineBuilder> bondOptionBuilder =
        QuantLib::ext::dynamic_pointer_cast<BondOptionEngineBuilder>(builder);
    QL_REQUIRE(bondOptionBuilder, "No Builder found for bondOption: " << id());

    QuantLib::ext::shared_ptr<BlackBondOptionEngine> blackEngine = QuantLib::ext::dynamic_pointer_cast<BlackBondOptionEngine>(
        bondOptionBuilder->engine(id(), currency, bondData_.creditCurveId(), bondData_.hasCreditRisk(),
                                  bondData_.securityId(), bondData_.referenceCurveId(), bondData_.volatilityCurveId()));
    bondoption->setPricingEngine(blackEngine);
    setSensitivityTemplate(*bondOptionBuilder);

    Real multiplier =
        bondData_.bondNotional() * (parsePositionType(optionData_.longShort()) == Position::Long ? 1.0 : -1.0);

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    addPremiums(additionalInstruments, additionalMultipliers, multiplier, optionData_.premiumData(),
                multiplier > 0.0 ? -1.0 : 1.0, currency, engineFactory,
                bondOptionBuilder->configuration(MarketContext::pricing));

    instrument_.reset(new VanillaInstrument(bondoption, multiplier, additionalInstruments, additionalMultipliers));
    
    // the required fixings are (at most) those of the underlying
    requiredFixings_ = underlying_->requiredFixings();
}

void BondOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);

    XMLNode* bondOptionNode = XMLUtils::getChildNode(node, "BondOptionData");
    QL_REQUIRE(bondOptionNode, "No BondOptionData Node");
    optionData_.fromXML(XMLUtils::getChildNode(bondOptionNode, "OptionData"));
    strike_.fromXML(bondOptionNode, true, true);
    redemption_ = XMLUtils::getChildValueAsDouble(bondOptionNode, "Redemption", false, 100.0);
    // don't need PriceType id a yield strike is provided
    if (strike_.type() == TradeStrike::Type::Price)
        priceType_ = XMLUtils::getChildValue(bondOptionNode, "PriceType", true);
    if (auto n = XMLUtils::getChildNode(bondOptionNode, "KnocksOut"))
        knocksOut_ = parseBool(XMLUtils::getNodeValue(n));
    else
        knocksOut_ = false;

    originalBondData_.fromXML(XMLUtils::getChildNode(bondOptionNode, "BondData"));
    bondData_ = originalBondData_;
}

XMLNode* BondOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);

    XMLNode* bondOptionNode = doc.allocNode("BondOptionData");
    XMLUtils::appendNode(node, bondOptionNode);
    XMLUtils::appendNode(bondOptionNode, optionData_.toXML(doc));
    XMLUtils::appendNode(bondOptionNode, strike_.toXML(doc));
    XMLUtils::addChild(doc, bondOptionNode, "Redemption", redemption_);
    if (!priceType_.empty())
        XMLUtils::addChild(doc, bondOptionNode, "PriceType", priceType_);
    XMLUtils::addChild(doc, bondOptionNode, "KnocksOut", knocksOut_);

    XMLUtils::appendNode(bondOptionNode, originalBondData_.toXML(doc));
    return node;
}

std::map<AssetClass, std::set<std::string>>
BondOption::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    result[AssetClass::BOND] = {bondData_.securityId()};
    return result;
}
} // namespace data
} // namespace ore
