/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file ored/model/hwbuilder.hpp
    \brief Build a hw model
    \ingroup models
*/

#pragma once

#include <map>
#include <ostream>
#include <vector>

#include <qle/models/hwmodel.hpp>

#include <ored/model/irhwmodeldata.hpp>
#include <ored/model/marketobserver.hpp>
#include <ored/model/modelbuilder.hpp>

namespace ore {
namespace data {
using namespace QuantLib;
using QuantExt::IrModel;
using QuantExt::HwModel;

//! Builder for a Hull White model or a HW component for the CAM
/* TODO this has a lot of overlap with LgmBuilder, factor out common logic */
class HwBuilder : public ModelBuilder {
public:
    /*! The configuration should refer to the calibration configuration here,
      alternative discounting curves are then usually set in the pricing
      engines for swaptions etc. */
    HwBuilder(const boost::shared_ptr<ore::data::Market>& market, const boost::shared_ptr<HwModelData>& data,
              const IrModel::Measure measure = IrModel::Measure::BA,
              const HwModel::Discretization discretization = HwModel::Discretization::Euler,
              const bool evaluateBankAccount = true, const std::string& configuration = Market::defaultConfiguration,
              Real bootstrapTolerance = 0.001, const bool continueOnError = false,
              const std::string& referenceCalibrationGrid = "", const bool setCalibrationInfo = false);
    //! Return calibration error
    Real error() const;

    //! \name Inspectors
    //@{
    std::string qualifier() { return data_->qualifier(); }
    std::string ccy() { return currency_; }
    boost::shared_ptr<QuantExt::HwModel> model() const;
    boost::shared_ptr<QuantExt::IrHwParametrization> parametrization() const;
    RelinkableHandle<YieldTermStructure> discountCurve() { return modelDiscountCurve_; }
    std::vector<boost::shared_ptr<BlackCalibrationHelper>> swaptionBasket() const;
    //@}

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    //@}

private:
    void performCalculations() const override;

    boost::shared_ptr<ore::data::Market> market_;
    const std::string configuration_;
    boost::shared_ptr<HwModelData> data_;
    IrModel::Measure measure_;
    HwModel::Discretization discretization_;
    bool evaluateBankAccount_;
    // const Real bootstrapTolerance_;
    // const bool continueOnError_;
    const std::string referenceCalibrationGrid_;
    // const bool setCalibrationInfo_;
    bool requiresCalibration_ = false;
    std::string currency_; // derived from data->qualifier()

    mutable Real error_;
    mutable boost::shared_ptr<QuantExt::HwModel> model_;
    mutable Array params_;
    mutable boost::shared_ptr<QuantExt::IrHwParametrization> parametrization_;

    // which swaptions in data->optionExpries() are actually in the basket?
    mutable std::vector<bool> swaptionActive_;
    mutable std::vector<boost::shared_ptr<BlackCalibrationHelper>> swaptionBasket_;
    mutable std::vector<Real> swaptionStrike_;
    mutable std::vector<boost::shared_ptr<SimpleQuote>> swaptionBasketVols_;
    mutable Array swaptionExpiries_;
    mutable Array swaptionMaturities_;
    mutable Date swaptionBasketRefDate_;

    RelinkableHandle<YieldTermStructure> modelDiscountCurve_;
    Handle<YieldTermStructure> calibrationDiscountCurve_;
    Handle<QuantLib::SwaptionVolatilityStructure> svts_;
    Handle<SwapIndex> swapIndex_, shortSwapIndex_;

    // TODO: Move CalibrationErrorType, optimizer and end criteria parameters to data
    boost::shared_ptr<OptimizationMethod> optimizationMethod_;
    EndCriteria endCriteria_;
    // BlackCalibrationHelper::CalibrationErrorType calibrationErrorType_;

    // Cache the swaption volatilities
    mutable std::vector<QuantLib::Real> swaptionVolCache_;

    bool forceCalibration_ = false;

    // LGM Observer
    boost::shared_ptr<MarketObserver> marketObserver_;
};
} // namespace data
} // namespace ore
