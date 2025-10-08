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

#include <ored/portfolio/bondutils.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/callablebondreferencedata.hpp>
#include <ored/portfolio/convertiblebondreferencedata.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/log.hpp>

#include <qle/pricingengines/forwardenabledbondengine.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>

namespace ore {
namespace data {

template <typename T> void overwrite(const string& label, T& current, const T& ref) {
    if (current.empty()) {
        current = ref;
        TLOG("overwrite field " + label + " with reference data");
    }
}

template <> void overwrite(const string& label, string& current, const string& ref) {
    if (current.empty()) {
        current = ref;
        TLOG("overwrite field " + label + " with reference data value " + ref);
    }
}

void populateFromBondReferenceData(std::string& subType, std::string& issuerId, std::string& settlementDays,
                                   std::string& calendar, std::string& issueDate, std::string& priceQuoteMethod,
                                   string& priceQuoteBaseValue, std::string& creditCurveId, std::string& creditGroup,
                                   std::string& referenceCurveId, std::string& incomeCurveId,
                                   std::string& volatilityCurveId, std::vector<LegData>& coupons,
                                   std::optional<QuantLib::Bond::Price::Type>& quotedDirtyPrices,
                                   const std::string& name,
                                   const QuantLib::ext::shared_ptr<BondReferenceDatum>& bondRefData,
                                   const std::string& startDate, const std::string& endDate) {
    DLOG("populating data bond from reference data");
    QL_REQUIRE(bondRefData, "populateFromBondReferenceData(): empty bond reference datum given");
    overwrite("subType", subType, bondRefData->bondData().subType);
    overwrite("issuerId", issuerId, bondRefData->bondData().issuerId);
    overwrite("settlementDays", settlementDays, bondRefData->bondData().settlementDays);
    overwrite("calendar", calendar, bondRefData->bondData().calendar);
    overwrite("issueDate", issueDate, bondRefData->bondData().issueDate);
    overwrite("priceQuoteMethod", priceQuoteMethod, bondRefData->bondData().priceQuoteMethod);
    overwrite("priceQuoteBaseValue", priceQuoteBaseValue, bondRefData->bondData().priceQuoteBaseValue);
    overwrite("creditCurveId", creditCurveId, bondRefData->bondData().creditCurveId);
    overwrite("creditGroup", creditGroup, bondRefData->bondData().creditGroup);
    overwrite("referenceCurveId", referenceCurveId, bondRefData->bondData().referenceCurveId);
    overwrite("incomeCurveId", incomeCurveId, bondRefData->bondData().incomeCurveId);
    overwrite("volatilityCurveId", volatilityCurveId, bondRefData->bondData().volatilityCurveId);
    overwrite("coupons", coupons, bondRefData->bondData().legData);
    if (!startDate.empty()) {
        if (coupons.size() == 1 && coupons.front().schedule().rules().size() == 1 &&
            coupons.front().schedule().dates().size() == 0) {
            string oldStart = coupons.front().schedule().rules().front().startDate();
            coupons.front().schedule().modifyRules().front().modifyStartDate() = startDate;
            string newStart = coupons.front().schedule().rules().front().startDate();
            DLOG("Modified start date " << oldStart << " -> " << newStart);
        } else {
            StructuredTradeErrorMessage(bondRefData->bondData().issuerId, "Bond-linked", "update reference data",
                                        "modifified start date cannot be applied to multiple legs/schedules")
                .log();
        }
    }
    if (!endDate.empty()) {
        if (coupons.size() == 1 && coupons.front().schedule().rules().size() == 1 &&
            coupons.front().schedule().dates().size() == 0) {
            string oldEnd = coupons.front().schedule().rules().front().endDate();
            coupons.front().schedule().modifyRules().front().modifyEndDate() = endDate;
            string newEnd = coupons.front().schedule().rules().front().endDate();
            DLOG("Modified end date " << oldEnd << " -> " << newEnd);
        } else {
            StructuredTradeErrorMessage(bondRefData->bondData().issuerId, "Bond-linked", "update reference data",
                                        "modifified end date cannot be applied to multiple legs/schedules")
                .log();
        }
    }
    if (!quotedDirtyPrices) {
        quotedDirtyPrices = bondRefData->bondData().quotedDirtyPrices;
        if (!quotedDirtyPrices) {
            quotedDirtyPrices = QuantLib::Bond::Price::Type::Clean;
            DLOG("the PriceType is being defaulted to 'Clean'.");
        }
    }

    DLOG("populating bond data from reference data done.");
}

Date getOpenEndDateReplacement(const std::string& replacementPeriodStr, const Calendar& calendar) {
    if (replacementPeriodStr.empty())
        return QuantLib::Null<QuantLib::Date>();
    Date today = Settings::instance().evaluationDate();
    Date result = Date::maxDate() - 365;
    try {
        // might throw because we are beyond the last allowed date
        result = (calendar.empty() ? NullCalendar() : calendar).advance(today, parsePeriod(replacementPeriodStr));
    } catch (...) {
    }
    DLOG("Compute open end date replacement as "
         << QuantLib::io::iso_date(result) << " (today = " << QuantLib::io::iso_date(today)
         << ", OpenEndDateReplacement from pricing engine config = " << replacementPeriodStr << ")");
    return result;
}

std::string getBondReferenceDatumType(const std::string& id,
                                      const QuantLib::ext::shared_ptr<ReferenceDataManager>& refData) {
    if (refData == nullptr)
        return std::string();

    if (refData->hasData(BondReferenceDatum::TYPE, id)) {
        return BondReferenceDatum::TYPE;
    } else if (refData->hasData(CallableBondReferenceDatum::TYPE, id)) {
        return CallableBondReferenceDatum::TYPE;
    } else if (refData->hasData(ConvertibleBondReferenceDatum::TYPE, id)) {
        return ConvertibleBondReferenceDatum::TYPE;
    }

    return std::string();
}

StructuredSecurityId::StructuredSecurityId(const std::string& id) : id_(id) {
    if (std::size_t ind = id_.find("_FUTURE_"); ind != std::string::npos) {
        securityId_ = id_.substr(0, ind);
        futureContract_ = id_.substr(ind + 8);
    } else if (std::size_t ind = id_.find("_FWDEXP_"); ind != std::string::npos) {
        securityId_ = id_.substr(0, ind);
        forwardExpiry_ = id_.substr(ind + 8);
    } else {
        securityId_ = id_;
    }
}

StructuredSecurityId::StructuredSecurityId(const std::string& securityId, const std::string& futureContract)
    : securityId_(securityId), futureContract_(futureContract) {
    if (!futureContract.empty()) {
        id_ = securityId_ + "_FUTURE_" + futureContract_;
    } else if (!forwardExpiry_.empty()) {
        id_ = securityId_ + "_FWDEXP_" + forwardExpiry_;
    } else {
        id_ = securityId_;
    }
}

std::pair<QuantLib::Date, QuantLib::Date>
BondFutureUtils::deduceDates(const boost::shared_ptr<BondFutureReferenceDatum>& refData) {
    Date expiry, settlement;
    if (!refData->bondFutureData().lastTrading.empty())
        expiry = parseDate(refData->bondFutureData().lastTrading);
    if (!refData->bondFutureData().lastDelivery.empty())
        settlement = parseDate(refData->bondFutureData().lastDelivery);
    if (expiry == Date() || settlement == Date()) {
        try {
        auto [tmpExpiry, tmpSettlement] =
            deduceDates(refData->bondFutureData().currency, refData->bondFutureData().contractMonth,
                        refData->bondFutureData().rootDate, refData->bondFutureData().expiryBasis,
                        refData->bondFutureData().settlementBasis, refData->bondFutureData().expiryLag,
                        refData->bondFutureData().settlementLag);
        if (expiry == Date())
            expiry = tmpExpiry;
        if (settlement == Date())
            settlement = tmpSettlement;
        } catch(const std::exception&) {
            QL_FAIL("BondFutureUtils::deduceDates(): failed to deduce dates for contract '" << refData->id() << "'");
        }
    }
    return std::make_pair(expiry, settlement);
}

std::pair<QuantLib::Date, QuantLib::Date>
BondFutureUtils::deduceDates(const std::string& currency, const std::string& contractMonth,
                             const std::string& rootDateStr, const std::string& expiryBasis,
                             const std::string& settlementBasis, const std::string& expiryLag,
                             const std::string& settlementLag) {
    Month contractMonth_ql;
    QL_REQUIRE(tryParse(contractMonth, contractMonth_ql,
                        std::function<QuantLib::Month(const std::string&)>(
                            [](const std::string& s) { return parseMonth(s); })),
               "BondFutureUtils::deduceDates(): can not parse month '" << contractMonth << "'");
    Date asof = Settings::instance().evaluationDate();
    int year = asof.year();
    if (asof.month() > contractMonth_ql)
        year++;

    Calendar cal = parseCalendar(currency);

    // calc root date
    Date rootDate;
    vector<string> tokens;
    boost::split(tokens, rootDateStr, boost::is_any_of(","));
    string day = boost::to_upper_copy(tokens[0]);
    if (day == "FIRST")
        rootDate = cal.adjust(Date(1, contractMonth_ql, year), QuantLib::Following);
    else if (day == "END")
        rootDate = cal.endOfMonth(Date(1, contractMonth_ql, year), QuantLib::Preceding);
    else { // nth weekday case expected (example format 'Fri,2' for second Friday)
        QL_REQUIRE(tokens.size() == 2, "BondFutureUtils::deduceDates(): RootDate " << rootDateStr << " unexpected");
        int n = parseInteger(tokens[1]);
        Weekday day = parseWeekday(tokens[0]);
        rootDate = Date::nthWeekday(n, day, contractMonth_ql, year);
    }

    // now calc expiry // settlement from root date
    string expiryBasis_up = boost::to_upper_copy(expiryBasis);
    string settlementBasis_up = boost::to_upper_copy(settlementBasis);
    Period expiryLag_ql = 0 * Days;
    if (!expiryLag.empty())
        expiryLag_ql = parsePeriod(expiryLag);
    Period settlementLag_ql = 0 * Days;
    if (!settlementLag.empty())
        settlementLag_ql = parsePeriod(settlementLag);
    BusinessDayConvention bdc_expiry = QuantLib::Following;
    if (expiryLag_ql < 0 * Days)
        bdc_expiry = QuantLib::Preceding;
    BusinessDayConvention bdc_settle = QuantLib::Following;
    if (settlementLag_ql < 0 * Days)
        bdc_settle = QuantLib::Preceding;

    Date expiry, settlementDate;

    if (expiryBasis_up == "ROOT" && settlementBasis_up == "EXPIRY") {
        expiry = cal.advance(rootDate, expiryLag_ql, bdc_expiry);
        settlementDate = cal.advance(expiry, settlementLag_ql, bdc_settle);
    } else if (settlementBasis_up == "ROOT" && expiryBasis_up == "SETTLEMENT") {
        settlementDate = cal.advance(rootDate, settlementLag_ql, bdc_settle);
        expiry = cal.advance(settlementDate, expiryLag_ql, bdc_expiry);
    } else if (expiryBasis_up == "ROOT" && settlementBasis_up == "ROOT") {
        expiry = cal.advance(rootDate, expiryLag_ql, bdc_expiry);
        settlementDate = cal.advance(rootDate, settlementLag_ql, bdc_settle);
    } else {
        QL_FAIL("BondFutureUtils::deduceDates(): expected either expiry or settlement or both to start with root");
    }

    return std::make_pair(expiry, settlementDate);
}

BondFutureUtils::BondFutureType BondFutureUtils::getBondFutureType(const std::string& deliverableGrade) {

    //                         Deliverable Maturities	    CME Globex  Bloomberg
    // 2-Year T-Note                 1 3/4 to 2 years	           ZT          TU
    // 3-Year T-Note                  9/12 to 3 years	          Z3N          3Y
    // 5-Year T-Note             4 1/6 to 5 1/4 years	           ZF          FV
    // 10-Year T-Note                6 1/2 to 8 years	           ZN          TY
    // Ultra 10-Year T-Note 	   9 5/12 to 10 Years	           TN         UXY
    // T-Bond                 15 years up to 25 years	           ZB          US
    // 20-Year T-Bond       19 2/12 to 19 11/12 years	          TWE         TWE
    // Ultra T-Bond	         25 years to 30 years	           UB          WN
    // source: https://www.cmegroup.com/trading/interest-rates/basics-of-us-treasury-futures.html

    string val_up = boost::to_upper_copy(deliverableGrade);

    if (val_up == "UB" || val_up == "WN")
        return BondFutureUtils::BondFutureType::LongTenorUS;
    else if (val_up == "ZB" || val_up == "US")
        return BondFutureUtils::BondFutureType::LongTenorUS;
    else if (val_up == "TWE")
        return BondFutureUtils::BondFutureType::LongTenorUS;
    else if (val_up == "TN" || val_up == "UXY")
        return BondFutureUtils::BondFutureType::LongTenorUS;
    else if (val_up == "ZN" || val_up == "TY")
        return BondFutureUtils::BondFutureType::LongTenorUS;
    else if (val_up == "ZF" || val_up == "FV")
        return BondFutureUtils::BondFutureType::ShortTenorUS;
    else if (val_up == "Z3N" || val_up == "3Y")
        return BondFutureUtils::BondFutureType::ShortTenorUS;
    else if (val_up == "ZT" || val_up == "TU")
        return BondFutureUtils::BondFutureType::ShortTenorUS;
    else
        QL_FAIL("BondFutureUtils::getBondFutureType(): FutureType '" << val_up << "' unkown");
}

void BondFutureUtils::checkDates(const QuantLib::Date& expiry, const QuantLib::Date& settlementDate) {
    Date asof = Settings::instance().evaluationDate();
    if (settlementDate < expiry)
        QL_FAIL("BondFutureUtils::checkDates(): settlement date " << io::iso_date(settlementDate)
                                                                  << " lies before expiry " << io::iso_date(expiry));
    if (expiry < asof)
        QL_FAIL("BondFutureUtils::checkDates(): asof " << io::iso_date(asof) << " vs. expiry " << io::iso_date(expiry));
    // refine + 9 * Months ...
    if (asof + 9 * Months < expiry)
        WLOG("BondFutureUtils::checkDates(): expiry may be not in standard cycle of next three quarters "
             << io::iso_date(expiry) << " vs asof " << io::iso_date(asof));
    DLOG("BondFutureUtils::checkDates(): expiryDate " << io::iso_date(expiry) << " SettlementDate "
                                                      << io::iso_date(settlementDate));
}

namespace {
double conversionFactorUSD(const double coupon, const BondFutureUtils::BondFutureType type, const Date& futureExpiry,
                           const Date& bondMaturity) {

    // inspired by:
    // CME GROUP, Calculating U.S. Treasury Futures Conversion Factors
    // https://www.cmegroup.com/trading/interest-rates/files/Calculating_U.S.Treasury_Futures_Conversion_Factors.pdf

    // 1) derive dates ...

    // z is the number of whole months between n and the maturity (or call) date
    // rounded down to the nearest quarter for UB, ZB, TWE, TN and ZN,
    // and to the nearest month for ZF, Z3N, and ZT

    // n is the number of whole years from the first day of the
    // delivery month to the maturity (or call) date of the bond or note.

    int full_months = (bondMaturity.year() - futureExpiry.year()) * 12 + (bondMaturity.month() - futureExpiry.month());
    int n = (int)std::floor(full_months / 12);
    int z = full_months % 12;

    // rounded down to the nearest quarter
    if (type == BondFutureUtils::BondFutureType::LongTenorUS) {
        int mod = z % 3;
        z -= mod;
    }

    // 2) calculation
    double v;
    if (z < 7) {
        v = z;
    } else {
        if (type == BondFutureUtils::BondFutureType::LongTenorUS)
            v = 3.0;
        else if (type == BondFutureUtils::BondFutureType::ShortTenorUS)
            v = (z - 6.0);
        else
            QL_FAIL("FutureType '" << type << "' unkown");
    }

    double a = 1.0 / std::pow(1.03, v / 6.0);
    double b = (coupon / 2.0) * (6.0 - v) / 6.0;
    double c;
    if (z < 7) {
        c = 1.0 / std::pow(1.03, 2.0 * n);
    } else {
        c = 1.0 / std::pow(1.03, 2.0 * n + 1.0);
    }
    double d = (coupon / 0.06) * (1 - c);
    double factor = a * ((coupon / 2.0) + c + d) - b;

    // where the factor is rounded to four decimal places
    return std::round(factor * 10000) / 10000;
}
} // namespace

double BondFutureUtils::conversionFactor(const BondFutureUtils::BondFutureType& type, const Date& futureExpiry,
                                         const double fixedRate, const Date bondMaturity) {

    QL_REQUIRE(type == LongTenorUS || type == ShortTenorUS,
               "BondFutureUtils::conversionFactor(): type "
                   << static_cast<int>(type) << " not supported - conversion factor has to be supplied as marketdata.");
    return conversionFactorUSD(fixedRate, type, futureExpiry, bondMaturity);
}

std::pair<std::string, double> BondFutureUtils::identifyCtdBond(const ext::shared_ptr<EngineFactory>& engineFactory,
                                                                const std::string& futureContract,
                                                                const bool noPricing) {

    DLOG("BondFutureUtils::identifyCtdBond() called.");

    double lowestValue = QL_MAX_REAL;
    string ctdSec;
    double ctdCf;

    QL_REQUIRE(engineFactory->referenceData()->hasData("BondFuture", futureContract),
               "BondFutureUtils::identifyCtdBond(): no bond future reference data found for " << futureContract);

    auto refData = QuantLib::ext::dynamic_pointer_cast<BondFutureReferenceDatum>(
        engineFactory->referenceData()->getData("BondFuture", futureContract));

    for (const auto& sec : refData->bondFutureData().deliveryBasket) {

        auto b = BondFactory::instance().build(engineFactory, engineFactory->referenceData(), sec);

        Real settlementPriceFuture =
            engineFactory->market()->securityPrice(StructuredSecurityId(sec, futureContract))->value();

        Date expiry = deduceDates(refData).first;
        double bondPriceAtExpiry =
            noPricing ? 1.0 : QuantExt::forwardPrice(b.bond, expiry, b.bond->settlementDate(expiry), true).second;

        if (refData->bondFutureData().dirtyQuotation.empty() || !parseBool(refData->bondFutureData().dirtyQuotation)) {
            bondPriceAtExpiry -=
                b.bond->accruedAmount(b.bond->settlementDate(expiry)) / 100.0 * b.bond->notional(expiry);
        }

        if (close_enough(b.bond->notional(expiry), 0.0))
            bondPriceAtExpiry = 0.0;
        else
            bondPriceAtExpiry /= b.bond->notional(expiry);

        double conversionFactor;
        try {
            conversionFactor =
                engineFactory->market()->conversionFactor(StructuredSecurityId(sec, futureContract))->value();
        } catch (const std::exception&) {
            DLOG("no conversion factor provided from market, calculate internally");
            QL_REQUIRE(!b.bond->cashflows().empty(), "BondFutureUtils::identifyCtdBond(): bond has no coupons");
            auto cpn = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(b.bond->cashflows().front());
            QL_REQUIRE(cpn, "BondFutureUtils::identifyCtdBond(): could not cast first bond coupon to FixedRateCoupon - "
                            "can not calculate conversion factor.");
            try {
            conversionFactor = BondFutureUtils::conversionFactor(
                BondFutureUtils::getBondFutureType(refData->bondFutureData().deliverableGrade), expiry, cpn->rate(),
                parseDate(b.bondData.maturityDate()));
            } catch (const std::exception& e) {
                QL_FAIL("BondFutureUtils::identifyCtdBond(): conversion factor for "
                        << sec << " in future contract " << futureContract
                        << " could not be retrieved from market data and can not be calculated (" << e.what()
                        << "). Add conversion factor to market data or check why it can not be calculated.");
            }
        }

        // see e.g. Hull, Options, Futures and other derivatives, 7th Edition, page 134
        double value = bondPriceAtExpiry - settlementPriceFuture * conversionFactor;
        DLOG(sec << " bondPriceAtExpiry " << bondPriceAtExpiry << " settlementPriceFuture " << settlementPriceFuture
                 << " conversionFactor " << conversionFactor << " -> value " << value);
        if (value < lowestValue) {
            lowestValue = value;
            ctdSec = sec;
            ctdCf = conversionFactor;
            DLOG("this underlying is new cheapeast bond");
        }
    }

    QL_REQUIRE(!ctdSec.empty(), "BondFutureUtils::identifyCtdBond(): no ctd bond found.");

    DLOG("BondFutureUtils::identifyCtdBond() finished, selected ctd bond for " << futureContract << " is " << ctdSec);

    return make_pair(ctdSec, ctdCf);
}

void BondFutureUtils::modifyToForwardBond(const Date& expiry, QuantLib::ext::shared_ptr<QuantLib::Bond>& bond,
                                             const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                             const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                                             const std::string& securityId) {
    DLOG("BondFutureUtils::modifyToForwardBond called for " << securityId);

    StructuredSecurityId structuredSecurityId(securityId);

    QL_REQUIRE(!structuredSecurityId.forwardExpiry().empty(),
               "BondFutureUtils::modifyToForwardBond(): no forward expiry found in '" << securityId << "'");
    QL_REQUIRE(structuredSecurityId.futureContract().empty(),
               "BondFutureUtils::modifyToForwardBond(): should not be called for future-specific securities ("
                   << securityId << ")");

    QL_REQUIRE(getBondReferenceDatumType(structuredSecurityId.securityId(), referenceData) == BondReferenceDatum::TYPE,
               "BondFutureUtils::modifyToForwardBond(): not implemented for bond type "
                   << getBondReferenceDatumType(structuredSecurityId.securityId(), referenceData));

    // truncate legs akin to fwd bond method...
    Leg modifiedLeg;
    for (auto& cf : bond->cashflows()) {
        if (!cf->hasOccurred(expiry))
            modifiedLeg.push_back(cf);
    }

    // uses old CTOR, so we can pass the notional flow deduces above, otherwise we get the notional flows twice
    QuantLib::ext::shared_ptr<QuantLib::Bond> modifiedBond = QuantLib::ext::make_shared<QuantLib::Bond>(
        bond->settlementDays(), bond->calendar(), 1.0, bond->maturityDate(), bond->issueDate(), modifiedLeg);

    // retrieve additional required information
    BondData data(securityId, 1.0);
    data.populateFromBondReferenceData(referenceData);

    // Set pricing engine
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("Bond");
    QuantLib::ext::shared_ptr<BondEngineBuilder> bondBuilder =
        QuantLib::ext::dynamic_pointer_cast<BondEngineBuilder>(builder);
    QL_REQUIRE(bondBuilder, "No Builder found for Bond: " << securityId);
    modifiedBond->setPricingEngine(bondBuilder->engine(parseCurrency(data.currency()), data.creditCurveId(), securityId,
                                                       data.referenceCurveId(), data.incomeCurveId()));

    // store modified bond
    bond = modifiedBond;
}


} // namespace data
} // namespace ore
