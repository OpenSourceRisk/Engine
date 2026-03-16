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

/*! \file portfolio/builders/capfloor.hpp
  \brief builder that returns an engine to price a cap or floor on IBOR instrument
  \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

namespace ore {
namespace data {

//! Engine Builder for Caps, Floors and Collars on an IborIndex
/*! Pricing engines are cached by currency
    \ingroup builders
*/
class CapFloorEngineBuilder : public CachingPricingEngineBuilder<string, const string&> {
public:
    CapFloorEngineBuilder() : CachingEngineBuilder("IborCapModel", "IborCapEngine", {"CapFloor"}) {}

protected:
    string keyImpl(const string& index) override { return index; }
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const std::string& index) override;
};
} // namespace data
} // namespace ore
