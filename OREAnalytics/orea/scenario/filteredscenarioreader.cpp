/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <orea/scenario/filteredscenarioreader.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

using namespace QuantLib;
using namespace ore::data;

namespace ore {
namespace analytics {

FilteredScenarioReader::FilteredScenarioReader(const QuantLib::ext::shared_ptr<ScenarioReader>& reader,
                                               const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simParams,
                                               const Period& filterTenor,
                                               const QuantLib::ext::shared_ptr<ScenarioFactory>& factory)
    : reader_(reader), simParams_(simParams), filterTenor_(filterTenor), factory_(factory) {
    QL_REQUIRE(reader_, "FilteredScenarioReader: underlying reader must not be null");
    QL_REQUIRE(simParams_, "FilteredScenarioReader: simulation market parameters must not be null");
    QL_REQUIRE(factory_, "FilteredScenarioReader: scenario factory must not be null");
    buildAllowedKeys();
    LOG("FilteredScenarioReader: filtering scenarios to tenor " << filterTenor_
        << ", allowed " << allowedKeys_.size() << " risk factor keys");
}

void FilteredScenarioReader::load(const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simParams,
                                  const QuantLib::ext::shared_ptr<TodaysMarketParameters>& marketParams) {
    reader_->load(simParams, marketParams);
}

bool FilteredScenarioReader::next() { return reader_->next(); }

Date FilteredScenarioReader::date() const { return reader_->date(); }

QuantLib::ext::shared_ptr<Scenario> FilteredScenarioReader::scenario() const {
    auto fullScenario = reader_->scenario();
    if (!fullScenario)
        return nullptr;

    auto filtered = factory_->buildScenario(fullScenario->asof(), fullScenario->isAbsolute(), fullScenario->isPar(),
                                            fullScenario->label(), fullScenario->getNumeraire());

    for (const auto& key : fullScenario->keys()) {
        if (allowedKeys_.count(key) > 0) {
            filtered->add(key, fullScenario->get(key));
        }
    }
    return filtered;
}

bool FilteredScenarioReader::tenorMatches(const std::vector<Period>& tenors, Size index) const {
    if (index < tenors.size())
        return tenors[index] == filterTenor_;
    return false;
}

void FilteredScenarioReader::buildAllowedKeys() {
    // We read the first scenario to discover all keys, then decide which to allow.
    // We don't need to actually read data here - we just need to know which keys exist.
    // Instead we build allowed keys purely from the simParams configuration.

    using KT = RiskFactorKey::KeyType;

    // Helper: for 1D tenor-based types, allow keys where the tenor at the given index matches
    auto allow1D = [&](KT keyType, const std::vector<std::string>& names,
                       const std::function<std::vector<Period>(const std::string&)>& getTenors) {
        for (const auto& name : names) {
            try {
                const auto& tenors = getTenors(name);
                for (Size i = 0; i < tenors.size(); ++i) {
                    if (tenors[i] == filterTenor_) {
                        allowedKeys_.insert(RiskFactorKey(keyType, name, i));
                        DLOG("FilteredScenarioReader: allowing " << keyType << "/" << name << "/" << i
                             << " (tenor " << tenors[i] << ")");
                    }
                }
            } catch (const std::exception& e) {
                DLOG("FilteredScenarioReader: skipping " << keyType << "/" << name << ": " << e.what());
            }
        }
    };

    // Helper: for types with no tenor (always allow index 0)
    auto allowNoTenor = [&](KT keyType, const std::vector<std::string>& names) {
        for (const auto& name : names) {
            allowedKeys_.insert(RiskFactorKey(keyType, name, 0));
        }
    };

    // --- 1D tenor-based types ---

    // DiscountCurve
    allow1D(KT::DiscountCurve, simParams_->discountCurveNames(),
            [&](const std::string& n) { return simParams_->yieldCurveTenors(n); });

    // YieldCurve
    allow1D(KT::YieldCurve, simParams_->yieldCurveNames(),
            [&](const std::string& n) { return simParams_->yieldCurveTenors(n); });

    // IndexCurve
    allow1D(KT::IndexCurve, simParams_->indices(),
            [&](const std::string& n) { return simParams_->yieldCurveTenors(n); });

    // SurvivalProbability
    allow1D(KT::SurvivalProbability, simParams_->defaultNames(),
            [&](const std::string& n) { return simParams_->defaultTenors(n); });

    // DividendYield
    allow1D(KT::DividendYield, simParams_->equityDividendYields(),
            [&](const std::string& n) { return simParams_->equityDividendTenors(n); });

    // ZeroInflationCurve
    allow1D(KT::ZeroInflationCurve, simParams_->paramsLookup(KT::ZeroInflationCurve),
            [&](const std::string& n) { return simParams_->zeroInflationTenors(n); });

    // YoYInflationCurve
    allow1D(KT::YoYInflationCurve, simParams_->paramsLookup(KT::YoYInflationCurve),
            [&](const std::string& n) { return simParams_->yoyInflationTenors(n); });

    // CommodityCurve
    allow1D(KT::CommodityCurve, simParams_->commodityNames(),
            [&](const std::string& n) { return simParams_->commodityCurveTenors(n); });

    // --- No-tenor types (always include) ---

    allowNoTenor(KT::FXSpot, simParams_->fxCcyPairs());
    allowNoTenor(KT::EquitySpot, simParams_->equityNames());

    // RecoveryRate
    allowNoTenor(KT::RecoveryRate, simParams_->paramsLookup(KT::RecoveryRate));

    // CPIIndex
    allowNoTenor(KT::CPIIndex, simParams_->paramsLookup(KT::CPIIndex));

    // SecuritySpread
    allowNoTenor(KT::SecuritySpread, simParams_->paramsLookup(KT::SecuritySpread));

    // --- Multi-dimensional vol types ---
    // For swaption vol: index = expiry_i * (num_terms * num_strikes) + term_j * num_strikes + strike_k
    // We match if expiry OR term matches the filter tenor
    {
        auto names = simParams_->swapVolKeys();
        for (const auto& name : names) {
            try {
                const auto& expiries = simParams_->swapVolExpiries(name);
                const auto& terms = simParams_->swapVolTerms(name);
                const auto& strikes = simParams_->swapVolStrikeSpreads(name);
                Size J = terms.size();
                Size K = strikes.size();
                for (Size i = 0; i < expiries.size(); ++i) {
                    for (Size j = 0; j < J; ++j) {
                        for (Size k = 0; k < K; ++k) {
                            if (expiries[i] == filterTenor_ || terms[j] == filterTenor_) {
                                Size index = i * J * K + j * K + k;
                                allowedKeys_.insert(RiskFactorKey(KT::SwaptionVolatility, name, index));
                            }
                        }
                    }
                }
            } catch (const std::exception& e) {
                DLOG("FilteredScenarioReader: skipping SwaptionVolatility/" << name << ": " << e.what());
            }
        }
    }

    // YieldVolatility: same structure as swaption vol but typically no cube
    {
        auto names = simParams_->yieldVolNames();
        for (const auto& name : names) {
            try {
                const auto& expiries = simParams_->yieldVolExpiries();
                const auto& terms = simParams_->yieldVolTerms();
                for (Size i = 0; i < expiries.size(); ++i) {
                    for (Size j = 0; j < terms.size(); ++j) {
                        if (expiries[i] == filterTenor_ || terms[j] == filterTenor_) {
                            Size index = i * terms.size() + j;
                            allowedKeys_.insert(RiskFactorKey(KT::YieldVolatility, name, index));
                        }
                    }
                }
            } catch (const std::exception& e) {
                DLOG("FilteredScenarioReader: skipping YieldVolatility/" << name << ": " << e.what());
            }
        }
    }

    // FXVolatility: if surface, idx = strike_i * num_expiries + expiry_j; if ATM, idx = expiry_j
    {
        auto names = simParams_->fxVolCcyPairs();
        for (const auto& name : names) {
            try {
                const auto& expiries = simParams_->fxVolExpiries(name);
                if (simParams_->fxVolIsSurface(name)) {
                    Size m = expiries.size();
                    Size n = simParams_->fxUseMoneyness(name) ? simParams_->fxVolMoneyness(name).size()
                                                              : simParams_->fxVolStdDevs(name).size();
                    for (Size i = 0; i < n; ++i) {
                        for (Size j = 0; j < m; ++j) {
                            if (expiries[j] == filterTenor_) {
                                Size idx = i * m + j;
                                allowedKeys_.insert(RiskFactorKey(KT::FXVolatility, name, idx));
                            }
                        }
                    }
                } else {
                    for (Size j = 0; j < expiries.size(); ++j) {
                        if (expiries[j] == filterTenor_) {
                            allowedKeys_.insert(RiskFactorKey(KT::FXVolatility, name, j));
                        }
                    }
                }
            } catch (const std::exception& e) {
                DLOG("FilteredScenarioReader: skipping FXVolatility/" << name << ": " << e.what());
            }
        }
    }

    // EquityVolatility: same indexing as FXVolatility
    {
        auto names = simParams_->paramsLookup(KT::EquityVolatility);
        for (const auto& name : names) {
            try {
                const auto& expiries = simParams_->equityVolExpiries(name);
                if (simParams_->equityVolIsSurface(name)) {
                    Size m = expiries.size();
                    Size n = simParams_->equityUseMoneyness(name) ? simParams_->equityVolMoneyness(name).size()
                                                                  : simParams_->equityVolStandardDevs(name).size();
                    for (Size i = 0; i < n; ++i) {
                        for (Size j = 0; j < m; ++j) {
                            if (expiries[j] == filterTenor_) {
                                Size idx = i * m + j;
                                allowedKeys_.insert(RiskFactorKey(KT::EquityVolatility, name, idx));
                            }
                        }
                    }
                } else {
                    for (Size j = 0; j < expiries.size(); ++j) {
                        if (expiries[j] == filterTenor_) {
                            allowedKeys_.insert(RiskFactorKey(KT::EquityVolatility, name, j));
                        }
                    }
                }
            } catch (const std::exception& e) {
                DLOG("FilteredScenarioReader: skipping EquityVolatility/" << name << ": " << e.what());
            }
        }
    }

    // OptionletVolatility: index = expiry_i * num_strikes + strike_j
    {
        auto names = simParams_->capFloorVolKeys();
        for (const auto& name : names) {
            try {
                const auto& expiries = simParams_->capFloorVolExpiries(name);
                const auto& strikes = simParams_->capFloorVolStrikes(name);
                Size J = strikes.empty() ? 1 : strikes.size();
                for (Size i = 0; i < expiries.size(); ++i) {
                    if (expiries[i] == filterTenor_) {
                        for (Size j = 0; j < J; ++j) {
                            Size index = i * J + j;
                            allowedKeys_.insert(RiskFactorKey(KT::OptionletVolatility, name, index));
                        }
                    }
                }
            } catch (const std::exception& e) {
                DLOG("FilteredScenarioReader: skipping OptionletVolatility/" << name << ": " << e.what());
            }
        }
    }

    // CDSVolatility: 1D expiry-based
    {
        auto names = simParams_->cdsVolNames();
        const auto& expiries = simParams_->cdsVolExpiries();
        for (const auto& name : names) {
            for (Size i = 0; i < expiries.size(); ++i) {
                if (expiries[i] == filterTenor_) {
                    allowedKeys_.insert(RiskFactorKey(KT::CDSVolatility, name, i));
                }
            }
        }
    }

    // CommodityVolatility: similar to FX vol with expiries x moneyness
    {
        auto names = simParams_->paramsLookup(KT::CommodityVolatility);
        for (const auto& name : names) {
            try {
                const auto& expiries = simParams_->commodityVolExpiries(name);
                const auto& moneyness = simParams_->commodityVolMoneyness(name);
                if (moneyness.size() > 1) {
                    Size m = expiries.size();
                    Size n = moneyness.size();
                    for (Size i = 0; i < n; ++i) {
                        for (Size j = 0; j < m; ++j) {
                            if (expiries[j] == filterTenor_) {
                                Size idx = i * m + j;
                                allowedKeys_.insert(RiskFactorKey(KT::CommodityVolatility, name, idx));
                            }
                        }
                    }
                } else {
                    for (Size j = 0; j < expiries.size(); ++j) {
                        if (expiries[j] == filterTenor_) {
                            allowedKeys_.insert(RiskFactorKey(KT::CommodityVolatility, name, j));
                        }
                    }
                }
            } catch (const std::exception& e) {
                DLOG("FilteredScenarioReader: skipping CommodityVolatility/" << name << ": " << e.what());
            }
        }
    }

    // BaseCorrelation: index = term_i * num_detach + detach_j - match on term
    {
        auto names = simParams_->paramsLookup(KT::BaseCorrelation);
        for (const auto& name : names) {
            try {
                const auto& terms = simParams_->baseCorrelationTerms();
                const auto& detach = simParams_->baseCorrelationDetachmentPoints();
                Size J = detach.size();
                for (Size i = 0; i < terms.size(); ++i) {
                    if (terms[i] == filterTenor_) {
                        for (Size j = 0; j < J; ++j) {
                            Size index = i * J + j;
                            allowedKeys_.insert(RiskFactorKey(KT::BaseCorrelation, name, index));
                        }
                    }
                }
            } catch (const std::exception& e) {
                DLOG("FilteredScenarioReader: skipping BaseCorrelation/" << name << ": " << e.what());
            }
        }
    }

    // Correlation: may have expiries
    {
        auto names = simParams_->paramsLookup(KT::Correlation);
        for (const auto& name : names) {
            try {
                const auto& expiries = simParams_->correlationExpiries();
                if (expiries.empty()) {
                    allowedKeys_.insert(RiskFactorKey(KT::Correlation, name, 0));
                } else {
                    const auto& strikes = simParams_->correlationStrikes();
                    Size J = strikes.empty() ? 1 : strikes.size();
                    for (Size i = 0; i < expiries.size(); ++i) {
                        if (expiries[i] == filterTenor_) {
                            for (Size j = 0; j < J; ++j) {
                                Size index = i * J + j;
                                allowedKeys_.insert(RiskFactorKey(KT::Correlation, name, index));
                            }
                        }
                    }
                }
            } catch (const std::exception& e) {
                DLOG("FilteredScenarioReader: skipping Correlation/" << name << ": " << e.what());
            }
        }
    }

    // YoYInflationCapFloorVolatility
    {
        auto names = simParams_->yoyInflationCapFloorVolNames();
        for (const auto& name : names) {
            try {
                const auto& expiries = simParams_->yoyInflationCapFloorVolExpiries(name);
                const auto& strikes = simParams_->yoyInflationCapFloorVolStrikes(name);
                Size J = strikes.empty() ? 1 : strikes.size();
                for (Size i = 0; i < expiries.size(); ++i) {
                    if (expiries[i] == filterTenor_) {
                        for (Size j = 0; j < J; ++j) {
                            Size index = i * J + j;
                            allowedKeys_.insert(RiskFactorKey(KT::YoYInflationCapFloorVolatility, name, index));
                        }
                    }
                }
            } catch (const std::exception& e) {
                DLOG("FilteredScenarioReader: skipping YoYInflationCapFloorVolatility/" << name << ": " << e.what());
            }
        }
    }

    // ZeroInflationCapFloorVolatility
    {
        auto names = simParams_->zeroInflationCapFloorVolNames();
        for (const auto& name : names) {
            try {
                const auto& expiries = simParams_->zeroInflationCapFloorVolExpiries(name);
                const auto& strikes = simParams_->zeroInflationCapFloorVolStrikes(name);
                Size J = strikes.empty() ? 1 : strikes.size();
                for (Size i = 0; i < expiries.size(); ++i) {
                    if (expiries[i] == filterTenor_) {
                        for (Size j = 0; j < J; ++j) {
                            Size index = i * J + j;
                            allowedKeys_.insert(RiskFactorKey(KT::ZeroInflationCapFloorVolatility, name, index));
                        }
                    }
                }
            } catch (const std::exception& e) {
                DLOG("FilteredScenarioReader: skipping ZeroInflationCapFloorVolatility/" << name << ": " << e.what());
            }
        }
    }

    // CPR - no tenor, always allow
    {
        auto names = simParams_->paramsLookup(KT::CPR);
        allowNoTenor(KT::CPR, names);
    }

    LOG("FilteredScenarioReader: built " << allowedKeys_.size() << " allowed risk factor keys for tenor " << filterTenor_);
}

} // namespace analytics
} // namespace ore
