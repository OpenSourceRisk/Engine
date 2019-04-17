/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/termstructures/brlcdiratehelper.hpp
    \brief Rate helper based on standard BRL CDI swap
    \ingroup termstructures
*/

#ifndef quantext_brl_cdi_rate_helper_hpp
#define quantext_brl_cdi_rate_helper_hpp

#include <qle/termstructures/oisratehelper.hpp>
#include <qle/indexes/ibor/brlcdi.hpp>

namespace QuantExt {

/*! Tenor based rate helper for bootstrapping using standard BRL CDI swaps
    \ingroup termstructures
*/
class BRLCdiRateHelper : public OISRateHelper {
public:
    BRLCdiRateHelper(
        const QuantLib::Period& swapTenor,
        const QuantLib::Handle<QuantLib::Quote>& fixedRate,
        const boost::shared_ptr<BRLCdi>& brlCdiIndex,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discountingCurve = 
            QuantLib::Handle<QuantLib::YieldTermStructure>(),
        bool telescopicValueDates = false);

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor&);
    //@}

protected:
    void initializeDates();
};

/*! Absolute date based rate helper for bootstrapping using standard BRL CDI swaps
    \ingroup termstructures
*/
class DatedBRLCdiRateHelper : public DatedOISRateHelper {
public:
    DatedBRLCdiRateHelper(
        const QuantLib::Date& startDate,
        const QuantLib::Date& endDate,
        const QuantLib::Handle<QuantLib::Quote>& fixedRate,
        const boost::shared_ptr<BRLCdi>& brlCdiIndex,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discountingCurve =
            QuantLib::Handle<QuantLib::YieldTermStructure>(),
        bool telescopicValueDates = false);

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor&);
    //@}
};

}

#endif
