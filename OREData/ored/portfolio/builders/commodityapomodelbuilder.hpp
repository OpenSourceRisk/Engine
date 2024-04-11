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

/*! \file ored/portfolio/builders/commodityapomodelbuilder.hpp
    \brief model builder for commodityapos
    \ingroup utilities
*/

#pragma once

#include <ored/model/blackscholesmodelbuilderbase.hpp>

#include <qle/instruments/commodityapo.hpp>

namespace ore {
namespace data {

using namespace QuantExt;
using namespace QuantLib;

class CommodityApoModelBuilder : public BlackScholesModelBuilderBase {
public:
    CommodityApoModelBuilder(const Handle<YieldTermStructure>& curve,
                             const QuantLib::Handle<QuantLib::BlackVolTermStructure>& vol,
                             const QuantLib::ext::shared_ptr<QuantExt::CommodityAveragePriceOption>& apo,
                             const bool dontCalibrate);

protected:
    void setupDatesAndTimes() const override;
    std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> getCalibratedProcesses() const override;
    std::vector<std::vector<Real>> getCurveTimes() const override;
    std::vector<std::vector<std::pair<Real, Real>>> getVolTimesStrikes() const override;

    QuantLib::ext::shared_ptr<QuantExt::CommodityAveragePriceOption> apo_;
    bool dontCalibrate_ = false;
};

} // namespace data
} // namespace ore
