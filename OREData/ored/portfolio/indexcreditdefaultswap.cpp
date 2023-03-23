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

#include <ored/portfolio/builders/indexcreditdefaultswap.hpp>
#include <ored/portfolio/indexcreditdefaultswap.hpp>
#include <qle/instruments/indexcreditdefaultswap.hpp>

#include <ored/portfolio/legbuilders.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void IndexCreditDefaultSwap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("IndexCreditDefaultSwap::build() called for trade " << id());

    const boost::shared_ptr<Market> market = engineFactory->market();
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("IndexCreditDefaultSwap");

    QL_REQUIRE(swap_.leg().legType() == "Fixed", "IndexCreditDefaultSwap requires Fixed leg");
    boost::shared_ptr<FixedLegData> fixedLegData =
        boost::dynamic_pointer_cast<FixedLegData>(swap_.leg().concreteLegData());

    auto configuration = builder->configuration(MarketContext::pricing);
    FixedLegBuilder flb;
    legs_.resize(1);
    legs_[0] = flb.buildLeg(swap_.leg(), engineFactory, requiredFixings_, configuration);

    Schedule schedule = makeSchedule(swap_.leg().schedule());
    BusinessDayConvention payConvention = parseBusinessDayConvention(swap_.leg().paymentConvention());
    Protection::Side prot = swap_.leg().isPayer() ? Protection::Side::Buyer : Protection::Side::Seller;

    notional_ = notional();
    DayCounter dc = parseDayCounter(swap_.leg().dayCounter());

    // In general for CDS and CDS index trades, the standard day counter is Actual/360 and the final
    // period coupon accrual includes the maturity date.
    Actual360 standardDayCounter;
    DayCounter lastPeriodDayCounter = dc == standardDayCounter ? Actual360(true) : dc;

    QL_REQUIRE(fixedLegData->rates().size() == 1, "IndexCreditDefaultSwap requires single rate");

    Real indexFactor = 1.0;

    // From the basket data or reference data, we need a vector of notionals and a vector of creditCurves
    vector<Real> basketNotionals;
    vector<string> basketCreditCurves;

    if (!swap_.basket().constituents().empty()) {

        const auto& constituents = swap_.basket().constituents();
        DLOG("Building constituents from basket data containing " << constituents.size() << " elements.");

        Real totalNtl = 0.0;
        for (const auto& c : constituents) {
            Real ntl = Null<Real>();

            const auto& creditCurve = c.creditCurveId();

            if (c.weightInsteadOfNotional()) {
                ntl = c.weight() * notional_;
            } else {
                ntl = c.notional();
            }

            if (!close(0.0, ntl) && ntl > 0.0) {
                if (std::find(basketCreditCurves.begin(), basketCreditCurves.end(), creditCurve) ==
                    basketCreditCurves.end()) {
                    DLOG("Adding underlying " << creditCurve << " with notional " << ntl);
                    constituents_[creditCurve] = ntl;
                    basketCreditCurves.push_back(creditCurve);
                    basketNotionals.push_back(ntl);
                    totalNtl += ntl;
                } else {
                    ALOG(StructuredTradeErrorMessage(id(), "IndexCDS", "Error building trade",
                                                     ("Invalid Basket: found a duplicate credit curve " + creditCurve +
                                                      ", skip it. Check the basket data for possible errors.")
                                                         .c_str()));
                }

            } else {
                DLOG("Skipped adding underlying, " << creditCurve << ", because its notional, " << ntl
                                                   << ", was non-positive.");
            }
        }
        QL_REQUIRE(basketCreditCurves.size() == basketNotionals.size(),
                   "numbers of defaults curves (" << basketCreditCurves.size() << ") and notionals ("
                                                  << basketNotionals.size() << ") doesnt match");
        DLOG("All underlyings added, total notional = " << totalNtl);

        if (totalNtl > notional_ * (1.0 + 1.0E-4)) {
            ALOG(StructuredTradeErrorMessage(id(), "IndexCDS", "Error building trade",
                                             ("Sum of basket notionals (" + std::to_string(totalNtl) +
                                              ") is greater than trade notional (" + std::to_string(notional_) +
                                              "). Check the basket data for possible errors.")
                                                 .c_str()));
        }

        indexFactor = totalNtl / notional_;

        DLOG("Finished building constituents using basket data.");

    } else {
        // get data from ReferenceDatum
        string id = ore::data::splitCurveIdWithTenor(swap_.creditCurveId()).first;
        DLOG("Getting CreditIndexReferenceDatum for id " << id);
        QL_REQUIRE(engineFactory->referenceData(), "No BasketData or ReferenceDataManager");
        QL_REQUIRE(engineFactory->referenceData()->hasData(CreditIndexReferenceDatum::TYPE, id),
                   "No CreditIndex reference data for " << id);
        boost::shared_ptr<ReferenceDatum> refData =
            engineFactory->referenceData()->getData(CreditIndexReferenceDatum::TYPE, id);
        boost::shared_ptr<CreditIndexReferenceDatum> creditRefData =
            boost::dynamic_pointer_cast<CreditIndexReferenceDatum>(refData);
        DLOG("Got CreditIndexReferenceDatum for id " << id);

        Real totalWeight = 0.0;
        for (const auto& c : creditRefData->constituents()) {

            const auto& name = c.name();
            auto weight = c.weight();

            if (!close(0.0, weight) && weight > 0.0) {
                DLOG("Adding underlying " << name << " with weight " << weight);
                constituents_[name] = notional_ * weight;
                basketCreditCurves.push_back(name);
                basketNotionals.push_back(notional_ * weight);
                totalWeight += weight;
            } else {
                DLOG("Skipped adding underlying, " << name << ", because its weight, " << weight
                                                   << ", was non-positive.");
            }
        }

        indexFactor = totalWeight;

        DLOG("All underlyings added, total weight = " << totalWeight);

        if (!close(1.0, totalWeight) && totalWeight > 1.0) {
            ALOG("Total weight is greater than 1, possible error in CreditIndexReferenceDatum");
        }
    }

    boost::shared_ptr<QuantExt::IndexCreditDefaultSwap> cds;
    if (swap_.upfrontFee() == Null<Real>()) {
        cds = boost::make_shared<QuantExt::IndexCreditDefaultSwap>(
            prot, indexFactor * notional_, basketNotionals, fixedLegData->rates().front(), schedule, payConvention, dc,
            swap_.settlesAccrual(), swap_.protectionPaymentTime(), swap_.protectionStart(), boost::shared_ptr<Claim>(),
            lastPeriodDayCounter, true, swap_.tradeDate(), swap_.cashSettlementDays());
    } else {
        cds = boost::make_shared<QuantExt::IndexCreditDefaultSwap>(
            prot, indexFactor * notional_, basketNotionals, swap_.upfrontFee(), fixedLegData->rates().front(), schedule,
            payConvention, dc, swap_.settlesAccrual(), swap_.protectionPaymentTime(), swap_.protectionStart(),
            swap_.upfrontDate(), boost::shared_ptr<Claim>(), lastPeriodDayCounter, true, swap_.tradeDate(),
            swap_.cashSettlementDays());
    }

    boost::shared_ptr<IndexCreditDefaultSwapEngineBuilder> cdsBuilder =
        boost::dynamic_pointer_cast<IndexCreditDefaultSwapEngineBuilder>(builder);

    npvCurrency_ = swap_.leg().currency();
    notionalCurrency_ = swap_.leg().currency();

    QL_REQUIRE(cdsBuilder, "No Builder found for IndexCreditDefaultSwap: " << id());
    std::string curveIdWithTerm = swap_.creditCurveIdWithTerm();
    // warn if the term can not be implied, except when a  custom baskets is defined
    if (swap_.basket().constituents().empty() && splitCurveIdWithTenor(curveIdWithTerm).second == 0 * Days) {
        ALOG(StructuredTradeWarningMessage(
            id(), tradeType(), "Could not imply Index CDS term.",
            "Index CDS term could not be derived from start, end date, are these dates correct (credit curve id is '" +
                swap_.creditCurveId() + "')"));
    }

    cds->setPricingEngine(cdsBuilder->engine(parseCurrency(npvCurrency_), swap_.creditCurveIdWithTerm(),
                                             basketCreditCurves, boost::none, swap_.recoveryRate()));

    instrument_.reset(new VanillaInstrument(cds));

    maturity_ = cds->coupons().back()->date();

    legs_ = {cds->coupons()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {swap_.leg().isPayer()};

    if (swap_.protectionStart() != Date())
        additionalData_["startDate"] = to_string(swap_.protectionStart());
    else
        additionalData_["startDate"] = to_string(schedule.dates().front());

    sensitivityDecomposition_ = cdsBuilder->sensitivityDecomposition();

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("Index");
    // Deferring the mapping of creditCurveId to CDX, LCDX, MCDX, iTraxx, ABX, CMBX, IOS, MBX, PO, PrimeX, TRX, SP
    additionalData_["isdaSubProduct"] = swap_.creditCurveId(); 
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");  

}

const std::map<std::string, boost::any>& IndexCreditDefaultSwap::additionalData() const {
    setLegBasedAdditionalData(0, 2);
    additionalData_["legNPV[1]"] = instrument_->qlInstrument()->result<Real>("protectionLegNPV");
    additionalData_["legNPV[2]"] = instrument_->qlInstrument()->result<Real>("premiumLegNPVDirty") +
                                   instrument_->qlInstrument()->result<Real>("upfrontPremiumNPV") +
                                   instrument_->qlInstrument()->result<Real>("accrualRebateNPV");
    additionalData_["isPayer[1]"] = !swap_.leg().isPayer();
    additionalData_["isPayer[2]"] = swap_.leg().isPayer();
    additionalData_["legType[2]"] = swap_.leg().legType();
    additionalData_["legType[1]"] = std::string("Protection");
    additionalData_["currentNotional[1]"] = additionalData_["currentNotional[2]"];
    additionalData_["originalNotional[1]"] = additionalData_["originalNotional[2]"];
    additionalData_["notionalCurrency[1]"] = notionalCurrency_;
    additionalData_["notionalCurrency[2]"] = notionalCurrency_;
    return additionalData_;
}


QuantLib::Real IndexCreditDefaultSwap::notional() const {
    Date asof = Settings::instance().evaluationDate();
    // get the current notional from the premium leg
    for (Size i = 0; i < legs_[0].size(); ++i) {
        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(legs_[0][i]);
        if (coupon->date() > asof)
            return coupon->nominal();
    }

    // if not provided, return null
    ALOG("Error retrieving current notional for index credit default swap " << id() << " as of " << io::iso_date(asof));
    return Null<Real>();
}

void IndexCreditDefaultSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* cdsNode = XMLUtils::getChildNode(node, "IndexCreditDefaultSwapData");
    QL_REQUIRE(cdsNode, "No IndexCreditDefaultSwapData Node");
    swap_.fromXML(cdsNode);
}

XMLNode* IndexCreditDefaultSwap::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLUtils::appendNode(node, swap_.toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
