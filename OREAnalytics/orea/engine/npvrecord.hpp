/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file orea/engine/npvrecord.hpp
    \brief Struct for holding an NPV record
*/

#pragma once

#include <ql/types.hpp>
#include <ql/time/date.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/composite_key.hpp>

#include <string>

namespace ore {
namespace analytics {

/*! A container for holding single NPV record or aggregated NPV records.
*/
struct NpvRecord {
    std::string tradeId;
    std::string portfolioId;
    std::string baseCurrency;
    QuantLib::Real baseAmount;
    QuantLib::Date valuationDate;

    //! Define how CRIF records are compared
    bool operator<(const NpvRecord& nr) const {
        return std::tie(tradeId, portfolioId, valuationDate, baseCurrency) <
            std::tie(nr.tradeId, nr.portfolioId, nr.valuationDate, nr.baseCurrency);
    }
    bool operator==(const NpvRecord& nr) const {
        return std::tie(tradeId, portfolioId, valuationDate, baseCurrency) ==
            std::tie(nr.tradeId, nr.portfolioId, nr.valuationDate, nr.baseCurrency);
    }
};

//! Enable writing of a NpvRecord
std::ostream& operator<<(std::ostream& out, const NpvRecord& nr);


}
}
