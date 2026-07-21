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
#include <qle/termstructures/sabrparametricvolatility.hpp>
#include <qle/utilities/cashflows.hpp>
#include <qle/utilities/time.hpp>

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

class SabrStrippedOptionletAdapterBase {
public:
    using SliceParamInfo = SabrParametricVolatility::SliceParamInfo;
    using ModelParamData = std::vector<SliceParamInfo>;
    using ResidualCorrection = SabrParametricVolatility::ResidualCorrection;

    virtual ~SabrStrippedOptionletAdapterBase() = default;
    // The strikes of the underlying optionlets for the i-th expiry time.
    virtual std::vector<QuantLib::Real> optionletStrikes(QuantLib::Size i) const = 0;
    virtual const ModelParamData& initialModelParameters() const = 0;
};

template <class TimeInterpolator>
class SabrStrippedOptionletAdapter : public QuantLib::OptionletVolatilityStructure, public QuantLib::LazyObject,
    public SabrStrippedOptionletAdapterBase {

public:
    /*! Constructor that does not take a reference date. The settlement days is derived from \p sob and the term
        structure will be a \e moving term structure.

        The `iborIndexCalib`, if given, is used to provide the forward rates during the calibration of the SABR model.
        If `iborIndexCalib` is not provided, the forward rates are taken from the underlying stripped optionlet base 
        using linear interpolation. The `iborIndexRead` parameter, if given, is used in `smileSectionImpl` to provide
        the forward rate when this structure is being asked for a volatility at a given strike. It is generally not 
        provided, and the forward rate is obtained from `iborIndexCalib`, if given, and otherwise from the underlying
        stripped optionlet base via linear interpolation. However, for sensitivity analysis for example, it can be 
        convenient to provide an `iborIndexRead` that is linked to a forward curve that is bumped, so that the forward
        rate used in the volatility calculation is consistent with the bumped forward curve and all other parameters 
        remain the same. This gives the smile adjusted or SABR delta for the optionlet sensitivity. Omitting it and 
        using the `iborIndexCalib` for both calibration and reading will give the model or sticky strike delta.
    */
    SabrStrippedOptionletAdapter(
        const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
        const SabrParametricVolatility::ModelVariant modelVariant,
        const TimeInterpolator& ti = TimeInterpolator(),
        const QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType = QuantLib::ext::nullopt,
        const QuantLib::Real outputDisplacement = Null<Real>(),
        const QuantLib::Real modelDisplacement = Null<Real>(),
        const ModelParamData& initialModelParameters = {},
        const QuantLib::Size maxCalibrationAttempts = 10,
        const QuantLib::Real exitEarlyErrorThreshold = 0.005,
        const QuantLib::Real maxAcceptableError = 0.05,
        QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndexCalib = nullptr,
        QuantLib::Period rateCompPeriod = 0 * QuantLib::Days,
        QuantLib::ext::optional<ResidualCorrection> residualCorrection = QuantLib::ext::nullopt,
        QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndexRead = nullptr);

    /*! Constructor taking an explicit \p referenceDate and the term structure will therefore be not \e moving.
     */
    SabrStrippedOptionletAdapter(
        const QuantLib::Date& referenceDate,
        const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
        const SabrParametricVolatility::ModelVariant modelVariant,
        const TimeInterpolator& ti = TimeInterpolator(),
        const QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType = QuantLib::ext::nullopt,
        const QuantLib::Real outputDisplacement = Null<Real>(),
        const QuantLib::Real modelDisplacement = Null<Real>(),
        const ModelParamData& initialModelParameters = {},
        const QuantLib::Size maxCalibrationAttempts = 10,
        const QuantLib::Real exitEarlyErrorThreshold = 0.005,
        const QuantLib::Real maxAcceptableError = 0.05,
        QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndexCalib = nullptr,
        QuantLib::Period rateCompPeriod = 0 * QuantLib::Days,
        QuantLib::ext::optional<ResidualCorrection> residualCorrection = QuantLib::ext::nullopt,
        QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndexRead = nullptr);

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
    bool useEffectiveVolatility() const override;
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

    //! \name SabrStrippedOptionletAdapterBase interface
    //@{
    std::vector<QuantLib::Real> optionletStrikes(QuantLib::Size i) const override {
        return optionletBase_->optionletStrikes(i);
    }
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
    const ModelParamData& initialModelParameters() const override { return initialModelParameters_; }
    QuantLib::Size maxCalibrationAttempts() const { return maxCalibrationAttempts_; }
    QuantLib::Real exitEarlyErrorThreshold() const { return exitEarlyErrorThreshold_; }
    QuantLib::Real maxAcceptableError() const { return maxAcceptableError_; }
    QuantLib::ext::optional<ResidualCorrection> residualCorrection() const { return residualCorrection_; }
    //@}

    // Trigger a calibration and then reset the model parameters using the template provided.
    // The main purpose of this method is to allow the user to change the model parameters in preparation for a 
    // sensitivity analysis. For example, do a normal calibration and then on updates only imply alpha for example.
    void amendModelParameters(const SliceParamInfo& sspi);

protected:
    //! \name OptionletVolatilityStructure interface
    //@{
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const override;
    QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate strike) const override;
    //@}

private:
    using MMT = ParametricVolatility::MarketModelType;
    using MQT = ParametricVolatility::MarketQuoteType;

    //! Base optionlet object that provides the stripped optionlet volatilities
    QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase> optionletBase_;

    //! The interpolation object in the time direction
    TimeInterpolator ti_;

    //! SABR specific inputs
    QuantExt::SabrParametricVolatility::ModelVariant modelVariant_;
    QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType_;
    QuantLib::Real outputDisplacement_;
    QuantLib::Real modelDisplacement_ = Null<Real>();
    ModelParamData initialModelParameters_;
    QuantLib::Size maxCalibrationAttempts_;
    QuantLib::Real exitEarlyErrorThreshold_;
    QuantLib::Real maxAcceptableError_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndexCalib_;
    QuantLib::Period rateCompPeriod_;
    QuantLib::ext::optional<ResidualCorrection> residualCorrection_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndexRead_;

    //! State
    mutable std::map<Real, QuantLib::ext::shared_ptr<ParametricVolatilitySmileSection>> cache_;
    mutable QuantLib::ext::shared_ptr<ParametricVolatility> parametricVolatility_;
    mutable std::unique_ptr<FlatExtrapolation> atmInterpolation_;

    // Calculate the ATM rate.
    QuantLib::Real atmRate(QuantLib::Time optionTime, const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& iborIndex,
        const QuantLib::Date& fixingDate = QuantLib::Date()) const;

    // Helper to get parameter values with a check.
    static const QuantLib::Matrix& getSafeParam(const QuantLib::Matrix& m, const char* name,
        QuantLib::Size nExpiryTimes);
};

template <class TimeInterpolator>
SabrStrippedOptionletAdapter<TimeInterpolator>::SabrStrippedOptionletAdapter(
    const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
    const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
    const TimeInterpolator& ti,
    const QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType,
    const QuantLib::Real outputDisplacement,
    const QuantLib::Real modelDisplacement,
    const ModelParamData& initialModelParameters,
    const QuantLib::Size maxCalibrationAttempts,
    const QuantLib::Real exitEarlyErrorThreshold,
    const QuantLib::Real maxAcceptableError,
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndexCalib,
    QuantLib::Period rateCompPeriod,
    QuantLib::ext::optional<ResidualCorrection> residualCorrection,
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndexRead)
    : OptionletVolatilityStructure(sob->settlementDays(), sob->calendar(), sob->businessDayConvention(),
      sob->dayCounter()), optionletBase_(sob), ti_(ti), modelVariant_(modelVariant),
      outputVolatilityType_(outputVolatilityType), outputDisplacement_(outputDisplacement),
      initialModelParameters_(initialModelParameters), maxCalibrationAttempts_(maxCalibrationAttempts),
      exitEarlyErrorThreshold_(exitEarlyErrorThreshold), maxAcceptableError_(maxAcceptableError),
      iborIndexCalib_(std::move(iborIndexCalib)), rateCompPeriod_(std::move(rateCompPeriod)),
      residualCorrection_(std::move(residualCorrection)),
      iborIndexRead_(iborIndexRead ? std::move(iborIndexRead) : iborIndexCalib_) {
    registerWith(optionletBase_);
    // We only want to react to changes in the Ibor index used for calibration.
    if (iborIndexCalib_)
        registerWith(iborIndexCalib_);
}

template <class TimeInterpolator>
SabrStrippedOptionletAdapter<TimeInterpolator>::SabrStrippedOptionletAdapter(
    const QuantLib::Date& referenceDate,
    const QuantLib::ext::shared_ptr<QuantLib::StrippedOptionletBase>& sob,
    const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
    const TimeInterpolator& ti,
    const QuantLib::ext::optional<QuantLib::VolatilityType> outputVolatilityType,
    const QuantLib::Real outputDisplacement,
    const QuantLib::Real modelDiscplacement,
    const ModelParamData& initialModelParameters,
    const QuantLib::Size maxCalibrationAttempts,
    const QuantLib::Real exitEarlyErrorThreshold,
    const QuantLib::Real maxAcceptableError,
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndexCalib,
    QuantLib::Period rateCompPeriod,
    QuantLib::ext::optional<ResidualCorrection> residualCorrection,
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndexRead)
    : OptionletVolatilityStructure(referenceDate, sob->calendar(), sob->businessDayConvention(), sob->dayCounter()),
      optionletBase_(sob), ti_(ti), modelVariant_(modelVariant), outputVolatilityType_(outputVolatilityType),
      outputDisplacement_(outputDisplacement), initialModelParameters_(initialModelParameters),
      maxCalibrationAttempts_(maxCalibrationAttempts), exitEarlyErrorThreshold_(exitEarlyErrorThreshold),
      maxAcceptableError_(maxAcceptableError), iborIndexCalib_(std::move(iborIndexCalib)),
      rateCompPeriod_(std::move(rateCompPeriod)), residualCorrection_(std::move(residualCorrection)),
      iborIndexRead_(iborIndexRead ? std::move(iborIndexRead) : iborIndexCalib_) {
    registerWith(optionletBase_);
    // We only want to react to changes in the Ibor index used for calibration.
    if (iborIndexCalib_)
        registerWith(iborIndexCalib_);
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

template <class TimeInterpolator>
inline bool SabrStrippedOptionletAdapter<TimeInterpolator>::useEffectiveVolatility() const {
    return optionletBase_->useEffectiveVolatility();
}

template <class TimeInterpolator> inline void SabrStrippedOptionletAdapter<TimeInterpolator>::update() {
    optionletBase_->update();
    TermStructure::update();
    LazyObject::update();
}

template <class TimeInterpolator>
inline void SabrStrippedOptionletAdapter<TimeInterpolator>::performCalculations() const {
    cache_.clear();

    // If an Ibor index is not provided, we use interpolation of the optionlet base structure's ATM rates.
    const auto& fixingTimes = optionletBase_->optionletFixingTimes();
    if (!iborIndexCalib_) {
        atmInterpolation_ = std::make_unique<FlatExtrapolation>(QuantLib::ext::make_shared<LinearInterpolation>(
            fixingTimes.begin(), fixingTimes.end(), optionletBase_->atmOptionletRates().begin()));
        atmInterpolation_->enableExtrapolation();
        atmInterpolation_->update();
    }

    auto nInitMp = initialModelParameters_.size();
    QL_REQUIRE(initialModelParameters_.empty() || nInitMp == 1 || nInitMp == fixingTimes.size(),
        "SabrStrippedOptionletAdapter: initial model parameters must be empty or their size (" << nInitMp <<
        ") must be 1 or it must match the number of optionlet fixing times (" << fixingTimes.size() << ")");

    std::vector<ParametricVolatility::MarketSmile> marketSmiles;
    SabrParametricVolatility::ParamInfo modelParameters;
    const auto& fixingDates = optionletBase_->optionletFixingDates();
    for (Size i = 0; i < fixingTimes.size(); ++i) {
        Real forward = atmRate(fixingTimes[i], iborIndexCalib_, fixingDates[i]);
        marketSmiles.push_back(ParametricVolatility::MarketSmile{fixingTimes[i], Null<Real>(), forward,
            optionletBase_->displacement(), {}, optionletBase_->optionletStrikes(i),
            optionletBase_->optionletVolatilities(i)});

        if (!initialModelParameters_.empty()) {
            const auto& mp = initialModelParameters_[nInitMp == 1 ? 0 : i];
            modelParameters[std::make_pair(fixingTimes[i], Null<Real>())] = mp;
        }
    }

    std::map<Real, Real> modelShift;
    if (modelDisplacement_ != Null<Real>()) {
        modelShift[Null<Real>()] = modelDisplacement_;
    }

    auto outputMqt = optionletBase_->volatilityType() == QuantLib::Normal
        ? MQT::NormalVolatility : MQT::ShiftedLognormalVolatility;
    parametricVolatility_ = QuantLib::ext::make_shared<SabrParametricVolatility>(modelVariant_, marketSmiles,
        MMT::Black76, outputMqt, Handle<YieldTermStructure>(), modelParameters, modelShift, maxCalibrationAttempts_,
        exitEarlyErrorThreshold_, maxAcceptableError_, residualCorrection_);
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
inline void SabrStrippedOptionletAdapter<TimeInterpolator>::amendModelParameters(const SliceParamInfo& sspi)
{
    // For ease of notation below.
    using PVPC = ParametricVolatility::ParameterCalibration;

    calculate();

    // Amend the initial model parameters with the new ones provided by the user.
    auto sabrPv = QuantLib::ext::static_pointer_cast<SabrParametricVolatility>(parametricVolatility_);
    const auto& expiryTimes = sabrPv->timeToExpiries();
    auto nExpiryTimes = expiryTimes.size();
    const auto& modelParamInfo = sabrPv->modelParameters();
    QL_REQUIRE(modelParamInfo.size() == 1 || modelParamInfo.size() == nExpiryTimes,
        "SabrStrippedOptionletAdapter: expected size of SabrParametricVolatility model parameters ("
        << modelParamInfo.size() << ") to be 1 or equal to size of expiry times (" << nExpiryTimes << ")");
    QL_REQUIRE(sspi.size() == 4, "SabrStrippedOptionletAdapter: expected SliceParamInfo to have 4 elements.");

    // References to the already calibrated parameter values.
    const Matrix& alpha = getSafeParam(sabrPv->alpha(), "alpha", nExpiryTimes);
    const Matrix& beta = getSafeParam(sabrPv->beta(), "beta", nExpiryTimes);
    const Matrix& nu = getSafeParam(sabrPv->nu(), "nu", nExpiryTimes);
    const Matrix& rho = getSafeParam(sabrPv->rho(), "rho", nExpiryTimes);
    const std::array<std::reference_wrapper<const Matrix>, 4> params{ alpha, beta, nu, rho };

    // In the loop below, we avoid accessing the modelParamInfo by key as it consists of doubles.
    auto itMpi = modelParamInfo.begin();

    // The updated model parameters to be populated in the loop below.
    ModelParamData newMpd;
    newMpd.reserve(nExpiryTimes);

    for (QuantLib::Size i = 0; i < nExpiryTimes; ++i) {
        // Existing SABR parameters for the current i-th slice at i-th expiry time.
        const SliceParamInfo& mp = itMpi->second;
        QL_REQUIRE(mp.size() == 4, "SabrStrippedOptionletAdapter: expected SliceParamInfo to have 4 elements.");

        // Modified SABR parameters, to be created below, for the current i-th slice at i-th expiry time.
        SliceParamInfo newSlice;
        newSlice.reserve(4);

        for (QuantLib::Size j = 0; j < 4; ++j) {
            const Matrix& paramMtx = params[j].get();
            auto [paramValue, paramCalibType] = mp[j];
            if (paramValue != QuantLib::Null<QuantLib::Real>() && paramCalibType == PVPC::Fixed) {
                // If the original parameter value is fixed and has a valid value, we keep it as is.
                newSlice.emplace_back(mp[j]);
            } else if (sspi[j].first != QuantLib::Null<QuantLib::Real>()) {
                // If a concrete new value is provided, use it along with whatever calibration type is specified.
                newSlice.emplace_back(sspi[j]);
            } else {
                // In all other cases, we use the calibrated value and set the calibration type to the one given.
                newSlice.emplace_back(paramMtx[0][i], sspi[j].second);
            }
        }

        newMpd.emplace_back(std::move(newSlice));

        if (modelParamInfo.size() > 1)
            itMpi++;
    }

    // Update the initial model parameters with the new values.
    initialModelParameters_ = std::move(newMpd);
}

template <class TimeInterpolator>
inline QuantLib::ext::shared_ptr<QuantLib::SmileSection>
SabrStrippedOptionletAdapter<TimeInterpolator>::smileSectionImpl(QuantLib::Time optionTime) const {
    calculate();

    // The following logic is to avoid returning a smile section based on a stale forward. So, if iborIndexRead_ is 
    // non-null and is different from iborIndexCalib_ (if it is the same as iborIndexCalib_, this structure reacts to 
    // it and will clear the cache anyway in performCalculations), we check that the forward rate calculated from 
    // iborIndexRead_ is the same as the forward rate used in the cached smile section. If it is not, we create a new
    // smile section with the updated forward rate.
    Real forward;
    auto c = cache_.find(optionTime);
    if (c != cache_.end()) {
        if (!iborIndexRead_ || iborIndexCalib_ == iborIndexRead_)
            return c->second;

        forward = atmRate(optionTime, iborIndexRead_);
        if (QuantLib::close(forward, c->second->atmLevel()))
            return c->second;
    } else {
        forward = atmRate(optionTime, iborIndexRead_);
    }

    // Create new smile section.
    auto mqt = volatilityType() == QuantLib::Normal ? MQT::NormalVolatility : MQT::ShiftedLognormalVolatility;
    auto tmp = QuantLib::ext::make_shared<ParametricVolatilitySmileSection>(optionTime, Null<Real>(), forward,
        parametricVolatility_, mqt, displacement());

    // Update the cache with the new smile section.
    if (c != cache_.end())
        c->second = tmp;
    else
        cache_.emplace(optionTime, tmp);

    return tmp;
}

template <class TimeInterpolator>
inline QuantLib::Volatility
SabrStrippedOptionletAdapter<TimeInterpolator>::volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const {
    return smileSectionImpl(optionTime)->volatility(strike);
}

template <class TimeInterpolator>
inline QuantLib::Real
SabrStrippedOptionletAdapter<TimeInterpolator>::atmRate(QuantLib::Time optionTime,
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& iborIndex, const QuantLib::Date& fixingDate) const
{
    // If an Ibor index is not provided, just use linear interpolation of ATM rates from the optionlet base.
    if (!iborIndex)
        return (*atmInterpolation_)(optionTime);

    // If an Ibor index is provided, we calculate the ATM rate from the index.
    Date d = fixingDate != QuantLib::Date() ? fixingDate : dateFromTime(*this, optionTime);
    return getIndexRate(d, iborIndex, rateCompPeriod_);
}

template <class TimeInterpolator>
inline const QuantLib::Matrix& SabrStrippedOptionletAdapter<TimeInterpolator>::getSafeParam(const QuantLib::Matrix& m,
    const char* name, QuantLib::Size nExpiryTimes)
{
    QL_REQUIRE(m.rows() == 2 && m.columns() == nExpiryTimes, "SabrStrippedOptionletAdapter: expected calibrated "
        << name << " matrix to have 2 rows (" << m.rows() << ") and " << nExpiryTimes << " columns ("
        << m.columns() << ").");
    return m;
}

} // namespace QuantExt

#endif
