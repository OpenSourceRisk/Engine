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

/*! \file ored/model/fxbsbuilder.hpp
    \brief Builder for a Lognormal FX model component
    \ingroup models
*/

#pragma once

#include <map>
#include <ostream>
#include <vector>

#include <ored/marketdata/market.hpp>
#include <ored/model/fxbsdata.hpp>
#include <ored/model/marketobserver.hpp>
#include <ored/model/modelbuilder.hpp>

#include <qle/models/crossassetmodel.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! Builder for a Lognormal FX model component
/*!
  This class is a utility to turn an FX model component's description
  into an FX model parametrization which can be used to ultimately
  instanciate a CrossAssetModel.

  \ingroup models
 */
class FxBsBuilder : public ModelBuilder {
public:
    //! Constructor
    FxBsBuilder( //! Market object
        const boost::shared_ptr<ore::data::Market>& market,
        //! FX model parameters/dscription
        const boost::shared_ptr<FxBsData>& data,
        //! Market configuration to use
        const std::string& configuration = Market::defaultConfiguration,
        //! the reference calibration grid
        const std::string& referenceCalibrationGrid = "");

    //! Return calibration error
    Real error() const;

    //! \name Inspectors
    //@{
    std::string foreignCurrency() { return data_->foreignCcy(); }
    boost::shared_ptr<QuantExt::FxBsParametrization> parametrization() const;
    std::vector<boost::shared_ptr<BlackCalibrationHelper>> optionBasket() const;
    //@}

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    //@}

private:
    void performCalculations() const override;
    Real optionStrike(const Size j) const;
    Date optionExpiry(const Size j) const;
    void buildOptionBasket() const;
    // checks whether fx vols have changed compared to cache and updates the cache if requested
    bool volSurfaceChanged(const bool updateCache) const;

    // input data
    const boost::shared_ptr<ore::data::Market> market_;
    const std::string configuration_;
    const boost::shared_ptr<FxBsData> data_;
    const std::string referenceCalibrationGrid_;

    // computed
    mutable Real error_;
    boost::shared_ptr<QuantExt::FxBsParametrization> parametrization_;

    // which options in data->optionExpiries() are actually in the basket?
    mutable std::vector<bool> optionActive_;
    mutable std::vector<boost::shared_ptr<BlackCalibrationHelper>> optionBasket_;
    mutable Array optionExpiries_;

    // relevant market data
    Handle<Quote> fxSpot_;
    Handle<YieldTermStructure> ytsDom_, ytsFor_;
    Handle<BlackVolTermStructure> fxVol_;

    // Cache the fx volatilities
    mutable std::vector<QuantLib::Real> fxVolCache_;

    // helper flag to process forRecalculate()
    bool forceCalibration_ = false;

    // market observer
    boost::shared_ptr<MarketObserver> marketObserver_;
};

} // namespace data
} // namespace ore
