/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/cpicapfloor.hpp
    \brief
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for CPI Caps/Floors
/*! Pricing engines are cached by index name
    \ingroup builders
*/
class CPICapFloorEngineBuilderBase : public CachingPricingEngineBuilder<string, const string&, const string&> {
public:
    CPICapFloorEngineBuilderBase(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"CPICapFloor"}) {}

protected:
    virtual string keyImpl(const string& indexName, const string& capFloor) override {
        return indexName + "_" + capFloor;
    }
};

//! Engine Builder for CPI Caps/Flors
/*!
  Using the QuantLib CPICapFLoor pricing engine that interpolates a CPICapFloorPriceSurface
  \ingroup builders
*/
class CPICapFloorEngineBuilder : public CPICapFloorEngineBuilderBase {
public:
    CPICapFloorEngineBuilder() : CPICapFloorEngineBuilderBase("CPICapFloorModel", "CPICapFloorEngine") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& indexName, const string& capFloor = "") override;
};

//! Engine Builder for CPI Caps/Flors
/*!
  Using the QuantExt Black engine based on a CPIVolatilitySurface
  \ingroup builders
*/
class CPIBlackCapFloorEngineBuilder : public CPICapFloorEngineBuilderBase {
public:
    CPIBlackCapFloorEngineBuilder() : CPICapFloorEngineBuilderBase("CPICapFloorModel", "CPIBlackCapFloorEngine") {}

protected:
    virtual boost::shared_ptr<PricingEngine> engineImpl(const string& indexName, const string& capFloor) override;
};

} // namespace data
} // namespace ore
