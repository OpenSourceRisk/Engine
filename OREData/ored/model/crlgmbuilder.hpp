/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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

#pragma once

#include <map>
#include <ostream>
#include <vector>


#include <ored/model/crlgmdata.hpp>
#include <qle/models/modelbuilder.hpp>
#include <qle/models/crlgm1fparametrization.hpp>

namespace ore {
using namespace data;
namespace data {
using namespace QuantLib;

class CrLgmBuilder : public QuantExt::ModelBuilder {
public:
    /*! the configuration should refer to the calibration configuration here,
      alternative discounting curves are then usually set in the pricing
      engines for swaptions etc. */
    /*! this builder should be replaced by the OREData standard builder for cr lgm */
    CrLgmBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<CrLgmData>& data,
                 const std::string& configuration = Market::defaultConfiguration);
    QuantLib::ext::shared_ptr<QuantExt::CrLgm1fParametrization> parametrization() const { return parametrization_; }

    bool requiresRecalibration() const override { return false; }
    void performCalculations() const override {}

private:
    // void buildOptionBasket();
    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    const std::string configuration_;
    QuantLib::ext::shared_ptr<CrLgmData> data_;
    QuantLib::ext::shared_ptr<QuantExt::CrLgm1fParametrization> parametrization_;
    RelinkableHandle<DefaultProbabilityTermStructure> modelDefaultCurve_;
};

} // namespace data
} // namespace ore
