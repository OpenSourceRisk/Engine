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

#include <qle/termstructures/parametricvolatilitysmilesection.hpp>

#include <ql/math/interpolation.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/termstructures/volatility/flatsmilesection.hpp>
#include <ql/termstructures/volatility/interpolatedsmilesection.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletbase.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <algorithm>
#include <vector>

namespace QuantExt {

template <class TimeInterpolator>
class SabrStrippedOptionletAdapter : public QuantLib::OptionletVolatilityStructure, public QuantLib::LazyObject {

public:
    /*! Constructor that does not take a reference date. The settlement days is derived from \p sob and the term
        structure will be a \e moving term structure.
    */
    SabrStrippedOptionletAdapter(const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
                                 const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
                                 const TimeInterpolator& ti = TimeInterpolator(),
                                 const boost::optional<QuantLib::VolatilityType> outputVolatilityType = boost::none,
                                 const std::vector<std::vector<std::pair<Real, bool>>>& initialModelParameters = {},
                                 const QuantLib::Size maxCalibrationAttempts = 10,
                                 const QuantLib::Real exitEarlyErrorThreshold = 0.005,
                                 const QuantLib::Real maxAcceptableError = 0.05);

    /*! Constructor taking an explicit \p referenceDate and the term structure will therefore be not \e moving.
     */
    SabrStrippedOptionletAdapter(const QuantLib::Date& referenceDate,
                                 const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
                                 const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
                                 const TimeInterpolator& ti = TimeInterpolator(),
                                 const boost::optional<QuantLib::VolatilityType> outputVolatilityType = boost::none,
                                 const std::vector<std::vector<std::pair<Real, bool>>>& initialModelParameters = {},
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
    QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase> optionletBase() const;
    QuantLib::ext::shared_ptr<QuantExt::ParametricVolatility> parametricVolatility() const {
        return parametricVolatility_;
    }
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

    //! SABR specific inputs
    QuantExt::SabrParametricVolatility::ModelVariant modelVariant_;
    boost::optional<QuantLib::VolatilityType> outputVolatilityType_;
    std::vector<std::vector<std::pair<Real, bool>>> initialModelParameters_;
    QuantLib::Size maxCalibrationAttempts_;
    QuantLib::Real exitEarlyErrorThreshold_;
    QuantLib::Real maxAcceptableError_;

    //! State
    mutable std::map<Real, QuantLib::ext::shared_ptr<ParametricVolatilitySmileSection>> cache_;
    mutable QuantLib::ext::shared_ptr<ParametricVolatility> parametricVolatility_;
    mutable std::unique_ptr<FlatExtrapolation> atmInterpolation_;
};

template <class TimeInterpolator>
SabrStrippedOptionletAdapter<TimeInterpolator>::SabrStrippedOptionletAdapter(
    const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
    const QuantExt::SabrParametricVolatility::ModelVariant modelVariant, const TimeInterpolator& ti,
    const boost::optional<QuantLib::VolatilityType> outputVolatilityType,
    const std::vector<std::vector<std::pair<Real, bool>>>& initialModelParameters,
    const QuantLib::Size maxCalibrationAttempts, const QuantLib::Real exitEarlyErrorThreshold,
    const QuantLib::Real maxAcceptableError)
    : OptionletVolatilityStructure(sob->settlementDays(), sob->calendar(), sob->businessDayConvention(),
                                   sob->dayCounter()),
      optionletBase_(sob), ti_(ti), modelVariant_(modelVariant), outputVolatilityType_(outputVolatilityType),
      initialModelParameters_(initialModelParameters), maxCalibrationAttempts_(maxCalibrationAttempts),
      exitEarlyErrorThreshold_(exitEarlyErrorThreshold), maxAcceptableError_(maxAcceptableError) {
    registerWith(optionletBase_);
}

template <class TimeInterpolator>
SabrStrippedOptionletAdapter<TimeInterpolator>::SabrStrippedOptionletAdapter(
    const QuantLib::Date& referenceDate, const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
    const QuantExt::SabrParametricVolatility::ModelVariant modelVariant, const TimeInterpolator& ti,
    const boost::optional<QuantLib::VolatilityType> outputVolatilityType,
    const std::vector<std::vector<std::pair<Real, bool>>>& initialModelParameters,
    const QuantLib::Size maxCalibrationAttempts, const QuantLib::Real exitEarlyErrorThreshold,
    const QuantLib::Real maxAcceptableError)
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
    cache_.clear();

    atmInterpolation_ = std::make_unique<FlatExtrapolation>(QuantLib::ext::make_shared<LinearInterpolation>(
        this->optionletBase()->optionletFixingTimes().begin(), this->optionletBase()->optionletFixingTimes().end(),
        this->optionletBase()->atmOptionletRates().begin()));
    atmInterpolation_->enableExtrapolation();
    atmInterpolation_->update();

    std::vector<ParametricVolatility::MarketSmile> marketSmiles;
    std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters;
    QL_REQUIRE(initialModelParameters_.empty() || initialModelParameters_.size() == 1 ||
                   initialModelParameters_.size() == this->optionletBase()->optionletFixingTimes().size(),
               "SabrStrippedOptionletAdapter: initial model parameters must be empty or their size ("
                   << initialModelParameters_.size()
                   << ") must be 1 or it must match the number of optionlet fixing times ("
                   << this->optionletBase()->optionletFixingTimes().size() << ")");
    for (Size i = 0; i < this->optionletBase()->optionletFixingTimes().size(); ++i) {
        Real forward = atmInterpolation_->operator()(this->optionletBase()->optionletFixingTimes()[i]);
        marketSmiles.push_back(ParametricVolatility::MarketSmile{this->optionletBase()->optionletFixingTimes()[i],
                                                                 Null<Real>(),
                                                                 forward,
                                                                 displacement(),
                                                                 {},
                                                                 this->optionletBase()->optionletStrikes(i),
                                                                 this->optionletBase()->optionletVolatilities(i)});
        if (!initialModelParameters_.empty()) {
            modelParameters[std::make_pair(this->optionletBase()->optionletFixingTimes()[i], Null<Real>())] =
                initialModelParameters_.size() == 1 ? initialModelParameters_.front() : initialModelParameters_[i];
        }
    }

    parametricVolatility_ = QuantLib::ext::make_shared<SabrParametricVolatility>(
        modelVariant_, marketSmiles, ParametricVolatility::MarketModelType::Black76,
        volatilityType() == QuantLib::Normal ? ParametricVolatility::MarketQuoteType::NormalVolatility
                                             : ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
        Handle<YieldTermStructure>(), modelParameters, maxCalibrationAttempts_, exitEarlyErrorThreshold_,
        maxAcceptableError_);
}

template <class TimeInterpolator> inline void SabrStrippedOptionletAdapter<TimeInterpolator>::deepUpdate() {
    optionletBase_->update();
    update();
}

template <class TimeInterpolator>
inline QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>
SabrStrippedOptionletAdapter<TimeInterpolator>::optionletBase() const {
    return optionletBase_;
}

template <class TimeInterpolator>
inline QuantLib::ext::shared_ptr<QuantLib::SmileSection>
SabrStrippedOptionletAdapter<TimeInterpolator>::smileSectionImpl(QuantLib::Time optionTime) const {
    calculate();
    if (auto c = cache_.find(optionTime); c != cache_.end()) {
        return c->second;
    }
    Real forward = atmInterpolation_->operator()(optionTime);
    QuantLib::VolatilityType outVolType = outputVolatilityType_ ? *outputVolatilityType_ : volatilityType();
    auto tmp = QuantLib::ext::make_shared<ParametricVolatilitySmileSection>(
        optionTime, Null<Real>(), forward, parametricVolatility_,
        outVolType == QuantLib::Normal ? ParametricVolatility::MarketQuoteType::NormalVolatility
                                       : ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility);
    cache_[optionTime] = tmp;
    return tmp;
}

template <class TimeInterpolator>
inline QuantLib::Volatility
SabrStrippedOptionletAdapter<TimeInterpolator>::volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const {
    return smileSectionImpl(optionTime)->volatility(strike);
}

} // namespace QuantExt

#endif
