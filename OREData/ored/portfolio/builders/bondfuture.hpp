/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/bondfuture.hpp
    \brief Engine builder for bond futures
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/bondutils.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/pricingengines/discountingbondfutureengine.hpp>

namespace ore {
namespace data {

class BondFutureEngineBuilder : public CachingPricingEngineBuilder<std::string, const std::string&>
{
public:
    const BondFutureUtils::IndexResults& indexResults() const {
        return indexResults_;
    }

protected:
    BondFutureEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"BondFuture"}) {}

    std::string keyImpl(const std::string& contractName) override {
        return contractName;
    }

    void populateIndexResults(const std::string& contractName)
    {
        // Wrapping a non-owned pointer in a shared_ptr like this is not recommended but should be safe here.
        // The alternative is large chunks of code being refactored / added to take `EngineFactory&` instead of 
        // `shared_ptr<EngineFactory>`.
        auto engineFactory = QuantLib::ext::shared_ptr<EngineFactory>(engineFactory_, [](EngineFactory*) {});

        // Create the bond future index and get all associated results.
        indexResults_ = BondFutureUtils::createIndex(contractName, engineFactory);
    }

private:
    // Store the result of the index creation in case it is needed from the builder.
    BondFutureUtils::IndexResults indexResults_;
};

class DiscountingBondFutureEngineBuilder : public BondFutureEngineBuilder {
public:
    DiscountingBondFutureEngineBuilder()
        : BondFutureEngineBuilder("DiscountedCashflows", "DiscountingBondFutureEngine") {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& contractName) override
    {
        populateIndexResults(contractName);
        const BondFutureUtils::IndexResults& indexResults = this->indexResults();
        std::string ccy = indexResults.refData->bondFutureData().currency;

        return QuantLib::ext::make_shared<QuantExt::DiscountingBondFutureEngine>(
            market_->discountCurve(ccy, configuration(MarketContext::pricing)),
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(indexResults.ctdConversionFactor)));
    }
};

} // namespace data
} // namespace ore
