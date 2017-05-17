/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/marketdata/loader.hpp
    \brief Market Datum Loader Interface
    \ingroup marketdata
*/

#pragma once

#include <boost/shared_ptr.hpp>
#include <ored/marketdata/fixings.hpp>
#include <ored/marketdata/marketdatum.hpp>
#include <ql/time/date.hpp>
#include <vector>

namespace ore {
namespace data {

//! Market data loader base class
/*! \ingroup marketdata
 */
class Loader {
public:
    virtual ~Loader() {}

    //! \name Interface
    //@{
    virtual const std::vector<boost::shared_ptr<MarketDatum>>& loadQuotes(const QuantLib::Date&) const = 0;

    virtual const boost::shared_ptr<MarketDatum>& get(const std::string& name, const QuantLib::Date&) const = 0;

    virtual const std::vector<Fixing>& loadFixings() const = 0;
    //@}
};
} // namespace data
} // namespace ore
