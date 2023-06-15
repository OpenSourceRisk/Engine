/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <orea/engine/parametricvar.hpp>

#include <orea/engine/riskfilter.hpp>
#include <orea/engine/historicalsensipnlcalculator.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <orea/scenario/scenarioshiftcalculator.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/math/deltagammavar.hpp>

#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>

#include <boost/regex.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {   

ParametricVarCalculator::ParametricVarParams::ParametricVarParams(const std::string& m, QuantLib::Size samp,
                                                                  QuantLib::Size sd)
    : method(parseParametricVarMethod(m)), samples(samp), seed(sd) {}

ParametricVarCalculator::ParametricVarParams::Method parseParametricVarMethod(const std::string& s) {
    static map<std::string, ParametricVarCalculator::ParametricVarParams::Method> m = {
        {"Delta", ParametricVarCalculator::ParametricVarParams::Method::Delta},
        {"DeltaGammaNormal", ParametricVarCalculator::ParametricVarParams::Method::DeltaGammaNormal},
        {"MonteCarlo", ParametricVarCalculator::ParametricVarParams::Method::MonteCarlo},
        {"Cornish-Fisher", ParametricVarCalculator::ParametricVarParams::Method::CornishFisher},
        {"Saddlepoint", ParametricVarCalculator::ParametricVarParams::Method::Saddlepoint}
    };

    auto it = m.find(s);
    if (it != m.end())
        return it->second;
    else
        QL_FAIL("ParametricVarParams Method \"" << s << "\" not recognized");
}

std::ostream& operator<<(std::ostream& out, const ParametricVarCalculator::ParametricVarParams::Method& method) {
    switch (method) {
    case ParametricVarCalculator::ParametricVarParams::Method::Delta:
        return out << "Delta";
    case ParametricVarCalculator::ParametricVarParams::Method::DeltaGammaNormal:
        return out << "DeltaGammaNormal";
    case ParametricVarCalculator::ParametricVarParams::Method::MonteCarlo:
        return out << "MonteCarlo";
    case ParametricVarCalculator::ParametricVarParams::Method::CornishFisher:
        return out << "Cornish-Fisher";
    case ParametricVarCalculator::ParametricVarParams::Method::Saddlepoint:
        return out << "Saddlepoint";
    default:
        QL_FAIL("Invalid ParametricVarCalculator::ParametricVarParams::Method");
    }
}

QuantLib::Real ParametricVarCalculator::var(QuantLib::Real confidence, const bool isCall, 
    const set<pair<string, Size>>& tradeIds) {
    Real factor = isCall ? 1.0 : -1.0;

    Array delta(deltas_.size(), 0.0);
    Matrix gamma(deltas_.size(), deltas_.size(), 0.0);

    if (includeDeltaMargin_) {
        Size counter = 0;
        for (auto it = deltas_.begin(); it != deltas_.end(); it++)
            delta[counter++] = factor * it->second;
    }

    if (includeGammaMargin_) {
        Size outerIdx = 0;
        for (auto ito = deltas_.begin(); ito != deltas_.end(); ito++) {
            Size innerIdx = 0;
            // Error if no diagonal element
            gamma[outerIdx][outerIdx] = factor * gammas_.at(std::make_pair(ito->first, ito->first));
            for (auto iti = deltas_.begin(); iti != ito; iti++) {
                auto it = gammas_.find(std::make_pair(iti->first, ito->first));
                if (it != gammas_.end()) {
                    gamma[innerIdx][outerIdx] = factor * it->second;
                    gamma[outerIdx][innerIdx] = factor * it->second;
                }
                innerIdx++;
            }
            outerIdx++;
        }
    }

    if (parametricVarParams_.method == ParametricVarCalculator::ParametricVarParams::Method::Delta)
        return QuantExt::deltaVar(omega_, delta, confidence, *covarianceSalvage_);
    else if (parametricVarParams_.method ==
                ParametricVarCalculator::ParametricVarParams::Method::DeltaGammaNormal)
        return QuantExt::deltaGammaVarNormal(omega_, delta, gamma, confidence, *covarianceSalvage_);
    else if (parametricVarParams_.method == ParametricVarCalculator::ParametricVarParams::Method::MonteCarlo) {
        QL_REQUIRE(parametricVarParams_.samples != Null<Size>(),
                    "ParametricVarCalculator::computeVar(): method MonteCarlo requires mcSamples");
        QL_REQUIRE(parametricVarParams_.seed != Null<Size>(),
                    "ParametricVarCalculator::computeVar(): method MonteCarlo requires mcSamples");
        return QuantExt::deltaGammaVarMc<PseudoRandom>(omega_, delta, gamma, confidence, parametricVarParams_.samples,
                                                        parametricVarParams_.seed, *covarianceSalvage_);
    } else if (parametricVarParams_.method == ParametricVarCalculator::ParametricVarParams::Method::CornishFisher)
        return QuantExt::deltaGammaVarCornishFisher(omega_, delta, gamma, confidence, *covarianceSalvage_);
    else if (parametricVarParams_.method == ParametricVarCalculator::ParametricVarParams::Method::Saddlepoint) {
        Real res;
        try {
            res = QuantExt::deltaGammaVarSaddlepoint(omega_, delta, gamma, confidence, *covarianceSalvage_);
        } catch (const std::exception& e) {
            ALOG("Saddlepoint VaR computation exited with an error: " << e.what()
                                                                        << ", falling back on Monte-Carlo");
            res = QuantExt::deltaGammaVarMc<PseudoRandom>(omega_, delta, gamma, confidence,
                parametricVarParams_.samples, parametricVarParams_.seed, *covarianceSalvage_);
        }        
        return res;
    } else
        QL_FAIL("ParametricVarCalculator::computeVar(): method " << parametricVarParams_.method << " not known.");
}

ParametricVarReport::ParametricVarReport(
    const map<string, set<pair<string, Size>>>& tradePortfolios, 
    const string& portfolioFilter,
    const ext::shared_ptr<SensitivityStream>& sensitivities,
    const map<pair<RiskFactorKey, RiskFactorKey>, Real> covariance, const vector<Real>& p, 
    const ParametricVarCalculator::ParametricVarParams& parametricVarParams,
    const bool breakdown, const bool salvageCovarianceMatrix)
    : tradePortfolios_(tradePortfolios), portfolioFilter_(portfolioFilter), sensitivities_(sensitivities),
      covariance_(covariance), p_(p), parametricVarParams_(parametricVarParams),
      breakdown_(breakdown), salvageCovarianceMatrix_(salvageCovarianceMatrix) {}

ParametricVarReport::ParametricVarReport(
    const map<string, set<pair<string, Size>>>& tradePortfolios,
    const string& portfolioFilter, const ext::shared_ptr<SensitivityStream>& sensitivities,
    const ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
    const ore::data::TimePeriod& benchmarkPeriod,
    const ext::shared_ptr<SensitivityScenarioData>& sensitivityConfig,
    const ext::shared_ptr<ScenarioSimMarketParameters>& simMarketConfig, 
    const vector<Real>& p,
    const ParametricVarCalculator::ParametricVarParams& parametricVarParams,
    const bool breakdown,
    const bool salvageCovarianceMatrix)
    : tradePortfolios_(tradePortfolios), portfolioFilter_(portfolioFilter), sensitivities_(sensitivities),
      hisScenGen_(hisScenGen), benchmarkPeriod_(benchmarkPeriod), sensitivityConfig_(sensitivityConfig), 
      simMarketConfig_(simMarketConfig), p_(p), parametricVarParams_(parametricVarParams), 
      breakdown_(breakdown), salvageCovarianceMatrix_(salvageCovarianceMatrix) {}

void ParametricVarReport::calculate(ore::data::Report& report) {
    // prepare report
    report.addColumn("Portfolio", string()).addColumn("RiskClass", string()).addColumn("RiskType", string());
    for (Size i = 0; i < p_.size(); ++i)
        report.addColumn("Quantile_" + std::to_string(p_[i]), double(), 6);

    // build portfolio filter, if given
    bool hasFilter = false;
    boost::regex filter;
    if (portfolioFilter_ != "") {
        hasFilter = true;
        filter = boost::regex(portfolioFilter_);
        LOG("Portfolio filter: " << portfolioFilter_);
    }

    vector<string> portfolioIds;
    for (const auto& [portfolioId, portfolio] : tradePortfolios_)
        portfolioIds.push_back(portfolioId);

    string allStr = "(all)";
    if (breakdown_ && tradePortfolios_.size() > 1) {
        // Create a summary portfolio
        portfolioIds.insert(portfolioIds.begin(), allStr);
        set<pair<string, Size>> allTradeIds;
        for (const auto& [portfolioId, portfolio] : tradePortfolios_) {
            if (!hasFilter || boost::regex_match(portfolioId, filter)) {
                for (const auto& trade : portfolio)
                    allTradeIds.insert(make_pair(trade.first, Null<Size>()));
            }
        } 
        tradePortfolios_[allStr] = allTradeIds;
    }

    SensitivityAggregator sensiAgg(tradePortfolios_);

    // create a calculator for the VAR

    Matrix covariance;
    map<RiskFactorKey, Real> deltas;
    map<pair<RiskFactorKey, RiskFactorKey>, Real> gammas;
    boost::shared_ptr<QuantExt::CovarianceSalvage> covarianceSalvage =
        boost::make_shared<QuantExt::SpectralCovarianceSalvage>();
    bool includeGammaMargin = true;
    bool includeDeltaMargin = true;
    ParametricVarCalculator calculator(parametricVarParams_, covariance, deltas, gammas, covarianceSalvage,
                                       includeGammaMargin, includeDeltaMargin); 

    // loop over risk class and type filters (index 0 == all risk types)
    for (Size j = 0; j < (breakdown_ ? RiskFilter::numberOfRiskClasses() : 1); ++j) {
        for (Size k = 0; k < (breakdown_ ? RiskFilter::numberOfRiskTypes() : 1); ++k) {
            ext::shared_ptr<RiskFilter> rf = ext::make_shared<RiskFilter>(j, k);
            sensiAgg.aggregate(*sensitivities_, rf);
                        
            for (const auto& portfolioId : portfolioIds) {
                if (!hasFilter || portfolioId == allStr || boost::regex_match(portfolioId, filter)) {

                    set<SensitivityRecord> srs = sensiAgg.sensitivities(portfolioId);
                    deltas.clear();
                    gammas.clear();
                    // Populate the deltas and gammas for a parametric VAR benchmark calculation
                    sensiAgg.generateDeltaGamma(portfolioId, deltas, gammas);

                    std::vector<Real> var;
                    if (deltas.size() > 0) {
                        vector<RiskFactorKey> keys;
                        transform(deltas.begin(), deltas.end(), back_inserter(keys),
                                  [](const pair<RiskFactorKey, Real>& kv) { return kv.first; });

                        // If covariance is provided use it, otherwise generate
                        if (covariance_.size() > 0) {
                            std::vector<bool> sensiKeyHasNonZeroVariance(keys.size(), false);

                            // build global covariance matrix
                            covariance = Matrix(keys.size(), keys.size(), 0.0);
                            Size unusedCovariance = 0;
                            for (const auto& c : covariance_) {
                                auto k1 = std::find(keys.begin(), keys.end(), c.first.first);
                                auto k2 = std::find(keys.begin(), keys.end(), c.first.second);
                                if (k1 != keys.end() && k2 != keys.end()) {
                                    covariance(k1 - keys.begin(), k2 - keys.begin()) = c.second;
                                    if (k1 == k2)
                                        sensiKeyHasNonZeroVariance[k1 - keys.begin()] = true;
                                } else
                                    ++unusedCovariance;
                            }
                            DLOG("Found " << covariance_.size() << " covariance matrix entries, " << unusedCovariance
                                          << " do not match a portfolio sensitivity and will not be used.");
                            for (Size i = 0; i < sensiKeyHasNonZeroVariance.size(); ++i) {
                                if (!sensiKeyHasNonZeroVariance[i])
                                    WLOG("Zero variance assigned to sensitivity key " << keys[i]);
                            }
                        } else {
                            QL_REQUIRE(benchmarkPeriod_, "No benchmark period provided to generate covariance matrix");
                            ext::shared_ptr<CovarianceCalculator> covCalculator =
                                QuantLib::ext::make_shared<CovarianceCalculator>(benchmarkPeriod_.value());
                            ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator =
                                ext::make_shared<ScenarioShiftCalculator>(sensitivityConfig_, simMarketConfig_);
                            ext::shared_ptr<HistoricalSensiPnlCalculator> sensiPnlCalculator =
                                ext::make_shared<HistoricalSensiPnlCalculator>(hisScenGen_, sensitivities_);

                            ext::shared_ptr<NPVCube> npvCube;

                            sensiPnlCalculator->populateSensiShifts(npvCube, keys, shiftCalculator);
                            sensiPnlCalculator->calculateSensiPnl(srs, keys, npvCube, {}, covCalculator);

                            covariance = covCalculator->covariance();
                        }

                        // make covariance matrix positive semi-definite
                        DLOG("Covariance matrix has dimension " << keys.size() << " x " << keys.size());
                        if (!salvageCovarianceMatrix_) {
                            LOG("Covariance matrix is no salvaged, check for positive semi-definiteness");
                            SymmetricSchurDecomposition ssd(covariance);
                            Real evMin = ssd.eigenvalues().back();
                            QL_REQUIRE(evMin > 0.0 || close_enough(evMin, 0.0),
                                       "ParametricVar: input covariance matrix is not positive semi-definite, smallest "
                                       "eigenvalue is "
                                           << evMin);
                            DLOG("Smallest eigenvalue is " << evMin);
                            covarianceSalvage = boost::make_shared<QuantExt::NoCovarianceSalvage>();
                        }
                        DLOG("Covariance matrix salvage complete.");
                        // compute var and write to report
                        for (Size i = 0; i < p_.size(); i++)
                            var.push_back(calculator.var(p_[i]));
                    } else
                        var = std::vector<Real>(p_.size(), 0.0);

                    if (!close_enough(QuantExt::detail::absMax(var), 0.0)) {
                        report.next();
                        report.add(portfolioId);
                        report.add(rf->riskClassLabel());
                        report.add(rf->riskTypeLabel());
                        for (auto const& v : var)
                            report.add(v);
                    }
                }
            }
            sensiAgg.reset();
        }
    }
}

} // namespace analytics
} // namespace ore
