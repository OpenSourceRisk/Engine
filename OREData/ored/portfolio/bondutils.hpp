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

/*!
  \file portfolio/bondutils.hpp
  \brief bond utilities
  \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/legdata.hpp>

#include <ored/portfolio/referencedata.hpp>

namespace ore {
namespace data {

//! Populate bond data from name and ReferenceDataManager
/*! The following elements are references and updated, if empty:
    issuerId
    settlementDays
    calendar
    issueDate
    creditCurveId
    creditGroup
    referenceCurveId
    incomeCurveId
    volatilityCurveId
    coupons */

void populateFromBondReferenceData(std::string& subType, std::string& issuerId, std::string& settlementDays,
                                   std::string& calendar, std::string& issueDate, std::string& priceQuoteMethod,
                                   std::string& priceQuoteBaseValue, std::string& creditCurveId,
                                   std::string& creditGroup, std::string& referenceCurveId, std::string& incomeCurveId,
                                   std::string& volatilityCurveId, std::vector<LegData>& coupons, 
                                   std::optional<QuantLib::Bond::Price::Type>& quotedDirtyPrices,
                                   const std::string& name,
                                   const QuantLib::ext::shared_ptr<BondReferenceDatum>& bondRefData,
                                   const std::string& startDate = "", const std::string& endDate = "");

Date getOpenEndDateReplacement(const std::string& replacementPeriodStr, const Calendar& calendar = NullCalendar());

/* Returns the type of the bond reference data

   - BondReferenceDatum::TYPE             ("Bond")
   - ConvertibleBondReferenceDatum:TYPE   ("ConvertibleBond")

   or an empty string if no reference data was found. */

std::string getBondReferenceDatumType(const std::string& id,
                                      const QuantLib::ext::shared_ptr<ReferenceDataManager>& refData);

/* We use special security ids to mark securities being part of a bond future contract. For example,

   ISIN:US91282CDJ71

   is the securityId of a bond. If that bond is part of the future contract TYH25 we write

   ISIN:US91282CDJ71_FUTURE_TYH25

   The rationale is that we need security spreads implied from bond future quotes for the bond future underlyings.

   Similarly, we allow for the deprecated form with a forward expiry

   ISIN:US91282CDJ71_FWDEXP_2025-08-01

   which was the workaround to represent futures before the proper bond future implementation.
*/

class StructuredSecurityId {
public:
    // deliberately implicit
    StructuredSecurityId(const std::string& id);
    StructuredSecurityId(const std::string& securityId, const std::string& futureContract);
    // deliberately implicit
    operator string() const { return id_; }
    std::string securityId() const { return securityId_; }
    std::string futureContract() const { return futureContract_; }
    std::string forwardExpiry() const { return forwardExpiry_; }

private:
    std::string id_;
    std::string securityId_;
    std::string futureContract_;
    std::string forwardExpiry_;
};

struct BondFutureUtils {
    enum BondFutureType { ShortTenorUS, LongTenorUS };

    static std::pair<QuantLib::Date, QuantLib::Date>
    deduceDates(const boost::shared_ptr<BondFutureReferenceDatum>& refData);

    static std::pair<QuantLib::Date, QuantLib::Date>
    deduceDates(const std::string& currency, const std::string& contractMonth, const std::string& rootDateStr,
                const std::string& expiryBasis, const std::string& settlementBasis, const std::string& expiryLag,
                const std::string& settlementLag);

    static BondFutureType getBondFutureType(const std::string& deliverableGrade);

    static void checkDates(const QuantLib::Date& expiry, const QuantLib::Date& settlementDate);

    static double conversionFactor(const BondFutureUtils::BondFutureType& type, const Date& futureExpiry,
                                   const double fixedRate, const Date bondMaturity);

    static std::pair<std::string, double> identifyCtdBond(const ext::shared_ptr<EngineFactory>& engineFactory,
                                                          const std::string& futureContract, const bool pricing = true);

    //! deprecated, strip a bond to cashflows after given expiry (throws for non-vanilla bonds)
    static void modifyToForwardBond(const Date& expiry, QuantLib::ext::shared_ptr<QuantLib::Bond>& bond,
                                             const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                             const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                                             const std::string& securityId);
};

} // namespace data
} // namespace ore
