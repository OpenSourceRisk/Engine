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

ParametricVarCalculator::ParametricVarParams::Method parseParametricVarMethod(const string& s) {
    static map<string, ParametricVarCalculator::ParametricVarParams::Method> m = {
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

ParametricVarCalculator::ParametricVarParams::ParametricVarParams(const std::string& m, QuantLib::Size samp,
                                                                  QuantLib::Size sd)
    : method(parseParametricVarMethod(m)), samples(samp), seed(sd) {}

ParametricVarCalculator::ParametricVarCalculator(
    const map<string, set<pair<string, Size>>>& tradePortfolios, 
    const string& portfolioFilter,
    const ext::shared_ptr<SensitivityStream>& sensitivities,
    const map<pair<RiskFactorKey, RiskFactorKey>, Real> covariance, const vector<Real>& p, 
    const ParametricVarParams& parametricVarParams, const bool breakdown,
    const bool salvageCovarianceMatrix)
    : tradePortfolios_(tradePortfolios), portfolioFilter_(portfolioFilter), sensitivities_(sensitivities),
      covariance_(covariance), p_(p), parametricVarParams_(parametricVarParams),
      breakdown_(breakdown), salvageCovarianceMatrix_(salvageCovarianceMatrix) {}

ParametricVarCalculator::ParametricVarCalculator(
    const map<string, set<pair<string, Size>>>& tradePortfolios,
    const string& portfolioFilter, const ext::shared_ptr<SensitivityStream>& sensitivities,
    const ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
    const ore::data::TimePeriod& benchmarkPeriod,
    const ext::shared_ptr<SensitivityScenarioData>& sensitivityConfig,
    const ext::shared_ptr<ScenarioSimMarketParameters>& simMarketConfig, 
    const vector<Real>& p,
    const ParametricVarParams& parametricVarParams, const bool breakdown,
    const bool salvageCovarianceMatrix)
    : tradePortfolios_(tradePortfolios), portfolioFilter_(portfolioFilter), sensitivities_(sensitivities),
      hisScenGen_(hisScenGen), benchmarkPeriod_(benchmarkPeriod), sensitivityConfig_(sensitivityConfig), 
      simMarketConfig_(simMarketConfig), p_(p), parametricVarParams_(parametricVarParams), 
      breakdown_(breakdown), salvageCovarianceMatrix_(salvageCovarianceMatrix) {}


ParametricVarCalculator::ParametricVarCalculator(const std::vector<QuantLib::Real>& p,
                                                 const ParametricVarParams& parametricVarParams)
    : p_(p), parametricVarParams_(parametricVarParams) {}

void ParametricVarCalculator::calculate(ore::data::Report& report) {
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

    // loop over risk class and type filters (index 0 == all risk types)
    for (Size j = 0; j < (breakdown_ ? RiskFilter::numberOfRiskClasses() : 1); ++j) {
        for (Size k = 0; k < (breakdown_ ? RiskFilter::numberOfRiskTypes() : 1); ++k) {
            ext::shared_ptr<RiskFilter> rf = ext::make_shared<RiskFilter>(j, k);
            sensiAgg.aggregate(*sensitivities_, rf);
                        
            for (const auto& portfolioId : portfolioIds) {
                if (!hasFilter || portfolioId == allStr || boost::regex_match(portfolioId, filter)) {
                    map<RiskFactorKey, Real> deltas;
                    map<pair<RiskFactorKey, RiskFactorKey>, Real> gammas;

                    set<SensitivityRecord> srs = sensiAgg.sensitivities(portfolioId);

                    // Populate the deltas and gammas for a parametric VAR benchmark calculation
                    sensiAgg.generateDeltaGamma(portfolioId, deltas, gammas);
                    Matrix covariance;

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
                        boost::shared_ptr<QuantExt::CovarianceSalvage> covarianceSalvage;
                        DLOG("Covariance matrix has dimension " << keys.size() << " x " << keys.size());
                        if (salvageCovarianceMatrix_) {
                            DLOG("Make covariance matrix positive semi-definite using spectral method");
                            covarianceSalvage = boost::make_shared<QuantExt::SpectralCovarianceSalvage>();
                        } else {
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
                        var = computeVar(covariance, deltas, gammas, *covarianceSalvage);
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

std::vector<Real> ParametricVarCalculator::computeVar(const Matrix& omega, const map<RiskFactorKey, Real>& deltas,
    const map<CrossPair, Real>& gammas, const QuantExt::CovarianceSalvage& covarianceSalvage,
    Real factor, const bool includeGammaMargin, const bool includeDeltaMargin) {

    Array delta(deltas.size(), 0.0);
    Matrix gamma(deltas.size(), deltas.size(), 0.0);

     if (includeDeltaMargin) {
        Size counter = 0;
        for (auto it = deltas.begin(); it != deltas.end(); it++)
            delta[counter++] = factor * it->second;
    }

    if (includeGammaMargin) {
        Size outerIdx = 0;
        for (auto ito = deltas.begin(); ito != deltas.end(); ito++) {
            Size innerIdx = 0;
            // Error if no diagonal element
            gamma[outerIdx][outerIdx] = factor * gammas.at(std::make_pair(ito->first, ito->first));
            for (auto iti = deltas.begin(); iti != ito; iti++) {
                auto it = gammas.find(std::make_pair(iti->first, ito->first));
                if (it != gammas.end()) {
                    gamma[innerIdx][outerIdx] = factor * it->second;
                    gamma[outerIdx][innerIdx] = factor * it->second;
                }
                innerIdx++;
            }
            outerIdx++;
        }
    }

    if (parametricVarParams_.method == ParametricVarParams::Method::Delta) {
        std::vector<Real> res(p_.size());
        for (Size i = 0; i < p_.size(); ++i)
            res[i] = QuantExt::deltaVar(omega, delta, p_[i], covarianceSalvage);
        return res;
    } else if (parametricVarParams_.method == ParametricVarParams::Method::DeltaGammaNormal) {
        std::vector<Real> res(p_.size());
        for (Size i = 0; i < p_.size(); ++i) 
            res[i] = QuantExt::deltaGammaVarNormal(omega, delta, gamma, p_[i], covarianceSalvage);
        return res;
    } else if (parametricVarParams_.method == ParametricVarParams::Method::MonteCarlo) {
        QL_REQUIRE(parametricVarParams_.samples != Null<Size>(),
                   "ParametricVarCalculator::computeVar(): method MonteCarlo requires mcSamples");
        QL_REQUIRE(parametricVarParams_.seed != Null<Size>(),
                   "ParametricVarCalculator::computeVar(): method MonteCarlo requires mcSamples");
        return QuantExt::deltaGammaVarMc<PseudoRandom>(omega, delta, gamma, p_, parametricVarParams_.samples,
                                                       parametricVarParams_.seed, covarianceSalvage);
    } else if (parametricVarParams_.method == ParametricVarParams::Method::CornishFisher) {
        std::vector<Real> res(p_.size());
        for (Size i = 0; i < p_.size(); ++i)
            res[i] = QuantExt::deltaGammaVarCornishFisher(omega, delta, gamma, p_[i], covarianceSalvage);        
        return res;
    } else if (parametricVarParams_.method == ParametricVarParams::Method::Saddlepoint) {
        std::vector<Real> res(p_.size());
        for (Size i = 0; i < p_.size(); ++i) {
            try {            
                res[i] = QuantExt::deltaGammaVarSaddlepoint(omega, delta, gamma, p_[i], covarianceSalvage);
            } catch (const std::exception& e) {
                ALOG("Saddlepoint VaR computation exited with an error: " << e.what()
                                                                          << ", falling back on Monte-Carlo");
                res[i] = QuantExt::deltaGammaVarMc<PseudoRandom>(omega, delta, gamma, p_[i], 
                    parametricVarParams_.samples, parametricVarParams_.seed, covarianceSalvage);
            }
        }
        return res;
    } else
        QL_FAIL("ParametricVarCalculator::computeVar(): method " << parametricVarParams_.method << " not known.");
}

} // namespace analytics
} // namespace ore
