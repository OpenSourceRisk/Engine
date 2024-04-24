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

#include <ored/portfolio/builders/creditdefaultswap.hpp>
#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void CreditDefaultSwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("CreditDefaultSwap::build() called for trade " << id());

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("Single Name");
    // set isdaSubProduct to entityType in credit reference data
    additionalData_["isdaSubProduct"] = string("");
    string entity = swap_.referenceInformation() ? swap_.referenceInformation()->id() : swap_.creditCurveId();
    QuantLib::ext::shared_ptr<ReferenceDataManager> refData = engineFactory->referenceData();
    if (refData && refData->hasData("Credit", entity)) {
        auto refDatum = refData->getData("Credit", entity);
        QuantLib::ext::shared_ptr<CreditReferenceDatum> creditRefDatum =
            QuantLib::ext::dynamic_pointer_cast<CreditReferenceDatum>(refDatum);
        additionalData_["isdaSubProduct"] = creditRefDatum->creditData().entityType;
        if (creditRefDatum->creditData().entityType == "") {
            ALOG("EntityType is blank in credit reference data for entity " << entity);
        }
    } else {
        ALOG("Credit reference data missing for entity " << entity << ", isdaSubProduct left blank");
    }
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");  

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("CreditDefaultSwap");

    auto legData = swap_.leg(); // copy
    const auto& notionals = swap_.leg().notionals();

    npvCurrency_ = legData.currency();
    notionalCurrency_ = legData.currency();

    QL_REQUIRE(legData.legType() == "Fixed", "CreditDefaultSwap requires Fixed leg");
    Schedule schedule = makeSchedule(legData.schedule());
    QL_REQUIRE(schedule.size() > 1, "CreditDefaultSwap requires at least two dates in the schedule");

    BusinessDayConvention payConvention = Following;
    if (!legData.paymentConvention().empty()) {
        payConvention = parseBusinessDayConvention(legData.paymentConvention());
    }

    Protection::Side prot = legData.isPayer() ? Protection::Side::Buyer : Protection::Side::Seller;
    notional_ = notionals.front();

    DayCounter dc = Actual360();
    if (!legData.dayCounter().empty()) {
        dc = parseDayCounter(legData.dayCounter());
    }

    // In general for CDS and CDS index trades, the standard day counter is Actual/360 and the final
    // period coupon accrual includes the maturity date.
    DayCounter lastPeriodDayCounter;
    auto strLpdc = legData.lastPeriodDayCounter();
    if (!strLpdc.empty()) {
        lastPeriodDayCounter = parseDayCounter(strLpdc);
    } else {
        Actual360 standardDayCounter;
        if (dc == standardDayCounter) {
            lastPeriodDayCounter = dc == standardDayCounter ? Actual360(true) : dc;
            legData.lastPeriodDayCounter() = "A360 (Incl Last)";
        } else {
            lastPeriodDayCounter = dc;
        }
    }

    // Build the coupon leg
    auto legBuilder = engineFactory->legBuilder(legData.legType());
    auto couponLeg =
        legBuilder->buildLeg(legData, engineFactory, requiredFixings_, builder->configuration(MarketContext::pricing));
    // for the accrual rebate calculation we may need historical coupons that are already paid
    requiredFixings_.unsetPayDates();

    /* If we have an indexed leg we don't allow for an upfront fee, since we would need to index that
       as well, btu the ql instrument / engine cdoes not support this currently */
    QL_REQUIRE(legData.indexing().empty() || swap_.upfrontFee() == Null<Real>(),
               "CreditDefaultSwap with indexed coupon leg does not allow for an upfront fee");

    /* The rate is really only used to compute the fair spread in the add results and we support that only
       for fixed coupons with a sinlge rate, otherwise we set this rate to zero */
    Real fixedRate = 0.0;
    if (auto fixedData = QuantLib::ext::dynamic_pointer_cast<FixedLegData>(legData.concreteLegData())) {
        if (fixedData->rates().size() == 1)
            fixedRate = fixedData->rates().front();
    }

    QuantLib::ext::shared_ptr<QuantLib::CreditDefaultSwap> cds;

    if (swap_.upfrontFee() == Null<Real>()) {
        cds = QuantLib::ext::make_shared<QuantLib::CreditDefaultSwap>(
            prot, notional_, couponLeg, fixedRate, schedule, payConvention, dc, swap_.settlesAccrual(),
            swap_.protectionPaymentTime(), swap_.protectionStart(), QuantLib::ext::shared_ptr<Claim>(), lastPeriodDayCounter,
            swap_.rebatesAccrual(), swap_.tradeDate(), swap_.cashSettlementDays());
    } else {
        cds = QuantLib::ext::make_shared<QuantLib::CreditDefaultSwap>(
            prot, notional_, couponLeg, swap_.upfrontFee(), fixedRate, schedule, payConvention, dc,
            swap_.settlesAccrual(), swap_.protectionPaymentTime(), swap_.protectionStart(), swap_.upfrontDate(),
            QuantLib::ext::shared_ptr<Claim>(), lastPeriodDayCounter, swap_.rebatesAccrual(), swap_.tradeDate(),
            swap_.cashSettlementDays());
    }

    maturity_ = cds->coupons().back()->date();

    QuantLib::ext::shared_ptr<CreditDefaultSwapEngineBuilder> cdsBuilder =
        QuantLib::ext::dynamic_pointer_cast<CreditDefaultSwapEngineBuilder>(builder);

    QL_REQUIRE(cdsBuilder, "No Builder found for CreditDefaultSwap: " << id());
    cds->setPricingEngine(cdsBuilder->engine(parseCurrency(npvCurrency_), swap_.creditCurveId(), swap_.recoveryRate()));
    setSensitivityTemplate(*cdsBuilder);

    instrument_.reset(new VanillaInstrument(cds));

    legs_ = {cds->coupons()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {legData.isPayer()};

    if (swap_.protectionStart() != Date())
        additionalData_["startDate"] = to_string(swap_.protectionStart());
    else
        additionalData_["startDate"] = to_string(schedule.dates().front());

    issuer_ = swap_.issuerId();
}

const std::map<std::string, boost::any>& CreditDefaultSwap::additionalData() const {
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

QuantLib::Real CreditDefaultSwap::notional() const {
    Date asof = Settings::instance().evaluationDate();
    // get the current notional from CDS premium leg
    if (!legs_.empty()) {
        for (Size i = 0; i < legs_[0].size(); ++i) {
            QuantLib::ext::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(legs_[0][i]);
            if (coupon->date() > asof)
                return coupon->nominal();
        }
    }
    // if no coupons are given, return the initial notional by default
    return notional_;
}

void CreditDefaultSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* cdsNode = XMLUtils::getChildNode(node, "CreditDefaultSwapData");
    QL_REQUIRE(cdsNode, "No CreditDefaultSwapData Node");
    swap_.fromXML(cdsNode);
}

XMLNode* CreditDefaultSwap::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLUtils::appendNode(node, swap_.toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
