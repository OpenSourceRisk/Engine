/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/sabrstrippedoptionletadapter.hpp
    \brief Convert a StrippedOptionletBase in to an OptionletVolatilityStructure using a SABR model
    \ingroup termstructures
*/

#ifndef quantext_stripped_optionlet_adapter_sabr_h
#define quantext_stripped_optionlet_adapter_sabr_h

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

template <class TimeInterpolator>
class SabrStrippedOptionletAdapter : public QuantLib::OptionletVolatilityStructure, public QuantLib::LazyObject {

public:
    /*! Constructor that does not take a reference date. The settlement days is derived from \p sob and the term
        structure will be a \e moving term structure.
    */
    SabrStrippedOptionletAdapter(const boost::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
                                 const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
                                 const TimeInterpolator& ti = TimeInterpolator(),
                                 const boost::optional<QuantLib::VolatilityType> outputVolatilityType = boost::none,
                                 const std::vector<std::pair<Real, bool>>& initialModelParameters = {},
                                 const QuantLib::Size maxCalibrationAttempts = 10,
                                 const QuantLib::Real exitEarlyErrorThreshold = 0.005,
                                 const QuantLib::Real maxAcceptableError = 0.05);

    /*! Constructor taking an explicit \p referenceDate and the term structure will therefore be not \e moving.
     */
    SabrStrippedOptionletAdapter(const QuantLib::Date& referenceDate,
                                 const boost::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
                                 const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
                                 const TimeInterpolator& ti = TimeInterpolator(),
                                 const boost::optional<QuantLib::VolatilityType> outputVolatilityType = boost::none,
                                 const std::vector<std::pair<Real, bool>>& initialModelParameters = {},
                                 const QuantLib::Size maxCalibrationAttempts = 10,
                                 const QuantLib::Real exitEarlyErrorThreshold = 0.005,
                                 const QuantLib::Real maxAcceptableError = 0.05);

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
    boost::shared_ptr<QuantLib::StrippedOptionletBase> optionletBase() const;
    //@}

protected:
    //! \name OptionletVolatilityStructure interface
    //@{
    boost::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const override;
    QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate strike) const override;
    //@}

private:
    //! Base optionlet object that provides the stripped optionlet volatilities
    boost::shared_ptr<QuantLib::StrippedOptionletBase> optionletBase_;

    //! The interpolation object in the time direction
    TimeInterpolator ti_;

    //! SABR specific inputs
    QuantExt::SabrParametricVolatility::ModelVariant modelVariant_;
    boost::optional<QuantLib::VolatilityType> outputVolatilityType_;
    std::vector<std::pair<Real, bool>> initialModelParameters_;
    QuantLib::Size maxCalibrationAttempts_;
    QuantLib::Real exitEarlyErrorThreshold_;
    QuantLib::Real maxAcceptableError_;

    //! State
    mutable boost::shared_ptr<ParametricVolatility> parametricVolatility_;
};

template <class TimeInterpolator>
SabrStrippedOptionletAdapter<TimeInterpolator>::SabrStrippedOptionletAdapter(
    const boost::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
    const QuantExt::SabrParametricVolatility::ModelVariant modelVariant, const TimeInterpolator& ti,
    const boost::optional<QuantLib::VolatilityType> outputVolatilityType,
    const std::vector<std::pair<Real, bool>>& initialModelParameters, const QuantLib::Size maxCalibrationAttempts,
    const QuantLib::Real exitEarlyErrorThreshold, const QuantLib::Real maxAcceptableError)
    : OptionletVolatilityStructure(sob->settlementDays(), sob->calendar(), sob->businessDayConvention(),
                                   sob->dayCounter()),
      optionletBase_(sob), ti_(ti), modelVariant_(modelVariant), outputVolatilityType_(outputVolatilityType),
      initialModelParameters_(initialModelParameters), maxCalibrationAttempts_(maxCalibrationAttempts),
      exitEarlyErrorThreshold_(exitEarlyErrorThreshold), maxAcceptableError_(maxAcceptableError) {
    registerWith(optionletBase_);
}

template <class TimeInterpolator>
SabrStrippedOptionletAdapter<TimeInterpolator>::SabrStrippedOptionletAdapter(
    const QuantLib::Date& referenceDate, const boost::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
    const QuantExt::SabrParametricVolatility::ModelVariant modelVariant, const TimeInterpolator& ti,
    const boost::optional<QuantLib::VolatilityType> outputVolatilityType,
    const std::vector<std::pair<Real, bool>>& initialModelParameters, const QuantLib::Size maxCalibrationAttempts,
    const QuantLib::Real exitEarlyErrorThreshold, const QuantLib::Real maxAcceptableError)
    : OptionletVolatilityStructure(referenceDate, sob->calendar(), sob->businessDayConvention(), sob->dayCounter()),
      optionletBase_(sob), ti_(ti), modelVariant_(modelVariant), outputVolatilityType_(outputVolatilityType),
      initialModelParameters_(initialModelParameters), maxCalibrationAttempts_(maxCalibrationAttempts),
      exitEarlyErrorThreshold_(exitEarlyErrorThreshold), maxAcceptableError_(maxAcceptableError) {
    registerWith(optionletBase_);
}

template <class TimeInterpolator>
inline QuantLib::Date SabrStrippedOptionletAdapter<TimeInterpolator>::maxDate() const {
    return Date::maxDate();
}

template <class TimeInterpolator>
inline QuantLib::Rate SabrStrippedOptionletAdapter<TimeInterpolator>::minStrike() const {
    return -QL_MAX_REAL;
}

template <class TimeInterpolator>
inline QuantLib::Rate SabrStrippedOptionletAdapter<TimeInterpolator>::maxStrike() const {
    return QL_MAX_REAL;
}

template <class TimeInterpolator>
inline QuantLib::VolatilityType SabrStrippedOptionletAdapter<TimeInterpolator>::volatilityType() const {
    return optionletBase_->volatilityType();
}

template <class TimeInterpolator>
inline QuantLib::Real SabrStrippedOptionletAdapter<TimeInterpolator>::displacement() const {
    return optionletBase_->displacement();
}

template <class TimeInterpolator> inline void SabrStrippedOptionletAdapter<TimeInterpolator>::update() {
    optionletBase_->update();
    TermStructure::update();
    LazyObject::update();
}

template <class TimeInterpolator>
inline void SabrStrippedOptionletAdapter<TimeInterpolator>::performCalculations() const {

    /* TODO */
}

template <class TimeInterpolator> inline void SabrStrippedOptionletAdapter<TimeInterpolator>::deepUpdate() {
    optionletBase_->update();
    update();
}

template <class TimeInterpolator>
inline boost::shared_ptr<QuantLib::StrippedOptionletBase>
SabrStrippedOptionletAdapter<TimeInterpolator>::optionletBase() const {
    return optionletBase_;
}

template <class TimeInterpolator>
inline boost::shared_ptr<QuantLib::SmileSection>
SabrStrippedOptionletAdapter<TimeInterpolator>::smileSectionImpl(QuantLib::Time optionTime) const {

    /* TODO */
    return {};
}

template <class TimeInterpolator>
inline QuantLib::Volatility
SabrStrippedOptionletAdapter<TimeInterpolator>::volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const {

    /* TODO */

    return 0.0;
}

} // namespace QuantExt

#endif
