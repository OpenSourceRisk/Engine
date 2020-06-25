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

/*! \file ored/model/crossassetmodelbuilder.hpp
    \brief Build a cross asset model
    \ingroup models
*/

#pragma once

#include <vector>

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/types.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <ored/marketdata/market.hpp>
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/model/marketobserver.hpp>
#include <ored/model/modelbuilder.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! Cross Asset Model Builder
/*!
  CrossAssetModelBuilder takes a market snapshot, market conventions (the latter two
  passed to the constructor), and a model configuarion (passed to
  the "build" member function) to build and calibrate a cross asset model.

  \ingroup models
 */
class CrossAssetModelBuilder : public ModelBuilder {
public:
    /*! The market for the calibration can possibly be different from the final market
      defining the curves attached to the marginal LGM models; for example domestic OIS
      curves may be used for the in currency swaption calibration while the global model
      is operated under FX basis consistent discounting curves relative to the collateral
      OIS curve. */
    CrossAssetModelBuilder( //! Market object
        const boost::shared_ptr<Market>& market,
        //! cam configuration
        const boost::shared_ptr<CrossAssetModelData>& config,
        //! Market configuration for interest rate model calibration
        const std::string& configurationLgmCalibration = Market::defaultConfiguration,
        //! Market configuration for FX model calibration
        const std::string& configurationFxCalibration = Market::defaultConfiguration,
        //! Market configuration for EQ model calibration
        const std::string& configurationEqCalibration = Market::defaultConfiguration,
        //! Market configuration for INF model calibration
        const std::string& configurationInfCalibration = Market::defaultConfiguration,
        //! Market configuration for simulation
        const std::string& configurationFinalModel = Market::defaultConfiguration,
        //! Daycounter for date/time conversions
        const DayCounter& dayCounter = ActualActual(),
        //! calibrate the model?
        const bool dontCalibrate = false,
        //! continue if bootstrap error exceeds tolerance
        const bool continueOnError = false,
        //! reference calibration grid
        const std::string& referenceCalibrationGrid_ = "");

    //! Default destructor
    ~CrossAssetModelBuilder() {}

    //! return the model
    Handle<QuantExt::CrossAssetModel> model() const;

    //! \name Inspectors
    //@{
    const std::vector<Real>& swaptionCalibrationErrors();
    const std::vector<Real>& fxOptionCalibrationErrors();
    const std::vector<Real>& eqOptionCalibrationErrors();
    const std::vector<Real>& infCapFloorCalibrationErrors();
    //@}

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    //@}

private:
    void performCalculations() const override;
    void buildModel() const;
    void registerWithSubBuilders();
    void unregisterWithSubBuilders();

    mutable std::vector<std::vector<boost::shared_ptr<BlackCalibrationHelper>>> swaptionBaskets_;
    mutable std::vector<std::vector<boost::shared_ptr<BlackCalibrationHelper>>> fxOptionBaskets_;
    mutable std::vector<std::vector<boost::shared_ptr<BlackCalibrationHelper>>> eqOptionBaskets_;
    mutable std::vector<std::vector<boost::shared_ptr<BlackCalibrationHelper>>> infCapFloorBaskets_;
    mutable std::vector<Array> optionExpiries_;
    mutable std::vector<Array> swaptionMaturities_;
    mutable std::vector<Array> fxOptionExpiries_;
    mutable std::vector<Array> eqOptionExpiries_;
    mutable std::vector<Array> infCapFloorExpiries_;
    mutable std::vector<Real> swaptionCalibrationErrors_;
    mutable std::vector<Real> fxOptionCalibrationErrors_;
    mutable std::vector<Real> eqOptionCalibrationErrors_;
    mutable std::vector<Real> infCapFloorCalibrationErrors_;
    mutable std::set<boost::shared_ptr<ModelBuilder>> subBuilders_;

    const boost::shared_ptr<ore::data::Market> market_;
    const boost::shared_ptr<CrossAssetModelData> config_;
    const std::string configurationLgmCalibration_, configurationFxCalibration_, configurationEqCalibration_,
        configurationInfCalibration_, configurationFinalModel_;
    const DayCounter dayCounter_;
    const bool dontCalibrate_;
    const bool continueOnError_;
    const std::string referenceCalibrationGrid_;

    // TODO: Move CalibrationErrorType, optimizer and end criteria parameters to data
    boost::shared_ptr<OptimizationMethod> optimizationMethod_;
    EndCriteria endCriteria_;

    // helper flag to prcess forceRecalculate()
    bool forceCalibration_ = false;

    // market observer
    boost::shared_ptr<MarketObserver> marketObserver_;

    // resulting model
    mutable RelinkableHandle<QuantExt::CrossAssetModel> model_;
};

} // namespace data
} // namespace ore
