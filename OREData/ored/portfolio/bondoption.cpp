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
#include <ored/portfolio/fixingdates.hpp>
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
#include <ql/errors.hpp> 
#include <ql/exercise.hpp> 
#include <ql/instruments/compositeinstrument.hpp> 
#include <ql/instruments/vanillaoption.hpp> 
#include <ql/instruments/callabilityschedule.hpp> 
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp> 

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
            DLOG("Building Bond Option: " << id());

            const boost::shared_ptr<Market> market = engineFactory->market();

            boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("BondOption");

            Date issueDate = parseDate(issueDate_);
            Calendar calendar = parseCalendar(calendar_);
            Natural settlementDays = boost::lexical_cast<Natural>(settlementDays_);
            boost::shared_ptr<QuantExt::BondOption> bondoption;

            bool firstLegIsPayer = (coupons_.size() == 0) ? false : coupons_[0].isPayer();
            Real mult = firstLegIsPayer ? -1.0 : 1.0;
            if (zeroBond_) { // Zero coupon bond option
                // specila case of coupon bond option
            }
            else { // Coupon bond option
                std::vector<Leg> legs;              
                Rate rate = 0.0;
                std::vector<Rate> rates = std::vector<Rate>(1, rate);
                Real faceAmount = 100;
                Schedule schedule;
                DayCounter daycounter;
                BusinessDayConvention business_dc = Following;
                for (Size i = 0; i < coupons_.size(); ++i) {
                    bool legIsPayer = coupons_[i].isPayer();
                    QL_REQUIRE(legIsPayer == firstLegIsPayer, "Bond legs must all have same pay/receive flag");
                    if (i == 0) {
                        currency_ = coupons_[i].currency();
                        schedule = makeSchedule(coupons_[i].schedule());
                        faceAmount = coupons_[i].notionals().back();
                        daycounter = parseDayCounter(coupons_[i].dayCounter());
                        business_dc = parseBusinessDayConvention(coupons_[i].paymentConvention());
                        if (coupons_[i].legType() == "Fixed") {
                            boost::shared_ptr<FixedLegData> fixedLegData
                                = boost::dynamic_pointer_cast<FixedLegData>(coupons_[i].concreteLegData());
                            rates = buildScheduledVector(fixedLegData->rates(), fixedLegData->rateDates(), schedule);
                        }
                    } else {
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
                Callability::Price callabilityPrice;
                if (priceType_ == "Dirty") {
                    callabilityPrice = Callability::Price(strike(), Callability::Price::Dirty);
                } else {
                    callabilityPrice = Callability::Price(strike(), Callability::Price::Clean);
                }
                Callability::Type callabilityType;
                if (option().callPut() == "Call") {
                    callabilityType = Callability::Call;
                } else {
                    callabilityType = Callability::Put;
                }
                Date exerciseDate = parseDate(option().exerciseDates().back());
                boost::shared_ptr<Callability> callability;
                callability.reset(new Callability(callabilityPrice, callabilityType, exerciseDate));
                CallabilitySchedule callabilitySchedule = std::vector<boost::shared_ptr<Callability>>(1, callability);

                bondoption.reset(new QuantExt::FixedRateBondOption(settlementDays, faceAmount, schedule, rates, 
                    daycounter, business_dc, redemption(), issueDate, callabilitySchedule));
               
                for (auto const& c : leg)
                    bondoption->registerWith(c);
            }

            Currency currency = parseCurrency(currency_);

            boost::shared_ptr<BondOptionEngineBuilder> bondOptionBuilder = boost::dynamic_pointer_cast<BondOptionEngineBuilder>(builder);
            QL_REQUIRE(bondOptionBuilder, "No Builder found for bondOption: " << id());
            
            boost::shared_ptr<BlackBondOptionEngine> blackEngine = boost::dynamic_pointer_cast<BlackBondOptionEngine>(bondOptionBuilder->engine(currency, creditCurveId_, securityId_, referenceCurveId_));
            bondoption -> setPricingEngine(blackEngine);
            if (option().longShort() == "Short") {
                mult = -mult;
            }        
            instrument_.reset(new VanillaInstrument(bondoption, mult));

            npvCurrency_ = currency_;
            maturity_ = bondoption->cashflows().back()->date();
            notional_ = currentNotional(bondoption->cashflows());

            // Add legs (only 1)
            legs_ = { bondoption->cashflows() };
            legCurrencies_ = { npvCurrency_ };
            legPayers_ = { firstLegIsPayer };

            DLOG("Building Bond Option done");
        }

        map<string, set<Date>> BondOption::fixings(const Date& settlementDate) const {
            map<string, set<Date>> result;

            if (legs_.empty() || underlyingIndex_.empty()) {
                WLOG("Need to build BondOption before asking for fixings");
                return result;
            }

            result[underlyingIndex_] = fixingDates(legs_[0], settlementDate);

            return result;
        }

        void BondOption::fromXML(XMLNode* node) {
            Trade::fromXML(node);

            XMLNode* bondOptionNode = XMLUtils::getChildNode(node, "BondOptionData");
            QL_REQUIRE(bondOptionNode, "No BondOptionData Node");
            option_.fromXML(XMLUtils::getChildNode(bondOptionNode, "OptionData"));
            strike_ = XMLUtils::getChildValueAsDouble(bondOptionNode, "Strike", true);
            quantity_ = XMLUtils::getChildValueAsDouble(bondOptionNode, "Quantity", true);
            redemption_ = XMLUtils::getChildValueAsDouble(bondOptionNode, "Redemption", true);
            priceType_ = XMLUtils::getChildValue(bondOptionNode, "PriceType", true);

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

        //boost::shared_ptr<LegData> Bond::createLegData() const { return boost::make_shared<LegData>(); }
        boost::shared_ptr<LegData> BondOption::createLegData() const { return boost::make_shared<LegData>(); } //new

        XMLNode* BondOption::toXML(XMLDocument& doc) {
            XMLNode* node = Trade::toXML(doc);

            XMLNode* bondOptionNode = doc.allocNode("BondOptionData");
            XMLUtils::appendNode(node, bondOptionNode);
            XMLUtils::appendNode(bondOptionNode, option_.toXML(doc));
            XMLUtils::addChild(doc, bondOptionNode, "Strike", strike_);
            XMLUtils::addChild(doc, bondOptionNode, "Quantity", quantity_);
            XMLUtils::addChild(doc, bondOptionNode, "Redemption", redemption_);
            XMLUtils::addChild(doc, bondOptionNode, "PriceType", priceType_);

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
