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
                                   const std::string& name, const boost::shared_ptr<BondReferenceDatum>& bondRefData) {
    QL_REQUIRE(bondRefData, "populateFromBondReferenceData(): empty bond reference datum given");
    if (issuerId.empty())
        issuerId = bondRefData->bondData().issuerId;
    if (settlementDays.empty())
        settlementDays = bondRefData->bondData().settlementDays;
    if (calendar.empty())
        calendar = bondRefData->bondData().calendar;
    if (issueDate.empty())
        issueDate = bondRefData->bondData().issueDate;
    if (creditCurveId.empty())
        creditCurveId = bondRefData->bondData().creditCurveId;
    if (referenceCurveId.empty())
        referenceCurveId = bondRefData->bondData().referenceCurveId;
    if (proxySecurityId.empty())
        proxySecurityId = bondRefData->bondData().proxySecurityId;
    if (incomeCurveId.empty())
        incomeCurveId = bondRefData->bondData().incomeCurveId;
    if (volatilityCurveId.empty())
        volatilityCurveId = bondRefData->bondData().volatilityCurveId;
    if (coupons.empty())
        coupons = bondRefData->bondData().legData;
}

} // namespace data
} // namespace ore
