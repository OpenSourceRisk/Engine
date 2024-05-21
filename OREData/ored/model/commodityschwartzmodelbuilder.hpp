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

/*! \file ored/model/commodityschwartzmodelbuilder.hpp
    \brief Builder for a Lognormal COM model component
    \ingroup models
*/

#pragma once

#include <map>
#include <ostream>
#include <vector>

#include <ored/marketdata/market.hpp>
#include <ored/model/commodityschwartzmodeldata.hpp>
#include <qle/models/marketobserver.hpp>
#include <qle/models/modelbuilder.hpp>

#include <qle/models/crossassetmodel.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! Builder for a COM model component
/*!
  This class is a utility to turn a COM model component's description
  into a COM model parametrization which can be used to ultimately
  instantiate a CrossAssetModel.
h
  \ingroup models
 */
class CommoditySchwartzModelBuilder : public QuantExt::ModelBuilder {
public:
    //! Constructor
    CommoditySchwartzModelBuilder( //! Market object
        const QuantLib::ext::shared_ptr<ore::data::Market>& market,
        //! EQ model parameters/description
        const QuantLib::ext::shared_ptr<CommoditySchwartzData>& data,
        //! base currency for calibration
        const QuantLib::Currency& baseCcy,
        //! Market configuration to use
        const std::string& configuration = Market::defaultConfiguration,
        //! the reference calibration grid
        const std::string& referenceCalibrationGrid = "");

    //! Return calibration error
    Real error() const;

    //! \name Inspectors
    //@{
    std::string name() { return data_->name(); }
    QuantLib::ext::shared_ptr<QuantExt::CommoditySchwartzParametrization> parametrization() const;
    QuantLib::ext::shared_ptr<QuantExt::CommoditySchwartzModel> model() const;
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> optionBasket() const;
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
    // checks whether vols have changed compared to cache and updates the cache if requested
    bool volSurfaceChanged(const bool updateCache) const;

    // input data
    const QuantLib::ext::shared_ptr<ore::data::Market> market_;
    const std::string configuration_;
    const QuantLib::ext::shared_ptr<CommoditySchwartzData> data_;
    const std::string referenceCalibrationGrid_;
    const QuantLib::Currency baseCcy_;

    // computed
    mutable Real error_;
    mutable QuantLib::ext::shared_ptr<QuantExt::CommoditySchwartzParametrization> parametrization_;
    mutable QuantLib::ext::shared_ptr<QuantExt::CommoditySchwartzModel> model_;

    // which options in data->optionExpiries() are actually in the basket?
    mutable std::vector<bool> optionActive_;
    mutable std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> optionBasket_;
    mutable Array optionExpiries_;

    // relevant market data
    Handle<Quote> fxSpot_;
    Handle<QuantExt::PriceTermStructure> curve_;
    Handle<BlackVolTermStructure> vol_;

    // Cache the volatilities
    mutable std::vector<QuantLib::Real> volCache_;

    // helper flag to process forRecalculate()
    bool forceCalibration_ = false;

    // market observer
    QuantLib::ext::shared_ptr<QuantExt::MarketObserver> marketObserver_;

    mutable std::vector<Real> calibrationErrors_;

    mutable Array params_;
};
} // namespace data
} // namespace ore
