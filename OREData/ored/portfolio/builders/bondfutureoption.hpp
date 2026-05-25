/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file portfolio/builders/bondfutureoption.hpp
    \brief Engine builder for bond future options.
    \ingroup builders
*/
#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/bondutils.hpp>
#include <qle/pricingengines/analyticeuropeanengine.hpp>
#include <qle/quotes/bondfuturequote.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace ore {
namespace data {

class BondFutureOptionEngineBuilder : public CachingPricingEngineBuilder<std::string, const std::string&>
{
public:
    const BondFutureUtils::IndexResults& indexResults() const {
        return indexResults_;
    }

protected:
    BondFutureOptionEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"BondFutureOption"}) {}

    // Note: `contractName` here is the name of the bond future contract underlying the option.
    std::string keyImpl(const std::string& contractName) override {
        return contractName;
    }

    QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> createBsProcess(const std::string& contractName)
    {
        // Wrapping a non-owned pointer in a shared_ptr like this is not recommended but should be safe here.
        // The alternative is large chunks of code being refactored / added to take `EngineFactory&` instead of 
        // `shared_ptr<EngineFactory>`.
        auto engineFactory = QuantLib::ext::shared_ptr<EngineFactory>(engineFactory_, [](EngineFactory*) {});

        // Create the bond future index and get all associated results.
        indexResults_ = BondFutureUtils::createIndex(contractName, engineFactory);

        // Bond future quote linked to the bond future index.
        auto futurePrice = QuantLib::Handle<QuantLib::Quote>(
            QuantLib::ext::make_shared<BondFutureQuote>(indexResults_.index));

        // Discount curve
        std::string contractCcy = indexResults_.refData->bondFutureData().currency;
        auto discountCurve = market_->discountCurve(contractCcy, configuration(MarketContext::pricing));

        // Volatility.
        auto volPtr = QuantLib::ext::make_shared<QuantLib::BlackConstantVol>(
            0, QuantLib::NullCalendar(), 0.070496, QuantLib::Actual365Fixed());
        auto vol = QuantLib::Handle<QuantLib::BlackVolTermStructure>(volPtr);

        return QuantLib::ext::make_shared<QuantLib::BlackProcess>(futurePrice, discountCurve, vol);
    }

    // Store the result of the index creation in case it is needed from the builder.
    BondFutureUtils::IndexResults indexResults_;
};

class BondFutureEuropeanOptionEngineBuilder : public BondFutureOptionEngineBuilder {
public:
    BondFutureEuropeanOptionEngineBuilder()
        : BondFutureOptionEngineBuilder("BlackScholesMerton", "AnalyticEuropeanEngine") {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& contractName) override
    {
        return QuantLib::ext::make_shared<QuantExt::AnalyticEuropeanEngine>(createBsProcess(contractName));
    }
};

} // namespace data
} // namespace ore
