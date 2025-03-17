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

/*! \file portfolio/builders/cdo.hpp
    \brief Mid point CDO engines cached by currency
    \ingroup portfolio
*/

#pragma once

#include <qle/pricingengines/midpointcdoengine.hpp>

// avoid compile error from homogeneouspooldef.hpp
#include <boost/algorithm/string.hpp>

#include <ored/portfolio/creditdefaultswapdata.hpp>
#include <ored/portfolio/structuredconfigurationwarning.hpp>
#include <ored/utilities/parsers.hpp>
#include <qle/models/homogeneouspooldef.hpp>
#include <qle/models/inhomogeneouspooldef.hpp>
#include <qle/models/mcdefaultlossmodel.hpp>
#include <qle/models/poollossmodel.hpp>
#include <qle/pricingengines/indexcdstrancheengine.hpp>
#include <qle/quotes/basecorrelationquote.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/quotes/simplequote.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {
using namespace std;

//! Engine Builder base class for CDOs
/*! Pricing engines are cached by currency
    \ingroup portfolio
*/

std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>> buildPerformanceOptimizedDefaultCurves(
    const std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>>& curves);

class CdoEngineBuilder : public CachingPricingEngineBuilder<vector<string>, const Currency&, bool, const std::string&,
                                                            const std::vector<std::string>&,
                                                            const std::vector<Handle<DefaultProbabilityTermStructure>>&,
                                                            const std::vector<double>&, const bool, const Real> {
public:
    CdoEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"SyntheticCDO"}) {}

    virtual QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel>
    lossModel(const string& qualifier, const vector<Real>& recoveryRates, const Real& detachmentPoint,
              const QuantLib::Date& trancheMaturity, bool homogeneous, const std::vector<std::string>& names,
              const std::string& indexFamily, bool checkExpectedRecovery) = 0;

    CreditPortfolioSensitivityDecomposition sensitivityDecomposition() const {
        return parseCreditPortfolioSensitivityDecomposition(
            engineParameter("SensitivityDecomposition", {}, false, "Underlying"));
    }

    bool calibrateConstituentCurve() const {
        return parseBool(engineParameter("calibrateConstituentCurves", {}, false, "false"));
    }

    std::vector<QuantLib::Period> calibrationIndexTerms() const {
        return parseListOfValues<QuantLib::Period>(engineParameter("calibrationIndexTerms", {}, false, ""),
                                                   parsePeriod);
    }

    bool optimizedSensitivityCalculation() const {
        auto rt = globalParameters_.find("RunType");
        return sensitivityDecomposition() != CreditPortfolioSensitivityDecomposition::Underlying &&
               (rt != globalParameters_.end() &&
                (rt->second == "SensitivityDelta" || rt->second == "SensitivityDeltaGamma"));
    }

    bool useAssumedRecoveries() const { return parseBool(modelParameter("useAssumedRecovery", {}, false, "false")); }

    vector<double> recoveryGrid(const std::string& indexSubFamily, const std::string& tier) const {
        const vector<string> qualifiers = {indexSubFamily + "_" + tier, tier};
        string rrGridStr = modelParameter("recoveryRateGrid", qualifiers);
        vector<string> rrGridToken;
        boost::split(rrGridToken, rrGridStr, boost::is_any_of(","));
        return parseVectorOfValues<Real>(rrGridToken, &parseReal);
    }

    vector<double> recoveryProbabilities(const std::string& indexSubFamily, const std::string& tier) const {
        const vector<string> qualifiers = {indexSubFamily + "_" + tier, tier};
        string rrProbString = modelParameter("recoveryRateProbabilities", qualifiers);
        vector<string> rrProbStringTokens;
        boost::split(rrProbStringTokens, rrProbString, boost::is_any_of(","));
        return parseVectorOfValues<Real>(rrProbStringTokens, &parseReal);
    }

    double assumedRecovery(const std::string& indexSubFamily, const std::string& tier) const {
        auto rrGrid = recoveryGrid(indexSubFamily, tier);
        auto rrProbs = recoveryProbabilities(indexSubFamily, tier);
        return std::inner_product(rrGrid.begin(), rrGrid.end(), rrProbs.begin(), 0.0);
    }

    void updateRecoveryGrids(vector<vector<double>>& recoveryGrids, vector<vector<double>>& recoveryProbabilities,
                             const vector<string>& names, const vector<double>& recoveryRates,
                             const string& indexFamilyName, const bool useStochasticRecovery) {
        if (useStochasticRecovery) {
            for (size_t i = 0; i < names.size(); ++i) {
                CdsReferenceInformation cdsInfo;
                std::string tierString;
                if (tryParseCdsInformation(names[i], cdsInfo)) {
                    tierString = to_string(cdsInfo.tier());
                }
                const auto recoveryRate = recoveryRates[i];
                std::string key = indexFamilyName + tierString;
                std::vector<std::string> qualifiers;
                if (!indexFamilyName.empty() && !tierString.empty()) {
                    qualifiers.push_back(indexFamilyName + "_" + tierString);
                }
                if (!indexFamilyName.empty()) {
                    qualifiers.push_back(indexFamilyName);
                }
                if (!tierString.empty()) {
                    qualifiers.push_back(tierString);
                }
                string rrProbString = modelParameter("recoveryRateProbabilities", qualifiers);
                vector<string> rrProbStringTokens;
                boost::split(rrProbStringTokens, rrProbString, boost::is_any_of(","));
                auto rrProb = parseVectorOfValues<Real>(rrProbStringTokens, &parseReal);
                recoveryProbabilities.push_back(rrProb);

                string rrGridString = modelParameter("recoveryRateGrid", qualifiers);
                if (rrGridString == "Markit2020") {
                    WLOG("The recovery rate grid 'Markit2020' is deprecated. Please use the 'recoveryRateGrid' "
                         "parameter to specify the recovery rate grid explicitily (e.g. 0.7,0.4,0.1).");
                    if (recoveryRate >= 0.1 && recoveryRate <= 0.55) {
                        recoveryGrids.push_back({2 * recoveryRate - 0.1, recoveryRate, 0.1});

                    } else {
                        ALOG("Market recovery rate " << recoveryRates[i] << " for entity " << i
                                                     << " out of range [0.1, 0.55], using constant recovery");
                        recoveryGrids.push_back(std::vector<double>(rrProb.size(), recoveryRate));
                    }
                } else if (rrGridString == "Constant") {
                    WLOG("The recovery rate grid 'Constant' is deprecated. Please use the 'recoveryRateGrid' "
                         "parameter to specify the recovery rate grid explicitily (e.g. 0.7,0.4,0.1).");
                    recoveryGrids.push_back(std::vector<double>(rrProb.size(), recoveryRate));
                } else {
                    vector<string> rrGridTokens;
                    boost::split(rrGridTokens, rrGridString, boost::is_any_of(","));
                    auto rrGrid = parseVectorOfValues<Real>(rrGridTokens, &parseReal);
                    recoveryGrids.push_back(rrGrid);
                }
            }
        } else {
            for (Size i = 0; i < recoveryRates.size(); ++i) {
                // Use the same recovery rate probabilities across all entites
                recoveryProbabilities.push_back({1.0});
                recoveryGrids.push_back({recoveryRates[i]});
            }
        }
    }

protected:
    vector<string> keyImpl(const Currency& ccy, bool isIndex, const std::string& qualifier,
                           const std::vector<std::string>& creditCurves,
                           const std::vector<Handle<DefaultProbabilityTermStructure>>&, const std::vector<double>&,
                           const bool calibrated = false, const Real fixedRecovery = Null<Real>()) override {
        vector<string> res;
        res.reserve(creditCurves.size() + 5);
        res.emplace_back(ccy.code());
        res.emplace_back(qualifier);
        if (isIndex) {
            res.emplace_back("_indexCDS");
        }
        res.insert(res.end(), creditCurves.begin(), creditCurves.end());
        res.emplace_back(to_string(calibrated));
        if (fixedRecovery != Null<Real>())
            res.emplace_back(to_string(fixedRecovery));
        return res;
    }

    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const Currency&, bool isIndex, const std::string& qualifier,
                                                        const std::vector<std::string>& creditCurves,
                                                        const std::vector<Handle<DefaultProbabilityTermStructure>>&,
                                                        const std::vector<double>&, const bool calibrated = false,
                                                        const Real fixedRecovery = Null<Real>()) override;
};

class LossModelBuilder {
public:
    virtual ~LossModelBuilder() {}
    virtual QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel>
    lossModel(const vector<Real>& recoveryRates, const QuantLib::RelinkableHandle<Quote>& baseCorrelation,
              bool poolIsHomogenous, const vector<vector<double>>& rrGrids, const vector<vector<double>>& rrProbs,
              bool checkExpectedRecovery) const = 0;
};

class GaussCopulaBucketingLossModelBuilder : public LossModelBuilder {
public:
    GaussCopulaBucketingLossModelBuilder(double gaussCopulaMin, double gaussCopulaMax, Size gaussCopulaSteps,
                                         bool useQuadrature, Size nBuckets, bool homogeneousPoolWhenJustified,
                                         bool useStochasticRecovery)
        : gaussCopulaMin_(gaussCopulaMin), gaussCopulaMax_(gaussCopulaMax), gaussCopulaSteps_(gaussCopulaSteps),
          useQuadrature_(useQuadrature), nBuckets_(nBuckets),
          homogeneousPoolWhenJustified_(homogeneousPoolWhenJustified), useStochasticRecovery_(useStochasticRecovery) {}

    QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel>
    lossModel(const vector<Real>& recoveryRates, const QuantLib::RelinkableHandle<Quote>& baseCorrelation,
              bool poolIsHomogenous, const vector<vector<double>>& rrGrids, const vector<vector<double>>& rrProbs,
              bool checkExpectedRecovery) const override {
        Size poolSize = recoveryRates.size();

        QL_REQUIRE(recoveryRates.size() == rrGrids.size(), "Recovery rates, recovery rate grids ("
                                                               << rrGrids.size() << ") and recovery rates ("
                                                               << recoveryRates.size() << ") must have the same size.");
        QL_REQUIRE(recoveryRates.size() == rrProbs.size(), "Recovery rates, recovery rate grids "
                                                               << rrGrids.size() << " and recovery rate probabilities "
                                                               << rrProbs.size() << " must have the same size.");
        for (Size i = 0; i < recoveryRates.size(); ++i) {
            QL_REQUIRE(rrGrids[i].size() == rrProbs[i].size(),
                       "Recovery rate grids " << rrGrids[i].size() << " and recovery rate probabilities "
                                              << rrProbs[i].size() << " for constituent " << i
                                              << " must have the same size.");
            TLOG("Recovery rate grid for constituent " << i << " is " << recoveryRates[i])
            TLOG("RRGrid " << to_string(rrGrids[i]));
            TLOG("RRProbs " << to_string(rrProbs[i]));
        }

        DLOG("Build ExtendedGaussianConstantLossLM");

        QuantLib::ext::shared_ptr<QuantExt::ExtendedGaussianConstantLossLM> gaussLM =
            QuantLib::ext::make_shared<QuantExt::ExtendedGaussianConstantLossLM>(
                baseCorrelation, recoveryRates, rrProbs, rrGrids, LatentModelIntegrationType::GaussianQuadrature,
                poolSize, GaussianCopulaPolicy::initTraits(), checkExpectedRecovery);

        bool homogeneous = poolIsHomogenous && homogeneousPoolWhenJustified_;

        return QuantLib::ext::make_shared<QuantExt::GaussPoolLossModel>(
            homogeneous, gaussLM, nBuckets_, gaussCopulaMax_, gaussCopulaMin_, gaussCopulaSteps_, useQuadrature_,
            useStochasticRecovery_);
    }

private:
    double gaussCopulaMin_;
    double gaussCopulaMax_;
    Size gaussCopulaSteps_;
    bool useQuadrature_;
    Size nBuckets_;
    bool homogeneousPoolWhenJustified_;
    bool useStochasticRecovery_;
};

class GaussCopulaBucketingCdoEngineBuilder : public CdoEngineBuilder {
public:
    GaussCopulaBucketingCdoEngineBuilder() : CdoEngineBuilder("GaussCopula", "Bucketing") {}

    QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel>
    lossModel(const string& qualifier, const vector<Real>& recoveryRates, const Real& detachmentPoint,
              const QuantLib::Date& trancheMaturity, bool homogeneous, const std::vector<std::string>& names,
              const std::string& indexFamilyName, bool checkExpectedRecovery) override {

        Real gaussCopulaMin = parseReal(modelParameter("min"));
        Real gaussCopulaMax = parseReal(modelParameter("max"));
        Size gaussCopulaSteps = parseInteger(modelParameter("steps"));
        bool useQuadrature = parseBool(modelParameter("useQuadrature", {}, false, "false"));
        bool homogeneousPoolWhenJustified =
            parseBool(engineParameter("homogeneousPoolWhenJustified", {}, false, "false"));
        Size nBuckets = parseInteger(engineParameter("buckets"));
        // Create the base correlation quote.
        Handle<QuantExt::BaseCorrelationTermStructure> bcts =
            market_->baseCorrelation(qualifier, configuration(MarketContext::pricing));
        RelinkableHandle<Quote> correlation;
        if (detachmentPoint < 1.0) {
            const Date& bctsRd = bcts->referenceDate();
            QL_REQUIRE(trancheMaturity >= bctsRd, "Tranche maturity ("
                                                      << io::iso_date(trancheMaturity) << ") must be "
                                                      << " on or after base correlation structure's reference date ("
                                                      << io::iso_date(bctsRd) << ").");
            Period ttm = (trancheMaturity - bctsRd) * Days;
            correlation.linkTo(
                QuantLib::ext::make_shared<QuantExt::BaseCorrelationQuote>(bcts, ttm, detachmentPoint, true));
        } else {
            correlation.linkTo(QuantLib::ext::make_shared<SimpleQuote>(0.0));
        }
        DLOG("Base correlation quote value is " << correlation->value() << " at detachment point " << detachmentPoint);

        // Optional flag, set to false if omitted, i.e. we use determinsitic recovery by default
        bool useStochasticRecovery = parseBool(modelParameter("useStochasticRecovery", {}, false, "false"));

        std::vector<std::vector<double>> recoveryGrids;
        std::vector<std::vector<double>> recoveryProbabilities;

        updateRecoveryGrids(recoveryGrids, recoveryProbabilities, names, recoveryRates, indexFamilyName,
                            useStochasticRecovery);
        GaussCopulaBucketingLossModelBuilder modelbuilder(gaussCopulaMin, gaussCopulaMax, gaussCopulaSteps,
                                                          useQuadrature, nBuckets, homogeneousPoolWhenJustified,
                                                          useStochasticRecovery);
        return modelbuilder.lossModel(recoveryRates, correlation, homogeneous, recoveryGrids, recoveryProbabilities,
                                      checkExpectedRecovery);
    }
};

class GaussCopulaMonteCarloCdoEngineBuilder : public CdoEngineBuilder {
public:
    GaussCopulaMonteCarloCdoEngineBuilder() : CdoEngineBuilder("GaussCopula", "MonteCarlo") {}

    QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel>
    lossModel(const string& qualifier, const vector<Real>& recoveryRates, const Real& detachmentPoint,
              const QuantLib::Date& trancheMaturity, bool homogeneous, const std::vector<std::string>& names,
              const std::string& subIndexFamily, bool checkExpectedRecovery) override {

        Handle<QuantExt::BaseCorrelationTermStructure> bcts =
            market_->baseCorrelation(qualifier, configuration(MarketContext::pricing));

        // Create the base correlation quote.
        RelinkableHandle<Quote> correlation;
        if (detachmentPoint < 1.0) {
            const Date& bctsRd = bcts->referenceDate();
            QL_REQUIRE(trancheMaturity >= bctsRd, "Tranche maturity ("
                                                      << io::iso_date(trancheMaturity) << ") must be "
                                                      << " on or after base correlation structure's reference date ("
                                                      << io::iso_date(bctsRd) << ").");
            Period ttm = (trancheMaturity - bctsRd) * Days;
            correlation.linkTo(
                QuantLib::ext::make_shared<QuantExt::BaseCorrelationQuote>(bcts, ttm, detachmentPoint, true));
        } else {
            correlation.linkTo(QuantLib::ext::make_shared<SimpleQuote>(0.0));
        }
        DLOG("Base correlation quote value is " << correlation->value() << " at detachment point " << detachmentPoint);

        // Optional flag, set to false if omitted, i.e. we use determinsitic recovery by default
        // bool useStochasticRecovery = parseBool(modelParameter("useStochasticRecovery", {}, false, "false"));

        // Compile default recovery rate grids and probabilities for each name:
        // Recovery rate grids have three pillars, centered around market recovery, DECREASING order : [ 2*RR - 0.1, RR,
        // 0.1 ] Probabilities for the three pillars are symmetric around the center of the distribution and independent
        // of the concrete rate grid
        bool useStochasticRecovery = parseBool(modelParameter("useStochasticRecovery", {}, false, "false"));

        std::vector<std::vector<double>> recoveryProbabilities, recoveryGrids;

        updateRecoveryGrids(recoveryGrids, recoveryProbabilities, names, recoveryRates, subIndexFamily,
                            useStochasticRecovery);
        DLOG("Build MonteCarloModel");
        Size nSimulations = parseInteger(engineParameter("nSimulations"));
        auto model = ext::make_shared<QuantExt::GaussianOneFactorMonteCarloLossModel>(
            correlation, recoveryGrids, recoveryProbabilities, nSimulations);
        return model;
    }
};

} // namespace data
} // namespace ore
