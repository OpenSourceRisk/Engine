/*
 Copyright (C) 2020 Quaternion Risk Management Ltd

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

#include <ored/portfolio/builders/ascot.hpp>
#include <qle/pricingengines/intrinsicascotengine.hpp>

#include <qle/termstructures/discountratiomodifiedcurve.hpp>

#include <ql/quotes/compositequote.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

std::string AscotEngineBuilder::keyImpl(const std::string& id, const std::string& ccy) {
    return id;
}

QuantLib::ext::shared_ptr<QuantExt::PricingEngine> AscotIntrinsicEngineBuilder::engineImpl(
    const std::string& id, const std::string& ccy) {

    std::string config = this->configuration(ore::data::MarketContext::pricing);

    auto discountCurve = market_->discountCurve(ccy, config);
    
    return QuantLib::ext::make_shared<QuantExt::IntrinsicAscotEngine>(discountCurve);
}

} // namespace data
} // namespace ore
