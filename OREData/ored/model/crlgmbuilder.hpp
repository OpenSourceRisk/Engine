/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#pragma once

#include <map>
#include <ostream>
#include <vector>


#include <ored/model/modelbuilder.hpp>
#include <ored/model/crlgmdata.hpp>
#include <qle/models/crlgm1fparametrization.hpp>

namespace ore {
using namespace data;
namespace data {
using namespace QuantLib;

class CrLgmBuilder : public ModelBuilder {
public:
    /*! the configuration should refer to the calibration configuration here,
      alternative discounting curves are then usually set in the pricing
      engines for swaptions etc. */
    /*! this builder should be replaced by the OREData standard builder for cr lgm */
    CrLgmBuilder(const boost::shared_ptr<ore::data::Market>& market, const boost::shared_ptr<CrLgmData>& data,
                 const std::string& configuration = Market::defaultConfiguration);
    boost::shared_ptr<QuantExt::CrLgm1fParametrization> parametrization() const { return parametrization_; }

    bool requiresRecalibration() const { return false; }
    void performCalculations() const {}

private:
    // void buildOptionBasket();
    boost::shared_ptr<ore::data::Market> market_;
    const std::string configuration_;
    boost::shared_ptr<CrLgmData> data_;
    boost::shared_ptr<QuantExt::CrLgm1fParametrization> parametrization_;
    RelinkableHandle<DefaultProbabilityTermStructure> modelDefaultCurve_;
};

} // namespace data
} // namespace ore
