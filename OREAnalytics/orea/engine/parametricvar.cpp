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
#include <orea/scenario/shiftscenariogenerator.hpp>
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

ParametricVarCalculator::ParametricVarCalculator(
    const std::map<std::string, std::set<string>>& tradePortfolios, const std::string& portfolioFilter,
    const boost::shared_ptr<SensitivityStream>& sensitivities,
    const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance, const std::vector<Real>& p,
    const std::string& method, const Size mcSamples, const Size mcSeed, const bool breakdown,
    const bool salvageCovarianceMatrix)
    : tradePortfolios_(tradePortfolios), portfolioFilter_(portfolioFilter), sensitivities_(sensitivities),
      covariance_(covariance), p_(p), method_(method), mcSamples_(mcSamples), mcSeed_(mcSeed), breakdown_(breakdown),
      salvageCovarianceMatrix_(salvageCovarianceMatrix) {}

void ParametricVarCalculator::calculate(ore::data::Report& report) {
    LOG("Parametric VaR calculation started...");

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
    } else {
        LOG("No portfolio filter will be applied.");
    }

    // read sensitivities and preaggregate them per portfolio
    LOG("Preaggregate sensitivities per portfolio");
    std::set<RiskFactorKey> sensiKeysTmp;
    std::set<std::string> portfoliosTmp;
    std::map<std::string, std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real>> value1, value2;
    std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> value1All, value2All;
    while (SensitivityRecord sr = sensitivities_->next()) {
        std::set<std::string> portfolios;
        auto pn = tradePortfolios_.find(sr.tradeId);
        if (pn != tradePortfolios_.end()) {
            if (pn->second.empty())
                portfolios = {"(empty)"};
            else
                portfolios = pn->second;
        } else
            portfolios = {"(unknown)"};
        RiskFactorKey k1 = sr.key_1;
        RiskFactorKey k2 = sr.key_2;
        auto key = std::make_pair(k1, k2);
        if (k1 != RiskFactorKey())
            sensiKeysTmp.insert(k1);
        if (k2 != RiskFactorKey())
            sensiKeysTmp.insert(k2);
        bool relevant = false;
        for (auto const& p : portfolios) {
            if (!hasFilter || boost::regex_match(p, filter)) {
                relevant = true;
                portfoliosTmp.insert(p);
                if (sr.isCrossGamma()) {
                    value1[p][key] += sr.gamma;
                } else {
                    value1[p][key] += sr.delta;
                }
                if (!sr.isCrossGamma()) {
                    value2[p][key] += sr.gamma;
                }
            }
        }
        if (relevant) {
            if (sr.isCrossGamma()) {
                value1All[key] += sr.gamma;
            } else {
                value1All[key] += sr.delta;
            }
            if (!sr.isCrossGamma()) {
                value2All[key] += sr.gamma;
            }
        }
    }
    std::vector<RiskFactorKey> sensiKeys(sensiKeysTmp.begin(), sensiKeysTmp.end());
    std::vector<bool> sensiKeyHasNonZeroVariance(sensiKeys.size(), false);
    std::vector<std::string> portfolios(portfoliosTmp.begin(), portfoliosTmp.end());
    LOG("Have " << sensiKeys.size() << " sensitivity keys in " << portfolios.size() << " portfolios");

    // build global covariance matrix
    Matrix omega(sensiKeys.size(), sensiKeys.size(), 0.0);
    Size unusedCovariance = 0;
    for (const auto& c : covariance_) {
        auto k1 = std::find(sensiKeys.begin(), sensiKeys.end(), c.first.first);
        auto k2 = std::find(sensiKeys.begin(), sensiKeys.end(), c.first.second);
        if (k1 != sensiKeys.end() && k2 != sensiKeys.end()) {
            omega(k1 - sensiKeys.begin(), k2 - sensiKeys.begin()) = c.second;
            if (k1 == k2)
                sensiKeyHasNonZeroVariance[k1 - sensiKeys.begin()] = true;
        } else {
            ++unusedCovariance;
        }
    }
    LOG("Found " << covariance_.size() << " covariance matrix entries, " << unusedCovariance
                 << " do not match a portfolio sensitivity and will not be used.");
    for (Size i = 0; i < sensiKeyHasNonZeroVariance.size(); ++i) {
        if (!sensiKeyHasNonZeroVariance[i]) {
            WLOG("Zero variance assigned to sensitivity key " << sensiKeys[i]);
        }
    }

    // make covariance matrix positive semi-definite
    boost::shared_ptr<QuantExt::CovarianceSalvage> covarianceSalvage;
    LOG("Covariance matrix has dimension " << sensiKeys.size() << " x " << sensiKeys.size());
    if (salvageCovarianceMatrix_) {
        LOG("Make covariance matrix positive semi-definite using spectral method");
        covarianceSalvage = boost::make_shared<QuantExt::SpectralCovarianceSalvage>();
    } else {
        LOG("Covariance matrix is no salvaged, check for positive semi-definiteness");
        SymmetricSchurDecomposition ssd(omega);
        Real evMin = ssd.eigenvalues().back();
        QL_REQUIRE(evMin > 0.0 || close_enough(evMin, 0.0),
                   "ParametricVar: input covariance matrix is not positive semi-definite, smallest eigenvalue is "
                       << evMin);
        LOG("Smallest eigenvalue is " << evMin);
        covarianceSalvage = boost::make_shared<QuantExt::NoCovarianceSalvage>();
    }
    LOG("Done.");

    // loop over portfolios (index 0 = all portfolios)
    for (Size i = 0; i <= (!breakdown_ || portfolios.size() == 1 ? 0 : portfolios.size()); ++i) {
        std::string portfolioName = i == 0 ? (portfolios.size() > 1 ? "(all)" : portfolios.front()) : portfolios[i - 1];
        // build delta and gamma for given portfolio
        const auto& val1 = (i == 0 ? value1All : value1[portfolios[i - 1]]);
        const auto& val2 = (i == 0 ? value2All : value2[portfolios[i - 1]]);
        Array delta(sensiKeys.size(), 0.0);
        Matrix gamma(sensiKeys.size(), sensiKeys.size(), 0.0);
        for (auto const& p : val1) {
            auto k1 = p.first.first;
            auto k2 = p.first.second;
            Size idx1 = std::find(sensiKeys.begin(), sensiKeys.end(), k1) - sensiKeys.begin();
            QL_REQUIRE(idx1 < sensiKeys.size(), "ParametricVarCalculator::computeVar: key1 \""
                                                    << k1 << "\" in value1 not found, this is unexpected.");
            if (k2 == RiskFactorKey()) {
                // delta
                delta[idx1] += p.second;
            } else {
                // cross gamma
                Size idx2 = std::find(sensiKeys.begin(), sensiKeys.end(), k2) - sensiKeys.begin();
                QL_REQUIRE(idx2 < sensiKeys.size(), "ParametricVarCalculator::computeVar: key2 \""
                                                        << k2 << "\" in value1 not found, this is unexpected.");
                gamma[idx1][idx2] = gamma[idx2][idx1] = p.second;
            }
        }
        for (auto const& p : val2) {
            // diagonal gamma
            auto k1 = p.first.first;
            Size idx1 = std::find(sensiKeys.begin(), sensiKeys.end(), k1) - sensiKeys.begin();
            QL_REQUIRE(idx1 < sensiKeys.size(), "ParametricVarCalculator::computeVar: key1 \""
                                                    << k1 << "\" in value2 not found, this is unexpected.");
            gamma[idx1][idx1] = p.second;
        }
        // loop over risk class and type filters (index 0 == all risk types)
        for (Size j = 0; j < (breakdown_ ? RiskFilter::numberOfRiskClasses() : 1); ++j) {
            for (Size k = 0; k < (breakdown_ ? RiskFilter::numberOfRiskTypes() : 1); ++k) {
                // TODO should we rather project on the set of indices with non-zero sensis
                // instead of copying the initial sensis and setting the non-relevant entries
                // to zero?
                Array deltaFiltered(delta);
                Matrix gammaFiltered(gamma);
                RiskFilter rf(j, k);
                LOG("Compute parametric var for portfolio \"" << portfolioName << "\""
                                                              << ", risk class " << rf.riskClassLabel()
                                                              << ", risk type " << rf.riskTypeLabel());
                // set sensis which do not belong to risk type filter to zero
                for (Size idx = 0; idx < sensiKeys.size(); ++idx) {
                    if (!rf.allowed(sensiKeys[idx].keytype)) {
                        deltaFiltered[idx] = 0.0;
                        for (Size ii = 0; ii < sensiKeys.size(); ++ii) {
                            gammaFiltered[idx][ii] = gammaFiltered[ii][idx] = 0.0;
                        }
                    }
                }
                // are all sensis zero, then skip the computation
                bool zeroSensis = close_enough(QuantExt::detail::absMax(deltaFiltered), 0.0) &&
                                  close_enough(QuantExt::detail::absMax(gammaFiltered), 0.0);
                // compute var and write to report
                std::vector<Real> var = zeroSensis
                                            ? std::vector<Real>(p_.size(), 0.0)
                                            : computeVar(omega, deltaFiltered, gammaFiltered, p_, *covarianceSalvage);
                if (!close_enough(QuantExt::detail::absMax(var), 0.0)) {
                    report.next();
                    report.add(portfolioName);
                    report.add(rf.riskClassLabel());
                    report.add(rf.riskTypeLabel());
                    for (auto const& v : var)
                        report.add(v);
                }
            } // for k (risk types)
        }     // for j (risk classes)
    }         // for i (portfolios)
    LOG("parametric var computation done.");
    report.end();

} // calculate

std::vector<Real> ParametricVarCalculator::computeVar(const Matrix& omega, const Array& delta, const Matrix& gamma,
                                                      const std::vector<Real>& p,
                                                      const QuantExt::CovarianceSalvage& covarianceSalvage) {
    if (method_ == "Delta") {
        std::vector<Real> res(p.size());
        for (Size i = 0; i < p.size(); ++i) {
            res[i] = QuantExt::deltaVar(omega, delta, p[i], covarianceSalvage);
        }
        return res;
    } else if (method_ == "DeltaGammaNormal") {
        std::vector<Real> res(p.size());
        for (Size i = 0; i < p.size(); ++i) {
            res[i] = QuantExt::deltaGammaVarNormal(omega, delta, gamma, p[i], covarianceSalvage);
        }
        return res;
    } else if (method_ == "MonteCarlo") {
        QL_REQUIRE(mcSamples_ != Null<Size>(),
                   "ParametricVarCalculator::computeVar(): method MonteCarlo requires mcSamples");
        QL_REQUIRE(mcSeed_ != Null<Size>(),
                   "ParametricVarCalculator::computeVar(): method MonteCarlo requires mcSamples");
        return QuantExt::deltaGammaVarMc<PseudoRandom>(omega, delta, gamma, p, mcSamples_, mcSeed_, covarianceSalvage);
    } else if (method_ == "Cornish-Fisher") {
        std::vector<Real> res(p.size());
        for (Size i = 0; i < p.size(); ++i) {
            res[i] = QuantExt::deltaGammaVarCornishFisher(omega, delta, gamma, p[i], covarianceSalvage);
        }
        return res;
    } else if (method_ == "Saddlepoint") {
        std::vector<Real> res(p.size());
        for (Size i = 0; i < p.size(); ++i) {
            res[i] = QuantExt::deltaGammaVarSaddlepoint(omega, delta, gamma, p[i], covarianceSalvage);
        }
        return res;
    } else {
        QL_FAIL("ParametricVarCalculator::computeVar(): method " << method_ << " not known.");
    }
}

void loadCovarianceDataFromCsv(std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real>& data,
                               const std::string& fileName, const char delim) {
    LOG("Load Covariance Data from file " << fileName);
    ore::data::CSVFileReader reader(fileName, false);
    std::vector<std::string> dummy;
    while (reader.next()) {
        data[std::make_pair(*parseRiskFactorKey(reader.get(0), dummy), *parseRiskFactorKey(reader.get(1), dummy))] =
            ore::data::parseReal(reader.get(2));
    }
    LOG("Read " << data.size() << " valid data lines from file " << fileName);
}

} // namespace analytics
} // namespace ore
