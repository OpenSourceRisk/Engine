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

#include <qle/indexes/ibor/brlcdi.hpp>
#include <qle/instruments/brlcdiswap.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>

namespace QuantExt {

/*! Tenor based rate helper for bootstrapping using standard BRL CDI swaps
    \ingroup termstructures
*/
class BRLCdiRateHelper : public QuantLib::RelativeDateRateHelper {
public:
    BRLCdiRateHelper(
        const QuantLib::Period& swapTenor,
        const QuantLib::Handle<QuantLib::Quote>& fixedRate,
        const boost::shared_ptr<BRLCdi>& brlCdiIndex,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discountingCurve = 
            QuantLib::Handle<QuantLib::YieldTermStructure>(),
        bool telescopicValueDates = false);

    //! \name inspectors
    //@{
    boost::shared_ptr<BRLCdiSwap> swap() const { return swap_; }
    //@}

    //! \name RateHelper interface
    //@{
    QuantLib::Real impliedQuote() const;
    void setTermStructure(QuantLib::YieldTermStructure*);
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor&);
    //@}

protected:
    void initializeDates();

    QuantLib::Period swapTenor_;
    boost::shared_ptr<BRLCdi> brlCdiIndex_;
    boost::shared_ptr<BRLCdiSwap> swap_;
    bool telescopicValueDates_;

    QuantLib::RelinkableHandle<QuantLib::YieldTermStructure> termStructureHandle_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountHandle_;
    QuantLib::RelinkableHandle<QuantLib::YieldTermStructure> discountRelinkableHandle_;
};

/*! Absolute date based rate helper for bootstrapping using standard BRL CDI swaps
    \ingroup termstructures
*/
class DatedBRLCdiRateHelper : public QuantLib::RateHelper {
public:
    DatedBRLCdiRateHelper(
        const QuantLib::Date& startDate,
        const QuantLib::Date& endDate,
        const QuantLib::Handle<QuantLib::Quote>& fixedRate,
        const boost::shared_ptr<BRLCdi>& brlCdiIndex,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discountingCurve =
            QuantLib::Handle<QuantLib::YieldTermStructure>(),
        bool telescopicValueDates = false);

    //! \name inspectors
    //@{
    boost::shared_ptr<BRLCdiSwap> swap() const { return swap_; }
    //@}

    //! \name RateHelper interface
    //@{
    QuantLib::Real impliedQuote() const;
    void setTermStructure(QuantLib::YieldTermStructure*);
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor&);
    //@}

protected:
    boost::shared_ptr<BRLCdi> brlCdiIndex_;
    boost::shared_ptr<BRLCdiSwap> swap_;
    bool telescopicValueDates_;

    QuantLib::RelinkableHandle<QuantLib::YieldTermStructure> termStructureHandle_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountHandle_;
    QuantLib::RelinkableHandle<QuantLib::YieldTermStructure> discountRelinkableHandle_;
};

}

#endif
