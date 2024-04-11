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

#include <algorithm>
#include <ql/math/interpolation.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/termstructures/volatility/flatsmilesection.hpp>
#include <ql/termstructures/volatility/interpolatedsmilesection.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletbase.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <vector>

namespace QuantExt {

/*! Adapter class for turning a StrippedOptionletBase into an OptionletVolatilityStructure.

    The class takes two template parameters indicating the interpolation in the time and strike direction respectively.

    The class can take a QuantLib::StrippedOptionletBase that has only one strike column. In this case, the strike
    interpolation is ignored and the volatility at one of the pillar tenors, and any strike, is merely the passed
    in volatility. In this case, the smile sections are flat. All of this enables the StrippedOptionletAdapter to
    represent a stripped ATM optionlet curve. The single strike in the QuantLib::StrippedOptionletBase is ignored.

    \ingroup termstructures
*/
template <class TimeInterpolator, class SmileInterpolator>
class StrippedOptionletAdapter : public QuantLib::OptionletVolatilityStructure, public QuantLib::LazyObject {

public:
    /*! Constructor that does not take a reference date. The settlement days is derived from \p sob and the term
        structure will be a \e moving term structure.
    */
    StrippedOptionletAdapter(const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
                             const TimeInterpolator& ti = TimeInterpolator(),
                             const SmileInterpolator& si = SmileInterpolator());

    /*! Constructor taking an explicit \p referenceDate and the term structure will therefore be not \e moving.
     */
    StrippedOptionletAdapter(const QuantLib::Date& referenceDate,
                             const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
                             const TimeInterpolator& ti = TimeInterpolator(),
                             const SmileInterpolator& si = SmileInterpolator());

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    QuantLib::Rate minStrike() const override;
    QuantLib::Rate maxStrike() const override;
    //@}

    //! \name OptionletVolatilityStructure interface
    //@{
    QuantLib::VolatilityType volatilityType() const override;
    QuantLib::Real displacement() const override;
    //@}

    //! \name LazyObject interface
    //@{
    void update() override;
    void performCalculations() const override;
    //@}

    //! \name Observer interface
    //@{
    void deepUpdate() override;
    //@}

    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase> optionletBase() const;
    //@}

protected:
    //! \name OptionletVolatilityStructure interface
    //@{
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const override;
    QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate strike) const override;
    //@}

private:
    //! Base optionlet object that provides the stripped optionlet volatilities
    QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase> optionletBase_;

    //! The interpolation object in the time direction
    TimeInterpolator ti_;

    //! The interpolation object in the strike direction
    SmileInterpolator si_;

    /*! The optionlet volatility vs strike section at each optionlet fixing date
        It is \c mutable so that we can call \c enableExtrapolation in \c performCalculations.
    */
    mutable std::vector<QuantLib::Interpolation> strikeSections_;

    //! Flag that indicates if optionletBase_ has only one strike for all option tenors
    bool oneStrike_;

    //! Method to populate oneStrike_ on initialisation
    void populateOneStrike();
};

template <class TimeInterpolator, class SmileInterpolator>
StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::StrippedOptionletAdapter(
    const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob, const TimeInterpolator& ti,
    const SmileInterpolator& si)
    : OptionletVolatilityStructure(sob->settlementDays(), sob->calendar(), sob->businessDayConvention(),
                                   sob->dayCounter()),
      optionletBase_(sob), ti_(ti), si_(si), strikeSections_(optionletBase_->optionletMaturities()) {
    registerWith(optionletBase_);
    populateOneStrike();
}

template <class TimeInterpolator, class SmileInterpolator>
StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::StrippedOptionletAdapter(
    const QuantLib::Date& referenceDate, const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
    const TimeInterpolator& ti, const SmileInterpolator& si)
    : OptionletVolatilityStructure(referenceDate, sob->calendar(), sob->businessDayConvention(), sob->dayCounter()),
      optionletBase_(sob), ti_(ti), si_(si), strikeSections_(optionletBase_->optionletMaturities()) {
    registerWith(optionletBase_);
    populateOneStrike();
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::Date StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::maxDate() const {
    return optionletBase_->optionletFixingDates().back();
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::Rate StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::minStrike() const {

    // If only one strike
    if (oneStrike_) {
        if (volatilityType() == QuantLib::ShiftedLognormal) {
            return displacement() > 0.0 ? -displacement() : 0.0;
        } else {
            return QL_MIN_REAL;
        }
    }

    // Return the minimum strike over all optionlet tenors
    QuantLib::Rate minStrike = optionletBase_->optionletStrikes(0).front();
    for (QuantLib::Size i = 1; i < optionletBase_->optionletMaturities(); ++i) {
        minStrike = std::min(optionletBase_->optionletStrikes(i).front(), minStrike);
    }
    return minStrike;
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::Rate StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::maxStrike() const {

    // If only one strike, there is no max strike
    if (oneStrike_) {
        return QL_MAX_REAL;
    }

    // Return the maximum strike over all optionlet tenors
    QuantLib::Rate maxStrike = optionletBase_->optionletStrikes(0).back();
    for (QuantLib::Size i = 1; i < optionletBase_->optionletMaturities(); ++i) {
        maxStrike = std::max(optionletBase_->optionletStrikes(i).back(), maxStrike);
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

    // Need this or some tests with ObservationMode::Mode::Disable fail
    // e.g. */SensitivityAnalysisTest/testPortfolioSensitivityDisableObs
    optionletBase_->update();

    TermStructure::update();
    LazyObject::update();
}

template <class TimeInterpolator, class SmileInterpolator>
inline void StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::performCalculations() const {

    // Some localised typedefs and using declarations to make the code more readable
    using QuantLib::Rate;
    using QuantLib::Size;
    using QuantLib::Volatility;
    using std::vector;

    // If only one strike, no strike section to update
    if (oneStrike_) {
        return;
    }

    for (Size i = 0; i < optionletBase_->optionletMaturities(); ++i) {
        const vector<Rate>& strikes = optionletBase_->optionletStrikes(i);
        const vector<Volatility>& vols = optionletBase_->optionletVolatilities(i);
        strikeSections_[i] = si_.interpolate(strikes.begin(), strikes.end(), vols.begin());
        // Extrapolation can be enabled here. Range checks are performed in the volatility methods.
        strikeSections_[i].enableExtrapolation();
    }
}

template <class TimeInterpolator, class SmileInterpolator>
inline void StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::deepUpdate() {
    optionletBase_->update();
    update();
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>
StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::optionletBase() const {
    return optionletBase_;
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::ext::shared_ptr<QuantLib::SmileSection>
StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::smileSectionImpl(QuantLib::Time optionTime) const {

    // Some localised typedefs and using declarations to make the code more readable
    using QuantLib::Null;
    using QuantLib::Rate;
    using QuantLib::Real;
    using QuantLib::Size;
    using QuantLib::Volatility;
    using QuantLib::io::ordinal;
    using std::equal;
    using std::sqrt;
    using std::vector;

    // Leave ATM rate as Null<Real>() for now but could interpolate optionletBase_->atmOptionletRates()?
    Real atmRate = Null<Real>();

    // If one strike, return a flat smile section
    if (oneStrike_) {
        Volatility vol = volatility(optionTime, optionletBase_->optionletStrikes(0)[0], true);
        return QuantLib::ext::make_shared<QuantLib::FlatSmileSection>(optionTime, vol, optionletBase_->dayCounter(), atmRate,
                                                              volatilityType(), displacement());
    }

    /* we choose the strikes from the first fixing time for interpolation
       - if only fixed strikes are used, they are the same for all times anyway
       - if atm is used in addition, the first time's strikes are a superset of all others, i.e. the densest */
    const vector<Rate>& strikes = optionletBase_->optionletStrikes(0);

    // Store standard deviation at each strike
    vector<Real> stdDevs;
    for (Size i = 0; i < strikes.size(); i++) {
        stdDevs.push_back(sqrt(blackVariance(optionTime, strikes[i], true)));
    }

    // Return the smile section.
    return QuantLib::ext::make_shared<QuantLib::InterpolatedSmileSection<SmileInterpolator> >(
        optionTime, strikes, stdDevs, atmRate, si_, optionletBase_->dayCounter(), volatilityType(), displacement());
}

template <class TimeInterpolator, class SmileInterpolator>
inline QuantLib::Volatility
StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::volatilityImpl(QuantLib::Time optionTime,
                                                                              QuantLib::Rate strike) const {

    // Some localised typedefs and using declarations to make the code more readable
    using QuantLib::Interpolation;
    using QuantLib::Size;
    using QuantLib::Time;
    using QuantLib::Volatility;
    using std::vector;

    calculate();

    vector<Volatility> vols(optionletBase_->optionletMaturities());
    for (Size i = 0; i < optionletBase_->optionletMaturities(); ++i) {
        vols[i] = oneStrike_ ? optionletBase_->optionletVolatilities(i)[0] : strikeSections_[i](strike);
    }

    vector<Time> fixingTimes = optionletBase_->optionletFixingTimes();
    Interpolation ti = ti_.interpolate(fixingTimes.begin(), fixingTimes.end(), vols.begin());

    // Extrapolation can be enabled at this level. The range checks will have already been performed in
    // the public OptionletVolatilityStructure::volatility method that calls this `Impl`
    ti.enableExtrapolation();

    return ti(optionTime);
}

template <class TimeInterpolator, class SmileInterpolator>
inline void StrippedOptionletAdapter<TimeInterpolator, SmileInterpolator>::populateOneStrike() {
    oneStrike_ = true;
    for (QuantLib::Size i = 0; i < optionletBase_->optionletMaturities(); ++i) {
        if (optionletBase_->optionletStrikes(i).size() > 1) {
            oneStrike_ = false;
            return;
        }
    }
}

} // namespace QuantExt

#endif
