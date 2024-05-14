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
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

void populateFromBondReferenceData(std::string& subType,
                                   std::string& issuerId, std::string& settlementDays, std::string& calendar,
                                   std::string& issueDate, std::string& priceQuoteMethod, string& priceQuoteBaseValue,
                                   std::string& creditCurveId, std::string& creditGroup, std::string& referenceCurveId,
                                   std::string& incomeCurveId, std::string& volatilityCurveId,
                                   std::vector<LegData>& coupons, const std::string& name,
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
        if (coupons.size() == 1 && coupons.front().schedule().rules().size() == 1 && coupons.front().schedule().dates().size() == 0) {
	    string oldStart = coupons.front().schedule().rules().front().startDate();
	    coupons.front().schedule().modifyRules().front().modifyStartDate() = startDate;
	    string newStart = coupons.front().schedule().rules().front().startDate();
	    DLOG("Modified start date " << oldStart << " -> " << newStart);
	}
	else {
	  StructuredTradeErrorMessage(bondRefData->bondData().issuerId, "Bond-linked", "update reference data",
                                        "modifified start date cannot be applied to multiple legs/schedules")
                .log();
	}
    }
    if (!endDate.empty()) {
        if (coupons.size() == 1 && coupons.front().schedule().rules().size() == 1 && coupons.front().schedule().dates().size() == 0) {
	    string oldEnd = coupons.front().schedule().rules().front().endDate();
	    coupons.front().schedule().modifyRules().front().modifyEndDate() = endDate;
	    string newEnd = coupons.front().schedule().rules().front().endDate();
	    DLOG("Modified end date " << oldEnd << " -> " << newEnd);
	}
	else {
	  StructuredTradeErrorMessage(bondRefData->bondData().issuerId, "Bond-linked", "update reference data",
                                        "modifified end date cannot be applied to multiple legs/schedules")
                .log();
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

} // namespace data
} // namespace ore
