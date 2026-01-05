/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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


#include <ored/portfolio/bondbasket.hpp>

#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/indexes/fxindex.hpp>

#include <ql/currencies/europe.hpp>
#include <ql/errors.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <qle/termstructures/hazardspreadeddefaulttermstructure.hpp>
#include <ql/math/functional.hpp>
#include <ql/quotes/derivedquote.hpp>

using namespace QuantLib;
using std::string;
using std::vector;

namespace ore {
namespace data {

static DefaultProbKey dummyDefaultProbKey() {
    Currency currency = QuantLib::EURCurrency();
    Seniority seniority = NoSeniority;
    QuantLib::ext::shared_ptr<DefaultType> defaultType(new DefaultType());
    vector<QuantLib::ext::shared_ptr<DefaultType>> defaultTypes(1, defaultType);
    DefaultProbKey key(defaultTypes, currency, seniority);
    return key;
}

static Issuer dummyIssuer(Handle<DefaultProbabilityTermStructure> dts) {
    Issuer::key_curve_pair curve(dummyDefaultProbKey(), dts);
    return Issuer(vector<Issuer::key_curve_pair>(1, curve));
}

QuantLib::ext::shared_ptr<QuantExt::BondBasket> BondBasket::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                                          const QuantLib::Currency& baseCcy,
                                                          const string& reinvestmentEndDate) {
    DLOG("BondBasket::build() called");

    QuantLib::ext::shared_ptr<Pool> pool(new Pool());
    vector<Issuer> issuerPair;
    set<Currency> currencies_unique;

    map<string,  QuantLib::ext::shared_ptr<QuantLib::Bond>> qlBonds;
    map<string, double> recoveries;
    map<string, double> multipliers;
    map<string, Handle<YieldTermStructure>> yieldTermStructures;
    map<string, Currency> currencies;

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();

    for (size_t i = 0; i < bonds_.size(); i++) {
        DLOG("BondBasket::build() -- processing issuer number " << i);

        bonds_[i]->build(engineFactory);
        string creditId = bonds_[i]->bondData().creditCurveId();
        string securityId = bonds_[i]->bondData().securityId();

        Handle<DefaultProbabilityTermStructure> defaultOriginal =
            securitySpecificCreditCurve(market, securityId, creditId)->curve();
        Handle<DefaultProbabilityTermStructure> defaultTS;
        Handle<Quote> recovery;
        try {
            recovery = market->recoveryRate(securityId, Market::defaultConfiguration);
        } catch (...) {
        }

        try {

            Real rr = recovery.empty() ? 0.0 : recovery->value();
            auto m = [rr](Real x) { return x / (1.0 - rr); };
            Handle<Quote> scaledSpread(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(
                market->securitySpread(securityId, Market::defaultConfiguration), m));

            defaultTS = Handle<DefaultProbabilityTermStructure>(
                    QuantLib::ext::make_shared<QuantExt::HazardSpreadedDefaultTermStructure>(defaultOriginal, scaledSpread));

            bonds_[i]->instrument()->qlInstrument()->registerWith(scaledSpread);
            recoveries[bonds_[i]->id()] = rr;

        } catch (...) {
            defaultTS = defaultOriginal;
            recoveries[bonds_[i]->id()] = 0.0;
        }


        issuerPair.push_back(dummyIssuer(defaultTS));
        pool->add(bonds_[i]->id(), issuerPair[i]);
        currencies_unique.insert(parseCurrency(bonds_[i]->bondData().currency()));
        requiredFixings_.addData(bonds_[i]->requiredFixings());

        QuantLib::ext::shared_ptr<QuantLib::Bond> qlBond = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(bonds_[i]->instrument()->qlInstrument());
        QL_REQUIRE(qlBond, "ql bond expected " << bonds_[i]->id());
        qlBonds[bonds_[i]->id()]= qlBond;

        multipliers[bonds_[i]->id()] = bonds_[i]->instrument()->multiplier();
        yieldTermStructures[bonds_[i]->id()] = market->discountCurve(bonds_[i]->bondData().currency());
        currencies[bonds_[i]->id()] = parseCurrency(bonds_[i]->bondData().currency());

    }

    DLOG("pool names");
    for (size_t i = 0; i < pool->names().size(); i++) {
        DLOG("name: " << pool->names()[i] << ", included: " << pool->has(bonds_[i]->id()));
    }

    //create a fx index map for each requred currency...
    for (auto it = currencies_unique.begin(); it!=currencies_unique.end(); ++it){
        if(it->code() != baseCcy.code()){
            string source = it->code();
            string target = baseCcy.code();
            Handle<YieldTermStructure> sorTS = market->discountCurve(source);
            Handle<YieldTermStructure> tarTS = market->discountCurve(target);
            Handle<Quote> spot = market->fxSpot(source + target);
            Calendar cal = NullCalendar();
            Natural fixingDays = 0;

            string name = source + target + "Index";

            QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxi = QuantLib::ext::make_shared<QuantExt::FxIndex>(
                name, fixingDays, parseCurrency(source), baseCcy, cal, spot, sorTS, tarTS);

            fxIndexMap_[source] = fxi;

            DLOG("BondBasket::build() -- created FX Index for " << source + target);
        }
    }

    reinvestment_ = Date().minDate();
    if(reinvestmentEndDate != "")
        reinvestment_ = ore::data::parseDate(reinvestmentEndDate);

    setReinvestmentScalar();

    QuantLib::ext::shared_ptr<QuantExt::BondBasket> basket(
        new QuantExt::BondBasket(qlBonds, recoveries, multipliers, yieldTermStructures, currencies,
                                    pool, baseCcy, fxIndexMap_, reinvestment_, reinvestmentScalar_, flowType_));

    DLOG("BondBasket::build() -- completed");

    return basket;
}

bool BondBasket::isFeeFlow(const ext::shared_ptr<QuantLib::CashFlow>& cf, const std::string& name) {

        //in order to identify as fees, the method expects, (upfront) fees in the form of CashflowData legdata within xml representation
        //this could be dangerous, as 5% upfront and 5% amortisation at the same date could trigger wrong assessment

        bool result = false;
        for(auto& bond : bonds_){
            if(bond->id() == name){
                auto oreBondData = QuantLib::ext::make_shared<ore::data::BondData>(bond->bondData());
                if(oreBondData){
                    for (auto legData : oreBondData->coupons()){
                        auto cfData = QuantLib::ext::dynamic_pointer_cast<ore::data::CashflowData>(legData.concreteLegData());
                        if(cfData){
                            auto amounts = cfData->amounts();
                            auto dates = cfData->dates();
                            for(size_t d = 0; d < dates.size(); d++){
                                if(cf->date() == ore::data::parseDate(dates[d])){
                                    if(cf->amount() == amounts[d])
                                        result = true;
                                }
                            }
                        }
                    }
                }
            }
        }
        return result;
    }

void BondBasket::setReinvestmentScalar(){

    Date today = Settings::instance().evaluationDate();

    for(auto& bond : bonds_){

        string name = bond->id();
        if(bond->maturity() <=  reinvestment_)
            ALOG("bond " << name << " maturity " << io::iso_date(bond->maturity()) << " <= " << "reinvestment end " << io::iso_date(reinvestment_));

        //identify adjustment factor (scalars) -> rescale to todays notional
        const vector<Leg>& legs = bond->legs();
        if(legs.size() > 1)
            ALOG("bond " << name << " has more than one leg, only the first is considered.")
        const Leg& leg = legs[0];

        size_t jmax = 0;    //index for first coupon date after reinvestment period end date
        for (size_t j = 0; j < leg.size(); j++) {
            QuantLib::ext::shared_ptr<QuantLib::Coupon> ptrCoupon =
                QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(leg[j]);
            if(ptrCoupon){
                if(ptrCoupon->date() > reinvestment_){
                    jmax = j;
                    break;
                }
            }
        }

        double baseNotional = bond->notional(); //notional as of today, as set in the ore::data::Bond::build
        vector<double>scalars(leg.size(), 1.0);
        vector<string>flowType(leg.size(), "");
        double currentScalar = 1.0;

        for (size_t j = 0; j < leg.size(); j++) {

            QuantLib::ext::shared_ptr<QuantLib::Coupon> ptrCoupon =
                QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(leg[j]);
            flowType[j] = "xnl"; //notional
            if(isFeeFlow(leg[j], name))
                flowType[j] = "fee"; //fees

            if(ptrCoupon){
                flowType[j] = "int"; //interest
                if(j <= jmax && ptrCoupon->date() >= today){
                    double periodNotional = ptrCoupon->nominal();

                    if(periodNotional < 1e-10)
                        ALOG(name <<  " amortises too early: periodNotional " << periodNotional
                            << " cpnDate " << io::iso_date(ptrCoupon->date())
                            << " (first period after) reinvestment end " << io::iso_date(reinvestment_));

                    if(periodNotional < baseNotional && periodNotional > 1e-10)
                        currentScalar = baseNotional/periodNotional;
                }
            }
            scalars[j] = currentScalar;
        }
        reinvestmentScalar_[name] = scalars;
        flowType_[name] = flowType;
    }
}

void BondBasket::clear() {

    bonds_.clear();
}

void BondBasket::fromXML(XMLNode* node) {

    clear();

    XMLUtils::checkNode(node, "BondBasketData");
    bonds_.clear();
    for (XMLNode* child = XMLUtils::getChildNode(node, "Trade"); child; child = XMLUtils::getNextSibling(child)) {
        string id = XMLUtils::getAttribute(child, "id");
        auto bonddata = QuantLib::ext::make_shared<ore::data::Bond>();
        bonddata->fromXML(child);
        bonddata->id() = id;
        bonds_.push_back(bonddata);
    }
}

XMLNode* BondBasket::toXML(ore::data::XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("BondBasketData");
    for (Size i = 0; i < bonds_.size(); i++) {
        XMLUtils::appendNode(node, bonds_[i]->toXML(doc));
    }
    return node;
}

std::map<AssetClass, std::set<std::string>> BondBasket::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    result[AssetClass::BOND] = {};
    for ( auto b : bonds_)
        result[AssetClass::BOND].insert(b->bondData().securityId());
    return result;
}

} // namespace data
} // namespace orep
