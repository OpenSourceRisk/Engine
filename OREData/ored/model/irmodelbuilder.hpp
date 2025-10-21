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

/*! \file ored/model/irmodelbuilder.hpp
    \brief Build a lgm or hw model
    \ingroup models
*/

#pragma once

#include <ored/model/irmodeldata.hpp>

#include <qle/models/marketobserver.hpp>
#include <qle/models/modelbuilder.hpp>
#include <qle/models/irmodel.hpp>

#include <ql/models/calibrationhelper.hpp>

#include <map>
#include <ostream>
#include <vector>

namespace ore {
namespace data {

using namespace QuantLib;

//! Builder base class for a lgm or hw model
class IrModelBuilder : public QuantExt::ModelBuilder {
public:
    // We apply certain fallback rules to ensure a robust calibration:
    enum class FallbackType { NoFallback, FallbackRule1 };

    /* Rule 1 If the helper's strike is too far away from the ATM level in terms of the relevant std dev, we move the
              calibration strike closer to the ATM level */
    static constexpr Real maxAtmStdDev = 3.0;

    /*! The configuration refers to the configuration to read swaption vol and swap index from the market.
        The discounting curve to price calibrating swaptions is derived from the swap index directly though,
        i.e. it is not read as a discount curve from the market (except as a fallback in case we do not find
        the swap index). */
    IrModelBuilder(
        const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<IrModelData>& data,
        const std::vector<std::string>& optionExpires, const std::vector<std::string>& optionTerms,
        const std::vector<std::string>& optionStrikes, const std::string& configuration = Market::defaultConfiguration,
        Real bootstrapTolerance = 0.001, const bool continueOnError = false,
        const std::string& referenceCalibrationGrid = "",
        BlackCalibrationHelper::CalibrationErrorType calibrationErrorType = BlackCalibrationHelper::RelativePriceError,
        const bool allowChangingFallbacksUnderScenarios = false, const bool allowModelFallbacks = false,
        const bool requiresCalibration = true, const std::string& modelLabel = "unknown",
        const std::string& id = "unknown");

    //! Return calibration error
    Real error() const;

    //! \name Inspectors
    //@{
    std::string qualifier() { return data_->qualifier(); }
    std::string ccy() { return currency_; }
    QuantLib::ext::shared_ptr<QuantExt::IrModel> model() const;
    /* The curve used to build the model parametrization. This is initially the swap index discount curve (see ctor). It
       can be relinked later outside this builder to calibrate fx processes, for which one wants to use a xccy curve
       instead of the in-ccy curve that is used to calibrate the LGM model within this builder. */
    RelinkableHandle<YieldTermStructure> discountCurve() { return modelDiscountCurve_; }
    QuantLib::ext::shared_ptr<QuantExt::Parametrization> parametrization() const;
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> swaptionBasket() const;
    //@}

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    void recalibrate() const override;
    void newCalcWithoutRecalibration() const override;
    //@}

protected:
    virtual void initParametrization() const = 0;
    virtual void calibrate() const = 0;
    virtual QuantLib::ext::shared_ptr<PricingEngine> getPricingEngine() const = 0;

    void processException(const std::string& s, const std::exception& e);
    void performCalculations() const override;
    void buildSwaptionBasket(const bool enforceFullRebuild) const;
    void updateSwaptionBasketVols() const;
    std::string getBasketDetails(std::vector<QuantExt::SwaptionData>& swaptionData) const;
    // checks whether swaption vols have changed compared to cache and updates the cache if requested
    bool volSurfaceChanged(const bool updateCache) const;
    // populate expiry and term
    void getExpiryAndTerm(const Size j, Period& expiryPb, Period& termPb, Date& expiryDb, Date& termDb, Real& termT,
                          bool& expiryDateBased, bool& termDateBased) const;
    // get strike for jth option (or Null<Real>() if ATM)
    Real getStrike(const Size j) const;

    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    std::string configuration_;
    QuantLib::ext::shared_ptr<IrModelData> data_;
    std::vector<std::string> optionExpiries_;
    std::vector<std::string> optionTerms_;
    std::vector<std::string> optionStrikes_;
    Real bootstrapTolerance_;
    bool continueOnError_;
    std::string referenceCalibrationGrid_;
    bool setCalibrationInfo_;
    BlackCalibrationHelper::CalibrationErrorType calibrationErrorType_;
    bool allowChangingFallbacksUnderScenarios_;

    bool allowModelFallbacks_ = false;
    bool requiresCalibration_ = false;
    std::string modelLabel_;
    std::string id_;

    std::string currency_;

    mutable bool parametrizationIsInitialized_ = false;
    mutable Real error_;
    mutable QuantLib::ext::shared_ptr<QuantExt::IrModel> model_;
    mutable Array params_;
    mutable QuantLib::ext::shared_ptr<QuantExt::Parametrization> parametrization_;

    // index of swaption in swaptionBasket for expiries in data->optionExpries(), or null if inactive
    mutable std::vector<Size> swaptionIndexInBasket_;

    mutable std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> swaptionBasket_;
    mutable std::vector<Real> swaptionStrike_;
    mutable std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> swaptionBasketVols_;
    mutable std::vector<FallbackType> swaptionFallbackType_;

    mutable std::set<double> swaptionExpiries_;
    mutable std::set<double> swaptionMaturities_;

    mutable Date swaptionBasketRefDate_;

    Handle<QuantLib::SwaptionVolatilityStructure> svts_;
    Handle<SwapIndex> swapIndex_, shortSwapIndex_;
    RelinkableHandle<YieldTermStructure> modelDiscountCurve_;
    Handle<YieldTermStructure> calibrationDiscountCurve_;

    // TODO: Move CalibrationErrorType, optimizer and end criteria parameters to data
    QuantLib::ext::shared_ptr<OptimizationMethod> optimizationMethod_;
    EndCriteria endCriteria_;

    // Cache the swaption volatilities
    mutable std::vector<QuantLib::Real> swaptionVolCache_;

    bool forceCalibration_ = false;
    mutable bool suspendCalibration_ = false;

    // Market Observer
    QuantLib::ext::shared_ptr<QuantExt::MarketObserver> marketObserver_;
};

} // namespace data
} // namespace ore
