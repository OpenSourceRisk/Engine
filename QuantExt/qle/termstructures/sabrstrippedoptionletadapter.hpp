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
    SabrStrippedOptionletAdapter(
        const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
        const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
        const TimeInterpolator& ti = TimeInterpolator(),
        const QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType = QuantLib::ext::nullopt,
        const QuantLib::Real outputDisplacement = Null<Real>(), const QuantLib::Real modelDisplacement = Null<Real>(),
        const std::vector<std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>&
            initialModelParameters = {},
        const QuantLib::Size maxCalibrationAttempts = 10, const QuantLib::Real exitEarlyErrorThreshold = 0.005,
        const QuantLib::Real maxAcceptableError = 0.05,
        const std::vector<std::vector<Real>>& strikes = {},
        const std::vector<std::vector<Handle<Quote>>>& volSpreads = {},
        bool stickySabr = false);

    /*! Constructor taking an explicit \p referenceDate and the term structure will therefore be not \e moving.
     */
    SabrStrippedOptionletAdapter(
        const QuantLib::Date& referenceDate, const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
        const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
        const TimeInterpolator& ti = TimeInterpolator(),
        const QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType = QuantLib::ext::nullopt,
        const QuantLib::Real outputDisplacement = Null<Real>(), const QuantLib::Real modelDisplacement = Null<Real>(),
        const std::vector<std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>&
            initialModelParameters = {},
        const QuantLib::Size maxCalibrationAttempts = 10, const QuantLib::Real exitEarlyErrorThreshold = 0.005,
        const QuantLib::Real maxAcceptableError = 0.05,
        const std::vector<std::vector<Real>>& strikes = {},
        const std::vector<std::vector<Handle<Quote>>>& volSpreads = {},
        bool stickySabr = false);

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
        calculate();
        return parametricVolatility_;
    }
    QuantExt::SabrParametricVolatility::ModelVariant modelVariant() const { return modelVariant_; }
    QuantLib::Real modelDisplacement() const { return modelDisplacement_; }
    const std::vector<std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>&
        initialModelParameters() const { return initialModelParameters_; }
    QuantLib::Size maxCalibrationAttempts() const { return maxCalibrationAttempts_; }
    QuantLib::Real exitEarlyErrorThreshold() const { return exitEarlyErrorThreshold_; }
    const std::vector<std::vector<Real>>& strikes() const { return strikes_; }
    QuantLib::Real maxAcceptableError() const { return maxAcceptableError_; }
    //@}

protected:
    //! \name OptionletVolatilityStructure interface
    //@{
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const override;
    QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate strike) const override;
    //@}

private:
    void init();

    //! Base optionlet object that provides the stripped optionlet volatilities
    QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase> optionletBase_;

    //! The interpolation object in the time direction
    TimeInterpolator ti_;

    //! SABR specific inputs
    QuantExt::SabrParametricVolatility::ModelVariant modelVariant_;
    QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType_;
    QuantLib::Real outputDisplacement_;
    QuantLib::Real modelDisplacement_;
    std::vector<std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>> initialModelParameters_;
    QuantLib::Size maxCalibrationAttempts_;
    QuantLib::Real exitEarlyErrorThreshold_;
    QuantLib::Real maxAcceptableError_;
    std::vector<std::vector<Real>> strikes_;
    std::vector<std::vector<Handle<Quote>>> volSpreads_;
    bool stickySabr_;

    //! State
    mutable std::map<Real, QuantLib::ext::shared_ptr<ParametricVolatilitySmileSection>> cache_;
    mutable QuantLib::ext::shared_ptr<ParametricVolatility> parametricVolatility_;
    mutable std::unique_ptr<FlatExtrapolation> atmInterpolation_;
};

template <class TimeInterpolator>
inline void SabrStrippedOptionletAdapter<TimeInterpolator>::init() {
    registerWith(optionletBase_);
    Size nFixingDates = optionletBase_->optionletFixingDates().size();

    // The dimension of input StrippedOptionletBase can be either
    //
    // - ATM only (only 1 optionlet strike for every fixing date)
    //   SABR cube will be calibrated to the skew defined by strikes_ and volSpreads_
    //
    // or,
    //
    // - Smile (more than 1 optionlet strike for at least 1 fixing date)
    //   SABR cube will be calibrated to the skew defined in the input StrippedOptionletBase

    bool isAtm = true;
    for (Size i = 0; i < nFixingDates; ++i)
        if (optionletBase_->optionletStrikes(i).size() > 1)
            isAtm = isAtm && false;

    if (!isAtm) {
        QL_REQUIRE(strikes_.empty(), 
                   "When StrippedOptionletBase contains smiles, strikes "
                   "inputs to SabrStrippedOptionletAdapter must be empty");
        strikes_.resize(nFixingDates);
        for (Size i = 0; i < nFixingDates; ++i) {
            strikes_[i] = optionletBase_->optionletStrikes(i);
        }
        if (volSpreads_.empty()) {
            volSpreads_.resize(nFixingDates);
            for (Size i = 0; i < nFixingDates; ++i) {
                volSpreads_[i] = std::vector<Handle<Quote>>(
                    strikes_[i].size(), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0)));
            }
        } else { /* do nothing, volSpreads_ will be validated below */ }
    }

    QL_REQUIRE(nFixingDates == volSpreads_.size(),
               "mismatch between number of fixing dates (" <<
               nFixingDates << ") and number of rows (" <<
               volSpreads_.size() << ")");
    for (Size i = 0; i < volSpreads_.size(); i++) {
        Size nStrikes = strikes_[i].size();
        QL_REQUIRE(nStrikes == volSpreads_[i].size(),
                   "mismatch between number of strikes (" << nStrikes <<
                   ") and number of columns (" << volSpreads_[i].size() <<
                   ") in the " << io::ordinal(i+1) << " row");
    }
    if (!stickySabr_) {
        for (auto const& v : volSpreads_)
            for (auto const& s : v)
                registerWith(s);
    }
}

template <class TimeInterpolator>
SabrStrippedOptionletAdapter<TimeInterpolator>::SabrStrippedOptionletAdapter(
    const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
    const QuantExt::SabrParametricVolatility::ModelVariant modelVariant, const TimeInterpolator& ti,
    const QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType, const QuantLib::Real outputDisplacement,
    const QuantLib::Real modelDisplacement,
    const std::vector<std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>& initialModelParameters,
    const QuantLib::Size maxCalibrationAttempts, const QuantLib::Real exitEarlyErrorThreshold,
    const QuantLib::Real maxAcceptableError, const std::vector<std::vector<Real>>& strikes,
    const std::vector<std::vector<Handle<Quote>>>& volSpreads, bool stickySabr)
    : OptionletVolatilityStructure(sob->settlementDays(), sob->calendar(), sob->businessDayConvention(),
                                   sob->dayCounter()),
      optionletBase_(sob), ti_(ti), modelVariant_(modelVariant), outputVolatilityType_(outputVolatilityType),
      outputDisplacement_(outputDisplacement), initialModelParameters_(initialModelParameters),
      maxCalibrationAttempts_(maxCalibrationAttempts), exitEarlyErrorThreshold_(exitEarlyErrorThreshold),
      maxAcceptableError_(maxAcceptableError), strikes_(strikes), volSpreads_(volSpreads), stickySabr_(stickySabr) {
    init();
}

template <class TimeInterpolator>
SabrStrippedOptionletAdapter<TimeInterpolator>::SabrStrippedOptionletAdapter(
    const QuantLib::Date& referenceDate, const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
    const QuantExt::SabrParametricVolatility::ModelVariant modelVariant, const TimeInterpolator& ti,
    const QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType, const QuantLib::Real outputDisplacement,
    const QuantLib::Real modelDiscplacement,
    const std::vector<std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>& initialModelParameters,
    const QuantLib::Size maxCalibrationAttempts, const QuantLib::Real exitEarlyErrorThreshold,
    const QuantLib::Real maxAcceptableError, const std::vector<std::vector<Real>>& strikes,
    const std::vector<std::vector<Handle<Quote>>>& volSpreads, bool stickySabr)
    : OptionletVolatilityStructure(referenceDate, sob->calendar(), sob->businessDayConvention(), sob->dayCounter()),
      optionletBase_(sob), ti_(ti), modelVariant_(modelVariant), outputVolatilityType_(outputVolatilityType),
      outputDisplacement_(outputDisplacement), initialModelParameters_(initialModelParameters),
      maxCalibrationAttempts_(maxCalibrationAttempts), exitEarlyErrorThreshold_(exitEarlyErrorThreshold),
      maxAcceptableError_(maxAcceptableError), strikes_(strikes), volSpreads_(volSpreads), stickySabr_(stickySabr) {
    init();
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
    return outputVolatilityType_ ? *outputVolatilityType_ : optionletBase_->volatilityType();
}

template <class TimeInterpolator>
inline QuantLib::Real SabrStrippedOptionletAdapter<TimeInterpolator>::displacement() const {
    return outputDisplacement_ != Null<Real>() ? outputDisplacement_ : optionletBase_->displacement();
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
    std::map<std::pair<QuantLib::Real, QuantLib::Real>,
             std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>
        modelParameters;
    QL_REQUIRE(initialModelParameters_.empty() || initialModelParameters_.size() == 1 ||
                   initialModelParameters_.size() == this->optionletBase()->optionletFixingTimes().size(),
               "SabrStrippedOptionletAdapter: initial model parameters must be empty or their size ("
                   << initialModelParameters_.size()
                   << ") must be 1 or it must match the number of optionlet fixing times ("
                   << this->optionletBase()->optionletFixingTimes().size() << ")");
    for (Size i = 0; i < this->optionletBase()->optionletFixingTimes().size(); ++i) {
        Real forward = atmInterpolation_->operator()(this->optionletBase()->optionletFixingTimes()[i]);
        auto optionletStrikes = strikes_.empty() ? this->optionletBase()->optionletStrikes(i) : strikes_[i];
        QL_REQUIRE(!optionletStrikes.empty(),
                   "SabrStrippedOptionletAdapter: no optionlet strikes for optionlet fixing time "
                       << this->optionletBase()->optionletFixingTimes()[i]);
        auto optionletVolatilities = this->optionletBase()->optionletVolatilities(i);
        QL_REQUIRE(!optionletVolatilities.empty(),
                   "SabrStrippedOptionletAdapter: no optionlet volatilities for optionlet fixing time "
                       << this->optionletBase()->optionletFixingTimes()[i]);
        if (optionletVolatilities.size() == 1 && optionletStrikes.size() > 1)
            optionletVolatilities = std::vector<Real>(optionletStrikes.size(), optionletVolatilities[0]);
        for (Size j = 0; j < optionletVolatilities.size(); ++j) {
            optionletVolatilities[j] += volSpreads_[i][j]->value();
        }
        marketSmiles.push_back(ParametricVolatility::MarketSmile{this->optionletBase()->optionletFixingTimes()[i],
                                                                 Null<Real>(),
                                                                 forward,
                                                                 optionletBase_->displacement(),
                                                                 {},
                                                                 optionletStrikes,
                                                                 optionletVolatilities});
        if (!initialModelParameters_.empty()) {
            modelParameters[std::make_pair(this->optionletBase()->optionletFixingTimes()[i], Null<Real>())] =
                initialModelParameters_.size() == 1 ? initialModelParameters_.front() : initialModelParameters_[i];
        }
    }

    // For sticky SABR, we only need to re-imply the alpha parameter after initial calibration
    if (stickySabr_) {
        if (auto sabr = QuantLib::ext::dynamic_pointer_cast<SabrParametricVolatility>(parametricVolatility_)) {
            parametricVolatility_ = sabr->clone(marketSmiles, {});
            return;
        }
    }

    std::map<Real, Real> modelShift;
    if (modelDisplacement_ != Null<Real>()) {
        modelShift[Null<Real>()] = modelDisplacement_;
    }

    parametricVolatility_ = QuantLib::ext::make_shared<SabrParametricVolatility>(
        modelVariant_, marketSmiles, ParametricVolatility::MarketModelType::Black76,
        optionletBase_->volatilityType() == QuantLib::Normal
            ? ParametricVolatility::MarketQuoteType::NormalVolatility
            : ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
        Handle<YieldTermStructure>(), modelParameters, modelShift, maxCalibrationAttempts_, exitEarlyErrorThreshold_,
        maxAcceptableError_);

    // for sticky SABR, after initial calibration, we re-create parametric volatility with only alpha to be implied
    // this ensures that basis between the two parametric volatilities is eliminated
    if (stickySabr_) {
        if (auto sabr = QuantLib::ext::dynamic_pointer_cast<SabrParametricVolatility>(parametricVolatility_)) {
            parametricVolatility_ = sabr->clone(marketSmiles,
                                                { ParametricVolatility::ParameterCalibration::Implied, 
                                                  ParametricVolatility::ParameterCalibration::Fixed,
                                                  ParametricVolatility::ParameterCalibration::Fixed, 
                                                  ParametricVolatility::ParameterCalibration::Fixed });
        }
    }
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
    auto tmp = QuantLib::ext::make_shared<ParametricVolatilitySmileSection>(
        optionTime, Null<Real>(), forward, parametricVolatility_,
        volatilityType() == QuantLib::Normal ? ParametricVolatility::MarketQuoteType::NormalVolatility
                                             : ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
        displacement());
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
