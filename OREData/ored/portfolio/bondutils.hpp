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

using namespace ore::data;

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
void populateFromBondReferenceData(std::string& subType,
                                   std::string& issuerId, std::string& settlementDays, std::string& calendar,
                                   std::string& issueDate, std::string& priceQuoteMethod,
                                   std::string& priceQuoteBaseValue, std::string& creditCurveId,
                                   std::string& creditGroup, std::string& referenceCurveId, std::string& incomeCurveId,
                                   std::string& volatilityCurveId, std::vector<LegData>& coupons,
                                   const std::string& name, const QuantLib::ext::shared_ptr<BondReferenceDatum>& bondRefData,
                                   const std::string& startDate = "", const std::string& endDate = "");

Date getOpenEndDateReplacement(const std::string& replacementPeriodStr, const Calendar& calendar = NullCalendar());

} // namespace data
} // namespace ore
