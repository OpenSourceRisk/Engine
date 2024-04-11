/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/ascot.hpp */

#pragma once

#include <qle/instruments/ascot.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

#include <ql/time/date.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {
using ore::data::CachingPricingEngineBuilder;

class AscotEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<std::string, const std::string&, const std::string&> {
protected:
    AscotEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"Ascot"}) {}

    std::string keyImpl(const std::string& id, const std::string& ccy) override;
};

class AscotIntrinsicEngineBuilder : public AscotEngineBuilder {
public:
    AscotIntrinsicEngineBuilder() : AscotEngineBuilder("BlackScholes", "Intrinsic") {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::PricingEngine>
    engineImpl(const std::string& id, const std::string& ccy) override;
};

} // namespace data
} // namespace ore
