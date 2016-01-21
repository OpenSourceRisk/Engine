/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 - 2015 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file blackinvertedvoltermstructure.hpp
    \brief Black volatility surface that inverts an existing surface.
*/

#ifndef quantext_black_inverted_vol_termstructure_hpp
#define quantext_black_inverted_vol_termstructure_hpp

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! Black volatility surface that inverts an existing surface.
    /*! This class is used when one wants a USD/EUR volatility, at a given USD/EUR strike
        when only a EUR/USD volatility surface is present.
    */
    class BlackInvertedVolTermStructure : public BlackVolTermStructure {
      public:

        //! Constructor takes a BlackVolTermStructure and takes everything from that
        /*! This will work with both a floating and fixed reference date underlying surface,
            since we are reimplementing the reference date and update methods */
        BlackInvertedVolTermStructure(const Handle<BlackVolTermStructure> &vol)
            : BlackVolTermStructure(vol->businessDayConvention(),
                                    vol->dayCounter()),
              vol_(vol) {
            registerWith(vol_);
        }

        //! return the underlying vol surface
        const Handle<BlackVolTermStructure>& underlyingVol() const { return vol_; }

        //! \name TermStructure interface
        //@{
        const Date& referenceDate() const { return vol_->referenceDate(); }
        Date maxDate() const { return vol_->maxDate(); }
        Natural settlementDays() const { return vol_->settlementDays(); }
        Calendar calendar() const { return vol_->calendar (); }
        //! \name Observer interface
        //@{
        void update() { notifyObservers(); }
        //@}
        //! \name VolatilityTermStructure interface
        //@{
        Real minStrike() const { return 1.0 / vol_->maxStrike(); }
        Real maxStrike() const { return 1.0 / vol_->minStrike(); }
        //@}
        //! \name Visitability
        //@{
        virtual void accept(AcyclicVisitor&);
        //@}
      protected:
        virtual Real blackVarianceImpl(Time t, Real strike) const {
            return vol_->blackVariance(
                t, strike == Null<Real>() ? Null<Real>() : 1.0 / strike);
        }

        virtual Volatility blackVolImpl(Time t, Real strike) const {
            return vol_->blackVol(t, strike == Null<Real>() ? Null<Real>()
                                                            : 1.0 / strike);
        }
      private:
        Handle<BlackVolTermStructure> vol_;
    };


    // inline definitions
    inline void BlackInvertedVolTermStructure::accept(AcyclicVisitor& v) {
        Visitor<BlackInvertedVolTermStructure>* v1 =
            dynamic_cast<Visitor<BlackInvertedVolTermStructure>*>(&v);
        if (v1 != 0)
            v1->visit(*this);
        else
            BlackInvertedVolTermStructure::accept(v);
    }

}


#endif
