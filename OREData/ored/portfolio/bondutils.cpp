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
#include <ored/portfolio/convertiblebondreferencedata.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

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
    if (subType.empty()) {
        subType = bondRefData->bondData().subType;
        TLOG("overwrite subType with '" << subType << "'");
    }
    if (issuerId.empty()) {
        issuerId = bondRefData->bondData().issuerId;
        TLOG("overwrite issuerId with '" << issuerId << "'");
    }
    if (settlementDays.empty()) {
        settlementDays = bondRefData->bondData().settlementDays;
        TLOG("overwrite settlementDays with '" << settlementDays << "'");
    }
    if (calendar.empty()) {
        calendar = bondRefData->bondData().calendar;
        TLOG("overwrite calendar with '" << calendar << "'");
    }
    if (issueDate.empty()) {
        issueDate = bondRefData->bondData().issueDate;
        TLOG("overwrite issueDate with '" << issueDate << "'");
    }
    if (priceQuoteMethod.empty()) {
        priceQuoteMethod = bondRefData->bondData().priceQuoteMethod;
        TLOG("overwrite priceQuoteMethod with '" << priceQuoteMethod << "'");
    }
    if (priceQuoteBaseValue.empty()) {
        priceQuoteBaseValue = bondRefData->bondData().priceQuoteBaseValue;
        TLOG("overwrite priceQuoteBaseValue with '" << priceQuoteBaseValue << "'");
    }
    if (creditCurveId.empty()) {
        creditCurveId = bondRefData->bondData().creditCurveId;
        TLOG("overwrite creditCurveId with '" << creditCurveId << "'");
    }
    if (creditGroup.empty()) {
        creditGroup = bondRefData->bondData().creditGroup;
        TLOG("overwrite creditGroup with '" << creditGroup << "'");
    }
    if (referenceCurveId.empty()) {
        referenceCurveId = bondRefData->bondData().referenceCurveId;
        TLOG("overwrite referenceCurveId with '" << referenceCurveId << "'");
    }
    if (incomeCurveId.empty()) {
        incomeCurveId = bondRefData->bondData().incomeCurveId;
        TLOG("overwrite incomeCurveId with '" << incomeCurveId << "'");
    }
    if (volatilityCurveId.empty()) {
        volatilityCurveId = bondRefData->bondData().volatilityCurveId;
        TLOG("overwrite volatilityCurveId with '" << volatilityCurveId << "'");
    }
    if (coupons.empty()) {
        coupons = bondRefData->bondData().legData;
        TLOG("overwrite coupons with " << coupons.size() << " LegData nodes");
    }
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

void populateFromBondFutureReferenceData(string& currency, string& contractMonth, string& deliverableGrade,
                                         string& fairPrice, string& settlement, string& settlementDirty,
                                         string& rootDate, string& expiryBasis, string& settlementBasis,
                                         string& expiryLag, string& settlementLag, string& lastTrading,
                                         string& lastDelivery, vector<string>& secList,
                                         const ext::shared_ptr<BondFutureReferenceDatum>& bondFutureRefData) {
    DLOG("populating data bondfuture from reference data");
    if (currency.empty()) {
        currency = bondFutureRefData->bondFutureData().currency;
        TLOG("overwrite currency with '" << currency << "'");
    }
    if (contractMonth.empty()) {
        contractMonth = bondFutureRefData->bondFutureData().contractMonth;
        TLOG("overwrite contractMonth with '" << contractMonth << "'");
    }
    if (deliverableGrade.empty()) {
        deliverableGrade = bondFutureRefData->bondFutureData().deliverableGrade;
        TLOG("overwrite deliverableGrade with '" << deliverableGrade << "'");
    }
    if (fairPrice.empty()) {
        fairPrice = bondFutureRefData->bondFutureData().fairPrice;
        TLOG("overwrite fairPrice with '" << fairPrice << "'");
    }
    if (settlement.empty()) {
        settlement = bondFutureRefData->bondFutureData().settlement;
        TLOG("overwrite settlement with '" << settlement << "'");
    }
    if (settlementDirty.empty()) {
        settlementDirty = bondFutureRefData->bondFutureData().settlementDirty;
        TLOG("overwrite settlementDirty with '" << settlementDirty << "'");
    }
    if (rootDate.empty()) {
        rootDate = bondFutureRefData->bondFutureData().rootDate;
        TLOG("overwrite rootDate with '" << rootDate << "'");
    }
    if (expiryBasis.empty()) {
        expiryBasis = bondFutureRefData->bondFutureData().expiryBasis;
        TLOG("overwrite expiryBasis with '" << expiryBasis << "'");
    }
    if (settlementBasis.empty()) {
        settlementBasis = bondFutureRefData->bondFutureData().settlementBasis;
        TLOG("overwrite settlementBasis with '" << settlementBasis << "'");
    }
    if (expiryLag.empty()) {
        expiryLag = bondFutureRefData->bondFutureData().expiryLag;
        TLOG("overwrite expiryLag with '" << expiryLag << "'");
    }
    if (settlementLag.empty()) {
        settlementLag = bondFutureRefData->bondFutureData().settlementLag;
        TLOG("overwrite settlementLag with '" << settlementLag << "'");
    }
    if (lastTrading.empty()) {
        lastTrading = bondFutureRefData->bondFutureData().lastTrading;
        TLOG("overwrite lastTrading with '" << lastTrading << "'");
    }
    if (lastDelivery.empty()) {
        lastDelivery = bondFutureRefData->bondFutureData().lastDelivery;
        TLOG("overwrite lastDelivery with '" << lastDelivery << "'");
    }
    if (secList.empty()) {
        secList = bondFutureRefData->bondFutureData().secList;
        TLOG("overwrite secList ");
    }
    DLOG("populating bondfuture data from reference data done.");
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
    } else if (refData->hasData(ConvertibleBondReferenceDatum::TYPE, id)) {
        return ConvertibleBondReferenceDatum::TYPE;
    }

    return std::string();
}

StructuredSecurityId::StructuredSecurityId(const std::string& id) : id_(id) {
    std::size_t ind = id_.find("_FUTURE_");
    if (ind == std::string::npos) {
        securityId_ = id_;
    } else {
        securityId_ = id_.substr(0, ind);
        futureContract_ = id_.substr(ind + 8);
    }
}

StructuredSecurityId::StructuredSecurityId(const std::string& securityId, const std::string& futureContract,
                                           const QuantLib::Date& expiryDate)
    : securityId_(securityId), futureContract_(futureContract), futureExpiryDate_(futureExpiryDate) {
    if (futureContract.empty()) {
        id_ = securityId_;
    } else {
        id_ = securityId_ + "_FUTURE_" + futureContract_;
    }
}

std::pair<QuantLib::Date, QuantLib::Date>
BondFutureUtils::deduceBondFutureDates(const std::string& currency, const std::string& contractMonth,
                                       const std::string& rootDateStr, const std::string& expiryBasis,
                                       const std::string& settlementBasis, const std::string& expiryLag,
                                       const std::string& settlementLag) {
    Month contractMonth_ql = parseMonth(contractMonth);
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
        QL_REQUIRE(tokens.size() == 2, "RootDate " << rootDate_ << " unexpected");
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
        QL_FAIL("expected either expiry or settlement or both to start with root");
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
        return FutureType::LongTenorUS;
    else if (val_up == "ZB" || val_up == "US")
        return FutureType::LongTenorUS;
    else if (val_up == "TWE")
        return FutureType::LongTenorUS;
    else if (val_up == "TN" || val_up == "UXY")
        return FutureType::LongTenorUS;
    else if (val_up == "ZN" || val_up == "TY")
        return FutureType::LongTenorUS;
    else if (val_up == "ZF" || val_up == "FV")
        return FutureType::ShortTenorUS;
    else if (val_up == "Z3N" || val_up == "3Y")
        return FutureType::ShortTenorUS;
    else if (val_up == "ZT" || val_up == "TU")
        return FutureType::ShortTenorUS;
    else
        QL_FAIL("BondFutureUtils::getBondFutureType(): FutureType " << val_up << " unkown");
}

void BondFutureUtils::checkDates(const QuantLib::Date& expiry, const QuantLib::Date& settlement) {
    Date asof = Settings::instance().evaluationDate();
    if (settlementDate < expiry)
        QL_FAIL("BondFutureUtils::checkDates(): for " << id() << " settlement date " << io::iso_date(settlementDate)
                                                      << " lies before expiry " << io::iso_date(expiry));
    if (expiry < asof)
        QL_FAIL("BondFutureUtils::checkDates(): for " << id() << " is expired, asof " << io::iso_date(asof)
                                                      << " vs. expiry " << io::iso_date(expiry));
    // refine + 9 * Months ...
    if (asof + 9 * Months < expiry)
        WLOG("BondFutureUtils::checkDates(): for "
             << id() << " expiry may be not in standard cycle of next three quarters " << io::iso_date(expiry)
             << " vs asof " << io::iso_date(asof));
    DLOG("BondFutureUtils::checkDates(): for " << id() << " ExpiryDate " << io::iso_date(expiry) << " SettlementDate "
                                               << io::iso_date(settlementDate));
}

static double BondFutureUtils::conversionFactor(double coupon, const FutureType& type, const Date& bondMaturity,
                                                const Date& futureExpiry) {

    QL_REQUIRE(type == LongTenorUS || type == ShortTenorUS,
               "BondFutureUtils::conversionFactor(): type " << static_cast<int>(type) << " not supported.");

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
    if (type == FutureType::LongTenorUS) {
        int mod = z % 3;
        z -= mod;
    }

    // 2) calculation
    double v;
    if (z < 7) {
        v = z;
    } else {
        if (type == FutureType::LongTenorUS)
            v = 3.0;
        else if (type == FutureType::ShortTenorUS)
            v = (z - 6.0);
        else
            QL_FAIL("FutureType " << type << " unkown");
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

static std::pair<std::string, double>
BondFutureUtils::identifyCtdBond(const ext::shared_ptr<EngineFactory>& engineFactory,
                                 const std::string& futureContract) {

    DLOG("BondFuture::identifyCtdBond called.");
    // get settlement price for future
    double settlementPriceFuture = getSettlementPriceFuture(engineFactory);

    double lowestValue = QL_MAX_REAL; // arbitray high number
    string ctdSec = string();
    double ctdCf = Real();

    for (const auto& sec : secList_) {
        // create bond instrument
        auto bond = BondFactory::instance().build(engineFactory, engineFactory->referenceData(), sec);
        // get value at expiry
        QuantLib::ext::shared_ptr<QuantExt::DiscountingRiskyBondEngine> drbe =
            QuantLib::ext::dynamic_pointer_cast<QuantExt::DiscountingRiskyBondEngine>(bond.bond->pricingEngine());
        QL_REQUIRE(drbe != nullptr, "could not find DiscountingRiskyBondEngine, unexpected");
        double cleanBondPriceAtExpiry =
            drbe->calculateNpv(expiry, expiry, bond.bond->cashflows()).npv - bond.bond->accruedAmount(expiry) / 100.0;

        // get conversion factor
        double conversionFactor = Real();
        try {
            conversionFactor = engineFactory->market()->conversionFactor(sec)->value();
        } catch (const std::exception& e) {
            DLOG("no conversion factor provided from market, start calculation");
            if (parseCurrency(currency_) == USDCurrency()) {
                // get fixed rate
                double coupon = Real();
                if (bond.bondData.coupons().size() == 1 &&
                    bond.bondData.coupons().front().concreteLegData()->legType() == LegType::Fixed) {
                    auto fixedLegData =
                        ext::dynamic_pointer_cast<FixedLegData>(bond.bondData.coupons().front().concreteLegData());
                    QL_REQUIRE(fixedLegData, "expecting FixedLegData object");
                    if (fixedLegData->rates().size() > 1)
                        ALOG("calc conversionFactor: there is a vector of rates, took the first. sec " << sec);
                    coupon = fixedLegData->rates().front();
                } else {
                    QL_FAIL("expected bond " << sec << " with one leg " << bond.bondData.coupons().size()
                                             << " of LegType::Fixed "
                                             << bond.bondData.coupons().front().concreteLegData()->legType());
                }
                Date endDate = Date();
                try {
                    endDate = parseDate(bond.bondData.coupons().data()->schedule().rules().data()->endDate());
                } catch (const std::exception& e) {
                    endDate = bond.trade->maturity();
                    ALOG("endDate for conversionfactor is set to maturity, given the calendar adjustment, this can "
                         "lead to inaccuracy.")
                }
                conversionFactor = conversionfactor_usd(coupon, selectTypeUS(deliverableGrade_), endDate, expiry);
                DLOG("calculated conversionFactor " << conversionFactor << " secId " << sec << " cpn " << coupon
                                                    << " deliverableGrade " << deliverableGrade_ << " secMat "
                                                    << io::iso_date(endDate) << " futureExpiry "
                                                    << io::iso_date(expiry));
            } else {
                QL_FAIL("Conversion factor calculation for other currency than USD not supported ");
            }
        }
        // do the test, inspired by
        //  HULL: OPTIONS, FUTURES, AND OTHER DERIVATIVES, 7th Edition, page 134
        double value = cleanBondPriceAtExpiry - settlementPriceFuture * conversionFactor;
        DLOG("BondFuture::identifyCtdBond underlying " << sec << " cleanBondPriceAtExpiry " << cleanBondPriceAtExpiry
                                                       << " conversionFactor " << conversionFactor << " Value "
                                                       << value);
        if (value < lowestValue) {
            lowestValue = value;
            ctdSec = sec;
            ctdCf = conversionFactor;
        }
    }
    QL_REQUIRE(!ctdSec.empty(), "No CTD bond found.");
    DLOG("BondFuture::identifyCtdBond -- selected CTD for " << id() << " is " << ctdSec);

    return make_pair(ctdSec, ctdCf);
}

} // namespace data
} // namespace ore
