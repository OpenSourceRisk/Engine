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

/*! \file portfolio/builders/yoycapfloor.hpp
    \brief Engine builder for year-on-year inflation caps/floors
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

namespace ore {
namespace data {

//! Engine Builder for Year on Year Caps, Floors and Collars on an IborIndex
/*! Pricing engines are cached by currency
\ingroup builders
*/

class YoYCapFloorEngineBuilder : public CachingPricingEngineBuilder<string, const string&> {
public:
    YoYCapFloorEngineBuilder() : CachingEngineBuilder("YYCapModel", "YYCapEngine", {"YYCapFloor"}) {}

protected:
    virtual string keyImpl(const string& indexName) override { return indexName; }

    virtual QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const string& indexName) override;
};
} // namespace data
} // namespace ore
