/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006 Ferdinando Ametrano
 Copyright (C) 2015 Peter Caspers

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file swaptionvolcube.hpp
    \brief Swaption volatility cube
*/

#ifndef quantext_swaption_volatility_cube_h
#define quantext_swaption_volatility_cube_h

#include <ql/termstructures/volatility/swaption/swaptionvoldiscrete.hpp>
#include <ql/termstructures/volatility/smilesection.hpp>
#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/indexes/swapindex.hpp>
#include <iostream>
using namespace QuantLib;

namespace QuantExt {

    //! swaption-volatility cube
    /*! 
    
    */
    class SwaptionVolatilityCube : public QuantLib::SwaptionVolatilityDiscrete {
      public:
        SwaptionVolatilityCube(
            const std::vector<Period>& optionTenors,
            const std::vector<Period>& swapTenors,
            const std::vector<Spread>& strikeSpreads,
            const std::vector<std::vector<std::vector<Handle<Quote> > > >& vols,
            const boost::shared_ptr<QuantLib::SwapIndex>& swapIndexBase,
            const boost::shared_ptr<QuantLib::SwapIndex>& shortSwapIndexBase,
            bool flatExtrapolation,
            VolatilityType volatilityType_,
            BusinessDayConvention bdc,
            const DayCounter& dc,
            const Calendar& cal,
            Natural settlementDays = 0,
            const std::vector<std::vector<Real> >& shifts  = std::vector<std::vector<Real> >()
            );


            
            
        //! \name VolatilityTermStructure interface
        //@{
        Rate minStrike() const { return -QL_MAX_REAL; }
        Rate maxStrike() const { return QL_MAX_REAL; }
        //@}

        Date maxDate() const { return Date::maxDate(); }
        Calendar calendar() const { return calendar_; }
        Natural settlementDays() const { return settlementDays_; }
        //! \name SwaptionVolatilityStructure interface
        //@{
        const Period& maxSwapTenor() const { return 100*Years; }
        //@}
        //! \name Other inspectors
        //@{
        Rate atmStrike(const Date& optionDate,
                       const Period& swapTenor) const;
        Rate atmStrike(const Period& optionTenor,
                       const Period& swapTenor) const {
            Date optionDate = optionDateFromTenor(optionTenor);
            return atmStrike(optionDate, swapTenor);
        }
        const std::vector<Spread>& strikeSpreads() const { return strikeSpreads_; }
        const std::vector<std::vector<std::vector<Handle<Quote> > > >& vols() const { return vols_; }
        const boost::shared_ptr<SwapIndex> swapIndexBase() const { return swapIndexBase_; }
        const boost::shared_ptr<SwapIndex> shortSwapIndexBase() const { return shortSwapIndexBase_; }
        //@}
        //! \name LazyObject interface
        //@{
        void performCalculations() const;
        //@}
        VolatilityType volatilityType() const { return volatilityType_; }
      protected:
        void registerWithVolatility();
        virtual Size requiredNumberOfStrikes() const { return 2; }
        Volatility volatilityImpl(Time optionTime,
                                  Time swapLength,
                                  Rate strike) const;
        Volatility volatilityImpl(const Date& optionDate,
                                  const Period& swapTenor,
                                  Rate strike) const;
        boost::shared_ptr<SmileSection> smileSectionImpl(const Date& optionDate, const Period& swapTenor) const;
        boost::shared_ptr<SmileSection> smileSectionImpl(Time optionTime, Time swapLength) const;
        Real shiftImpl(Time optionTime, Time swapLength) const;
        Size nStrikes_;
        std::vector<Spread> strikeSpreads_;
        mutable std::vector<Rate> localStrikes_;
        mutable std::vector<Volatility> localSmile_;
        std::vector<std::vector<std::vector<Handle<Quote> > > > vols_;
        boost::shared_ptr<QuantLib::SwapIndex> swapIndexBase_, shortSwapIndexBase_;
        mutable std::vector<Interpolation2D> volsInterpolator_;
        mutable std::vector<Matrix> volsMatrix_;
        bool flatExtrapolation_;
        VolatilityType volatilityType_;
        mutable Matrix shifts_;
        Interpolation2D interpolation_, interpolationShifts_;
        Calendar calendar_;
        Natural settlementDays_;
        };

    // inline

    inline Volatility SwaptionVolatilityCube::volatilityImpl(
                                                        Time optionTime,
                                                        Time swapLength,
                                                        Rate strike) const {
        return smileSectionImpl(optionTime, swapLength)->volatility(strike);
    }

    inline Volatility SwaptionVolatilityCube::volatilityImpl(
                                                    const Date& optionDate,
                                                    const Period& swapTenor,
                                                    Rate strike) const {
        return smileSectionImpl(optionDate, swapTenor)->volatility(strike);
    }

    inline Real SwaptionVolatilityCube::shiftImpl(Time optionTime,
                                                    Time swapLength) const {
        calculate();
        Real tmp = interpolationShifts_(swapLength, optionTime, true);
        return tmp;
    }
}

#endif
