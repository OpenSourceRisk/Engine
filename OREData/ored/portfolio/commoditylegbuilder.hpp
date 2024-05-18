/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/commoditylegbuilder.hpp
    \brief Commodity fixed and floating leg builders
    \ingroup portfolio
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ql/cashflow.hpp>

namespace ore {
namespace data {

class CommodityFixedLegBuilder : public ore::data::LegBuilder {
public:
    CommodityFixedLegBuilder() : LegBuilder("CommodityFixed") {}

    QuantLib::Leg buildLeg(const ore::data::LegData& data,
                           const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory,
                           RequiredFixings& requiredFixings, const std::string& configuration,
                           const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                           const bool useXbsCurves = false) const override;
};

class CommodityFloatingLegBuilder : public ore::data::LegBuilder {
public:
    CommodityFloatingLegBuilder()
        : LegBuilder("CommodityFloating"), allAveraging_(false) {}

    QuantLib::Leg buildLeg(const ore::data::LegData& data,
                           const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory,
                           RequiredFixings& requiredFixings, const std::string& configuration,
                           const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                           const bool useXbsCurves = false) const override;

    //! Inspect the \c allAveraging_ flag
    bool allAveraging() const { return allAveraging_; }

private:
    /*! A flag that is set if the leg is averaging and the conventions indicate that the commodity contract itself
        on which the leg is based is averaging. This flag is false in all other circumstances.
    */
    mutable bool allAveraging_;
};

} // namespace data
} // namespace ore
