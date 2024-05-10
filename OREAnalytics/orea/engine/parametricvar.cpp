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
#include <ored/utilities/to_string.hpp>

#include <qle/math/deltagammavar.hpp>

#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>

using namespace QuantLib;
using namespace ore::data;
using namespace std;

namespace ore {
namespace analytics {   

ParametricVarCalculator::ParametricVarParams::ParametricVarParams(const string& m, Size samp, Size sd)
    : method(parseParametricVarMethod(m)), samples(samp), seed(sd) {}

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

ostream& operator<<(ostream& out, const ParametricVarCalculator::ParametricVarParams::Method& method) {
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

Real ParametricVarCalculator::var(Real confidence, const bool isCall, 
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

ParametricVarReport::ParametricVarReport(const std::string& baseCurrency, const ext::shared_ptr<Portfolio>& portfolio,
                                         const string& portfolioFilter,
                                         const vector<Real>& p,
                                         const ParametricVarCalculator::ParametricVarParams& parametricVarParams,
                                         const bool salvageCovarianceMatrix,
                                         boost::optional<ore::data::TimePeriod> period,
                                         std::unique_ptr<SensiRunArgs> sensiArgs,
                                         const bool breakdown)
    : VarReport(baseCurrency, portfolio, portfolioFilter, p, period, nullptr, std::move(sensiArgs), nullptr, breakdown),
      parametricVarParams_(parametricVarParams), salvageCovarianceMatrix_(salvageCovarianceMatrix) {
    sensiBased_ = true;
}

ParametricVarReport::ParametricVarReport(
    const std::string& baseCurrency,
    const ext::shared_ptr<Portfolio>& portfolio,
    const string& portfolioFilter,
    const ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
    const vector<Real>& p,
    const ParametricVarCalculator::ParametricVarParams& parametricVarParams,
    const bool salvageCovarianceMatrix,
    boost::optional<ore::data::TimePeriod> period,
    std::unique_ptr<SensiRunArgs> sensiArgs,
    const bool breakdown)
    : VarReport(baseCurrency, portfolio, portfolioFilter, p, period, hisScenGen, std::move(sensiArgs), nullptr, breakdown),
      parametricVarParams_(parametricVarParams), salvageCovarianceMatrix_(salvageCovarianceMatrix) {
    sensiBased_ = true;
}

void ParametricVarReport::createVarCalculator() {
    varCalculator_ = QuantLib::ext::make_shared<ParametricVarCalculator>(
        parametricVarParams_, covarianceMatrix_, deltas_, gammas_, salvage_, includeGammaMargin_, includeDeltaMargin_);
}

} // namespace analytics
} // namespace ore
