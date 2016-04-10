/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file dynamicswaptionvolmatrix.hpp
    \brief dynamic swaption volatility matrix
*/

#ifndef quantext_dynamic_swaption_volatility_termstructure_hpp
#define quantext_dynamic_swaption_volatility_termstructure_hpp

#include <qle/termstructures/dynamicstype.hpp>

#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! This class takes a SwaptionVolatilityMatrix with fixed
    reference date and turns it into a floating reference date
    term structure.
    There are different ways of reacting to time decay that can be
    specified.
*/

class DynamicSwaptionVolatilityMatrix : public SwaptionVolatilityStructure {
  public:
    DynamicSwaptionVolatilityMatrix(
        const boost::shared_ptr<SwaptionVolatilityStructure> &source,
        Natural settlementDays, const Calendar &calendar,
        ReactionToTimeDecay decayMode = ConstantVariance);

  protected:
    /* SwaptionVolatilityStructure interface */
    const Period &maxSwapTenor() const;

    boost::shared_ptr<SmileSection> smileSectionImpl(Time optionTime,
                                                     Time swapLength) const;

    Volatility volatilityImpl(Time optionTime, Time swapLength,
                              Rate strike) const;

    Real shiftImpl(Time optionTime, Time swapLength) const;
    
    /* VolatilityTermStructure interface */
    Real minStrike() const;
    Real maxStrike() const;
    /* TermStructure interface */
    Date maxDate() const;
    /* Observer interface */
    void update();

  private:
    const boost::shared_ptr<SwaptionVolatilityStructure> source_;
    ReactionToTimeDecay decayMode_;
    const Date originalReferenceDate_;
};

} // namespace QuantExt

#endif
