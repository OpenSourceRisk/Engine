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

/*! \file ored/model/infdkbuilder.hpp
    \brief Builder for a Lognormal INF model component
    \ingroup models
*/

#pragma once

#include <map>
#include <ostream>
#include <vector>

#include <ored/marketdata/market.hpp>
#include <ored/model/infdkdata.hpp>
#include <ored/model/marketobserver.hpp>
#include <ored/model/modelbuilder.hpp>

#include <qle/models/crossassetmodel.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! Builder for a Lognormal INF model component
/*!
This class is a utility to turn an INF model component's description
into an INF model parametrization which can be used to ultimately
instanciate a CrossAssetModel.

\ingroup models
*/
class InfDkBuilder : public ModelBuilder {
public:
    //! Constructor
    InfDkBuilder( //! Market object
        const boost::shared_ptr<ore::data::Market>& market,
        //! INF model parameters/dscription
        const boost::shared_ptr<InfDkData>& data,
        //! Market configuration to use
        const std::string& configuration = Market::defaultConfiguration,
        //! the reference calibration grid
        const std::string& referenceCalibrationGrid = "");

    //! \name Inspectors
    //@{
    std::string infIndex() { return data_->infIndex(); }
    boost::shared_ptr<QuantExt::InfDkParametrization> parametrization() const;
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
    void buildCapFloorBasket() const;
    // checks whether inf vols have changed compared to cache and updates the cache if requested
    bool volSurfaceChanged(const bool updateCache) const;

    // input data
    const boost::shared_ptr<ore::data::Market> market_;
    const std::string configuration_;
    const boost::shared_ptr<InfDkData> data_;
    const std::string referenceCalibrationGrid_;

    // computed
    boost::shared_ptr<QuantExt::InfDkParametrization> parametrization_;

    // which option in data->optionExpries() are actually in the basket?
    mutable std::vector<bool> optionActive_;
    mutable std::vector<boost::shared_ptr<BlackCalibrationHelper>> optionBasket_;
    mutable Array optionExpiries_;

    // market data
    boost::shared_ptr<ZeroInflationIndex> inflationIndex_;
    Handle<CPIVolatilitySurface> infVol_;

    // Cache the fx volatilities
    mutable std::vector<QuantLib::Real> infPriceCache_;

    // helper flag to process forRecalculate()
    bool forceCalibration_ = false;

    // market observer
    boost::shared_ptr<MarketObserver> marketObserver_;
};
} // namespace data
} // namespace ore
