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

/*! \file qle/termstructures/swaptionvolatilityconverter.hpp
    \brief Convert swaption volatilities from one type to another
    \ingroup termstructures
*/

#ifndef quantext_swaptionvolatilityconverter_hpp
#define quantext_swaptionvolatilityconverter_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>

#include <boost/shared_ptr.hpp>

using namespace QuantLib;
using std::vector;

namespace QuantExt {

//! Container for holding swap conventions needed by the SwaptionVolatilityConverter
//! \ingroup termstructures
class SwapConventions {
public:
    //! Constructor
    SwapConventions(Natural settlementDays, const Period& fixedTenor, const Calendar& fixedCalendar,
                    BusinessDayConvention fixedConvention, const DayCounter& fixedDayCounter,
                    const boost::shared_ptr<IborIndex>& floatIndex)
        : settlementDays_(settlementDays), fixedTenor_(fixedTenor), fixedCalendar_(fixedCalendar),
          fixedConvention_(fixedConvention), fixedDayCounter_(fixedDayCounter), floatIndex_(floatIndex) {}

    //! \name Inspectors
    //@{
    Natural settlementDays() const { return settlementDays_; }
    const Period& fixedTenor() const { return fixedTenor_; }
    const Calendar& fixedCalendar() const { return fixedCalendar_; }
    BusinessDayConvention fixedConvention() const { return fixedConvention_; }
    const DayCounter& fixedDayCounter() const { return fixedDayCounter_; }
    const boost::shared_ptr<IborIndex> floatIndex() const { return floatIndex_; }
    //@}

private:
    Natural settlementDays_;
    Period fixedTenor_;
    Calendar fixedCalendar_;
    BusinessDayConvention fixedConvention_;
    DayCounter fixedDayCounter_;
    boost::shared_ptr<IborIndex> floatIndex_;
};

//! Class that converts a supplied SwaptionVolatilityStructure to one of another type with possibly different shifts
/*! \ingroup termstructures

    \warning the converted <tt>SwaptionVolatilityStructure</tt> object has a fixed reference date eqaul to
             <tt>asof</tt> and fixed market data regardless of the type of reference date and market data of the
             original <tt>SwaptionVolatilityStructure</tt> that is passed in
*/
class SwaptionVolatilityConverter {
public:
    //! Construct from SwapConventions
    SwaptionVolatilityConverter(const Date& asof, const boost::shared_ptr<SwaptionVolatilityStructure>& svsIn,
                                const Handle<YieldTermStructure>& discount,
                                const Handle<YieldTermStructure>& shortDiscount,
                                const boost::shared_ptr<SwapConventions>& conventions,
                                const boost::shared_ptr<SwapConventions>& shortConventions,
                                const Period& conventionsTenor, const Period& shortConventionsTenor,
                                const VolatilityType targetType, const Matrix& targetShifts = Matrix());
    //! Construct from SwapIndex
    SwaptionVolatilityConverter(const Date& asof, const boost::shared_ptr<SwaptionVolatilityStructure>& svsIn,
                                const boost::shared_ptr<SwapIndex>& swapIndex,
                                const boost::shared_ptr<SwapIndex>& shortSwapIndex, const VolatilityType targetType,
                                const Matrix& targetShifts = Matrix());

    //! Method that returns the converted <tt>SwaptionVolatilityStructure</tt>
    boost::shared_ptr<SwaptionVolatilityStructure> convert() const;

    //! Set implied volatility solver accuracy
    Real& accuracy() { return accuracy_; }
    //! Set implied volatility solver max evaluations
    Natural& maxEvaluations() { return maxEvaluations_; }

private:
    // Check inputs
    void checkInputs() const;

    // Method that is called depending on the type of svsIn
    boost::shared_ptr<SwaptionVolatilityStructure>
    convert(const boost::shared_ptr<SwaptionVolatilityMatrix>& svMatrix) const;

    // Convert a single vol associated with a given swaption
    Real convert(const Date& expiry, const Period& swapTenor, Real strikeSpread, const DayCounter& volDayCounter,
                 VolatilityType outType, Real outShift = 0.0) const;

    const Date asof_;
    const boost::shared_ptr<SwaptionVolatilityStructure> svsIn_;
    Handle<YieldTermStructure> discount_, shortDiscount_;
    const boost::shared_ptr<SwapConventions> conventions_, shortConventions_;
    const Period conventionsTenor_, shortConventionsTenor_;
    const VolatilityType targetType_;
    const Matrix targetShifts_;

    // Variables for implied volatility solver
    Real accuracy_;
    Natural maxEvaluations_;
    static const Volatility minVol_;
    static const Volatility maxVol_;
    static const Real minVega_;
};
} // namespace QuantExt

#endif
