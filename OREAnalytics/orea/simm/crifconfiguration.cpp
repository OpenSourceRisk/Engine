/*
 Copyright (C) 2024 AcadiaSoft Inc.
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

/*! \file orea/simm/crifconfiguration.hpp
    \brief CRIF configuration interface
*/

#include <boost/algorithm/string/predicate.hpp>
#include <orea/simm/crifconfiguration.hpp>
#include <qle/indexes/ibor/termrateindex.hpp>

namespace ore {
namespace analytics {

string periodToLabels2(const QuantLib::Period& p) {
    if ((p.units() == QuantLib::Months && p.length() == 3) || (p.units() == QuantLib::Weeks && p.length() == 13)) {
        return "Libor3m";
    } else if ((p.units() == QuantLib::Months && p.length() == 6) || (p.units() == QuantLib::Weeks && p.length() == 26)) {
        return "Libor6m";
    } else if ((p.units() == QuantLib::Days && p.length() == 1) || p == 1 * QuantLib::Weeks) {
        // 7 days here is based on ISDA SIMM FAQ and Implementation Questions, Sep 4, 2019 Section E.9
        // Sub curve to be used for CNY seven-day repo rate (closest is OIS).
        return "OIS";
    } else if ((p.units() == QuantLib::Months && p.length() == 1) || (p.units() == QuantLib::Weeks && p.length() == 2) ||
               (p.units() == QuantLib::Weeks && p.length() == 4) || (p.units() == QuantLib::Days && p.length() >= 28 && p.length() <= 31)) {
        // 2 weeks here is based on ISDA SIMM Methodology paragraph 14:
        // "Any sub curve not given on the above list should be mapped to its closest equivalent."
        // A 2 week rate is more like sub-period than OIS.
        return "Libor1m";
    } else if ((p.units() == QuantLib::Months && p.length() == 12) || (p.units() == QuantLib::Years && p.length() == 1) ||
               (p.units() == QuantLib::Weeks && p.length() == 52)) {
        return "Libor12m";
    } else {
        return "";
    }
}

std::string CrifConfiguration::label2(const QuantLib::Period& p) const {
    std::string label2 = periodToLabels2(p);
    QL_REQUIRE(!label2.empty(), "Could not determine SIMM Label2 for period " << p);
    return label2;
}

std::string CrifConfiguration::label2(const QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>& irIndex) const {
    std::string label2;
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        // There was no municipal until later so override this in
        // derived configurations and use 'Prime' in base
        label2 = "Prime";
    } else if (irIndex->familyName() == "Prime") {
        label2 = "Prime";
    } else if(QuantLib::ext::dynamic_pointer_cast<QuantExt::TermRateIndex>(irIndex) != nullptr) {
	// see ISDA-SIMM-FAQ_Methodology-and-Implementation_20220323_clean.pdf: E.8 Term RFR rate risk should be treated as RFR rate risk
        label2 = "OIS";
    } else {
        label2 = periodToLabels2(irIndex->tenor());
        QL_REQUIRE(!label2.empty(), "Could not determine SIMM Label2 for index " << irIndex->name());
    }
    return label2;
}
}}
