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

/*! \file ored/model/lgmbuilder.hpp
    \brief Build an lgm model
    \ingroup models
*/

#pragma once

#include <map>
#include <ostream>
#include <vector>

#include <qle/models/lgm.hpp>

#include <ored/model/irlgmdata.hpp>
#include <qle/models/marketobserver.hpp>
#include <qle/models/modelbuilder.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! Builder for a Linear Gauss Markov model component
/*!
  This class is a utility that turns a Linear Gauss Markov
  model description into an interest rate model parametrisation which
  can be used to instantiate a CrossAssetModel.

  \ingroup models
 */
class LgmBuilder : public QuantExt::ModelBuilder {
public:
    /*! The configuration refers to the configuration to read swaption vol and swap index from the market.
        The discounting curve to price calibrating swaptions is derived from the swap index directly though,
        i.e. it is not read as a discount curve from the market (except as a fallback in case we do not find
        the swap index). */
    LgmBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<IrLgmData>& data,
               const std::string& configuration = Market::defaultConfiguration, Real bootstrapTolerance = 0.001,
               const bool continueOnError = false, const std::string& referenceCalibrationGrid = "",
               const bool setCalibrationInfo = false, const std::string& id = "unknwon");
    //! Return calibration error
    Real error() const;

    //! \name Inspectors
    //@{
    std::string qualifier() { return data_->qualifier(); }
    std::string ccy() { return currency_; }
    QuantLib::ext::shared_ptr<QuantExt::LGM> model() const;
    /* The curve used to build the lgm parametrization. This is initially the swap index discount curve (see ctor). It
       can be relinked later outside this builder to calibrate fx processes, for which one wants to use a xccy curve
       instead of the in-ccy curve that is used to calibrate the LGM model within this builder. */
    RelinkableHandle<YieldTermStructure> discountCurve() { return modelDiscountCurve_; }
    QuantLib::ext::shared_ptr<QuantExt::IrLgm1fParametrization> parametrization() const;
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> swaptionBasket() const;
    //@}

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    //@}

private:
    void performCalculations() const override;
    void buildSwaptionBasket() const;
    void updateSwaptionBasketVols() const;
    std::string getBasketDetails(QuantExt::LgmCalibrationInfo& info) const;
    // checks whether swaption vols have changed compared to cache and updates the cache if requested
    bool volSurfaceChanged(const bool updateCache) const;
    // populate expiry and term
    void getExpiryAndTerm(const Size j, Period& expiryPb, Period& termPb, Date& expiryDb, Date& termDb, Real& termT,
                          bool& expiryDateBased, bool& termDateBased) const;
    // get strike for jth option (or Null<Real>() if ATM)
    Real getStrike(const Size j) const;

    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    const std::string configuration_;
    QuantLib::ext::shared_ptr<IrLgmData> data_;
    const Real bootstrapTolerance_;
    const bool continueOnError_;
    const std::string referenceCalibrationGrid_;
    const bool setCalibrationInfo_;
    const std::string id_;
    bool requiresCalibration_ = false;
    std::string currency_; // derived from data->qualifier()

    mutable Real error_;
    mutable QuantLib::ext::shared_ptr<QuantExt::LGM> model_;
    mutable Array params_;
    mutable QuantLib::ext::shared_ptr<QuantExt::IrLgm1fParametrization> parametrization_;

    // which swaptions in data->optionExpries() are actually in the basket?
    mutable std::vector<bool> swaptionActive_;
    mutable std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> swaptionBasket_;
    mutable std::vector<Real> swaptionStrike_;
    mutable std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> swaptionBasketVols_;
    mutable Array swaptionExpiries_;
    mutable Array swaptionMaturities_;
    mutable Date swaptionBasketRefDate_;

    Handle<QuantLib::SwaptionVolatilityStructure> svts_;
    Handle<SwapIndex> swapIndex_, shortSwapIndex_;
    RelinkableHandle<YieldTermStructure> modelDiscountCurve_;
    Handle<YieldTermStructure> calibrationDiscountCurve_;

    // TODO: Move CalibrationErrorType, optimizer and end criteria parameters to data
    QuantLib::ext::shared_ptr<OptimizationMethod> optimizationMethod_;
    EndCriteria endCriteria_;
    BlackCalibrationHelper::CalibrationErrorType calibrationErrorType_;

    // Cache the swaption volatilities
    mutable std::vector<QuantLib::Real> swaptionVolCache_;

    bool forceCalibration_ = false;

    // LGM Observer
    QuantLib::ext::shared_ptr<QuantExt::MarketObserver> marketObserver_;
};
} // namespace data
} // namespace ore
