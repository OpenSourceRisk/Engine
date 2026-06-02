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
#include <ored/portfolio/builders/utilities.hpp>
#include <qle/pricingengines/analyticeuropeanengine.hpp>
#include <qle/pricingengines/baroneadesiwhaleyengine.hpp>
#include <qle/pricingengines/fdblackscholesvanillaengine.hpp>
#include <qle/quotes/bondfuturequote.hpp>
#include <qle/termstructures/blackmonotonevarvoltermstructure.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace ore {
namespace data {

class BondFutureOptionEngineBuilder :
    public CachingPricingEngineBuilder<std::string, const std::string&, const std::string&, const QuantLib::Date&>
{
public:
    const BondFutureUtils::IndexResults& indexResults() const {
        return indexResults_;
    }

protected:
    BondFutureOptionEngineBuilder(const std::string& model, const std::string& engine,
        const std::set<std::string>& tradeTypes)
        : CachingEngineBuilder(model, engine, tradeTypes) {}

    // Note: `contractName` here is the name of the bond future contract underlying the option.
    //       `optTypeSuffix` is used to differentiate between call and put options when separate volatility surfaces 
    //        are used. So it may be `CALL` or `PUT` or empty i.e. ``.
    //        `expiryDate` is the expiry date of the option. It can be empty for some engines, e.g. European, but is
    //        required for others, e.g. American Finite Difference.
    std::string keyImpl(const std::string& contractName, const std::string& optTypeSuffix,
        const QuantLib::Date& expiryDate) override {

        std::string result = contractName;

        if (!optTypeSuffix.empty())
            result += "_" + optTypeSuffix;

        if (expiryDate != QuantLib::Date())
            result += "_" + to_string(expiryDate);

        return result;
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

    QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess> createBsProcess(
        const std::string& contractName, const std::string& optTypeSuffix, const std::vector<Time>& timePoints = {})
    {
        string config = configuration(ore::data::MarketContext::pricing);

        // Bond future quote linked to the bond future index.
        auto futurePrice = QuantLib::Handle<QuantLib::Quote>(
            QuantLib::ext::make_shared<BondFutureQuote>(indexResults_.index));

        // Discount curve
        std::string contractCcy = indexResults_.refData->bondFutureData().currency;
        auto discountCurve = market_->discountCurve(contractCcy, config);

        // Volatility.
        string bondFutureVolName = contractName + (optTypeSuffix.empty() ? "" : "_" + optTypeSuffix);
        auto vol = market_->bondFutureVol(bondFutureVolName, config);

        // It time points is non-empty, it means monotonic variance has been requested so wrap the volatility.
        if (!timePoints.empty()) {
            using VVTS = QuantExt::BlackMonotoneVarVolTermStructure;
            vol = Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<VVTS>(vol, timePoints));
            vol->enableExtrapolation();
        }

        return QuantLib::ext::make_shared<QuantLib::BlackProcess>(futurePrice, discountCurve, vol);
    }

protected:
    // Store the result of the index creation in case it is needed from the builder.
    BondFutureUtils::IndexResults indexResults_;
};

class BondFutureEuropeanOptionEngineBuilder : public BondFutureOptionEngineBuilder {
public:
    BondFutureEuropeanOptionEngineBuilder()
        : BondFutureOptionEngineBuilder("BlackScholesMerton", "AnalyticEuropeanEngine", { "BondFutureOption" }) {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& contractName,
        const std::string& optTypeSuffix, const QuantLib::Date& unusedExpiryDate) override
    {
        populateIndexResults(contractName);
        return QuantLib::ext::make_shared<QuantExt::AnalyticEuropeanEngine>(
            createBsProcess(contractName, optTypeSuffix));
    }
};

class BondFutureAmericanFDOptionEngineBuilder : public BondFutureOptionEngineBuilder {
public:
    BondFutureAmericanFDOptionEngineBuilder()
        : BondFutureOptionEngineBuilder("BlackScholesMerton", "FdBlackScholesVanillaEngine",
            { "BondFutureOptionAmerican" }) {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& contractName,
        const std::string& optTypeSuffix, const QuantLib::Date& expiryDate) override
    {
        // Need to do this first so that can get bond future currency to get the discount curve.
        populateIndexResults(contractName);

        // Discount curve
        string config = configuration(ore::data::MarketContext::pricing);
        std::string contractCcy = indexResults_.refData->bondFutureData().currency;
        auto discountCurve = market_->discountCurve(contractCcy, config);

        // Calculate the time to expiry needed by the finite difference engine.
        auto asof = discountCurve->referenceDate();
        QuantLib::Time expiry = discountCurve->dayCounter().yearFraction(asof, std::max(asof, expiryDate));

        // Create process and engine.
        FiniteDifferenceParams fdp = fdSchemeParams(*this, expiry);
        auto bsp = createBsProcess(contractName, optTypeSuffix, fdp.timePoints);
        return QuantLib::ext::make_shared<QuantExt::FdBlackScholesVanillaEngine2>(
            bsp, fdp.tGrid, fdp.xGrid, fdp.dampingSteps, fdp.scheme);
    }
};

class BondFutureAmericanBAWOptionEngineBuilder : public BondFutureOptionEngineBuilder {
public:
    BondFutureAmericanBAWOptionEngineBuilder()
        : BondFutureOptionEngineBuilder("BlackScholesMerton", "BaroneAdesiWhaleyApproximationEngine",
            { "BondFutureOptionAmerican" }) {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& contractName,
        const std::string& optTypeSuffix, const QuantLib::Date& unusedExpiryDate) override
    {
        populateIndexResults(contractName);
        return QuantLib::ext::make_shared<QuantExt::BaroneAdesiWhaleyApproximationEngine>(
            createBsProcess(contractName, optTypeSuffix));
    }
};

} // namespace data
} // namespace ore
