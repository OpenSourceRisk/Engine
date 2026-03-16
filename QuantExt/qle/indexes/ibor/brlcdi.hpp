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

/*! \file brlcdi.hpp
    \brief BRL-CDI index
    \ingroup indexes
*/

#ifndef quantext_brlcdi_hpp
#define quantext_brlcdi_hpp

#include <ql/currencies/america.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/brazil.hpp>
#include <ql/time/daycounters/business252.hpp>

namespace QuantExt {

//! BRL-CDI index

class BRLCdi : public QuantLib::OvernightIndex {
public:
    BRLCdi(const QuantLib::Handle<QuantLib::YieldTermStructure>& h = QuantLib::Handle<QuantLib::YieldTermStructure>())
        : QuantLib::OvernightIndex("BRL-CDI", 0, QuantLib::BRLCurrency(),
                                   QuantLib::Brazil(QuantLib::Brazil::Settlement), QuantLib::Business252(), h) {}

    //! \name InterestRateIndex interface
    //@{
    QuantLib::Rate forecastFixing(const QuantLib::Date& fixingDate) const override;
    //@}

    //! \name IborIndex interface
    //@{
    QuantLib::ext::shared_ptr<IborIndex> clone(const QuantLib::Handle<QuantLib::YieldTermStructure>& h) const override;
    //@}
};

} // namespace QuantExt

#endif
