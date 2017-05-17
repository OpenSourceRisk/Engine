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
    \brief
    \ingroup models
*/

#pragma once

#include <map>
#include <ostream>
#include <vector>

#include <ored/marketdata/market.hpp>
#include <ored/model/fxbsdata.hpp>
#include <qle/models/crossassetmodel.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

//! Builder for a Lognormal FX model component
/*!
  This class is a utility to turn an FX model component's description
  into an FX model parametrization which can be used to ultimately
  instanciate a CrossAssetModel.

  \ingroup models
 */
class FxBsBuilder {
public:
    //! Constructor
    FxBsBuilder( //! Market object
        const boost::shared_ptr<ore::data::Market>& market,
        //! FX model parameters/dscription
        const boost::shared_ptr<FxBsData>& data,
        //! Market configuration to use
        const std::string& configuration = Market::defaultConfiguration);

    //! Re-calibrate model component
    void update();

    //! Return calibration error
    Real error() { return error_; }

    //! \name Inspectors
    //@{
    std::string foreignCurrency() { return data_->foreignCcy(); }
    boost::shared_ptr<QuantExt::FxBsParametrization>& parametrization() { return parametrization_; }
    std::vector<boost::shared_ptr<CalibrationHelper>> optionBasket() { return optionBasket_; }
    //@}
private:
    void buildOptionBasket();

    boost::shared_ptr<ore::data::Market> market_;
    const std::string configuration_;
    boost::shared_ptr<FxBsData> data_;
    Real error_;
    boost::shared_ptr<QuantExt::FxBsParametrization> parametrization_;
    std::vector<boost::shared_ptr<CalibrationHelper>> optionBasket_;
    Array optionExpiries_;
};
}
}
