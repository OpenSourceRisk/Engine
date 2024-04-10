/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ored/portfolio/commodityposition.hpp>
#include <ored/portfolio/tradefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/conventionsbasedfutureexpiry.hpp>

namespace ore {
namespace data {

void CommodityPositionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CommodityPositionData");
    quantity_ = XMLUtils::getChildValueAsDouble(node, "Quantity", true);
    auto c = XMLUtils::getChildrenNodes(node, "Underlying");
    underlyings_.clear();
    for (auto const n : c) {
        underlyings_.push_back(CommodityUnderlying());
        underlyings_.back().fromXML(n);
    }
}

XMLNode* CommodityPositionData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("CommodityPositionData");
    XMLUtils::addChild(doc, n, "Quantity", quantity_);
    for (auto& u : underlyings_) {
        XMLUtils::appendNode(n, u.toXML(doc));
    }
    return n;
}

void CommodityPosition::build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) {

    // ISDA taxonomy: not a derivative, but define the asset class at least
    // so that we can determine a TRS asset class that has an EQ position underlying
    additionalData_["isdaAssetClass"] = string("Commodity");
    additionalData_["isdaBaseProduct"] = string("");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    DLOG("CommodityPosition::build() called for " << id());
    QL_REQUIRE(!data_.underlyings().empty(), "CommodityPosition::build(): no underlyings given");
    indices_.clear();
    weights_.clear();
    fxConversion_.clear();

    std::vector<std::string> currencies;
    for (auto const& u : data_.underlyings()) {
        auto pts = engineFactory->market()->commodityPriceCurve(u.name(), engineFactory->configuration(MarketContext::pricing));
        QL_REQUIRE(!pts.empty() && *pts != nullptr, "CommodityPosition, curve missing for '"<<u.name()<<"'");
        QL_REQUIRE(!pts->currency().empty(),
                   "CommodityPosition, Currency not set in curve config for commodity curve'" << u.name() << "'. Skip this trade.");
        auto index = parseCommodityIndex(u.name(), false, pts, NullCalendar(), u.priceType() == "FutureSettlement");
        QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
        pair<bool, QuantLib::ext::shared_ptr<Convention>> p = conventions->get(u.name(), Convention::Type::CommodityFuture);
        if (u.priceType() == "FutureSettlement"  && p.first) {
            auto convention = QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(p.second);
            ConventionsBasedFutureExpiry feCalc(*convention);
            Date expiry = Settings::instance().evaluationDate();
            Size nOffset = u.futureMonthOffset() == Null<Size>() ? 0 : u.futureMonthOffset();
            if (u.deliveryRollDays() != Null<Size>()) {
                auto cal =
                    u.deliveryRollCalendar().empty() ? convention->calendar() : parseCalendar(u.deliveryRollCalendar());
                expiry = cal.advance(expiry, u.deliveryRollDays() * Days, convention->businessDayConvention());
            }
            expiry = feCalc.nextExpiry(true, expiry, nOffset);
            if (!u.futureContractMonth().empty()) {
                QL_REQUIRE(u.futureContractMonth().size() == 7,
                           "FutureContractMonth has invalid format, please use MonYYYY, where 'Mon' is a 3 letter "
                           "month abbreviation.");
                auto month = parseMonth(u.futureContractMonth().substr(0, 3));
                auto year = parseInteger(u.futureContractMonth().substr(3, 4));
                Date contractDate(1, month, year);
                expiry = feCalc.expiryDate(contractDate, nOffset, false);
            } else if (!u.futureExpiryDate().empty()) {
                expiry = parseDate(u.futureExpiryDate());
                expiry = feCalc.nextExpiry(true, expiry, nOffset, false);
            }
            index = index->clone(expiry, pts);
        }
        indices_.push_back(index);
        weights_.push_back(u.weight());
        currencies.push_back(pts->currency().code());
    }

    // get fx quotes

    isSingleCurrency_ = true;
    npvCurrency_ = currencies.front();
    for (auto const& c : currencies) {
        // we use fxSpot() as opposed to fxRate() here to ensure consistency between NPV() and the fixing of an
        // equivalent index representing the same basket
        fxConversion_.push_back(
            engineFactory->market()->fxSpot(c + npvCurrency_, engineFactory->configuration(MarketContext::pricing)));
        if (npvCurrency_ != c)
            isSingleCurrency_ = false;
    }

    // set instrument
    auto qlInstr =
        QuantLib::ext::make_shared<CommodityPositionInstrumentWrapper>(data_.quantity(), indices_, weights_, fxConversion_);
    qlInstr->setPricingEngine(QuantLib::ext::make_shared<CommodityPositionInstrumentWrapperEngine>());
    setSensitivityTemplate(std::string());
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(qlInstr);

    // no sensible way to set these members
    maturity_ = Date::maxDate();
    notional_ = Null<Real>();
    notionalCurrency_ = "";
    
    // leave legs empty
}

void CommodityPosition::setNpvCurrencyConversion(const std::string& ccy, const Handle<Quote>& conversion) {
    npvCurrency_ = ccy;
    QuantLib::ext::static_pointer_cast<CommodityPositionInstrumentWrapper>(instrument_->qlInstrument())
        ->setNpvCurrencyConversion(conversion);
}

void CommodityPosition::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    data_.fromXML(XMLUtils::getChildNode(node, "CommodityPositionData"));
}

XMLNode* CommodityPosition::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLUtils::appendNode(node, data_.toXML(doc));
    return node;
}

std::map<AssetClass, std::set<std::string>>
CommodityPosition::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    for (auto const& u : data_.underlyings()) {
        result[AssetClass::COM].insert(u.name());
    }
    return result;
}

CommodityPositionInstrumentWrapper::CommodityPositionInstrumentWrapper(
    const Real quantity, const std::vector<QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>>& commodities,
    const std::vector<Real>& weights, const std::vector<Handle<Quote>>& fxConversion)
    : quantity_(quantity), commodities_(commodities), weights_(weights), fxConversion_(fxConversion) {
    QL_REQUIRE(commodities_.size() == weights_.size(), "CommodityPositionInstrumentWrapper: commodities size ("
                                                           << commodities_.size() << ") must match weights size ("
                                                        << weights_.size() << ")");
    QL_REQUIRE(fxConversion_.empty() || fxConversion_.size() == commodities_.size(),
               "CommodityPositionInstrumentWrapper: fxConversion size ("
                   << fxConversion_.size() << ") must match commodities size (" << commodities_.size() << ")");
    for (auto const& i : commodities_)
        registerWith(i);
    for (auto const& f : fxConversion_)
        registerWith(f);
    registerWith(npvCcyConversion_);
}

void CommodityPositionInstrumentWrapper::setNpvCurrencyConversion(const Handle<Quote>& npvCcyConversion) {
    npvCcyConversion_ = npvCcyConversion;
}

bool CommodityPositionInstrumentWrapper::isExpired() const { return false; }

void CommodityPositionInstrumentWrapper::setupExpired() const { Instrument::setupExpired(); }

void CommodityPositionInstrumentWrapper::setupArguments(PricingEngine::arguments* args) const {
    CommodityPositionInstrumentWrapper::arguments* a = dynamic_cast<CommodityPositionInstrumentWrapper::arguments*>(args);
    QL_REQUIRE(a != nullptr, "wrong argument type in CommodityPositionInstrumentWrapper");
    a->quantity_ = quantity_;
    a->commodities_ = commodities_;
    a->weights_ = weights_;
    a->fxConversion_ = fxConversion_;
    a->npvCcyConversion_ = npvCcyConversion_;
}

void CommodityPositionInstrumentWrapper::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);
}

void CommodityPositionInstrumentWrapperEngine::calculate() const {
    Real result = 0.0;
    Date today = Settings::instance().evaluationDate();
    for (Size i = 0; i < arguments_.commodities_.size(); ++i) {
        // TODO: if refering spot price need lookup the spot date instead of today
        // for future settlement the fixing date is not relvant, always lookup future expiry date
        Real tmp = arguments_.quantity_ * arguments_.commodities_[i]->fixing(today, true);
        if (!arguments_.fxConversion_[i].empty()) {
            tmp *= arguments_.fxConversion_[i]->value();
        }
        result += tmp * arguments_.weights_[i];
    }
    if (!arguments_.npvCcyConversion_.empty()) {
        result *= arguments_.npvCcyConversion_->value();
    }
    results_.value = result;
}

} // namespace data
} // namespace ore
