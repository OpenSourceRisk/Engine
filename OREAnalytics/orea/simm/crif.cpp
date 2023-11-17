/*
 Copyright (C) 2023 AcadiaSoft Inc.
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

/*! \file orea/simm/crif.cpp
    \brief Struct for holding CRIF records
*/



#include <orea/simm/crif.hpp>

namespace ore {
namespace analytics {

Crif Crif::aggregate() const {}

void writeCrifReport(const boost::shared_ptr<Crif>& crif, ore::data::Report& crifReport,
                     const bool writeUseCounterpartyTrade = false, const bool applyThreshold = true) const;

//! Initialise the CRIF report headers
void writeCrifReportHeaders(ore::data::Report& report, bool writeUseCounterpartyTrade) const;

//! Write a CRIF record to the report
void writeCrifRecord(const CrifPlusRecord& crifRecord, ore::data::Report& report, bool writeUseCounterpartyTrade,
                     const bool applyThreshold = true);
} // namespace analytics
} // namespace ore