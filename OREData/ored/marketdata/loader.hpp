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
#include <ored/utilities/log.hpp>
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

    //! Default implementation, returns false if get throws or returns a null pointer
    virtual bool has(const std::string& name, const QuantLib::Date& d) const {
        try {
            return get(name, d) != nullptr;
        } catch(...) {
            return false;
        }
    }

    /*! Default implementation for get that allows for the market data item to be optional. The first element of 
        the \p name pair is the name of the market point being sought and the second element of the \p name pair 
        is a flag to indicate if the market data point is optional, <code>true</code>, or not, <code>false</code>.
        - if the quote is in the loader for date \p d, it is returned
        - if the quote is not in the loader for date \p d and it is optional, 
          a warning is logged and a <code>boost::shared_ptr<MarketDatum>()</code> is returned
        - if the quote is not in the loader for date \p d and it is not optional, an exception is thrown
     */ 
    virtual boost::shared_ptr<MarketDatum> get(const std::pair<std::string, bool>& name, const QuantLib::Date& d) const {
        if (has(name.first, d)) {
            return get(name.first, d);
        } else {
            if (name.second) {
                WLOG("Could not find quote for ID " << name.first << " with as of date " << QuantLib::io::iso_date(d) << ".");
                return boost::shared_ptr<MarketDatum>();
            } else {
                QL_FAIL("Could not find quote for Mandatory ID " << name.first << " with as of date " << QuantLib::io::iso_date(d));
            }
        }
    }

    virtual const std::vector<Fixing>& loadFixings() const = 0;
    //@}
};
} // namespace data
} // namespace ore
