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

/*! \file qle/instruments/brlcdiswap.hpp
    \brief Standard BRL CDI swap
    \ingroup instruments
*/

#ifndef quantext_brl_cdi_swap_hpp
#define quantext_brl_cdi_swap_hpp

#include <ql/instruments/overnightindexedswap.hpp>
#include <qle/indexes/ibor/brlcdi.hpp>

namespace QuantExt {

//! Standard BRL CDI swap
class BRLCdiSwap : public QuantLib::OvernightIndexedSwap {
public:
    BRLCdiSwap(Type type, QuantLib::Real nominal, const QuantLib::Date& startDate, const QuantLib::Date& endDate,
               QuantLib::Rate fixedRate, const QuantLib::ext::shared_ptr<BRLCdi>& overnightIndex, QuantLib::Spread spread = 0.0,
               bool telescopicValueDates = false);

    //! \name Results
    //@{
    QuantLib::Real fixedLegBPS() const;
    QuantLib::Real fairRate() const;
    //@}

private:
    QuantLib::Date startDate_;
    QuantLib::Date endDate_;
    //! QuantLib does not implement the method OvernightIndexedSwap::overnightIndex() so I need this.
    QuantLib::ext::shared_ptr<QuantLib::OvernightIndex> index_;
};

} // namespace QuantExt

#endif
