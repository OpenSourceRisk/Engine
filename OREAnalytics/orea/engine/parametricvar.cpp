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

#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/math/deltagammavar.hpp>

#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

ParametricVarCalculator::ParametricVarCalculator(
    const std::map<std::string, std::string>& tradePortfolio, const boost::shared_ptr<SensitivityData>& sensitivities,
    const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance, const std::vector<Real>& p,
    const std::string& method, const Size mcSamples, const Size mcSeed, const bool breakdown,
    const bool salvageCovarianceMatrix)
    : tradePortfolio_(tradePortfolio), sensitivities_(sensitivities), covariance_(covariance), p_(p), method_(method),
      mcSamples_(mcSamples), mcSeed_(mcSeed), breakdown_(breakdown), salvageCovarianceMatrix_(salvageCovarianceMatrix) {
}

void ParametricVarCalculator::calculate(ore::data::Report& report) {
    LOG("Parametric VaR calculation started...");

    // prepare report
    report.addColumn("Portfolio", string()).addColumn("RiskClass", string()).addColumn("RiskType", string());
    for (Size i = 0; i < p_.size(); ++i)
        report.addColumn("Quantile_" + std::to_string(p_[i]), double(), 6);

    // read sensitivities and preaggregate them per prtfolio
    LOG("Preaggregate sensitivities per portfolio");
    std::set<RiskFactorKey> sensiKeysTmp;
    std::set<std::string> portfoliosTmp;
    std::map<std::string, std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real>> value1, value2;
    while (sensitivities_->next()) {
        std::string portfolio = "";
        auto pn = tradePortfolio_.find(sensitivities_->tradeId());
        if (pn != tradePortfolio_.end())
            portfolio = pn->second;
        portfoliosTmp.insert(portfolio);
        auto f1 = sensitivities_->factor1();
        auto f2 = sensitivities_->factor2();
        RiskFactorKey k1 = f1 == nullptr ? RiskFactorKey() : *f1;
        RiskFactorKey k2 = f2 == nullptr ? RiskFactorKey() : *f2;
        auto key = std::make_pair(k1, k2);
        if (f1 != nullptr)
            sensiKeysTmp.insert(*f1);
        if (f2 != nullptr)
            sensiKeysTmp.insert(*f2);
        if (sensitivities_->value() != Null<Real>())
            value1[portfolio][key] += sensitivities_->value();
        if (sensitivities_->value2() != Null<Real>())
            value2[portfolio][key] += sensitivities_->value2();
    }
    std::vector<RiskFactorKey> sensiKeys(sensiKeysTmp.begin(), sensiKeysTmp.end());
    std::vector<bool> sensiKeyHasNonZeroVariance(sensiKeys.size(), false);
    std::vector<std::string> portfolios(portfoliosTmp.begin(), portfoliosTmp.end());
    LOG("Have " << sensiKeys.size() << " sensitivity keys in " << value1.size() << " portfolios");

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
    Matrix omegaFinal;
    LOG("Covariance matrix has dimension " << sensiKeys.size() << " x " << sensiKeys.size());
    if (salvageCovarianceMatrix_) {
        LOG("Make covariance matrix positive semi-definite using spectral method");
        QuantLib::Matrix covSqrt = pseudoSqrt(omega, SalvagingAlgorithm::Spectral);
        omegaFinal = covSqrt * transpose(covSqrt);
    } else {
        LOG("Covariance matrix is no salvaged, check for positive semi-definiteness");
        SymmetricSchurDecomposition ssd(omega);
        Real evMin = ssd.eigenvalues().back();
        QL_REQUIRE(evMin > 0.0 || close_enough(evMin, 0.0),
                   "ParametricVar: input covariance matrix is not positive semi-definite, smallest eigenvalue is "
                       << evMin);
        LOG("Smallest eigenvalue is " << evMin);
        omegaFinal = omega;
    }
    LOG("Done.");

    // loop over portfolios (index 0 = all portfolios)
    Size loopLimitPf = breakdown_ && portfolios.size() > 1 ? portfolios.size() : 0;
    for (Size i = 0; i <= loopLimitPf; ++i) {
        std::string portfolio = i == 0 ? (portfolios.size() == 1 ? portfolios.front() : "(all)") : portfolios[i - 1];
        if (portfolio == "")
            portfolio = "(empty)";
        // build delta and gamma for given portfolio (or all portfolios)
        Array delta(sensiKeys.size(), 0.0);
        Matrix gamma(sensiKeys.size(), sensiKeys.size(), 0.0);
        Size jStart, jEnd;
        if (i == 0) {
            jStart = 0;
            jEnd = portfolios.size();
        } else {
            jStart = i - 1;
            jEnd = i;
        }
        for (Size j = jStart; j < jEnd; ++j) {
            for (auto const& p : value1[portfolios[j]]) {
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
            for (auto const& p : value2[portfolios[j]]) {
                // diagonal gamma
                auto k1 = p.first.first;
                Size idx1 = std::find(sensiKeys.begin(), sensiKeys.end(), k1) - sensiKeys.begin();
                QL_REQUIRE(idx1 < sensiKeys.size(), "ParametricVarCalculator::computeVar: key1 \""
                                                        << k1 << "\" in value2 not found, this is unexpected.");
                gamma[idx1][idx1] = p.second;
            }
        }
        // loop over risk class and type filters (index 0 == all risk types)
        for (Size j = 0; j < (breakdown_ ? RiskFilter::numberOfRiskClasses() : 1); ++j) {
            for (Size k = 0; k < (breakdown_ ? RiskFilter::numberOfRiskTypes() : 1); ++k) {
                // TODO should we rather project on the set of indices with non-zero sensis
                // instead of copying the initial sensis and setting the non-releveant entries
                // to zero?
                Array deltaFiltered(delta);
                Matrix gammaFiltered(gamma);
                RiskFilter rf(j, k);
                LOG("Compute parametric var for portfolio \"" << portfolio << "\""
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
                std::vector<Real> var = zeroSensis ? std::vector<Real>(p_.size(), 0.0)
                                                   : computeVar(omegaFinal, deltaFiltered, gammaFiltered, p_);
                if (!close_enough(QuantExt::detail::absMax(var), 0.0)) {
                    report.next();
                    report.add(portfolio);
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
                                                      const std::vector<Real>& p) {
    if (method_ == "Delta") {
        std::vector<Real> res(p.size());
        for (Size i = 0; i < p.size(); ++i) {
            res[i] = QuantExt::deltaVar(omega, delta, p[i]);
        }
        return res;
    } else if (method_ == "DeltaGammaNormal") {
        std::vector<Real> res(p.size());
        for (Size i = 0; i < p.size(); ++i) {
            res[i] = QuantExt::deltaGammaVarNormal(omega, delta, gamma, p[i]);
        }
        return res;
    } else if (method_ == "MonteCarlo") {
        return QuantExt::deltaGammaVarMc<PseudoRandom>(omega, delta, gamma, p, mcSamples_, mcSeed_);
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
