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
    \brief
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>

namespace ore {
namespace data {

//! Engine Builder for Caps, Floors and Collars on an IborIndex
/*! Pricing engines are cached by currency
    \ingroup portfolio
*/
class CapFloorEngineBuilder : public CachingPricingEngineBuilder<string, const Currency&> {
public:
    CapFloorEngineBuilder() : CachingEngineBuilder("IborCapModel", "IborCapEngine") {}

protected:
    virtual string keyImpl(const Currency& ccy) override { return ccy.code(); }

    virtual boost::shared_ptr<PricingEngine> engineImpl(const Currency& ccy) override;
};
}
}
