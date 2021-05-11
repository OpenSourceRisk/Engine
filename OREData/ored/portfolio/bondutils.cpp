/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
  All rights reserved.
*/

#include <ored/portfolio/bondutils.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

void populateFromBondReferenceData(std::string& issuerId, std::string& settlementDays, std::string& calendar,
                                   std::string& issueDate, std::string& creditCurveId, std::string& referenceCurveId,
                                   std::string& proxySecurityId, std::string& incomeCurveId,
                                   std::string& volatilityCurveId, std::vector<LegData>& coupons,
                                   const std::string& name, const boost::shared_ptr<BondReferenceDatum>& bondRefData, 
                                   bool hasCreditRisk) {
    QL_REQUIRE(bondRefData, "populateFromBondReferenceData(): empty bond reference datum given");
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
    if (creditCurveId.empty() && hasCreditRisk) {
        creditCurveId = bondRefData->bondData().creditCurveId;
        TLOG("overwrite creditCurveId with '" << creditCurveId << "'");
    }
    if (referenceCurveId.empty()) {
        referenceCurveId = bondRefData->bondData().referenceCurveId;
        TLOG("overwrite referenceCurveId with '" << referenceCurveId << "'");
    }
    if (proxySecurityId.empty()) {
        proxySecurityId = bondRefData->bondData().proxySecurityId;
        TLOG("overwrite proxySecurityId with '" << proxySecurityId << "'");
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
}

} // namespace data
} // namespace ore
