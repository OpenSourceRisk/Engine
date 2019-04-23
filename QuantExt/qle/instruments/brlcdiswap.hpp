/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/instruments/brlcdiswap.hpp
    \brief Standard BRL CDI swap
    \ingroup instruments
*/

#ifndef quantext_brl_cdi_swap_hpp
#define quantext_brl_cdi_swap_hpp

#include <qle/indexes/ibor/brlcdi.hpp>
#include <ql/instruments/overnightindexedswap.hpp>

namespace QuantExt {

//! Standard BRL CDI swap
class BRLCdiSwap : public QuantLib::OvernightIndexedSwap {
public:
    BRLCdiSwap(
        Type type,
        QuantLib::Real nominal, 
        const QuantLib::Date& startDate,
        const QuantLib::Date& endDate,
        QuantLib::Rate fixedRate,
        const boost::shared_ptr<BRLCdi>& overnightIndex,
        QuantLib::Spread spread = 0.0,
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
    boost::shared_ptr<QuantLib::OvernightIndex> index_;
};

}

#endif
