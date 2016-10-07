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

#include <ql/types.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <ored/marketdata/market.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ored/model/crossassetmodeldata.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

//! Cross Asset Model Builder
/*!
  CrossAssetModelBuilder takes a market snapshot, market conventions (the latter two
  passed to the constructor), and a model configuarion (passed to
  the "build" member function) to build and calibrate a cross asset model.

  \ingroup models
 */
class CrossAssetModelBuilder {
public:
    /*! The market for the calibration can possibly be different from the final market
      defining the curves attached to the marginal LGM models; for example domestic OIS
      curves may be used for the in currency swaption calibration while the global model
      is operated under FX basis consistent discounting curves relative to the collateral
      OIS curve. */
    CrossAssetModelBuilder( //! Market object
        const boost::shared_ptr<Market>& market,
        //! Market configuration for interest rate model calibration
        const std::string& configurationLgmCalibration = Market::defaultConfiguration,
        //! Market configuration for FX model calibration
        const std::string& configurationFxCalibration = Market::defaultConfiguration,
        //! Market configuration for simulation
        const std::string& configurationFinalModel = Market::defaultConfiguration,
        //! Daycounter for date/time conversions
        const DayCounter& dayCounter = ActualActual());

    //! Default destructor
    ~CrossAssetModelBuilder() {}

    //! Build and return the model for given configuration
    boost::shared_ptr<QuantExt::CrossAssetModel> build(const boost::shared_ptr<CrossAssetModelData>& config);

    //! \name Inspectors
    //@{
    const std::vector<Real>& swaptionCalibrationErrors() { return swaptionCalibrationErrors_; }
    const std::vector<Real>& fxOptionCalibrationErrors() { return fxOptionCalibrationErrors_; }
    //@}

private:
    std::vector<std::vector<boost::shared_ptr<CalibrationHelper>>> swaptionBaskets_;
    std::vector<std::vector<boost::shared_ptr<CalibrationHelper>>> fxOptionBaskets_;
    std::vector<Array> swaptionExpiries_;
    std::vector<Array> swaptionMaturities_;
    std::vector<Array> fxOptionExpiries_;
    std::vector<Real> swaptionCalibrationErrors_;
    std::vector<Real> fxOptionCalibrationErrors_;
    boost::shared_ptr<ore::data::Market> market_;
    const std::string configurationLgmCalibration_, configurationFxCalibration_, configurationFinalModel_;
    const DayCounter dayCounter_;

    // TODO: Move CalibrationErrorType, optimizer and end criteria parameters to data
    boost::shared_ptr<OptimizationMethod> optimizationMethod_;
    EndCriteria endCriteria_;
};
}
}
