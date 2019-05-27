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

/*! \file qle/termstructures/strippedoptionletadapter.hpp
    \brief Convert a StrippedOptionletBase in to an OptionletVolatilityStructure
    \ingroup termstructures
*/

#ifndef quantext_stripped_optionlet_adapter_h
#define quantext_stripped_optionlet_adapter_h

#include <ql/math/interpolation.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletbase.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <ql/termstructures/volatility/interpolatedsmilesection.hpp>
#include <boost/lambda/lambda.hpp>
#include <vector>
#include <algorithm>

namespace QuantExt {

/*! Adapter class for turning a StrippedOptionletBase into an OptionletVolatilityStructure.

    The class takes two template parameters indicating the interpolation in the time and strike direction respectively.

    \ingroup termstructures
*/
template <class TimeInterpolator, class SmileInterpolator>
class StrippedOptionletAdapter : public QuantLib::OptionletVolatilityStructure, public QuantLib::LazyObject {

public:
    /*! Constructor that does not take a reference date. The settlement days is derived from \p sob and the term 
        structure will be a \e moving term structure.
    */
    StrippedOptionletAdapter(
        const boost::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
        const TimeInterpolator& ti = TimeInterpolator(),
        const SmileInterpolator& si = SmileInterpolator());

    /*! Constructor taking an explicit \p referenceDate and the term structure will therefore be not \e moving.
    */
    StrippedOptionletAdapter(
        const QuantLib::Date& referenceDate,
        const boost::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
        const TimeInterpolator& ti = TimeInterpolator(),
        const SmileInterpolator& si = SmileInterpolator());

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const;
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    QuantLib::Rate minStrike() const;
    QuantLib::Rate maxStrike() const;
    //@}

    //! \name OptionletVolatilityStructure interface
    //@{
    QuantLib::VolatilityType volatilityType() const;
    QuantLib::Real displacement() const;
    //@}

    //! \name LazyObject interface
    //@{
    void update();
    void performCalculations() const;
    //@}

    //! \name Observer interface
    //@{
    void deepUpdate();
    //@}

    //! \name Inspectors
    //@{
    boost::shared_ptr<QuantLib::StrippedOptionletBase> optionletBase() const;
    //@}

protected:
    //! \name OptionletVolatilityStructure interface
    //@{
    boost::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const;
    QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate strike) const;
    //@}

private:
    //! Base optionlet object that provides the stripped optionlet volatilities
    boost::shared_ptr<QuantLib::StrippedOptionletBase> optionletBase_;
    
    //! The interpolation object in the time direction
    TimeInterpolator ti_;
    
    //! The interpolation object in the strike direction
    SmileInterpolator si_;

    //! The optionlet volatility vs strike section at each optionlet fixing date
    std::vector<QuantLib::Interpolation> strikeSections_;
};

template <class TimeInterpolator, class SmileInterpolator>
StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::StrippedOptionletAdapter(
    const boost::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
    const TimeInterpolator& ti,
    const SmileInterpolator& si)
    : OptionletVolatilityStructure(sob->settlementDays(), sob->calendar(), sob->businessDayConvention(), 
      sob->dayCounter()), optionletBase_(sob), ti_(ti), si_(si),
      strikeSections_(optionletBase_->optionletMaturities()) {
    registerWith(optionletBase_);
}

template <class TimeInterpolator, class SmileInterpolator>
StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::StrippedOptionletAdapter(
    const QuantLib::Date& referenceDate,
    const boost::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
    const TimeInterpolator& ti,
    const SmileInterpolator& si)
    : OptionletVolatilityStructure(referenceDate, sob->calendar(), sob->businessDayConvention(),
      sob->dayCounter()), optionletBase_(sob), ti_(ti), si_(si),
      strikeSections_(optionletBase_->optionletMaturities()) {
    registerWith(optionletBase_);
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::Date StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::maxDate() const {
    return optionletBase_->optionletFixingDates().back();
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::Rate StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::minStrike() const {
    // Return the minimum strike over all optionlet tenors
    QuantLib::Rate minStrike = optionletBase_->optionletStrikes(0).front();
    for (Size i = 1; i < optionletBase_->optionletMaturities(); ++i) {
        minStrike = std::min(optionletStripper_->optionletStrikes(i).front(), minStrike);
    }
    return minStrike;
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::Rate StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::maxStrike() const {
    // Return the maximum strike over all optionlet tenors
    QuantLib::Rate maxStrike = optionletBase_->optionletStrikes(0).back();
    for (Size i = 1; i < optionletBase_->optionletMaturities(); ++i) {
        maxStrike = std::max(optionletStripper_->optionletStrikes(i).back(), maxStrike);
    }
    return maxStrike;
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::VolatilityType StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::volatilityType() const {
    return optionletBase_->volatilityType();
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::Real StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::displacement() const {
    return optionletBase_->displacement();
}

template <class TimeInterpolator, class SmileInterpolator>
inline void StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::update() {
    TermStructure::update();
    LazyObject::update();
}

template <class TimeInterpolator, class SmileInterpolator>
inline void StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::performCalculations() const {
    
    // Some localised typedefs and using declarations to make the code more readable
    using QuantLib::Rate;
    using QuantLib::Size;
    using std::vector;

    for (Size i = 0; i < optionletBase_->optionletMaturities(); ++i) {
        const vector<Rate>& strikes = optionletBase_->optionletStrikes(i);
        const vector<Volatility>& vols = optionletBase_->optionletVolatilities(i);
        strikeSections_[i] = si_.interpolate(strikes.begin(), strikes.end(), vols.begin());
        strikeSections_[i].enableExtrapolation(allowsExtrapolation());
    }
}

template <class TimeInterpolator, class SmileInterpolator>
inline void StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::deepUpdate() {
    optionletBase_->update();
    update();
}

template <class TimeInterpolator, class SmileInterpolator>
inline boost::shared_ptr<QuantLib::StrippedOptionletBase> 
StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::optionletBase() const {
    return optionletBase_;
}

template <class TimeInterpolator, class SmileInterpolator>
inline boost::shared_ptr<QuantLib::SmileSection>
StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::smileSectionImpl(QuantLib::Time optionTime) const {
    
    // Some localised typedefs and using declarations to make the code more readable
    using QuantLib::Rate;
    using QuantLib::Real;
    using QuantLib::Size;
    using QuantLib::Null;
    using QuantLib::close;
    using QuantLib::io::ordinal;
    using boost::lambda::_1;
    using boost::lambda::_2;
    using std::vector;
    using std::equal;
    using std::sqrt;

    // This method can only return a valid value if the strikes are the same for all optionlet dates
    const vector<Rate>& strikes = optionletBase_->optionletStrikes(0);
    for (Size i = 1; i < optionletBase_->optionletMaturities(); ++i) {
        const vector<Rate>& compStrikes = optionletBase_->optionletStrikes(i);
        QL_REQUIRE(equal(strikes.begin(), strikes.end(), compStrikes.begin(), close(_1, _2)), "The strikes at the " <<
            ordinal(i) << " optionlet date do not equal those at the first optionlet date");
    }

    // Store standard deviation at each strike
    vector<Real> stdDevs;
    for (Size i = 0; i < strikes.size(); i++) {
        stdDevs.push_back(sqrt(blackVariance(optionTime, strikes[i])));
    }
    
    // Return the smile section.
    // Leave ATM rate as Null<Real>() for now but could interpolate optionletBase_->atmOptionletRates()?
    Real atmRate = Null<Real>();
    return boost::make_shared<QuantLib::InterpolatedSmileSection<SmileInterpolator> >(optionTime, strikes, stdDevs,
        atmRate, si_, optionletBase_->dayCounter(), volatilityType(), displacement());
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::Volatility StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::volatilityImpl(
    QuantLib::Time optionTime, QuantLib::Rate strike) const {

    // Some localised typedefs and using declarations to make the code more readable
    using QuantLib::Interpolation;
    using QuantLib::Size;
    using QuantLib::Time;
    using QuantLib::Volatility;
    using std::vector;

    calculate();

    vector<Volatility> vols(optionletBase_->optionletMaturities());
    for (Size i = 0; i < optionletBase_->optionletMaturities(); ++i) {
        vols[i] = strikeSections_[i](strike);
    }

    vector<Time> fixingTimes = optionletStripper_->optionletFixingTimes();
    Interpolation ti = ti_.interpolate(fixingTimes.begin(), fixingTimes.end(), vols.begin());
    ti.enableExtrapolation(allowsExtrapolation());

    return ti(optionTime);
}

}

#endif
