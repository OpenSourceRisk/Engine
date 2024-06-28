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

#include <qle/models/crossassetmodel.hpp>
#include <qle/models/infdkparametrization.hpp>
#include <qle/models/infjyparameterization.hpp>
#include <qle/models/marketobserver.hpp>
#include <qle/models/modelbuilder.hpp>

#include <ored/marketdata/market.hpp>
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/model/inflation/infdkdata.hpp>
#include <ored/model/inflation/infjydata.hpp>
#include <ored/model/inflation/infjybuilder.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! Cross Asset Model Builder
/*!
  CrossAssetModelBuilder takes a market snapshot, market conventions (the latter two
  passed to the constructor), and a model configuration (passed to
  the "build" member function) to build and calibrate a cross asset model.

  \ingroup models
 */
class CrossAssetModelBuilder : public QuantExt::ModelBuilder {
public:
    /*! The market for the calibration can possibly be different from the final market
      defining the curves attached to the marginal LGM models; for example domestic OIS
      curves may be used for the in currency swaption calibration while the global model
      is operated under FX basis consistent discounting curves relative to the collateral
      OIS curve. */
    CrossAssetModelBuilder( //! Market object
        const QuantLib::ext::shared_ptr<Market>& market,
        //! cam configuration
        const QuantLib::ext::shared_ptr<CrossAssetModelData>& config,
        //! Market configuration for interest rate model calibration
        const std::string& configurationLgmCalibration = Market::defaultConfiguration,
        //! Market configuration for FX model calibration
        const std::string& configurationFxCalibration = Market::defaultConfiguration,
        //! Market configuration for EQ model calibration
        const std::string& configurationEqCalibration = Market::defaultConfiguration,
        //! Market configuration for INF model calibration
        const std::string& configurationInfCalibration = Market::defaultConfiguration,
        //! Market configuration for CR model calibration
        const std::string& configurationCrCalibration = Market::defaultConfiguration,
        //! Market configuration for simulation
        const std::string& configurationFinalModel = Market::defaultConfiguration,
        //! calibrate the model?
        const bool dontCalibrate = false,
        //! continue if bootstrap error exceeds tolerance
        const bool continueOnError = false,
        //! reference calibration grid
        const std::string& referenceCalibrationGrid_ = "",
	//! salvaging algorithm to apply to correlation matrix
	const SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None,
        //! id of the builder
        const std::string& id = "unknown");

    //! Default destructor
    ~CrossAssetModelBuilder() {}

    //! return the model
    Handle<QuantExt::CrossAssetModel> model() const;

    //! \name Inspectors
    //@{
    const std::vector<Real>& swaptionCalibrationErrors();
    const std::vector<Real>& fxOptionCalibrationErrors();
    const std::vector<Real>& eqOptionCalibrationErrors();
    const std::vector<Real>& inflationCalibrationErrors();
    const std::vector<Real>& comOptionCalibrationErrors();
    //@}

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    //@}

private:
    void performCalculations() const override;
    void buildModel() const;
    void resetModelParams(const CrossAssetModel::AssetType t, const Size param, const Size index, const Size i) const;
    void copyModelParams(const CrossAssetModel::AssetType t0, const Size param0, const Size index0, const Size i0,
                         const CrossAssetModel::AssetType t1, const Size param1, const Size index1, const Size i1,
                         const Real mult) const;

    mutable std::vector<std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>> swaptionBaskets_;
    mutable std::vector<std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>> fxOptionBaskets_;
    mutable std::vector<std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>> eqOptionBaskets_;
    mutable std::vector<std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>> comOptionBaskets_;
    mutable std::vector<Array> optionExpiries_;
    mutable std::vector<Array> swaptionMaturities_;
    mutable std::vector<Array> fxOptionExpiries_;
    mutable std::vector<Array> eqOptionExpiries_;
    mutable std::vector<Array> comOptionExpiries_;
    mutable std::vector<Real> swaptionCalibrationErrors_;
    mutable std::vector<Real> fxOptionCalibrationErrors_;
    mutable std::vector<Real> eqOptionCalibrationErrors_;
    mutable std::vector<Real> inflationCalibrationErrors_;
    mutable std::vector<Real> comOptionCalibrationErrors_;

    //! Store model builders for each asset under each asset type.
    mutable std::map<QuantExt::CrossAssetModel::AssetType,
                     std::map<QuantLib::Size, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>>
        subBuilders_;
    mutable Array params_;

    const QuantLib::ext::shared_ptr<ore::data::Market> market_;
    const QuantLib::ext::shared_ptr<CrossAssetModelData> config_;
    const std::string configurationLgmCalibration_, configurationFxCalibration_, configurationEqCalibration_,
        configurationInfCalibration_, configurationCrCalibration_, configurationComCalibration_, configurationFinalModel_;
    const bool dontCalibrate_;
    const bool continueOnError_;
    const std::string referenceCalibrationGrid_;
    const SalvagingAlgorithm::Type salvaging_;
    const std::string id_;

    // TODO: Move CalibrationErrorType, optimizer and end criteria parameters to data
    QuantLib::ext::shared_ptr<OptimizationMethod> optimizationMethod_;
    EndCriteria endCriteria_;

    // helper flag to process forceRecalculate()
    bool forceCalibration_ = false;

    // market observer
    QuantLib::ext::shared_ptr<QuantExt::MarketObserver> marketObserver_;

    // resulting model
    mutable RelinkableHandle<QuantExt::CrossAssetModel> model_;

    // Calibrate DK inflation model
    void calibrateInflation(const InfDkData& data,
        QuantLib::Size modelIdx,
        const std::vector<QuantLib::ext::shared_ptr<QuantLib::BlackCalibrationHelper>>& calibrationBasket,
        const QuantLib::ext::shared_ptr<QuantExt::InfDkParametrization>& inflationParam) const;

    // Calibrate JY inflation model
    void calibrateInflation(const InfJyData& data,
        QuantLib::Size modelIdx,
        const QuantLib::ext::shared_ptr<InfJyBuilder>& jyBuilder,
        const QuantLib::ext::shared_ptr<QuantExt::InfJyParameterization>& inflationParam) const;

    // Attach JY engines to helpers for JY calibration
    void setJyPricingEngine(QuantLib::Size modelIdx,
        const std::vector<QuantLib::ext::shared_ptr<QuantLib::CalibrationHelper>>& calibrationBasket,
        bool indexIsInterpolated) const;
};

} // namespace data
} // namespace ore
