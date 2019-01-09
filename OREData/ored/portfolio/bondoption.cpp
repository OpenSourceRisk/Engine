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

#include <boost/lexical_cast.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/bondoption.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/bondoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>

#include <ql/errors.hpp> //new
#include <ql/exercise.hpp> //new
#include <ql/instruments/compositeinstrument.hpp> //new
#include <ql/instruments/vanillaoption.hpp> //new

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
    namespace data {

        namespace {
            Leg joinLegs(const std::vector<Leg>& legs) {
                Leg masterLeg;
                for (Size i = 0; i < legs.size(); ++i) {
                    // check if the periods of adjacent legs are consistent
                    if (i > 0) {
                        auto lcpn = boost::dynamic_pointer_cast<Coupon>(legs[i - 1].back());
                        auto fcpn = boost::dynamic_pointer_cast<Coupon>(legs[i].front());
                        QL_REQUIRE(lcpn, "joinLegs: expected coupon as last cashflow in leg #" << (i - 1));
                        QL_REQUIRE(fcpn, "joinLegs: expected coupon as first cashflow in leg #" << i);
                        QL_REQUIRE(lcpn->accrualEndDate() == fcpn->accrualStartDate(),
                            "joinLegs: accrual end date of last coupon in leg #"
                            << (i - 1) << " (" << lcpn->accrualEndDate()
                            << ") is not equal to accrual start date of first coupon in leg #" << i << " ("
                            << fcpn->accrualStartDate() << ")");
                    }
                    // copy legs together
                    masterLeg.insert(masterLeg.end(), legs[i].begin(), legs[i].end());
                }
                return masterLeg;
            }
        } // namespace

        void BondOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
            DLOG("BondOption::build() called for trade " << id());

            const boost::shared_ptr<Market> market = engineFactory->market();

            boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("BondOption"); //new
            boost::shared_ptr<EngineBuilder> builder_bd = engineFactory->builder("Bond");

            // --------------------------------------------------------------------------------

            QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for EquityOption");

            // Payoff
            Option::Type type = parseOptionType(option_.callPut());
            boost::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type, strike_));

            // Only European Vanilla supported for now
            QL_REQUIRE(option_.style() == "European", "Option Style unknown: " << option_.style());
            QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of excercise dates");
            Date expiryDate = parseDate(option_.exerciseDates().front());

            // Exercise
            boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(expiryDate);

            // Vanilla European/American.
            // If price adjustment is necessary we build a simple EU Option
            boost::shared_ptr<QuantLib::Instrument> instrument;

            // QL does not have an EquityOption, so we add a vanilla one here and wrap
            // it in a composite to get the notional in.
            boost::shared_ptr<Instrument> vanilla = boost::make_shared<VanillaOption>(payoff, exercise);

            // Position::Type positionType = parsePositionType(option_.longShort());
            // Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
            // Real mult = quantity_ * bsInd;

            maturity_ = expiryDate;

            // Notional - we really need todays spot to get the correct notional.
            // But rather than having it move around we use strike * quantity
            notional_ = strike_ * quantity_;

            // -------------------------------------------------------------------------------------------------------------

            Date issueDate = parseDate(issueDate_);
            Calendar calendar = parseCalendar(calendar_);
            Natural settlementDays = boost::lexical_cast<Natural>(settlementDays_);
            boost::shared_ptr<QuantLib::Bond> bond; // pointer to the quantlib Bond class; construct the cashflows of that bond
            //boost::shared_ptr<QuantExt::BondOption> bondOption; // pointer to bondOption; both are needed here. but only the bondOption equipped with pricer

            // FIXME: zero bonds are always long (firstLegIsPayer = false, mult = 1.0)
            bool firstLegIsPayer = (coupons_.size() == 0) ? false : coupons_[0].isPayer();
            Real mult = firstLegIsPayer ? -1.0 : 1.0;
            if (zeroBond_) { // Zero coupon bond
                bond.reset(new QuantLib::ZeroCouponBond(settlementDays, calendar, faceAmount_, parseDate(maturityDate_)));
            }
            else { // Coupon bond
                std::vector<Leg> legs;
                for (Size i = 0; i < coupons_.size(); ++i) {
                    bool legIsPayer = coupons_[i].isPayer();
                    QL_REQUIRE(legIsPayer == firstLegIsPayer, "Bond legs must all have same pay/receive flag");
                    if (i == 0)
                        currency_ = coupons_[i].currency();
                    else {
                        QL_REQUIRE(currency_ == coupons_[i].currency(), "leg #" << i << " currency (" << coupons_[i].currency()
                            << ") not equal to leg #0 currency ("
                            << coupons_[0].currency());
                    }
                    Leg leg;
                    auto configuration = builder->configuration(MarketContext::pricing);
                    auto legBuilder = engineFactory->legBuilder(coupons_[i].legType());
                    leg = legBuilder->buildLeg(coupons_[i], engineFactory, configuration);
                    legs.push_back(leg);
                } // for coupons_
                Leg leg = joinLegs(legs);
                bond.reset(new QuantLib::Bond(settlementDays, calendar, issueDate, leg));
                // workaround, QL doesn't register a bond with its leg's cashflows
                for (auto const& c : leg)
                    bond->registerWith(c);
            }

            Currency currency = parseCurrency(currency_);
            boost::shared_ptr<BondEngineBuilder> bondBuilder = boost::dynamic_pointer_cast<BondEngineBuilder>(builder_bd);
            QL_REQUIRE(bondBuilder, "No Builder found for Bond: " << id());
            // here we go with buildng the bondOptionEngine
            boost::shared_ptr<BondOptionEngineBuilder> bondOptionBuilder = boost::dynamic_pointer_cast<BondOptionEngineBuilder>(builder); //new
            QL_REQUIRE(bondOptionBuilder, "No Builder found for bondOption: " << id()); //new

            bond->setPricingEngine(bondBuilder->engine(currency, creditCurveId_, securityId_, referenceCurveId_));
            instrument_.reset(new VanillaInstrument(bond, mult));

            npvCurrency_ = currency_;
            maturity_ = bond->cashflows().back()->date();
            notional_ = currentNotional(bond->cashflows());

            // Add legs (only 1)
            legs_ = { bond->cashflows() };
            legCurrencies_ = { npvCurrency_ };
            legPayers_ = { firstLegIsPayer };

            // -------------------------------------------------------------------------------------------------
            // Handle<SwaptionVolatilityStructure> volatility = engineFactory->market()->swaptionVol(currency_, Market::configuration(MarketContext::pricing));
            // LOG("Implied vol for BondOption on " << securityId_ << " with maturity " << maturity_ << " and strike " << strike_ << " is " << volatility->volatility(maturity_, maturityDate_, strike_));
            // boost::shared_ptr<BondOptionEngineBuilder> bondOptionBuilder = boost::dynamic_pointer_cast<BondOptionEngineBuilder>(builder);
            // vanilla->setPricingEngine(bondOptionBuilder->engine(currency_, creditCurveId_, securityId_, referenceCurveId_));
        }

        void BondOption::fromXML(XMLNode* node) {
            Trade::fromXML(node);

            XMLNode* bondOptionNode = XMLUtils::getChildNode(node, "BondOptionData"); //new
            QL_REQUIRE(bondOptionNode, "No BondOptionData Node"); //new
            option_.fromXML(XMLUtils::getChildNode(bondOptionNode, "OptionData")); //new
            strike_ = XMLUtils::getChildValueAsDouble(bondOptionNode, "Strike", true); //new
            quantity_ = XMLUtils::getChildValueAsDouble(bondOptionNode, "Quantity", true); //new

            XMLNode* bondNode = XMLUtils::getChildNode(node, "BondData");
            QL_REQUIRE(bondNode, "No BondData Node");
            issuerId_ = XMLUtils::getChildValue(bondNode, "IssuerId", true);
            creditCurveId_ =
                XMLUtils::getChildValue(bondNode, "CreditCurveId", false); // issuer credit term structure not mandatory
            securityId_ = XMLUtils::getChildValue(bondNode, "SecurityId", true);
            referenceCurveId_ = XMLUtils::getChildValue(bondNode, "ReferenceCurveId", true);
            settlementDays_ = XMLUtils::getChildValue(bondNode, "SettlementDays", true);
            calendar_ = XMLUtils::getChildValue(bondNode, "Calendar", true);
            issueDate_ = XMLUtils::getChildValue(bondNode, "IssueDate", true);
            XMLNode* legNode = XMLUtils::getChildNode(bondNode, "LegData");
            while (legNode != nullptr) {
                // auto ld = createLegData();
                // ld->fromXML(legNode);
                // coupons_.push_back(*boost::static_pointer_cast<LegData>(ld));
                coupons_.push_back(LegData()); //new
                coupons_.back().fromXML(legNode); //new
                legNode = XMLUtils::getNextSibling(legNode, "LegData");
            }
        }

        // boost::shared_ptr<LegData> Bond::createLegData() const { return boost::make_shared<LegData>(); }

        XMLNode* BondOption::toXML(XMLDocument& doc) {
            XMLNode* node = Trade::toXML(doc);

            XMLNode* bondOptionNode = doc.allocNode("BondOptionData");
            XMLUtils::appendNode(node, bondOptionNode);
            XMLUtils::appendNode(bondOptionNode, option_.toXML(doc));
            XMLUtils::addChild(doc, bondOptionNode, "Strike", strike_);
            XMLUtils::addChild(doc, bondOptionNode, "Quantity", quantity_);

            XMLNode* bondNode = doc.allocNode("BondData");
            XMLUtils::appendNode(node, bondNode);
            XMLUtils::addChild(doc, bondNode, "IssuerId", issuerId_);
            XMLUtils::addChild(doc, bondNode, "CreditCurveId", creditCurveId_);
            XMLUtils::addChild(doc, bondNode, "SecurityId", securityId_);
            XMLUtils::addChild(doc, bondNode, "ReferenceCurveId", referenceCurveId_);
            XMLUtils::addChild(doc, bondNode, "SettlementDays", settlementDays_);
            XMLUtils::addChild(doc, bondNode, "Calendar", calendar_);
            XMLUtils::addChild(doc, bondNode, "IssueDate", issueDate_);
            for (auto& c : coupons_)
                XMLUtils::appendNode(bondNode, c.toXML(doc));
            return node;
        }
    } // namespace data
} // namespace ore
