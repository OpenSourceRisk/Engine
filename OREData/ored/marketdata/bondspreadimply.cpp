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

#include <ored/marketdata/bondspreadimply.hpp>
#include <ored/marketdata/bondspreadimplymarket.hpp>

#include <ored/portfolio/referencedata.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/inmemoryloader.hpp>
#include <ored/marketdata/security.hpp>
#include <ored/marketdata/structuredcurveerror.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondutils.hpp>

#include <ql/instruments/bond.hpp>
#include <ql/pricingengines/bond/bondfunctions.hpp>

#include <regex>

namespace ore {
namespace data {

std::map<std::string, QuantLib::ext::shared_ptr<Security>>
BondSpreadImply::requiredSecurities(const Date& asof, const QuantLib::ext::shared_ptr<TodaysMarketParameters>& params,
                                    const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs, const Loader& loader,
                                    const bool continueOnError, const std::string& excludeRegex) {
    std::regex excludePattern;
    if (!excludeRegex.empty())
        excludePattern = std::regex(excludeRegex);

    std::map<std::string, QuantLib::ext::shared_ptr<Security>> securities;
    for (const auto& configuration : params->configurations()) {
        LOG("identify securities that require a spread imply for configuration " << configuration.first);

        /* Loop over the security curve specs, do the spread imply where we have a price, but no spread
           and and store the calculated spread in the MarketImpl container */
        for (const auto& it : params->curveSpecs(configuration.first)) {
            auto securityspec = QuantLib::ext::dynamic_pointer_cast<SecuritySpec>(parseCurveSpec(it));
            if (securityspec) {
                std::string securityId = securityspec->securityID();
                if (std::regex_match(securityId, excludePattern)) {
                    DLOG("skip " << securityId << " because it matches the exclude regex (" << excludeRegex << ")");
                    continue;
                }
                if (curveConfigs->hasSecurityConfig(securityId)) {
                    if (curveConfigs->securityConfig(securityId)->spreadQuote().empty()) {
                        DLOG("no spread quote configuraed, skip security " << securityId);
                        continue;
                    }
                    QuantLib::ext::shared_ptr<Security> security;
                    try {
                        security = QuantLib::ext::make_shared<Security>(asof, *securityspec, loader, *curveConfigs);
                    } catch (const std::exception& e) {
                        if (continueOnError) {
                            StructuredCurveErrorMessage(securityId, "Bond spread imply failed",
                                                        "Will continue the calculations with a zero security spread: " +
                                                            std::string(e.what()))
                                .log();
                            continue;
                        } else {
                            QL_FAIL("Cannot process security " << securityId << " " << e.what());
                        }
                    }
                    if (security->spread().empty()) {
                        if (!security->price().empty()) {
                            LOG("empty spread, non-empty price: will imply spread for security " << securityId);
                            securities[securityId] = security;
                        } else {
                            DLOG("empty spread, empty price: spread will be left empty for security " << securityId);
                            StructuredCurveErrorMessage("Security/" + securityId, "Missing security spread",
                                                        "No security spread or bond price to imply the spread is "
                                                        "given. Will proceed assuming a zero spread.")
                                .log();
                        }
                    } else {
                        if (!security->price().empty()) {
                            WLOG("non-empty spread, non-empty price, will not overwrite existing spread for security "
                                 << securityId);
                        } else {
                            DLOG("non-empty spread, empty price, do nothing for security " << securityId);
                        }
                    }
                } else {
                    WLOG("do not have security curve config for '" << securityId
                                                                   << "' - skip this security in spread imply");
                }
            }
        }
    }
    LOG("got " << securities.size() << " securities");
    return securities;
}

QuantLib::ext::shared_ptr<Loader>
BondSpreadImply::implyBondSpreads(const std::map<std::string, QuantLib::ext::shared_ptr<Security>>& securities,
                                  const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager,
                                  const QuantLib::ext::shared_ptr<Market>& market,
                                  const QuantLib::ext::shared_ptr<EngineData>& engineData,
                                  const std::string& configuration, const IborFallbackConfig& iborFallbackConfig) {

    LOG("run bond spread imply");

    Settings::instance().evaluationDate() = market->asofDate();

    // build engine factory against which we build the bonds
    map<MarketContext, string> configurations;
    configurations[MarketContext::pricing] = configuration;
    auto spreadImplyMarket = QuantLib::ext::make_shared<BondSpreadImplyMarket>(market);
    auto edCopy = QuantLib::ext::make_shared<EngineData>(*engineData);
    edCopy->globalParameters()["RunType"] = "BondSpreadImply";
    auto engineFactory = QuantLib::ext::make_shared<EngineFactory>(edCopy, spreadImplyMarket, configurations,
                                                           referenceDataManager, iborFallbackConfig);

    // imply spreads and store the generate market data

    std::map<std::string, QuantLib::ext::shared_ptr<MarketDatum>> generatedSpreads;
    for (auto const& sec : securities) {
        auto storedSpread = generatedSpreads.find(sec.first);
        if (storedSpread == generatedSpreads.end()) {
            try {
                auto impliedSpread = QuantLib::ext::make_shared<SecuritySpreadQuote>(
                    implySpread(sec.first, sec.second->price()->value(), referenceDataManager, engineFactory,
                                spreadImplyMarket->spreadQuote(sec.first), configuration),
                    market->asofDate(), "BOND/YIELD_SPREAD/" + sec.first, sec.first);
                generatedSpreads[sec.first] = impliedSpread;
                LOG("spread imply succeded for security " << sec.first << ", got " << std::setprecision(10)
                                                          << impliedSpread->quote()->value());
            } catch (const std::exception& e) {
                StructuredCurveErrorMessage(
                    sec.first,
                    "bond spread imply failed (target price = " + std::to_string(sec.second->price()->value()) +
                        "). Will continue the calculations with a zero security spread.",
                    e.what()).log();
            }
        }
    }

    // add generated market data to loader and return the loader

    auto loader = QuantLib::ext::make_shared<InMemoryLoader>();

    for (auto const& s : generatedSpreads) {
        DLOG("adding market datum " << s.second->name() << " (" << s.second->quote()->value() << ") for asof "
                                    << market->asofDate() << " to loader");
        loader->add(market->asofDate(), s.second->name(), s.second->quote()->value());
    }

    LOG("bond spread imply finished.");
    return loader;
}

Real BondSpreadImply::implySpread(const std::string& securityId, const Real cleanPrice,
                                  const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager,
                                  const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                  const QuantLib::ext::shared_ptr<SimpleQuote>& spreadQuote, const std::string& configuration) {

    // checks, build bond from reference data

    QL_REQUIRE(referenceDataManager, "no reference data manager given");

    auto b = BondFactory::instance().build(engineFactory, referenceDataManager, securityId);
    Real adj = b.priceQuoteMethod == QuantExt::BondIndex::PriceQuoteMethod::CurrencyPerUnit
                   ? 1.0 / b.priceQuoteBaseValue
                   : 1.0;

    Real inflationFactor = b.inflationFactor();

    DLOG("implySpread for securityId " << securityId << ":");
    DLOG("settlement date         = " << QuantLib::io::iso_date(b.bond->settlementDate()));
    DLOG("market quote            = " << cleanPrice);
    DLOG("accrueds                = " << b.bond->accruedAmount());
    DLOG("inflation factor        = " << inflationFactor);
    DLOG("price quote method adj  = " << adj);
    DLOG("effective market price  = " << cleanPrice * inflationFactor * adj);

    auto targetFunction = [&b, spreadQuote, cleanPrice, adj, inflationFactor](const Real& s) {
        spreadQuote->setValue(s);
        if (b.modelBuilder != nullptr)
            b.modelBuilder->recalibrate();
        Real c = b.bond->cleanPrice() / 100.0;
        TLOG("--> spread imply: trying s = " << s << " yields clean price " << c);
        return c - cleanPrice * inflationFactor * adj;
    };

    // edge case: bond has a zero settlement value -> skip spread imply

    if (QuantLib::close_enough(b.bond->cleanPrice(), 0.0)) {
        DLOG("bond has a theoretical clean price of zero (no outstanding flows as of settlement date) -> skip spread "
             "imply and continue with zero security spread.");
        return 0.0;
    }

    // solve for spread and return result

    Brent brent;
    Real s = brent.solve(targetFunction, 1E-8, 0.0, 0.001);

    DLOG("theoretical pricing     = " << b.bond->cleanPrice() / 100.0);
    return s;
}

} // namespace data
} // namespace ore
