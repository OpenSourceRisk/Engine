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

/*! \file ored/model/inflation/infdkbuilder.hpp
    \brief Builder for a Dodgson-Kainth inflation model component
    \ingroup models
*/

#pragma once

#include <ored/marketdata/market.hpp>
#include <ored/model/inflation/infdkdata.hpp>
#include <qle/models/marketobserver.hpp>
#include <qle/models/modelbuilder.hpp>

#include <map>
#include <ostream>
#include <vector>

namespace ore {
namespace data {

/*! Builder for a Dodgson-Kainth inflation model component
    
    This class is a utility to turn a Dodgson-Kainth inflation model component description into an inflation model 
    parameterization which can be used to instantiate a CrossAssetModel.

    \ingroup models
*/
class InfDkBuilder : public QuantExt::ModelBuilder {
public:
    /*! Constructor
        \param market                   Market object
        \param data                     Dodgson-Kainth inflation model description
        \param configuration            Market configuration to use
        \param referenceCalibrationGrid The reference calibration grid
        \param dontCalibrate            Flag to use a dummy basecpi for the dependency market run
    */
    InfDkBuilder(
        const QuantLib::ext::shared_ptr<ore::data::Market>& market,
        const QuantLib::ext::shared_ptr<InfDkData>& data,
        const std::string& configuration = Market::defaultConfiguration,
        const std::string& referenceCalibrationGrid = "", 
        const bool dontCalibrate = false);

    //! \name Inspectors
    //@{
    std::string infIndex() { return data_->index(); }
    QuantLib::ext::shared_ptr<QuantExt::InfDkParametrization> parametrization() const;
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> optionBasket() const;
    //@}

    //! \name ModelBuilder interface
    //@{
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    //@}

    void setCalibrationDone() const;

private:
    void performCalculations() const override;
    Real optionStrikeValue(const Size j) const;
    Date optionMaturityDate(const Size j) const;
    void buildCapFloorBasket() const;

    // checks whether inf vols have changed compared to cache and updates the cache if requested
    bool volSurfaceChanged(const bool updateCache) const;

    // input data
    const QuantLib::ext::shared_ptr<ore::data::Market> market_;
    const std::string configuration_;
    const QuantLib::ext::shared_ptr<InfDkData> data_;
    const std::string referenceCalibrationGrid_;

    // computed
    QuantLib::ext::shared_ptr<QuantExt::InfDkParametrization> parametrization_;

    // which option in data->optionExpries() are actually in the basket?
    mutable std::vector<bool> optionActive_;
    mutable std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> optionBasket_;
    mutable QuantLib::Array optionExpiries_;

    // market data
    QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> inflationIndex_;
    Handle<YieldTermStructure> rateCurve_;
    QuantLib::Handle<QuantLib::CPIVolatilitySurface> infVol_;

    // Cache the fx volatilities
    mutable std::vector<QuantLib::Real> infPriceCache_;

    // helper flag to process forRecalculate()
    bool forceCalibration_ = false;

    // helper flag for process the DependencyMarket 
    bool dontCalibrate_ = false;

    // market observer
    QuantLib::ext::shared_ptr<QuantExt::MarketObserver> marketObserver_;
};

} // namespace data
} // namespace ore
