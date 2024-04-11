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

#include <qle/models/homogeneouspooldef.hpp>
#include <qle/models/inhomogeneouspooldef.hpp>
#include <qle/models/poollossmodel.hpp>
#include <qle/pricingengines/indexcdstrancheengine.hpp>

#include <ored/utilities/parsers.hpp>
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

std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>>
buildPerformanceOptimizedDefaultCurves(const std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>>& curves);

class CdoEngineBuilder
    : public CachingPricingEngineBuilder<vector<string>, const Currency&, bool, const vector<string>&,
                                         const QuantLib::ext::shared_ptr<QuantLib::SimpleQuote>&,
                                         const QuantLib::Real> {
public:
    CdoEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"SyntheticCDO"}) {}

    virtual QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel>
    lossModel(const string& qualifier, const vector<Real>& recoveryRates, const Real& detachmentPoint,
              const QuantLib::Date& trancheMaturity, bool homogeneous) = 0;

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

protected:
    virtual vector<string> keyImpl(const Currency& ccy, bool isIndexCDS, const vector<string>& creditCurves,
                                   const QuantLib::ext::shared_ptr<QuantLib::SimpleQuote>& calibrationFactor,
                                   const QuantLib::Real fixedRecovery) override {
        vector<string> res;
        res.reserve(creditCurves.size() + 4);
        res.emplace_back(ccy.code());
        if (isIndexCDS)
            res.emplace_back("_indexCDS");
        res.insert(res.end(), creditCurves.begin(), creditCurves.end());
        if (!close_enough(calibrationFactor->value(), 1.0) && calibrationFactor->value() != QuantLib::Null<Real>()) {
            res.emplace_back(to_string(calibrationFactor->value()));
        }            
        if (fixedRecovery != QuantLib::Null<QuantLib::Real>()) {
            res.emplace_back(to_string(fixedRecovery));
        }
        return res;
    }
};

class GaussCopulaBucketingCdoEngineBuilder : public CdoEngineBuilder {
public:
    GaussCopulaBucketingCdoEngineBuilder() : CdoEngineBuilder("GaussCopula", "Bucketing") {}

    QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel> lossModel(const string& qualifier, const vector<Real>& recoveryRates,
                                                            const Real& detachmentPoint,
                                                            const QuantLib::Date& trancheMaturity,
                                                            bool homogeneous) override {

        Size poolSize = recoveryRates.size();

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
        bool useStochasticRecovery = parseBool(modelParameter("useStochasticRecovery", {}, false, "false"));

        // Compile default recovery rate grids and probabilities for each name:
        // Recovery rate grids have three pillars, centered around market recovery, DECREASING order : [ 2*RR - 0.1, RR, 0.1 ]
        // Probabilities for the three pillars are symmetric around the center of the distribution and independent of the concrete rate grid
        std::vector<std::vector<Real>> recoveryProbabilities, recoveryGrids;
        if (useStochasticRecovery) {
            string rrProbString = modelParameter("recoveryRateProbabilities");
            string rrGridString = modelParameter("recoveryRateGrid");            
            vector<string> rrProbStringTokens;
            boost::split(rrProbStringTokens, rrProbString, boost::is_any_of(","));
            vector<Real> rrProb = parseVectorOfValues<Real>(rrProbStringTokens, &parseReal);
            for (Size i = 0; i < recoveryRates.size(); ++i) {
                // Use the same recovery rate probabilities across all entites
                recoveryProbabilities.push_back(rrProb);                 
                // recovery rate grid dependent on market recovery rate
                std::vector<Real> rrGrid(3, recoveryRates[i]); // constant recovery by default
                QL_REQUIRE(rrProb.size() == rrGrid.size(), "recovery grid size mismatch");
                if (rrGridString == "Markit2020") {
                    if (recoveryRates[i] >= 0.1 && recoveryRates[i] <= 0.55) {
                        rrGrid[0] = 2.0 * recoveryRates[i] - 0.1;
                        rrGrid[1] = recoveryRates[i];
                        rrGrid[2] = 0.1;
                        LOG("Using recovery rate grid for entity " << i << ": " << rrGrid[0] << " " << rrGrid[1] << " " << rrGrid[2]);
                    }
                    else
                        ALOG("Market recovery rate " << recoveryRates[i] << " for entity " << i << " out of range [0.1, 0.55], using constant recovery");
                    recoveryGrids.push_back(rrGrid);
                }
                else if (rrGridString != "Constant") {
                    QL_FAIL("recovery rate model code " << rrGridString << " not recognized");
                }
            }
        }
        
        DLOG("Build ExtendedGaussianConstantLossLM");
        QuantLib::ext::shared_ptr<QuantExt::ExtendedGaussianConstantLossLM> gaussLM(new QuantExt::ExtendedGaussianConstantLossLM(
            correlation, recoveryRates, recoveryProbabilities, recoveryGrids, LatentModelIntegrationType::GaussianQuadrature, poolSize,
            GaussianCopulaPolicy::initTraits()));
        Real gaussCopulaMin = parseReal(modelParameter("min"));
        Real gaussCopulaMax = parseReal(modelParameter("max"));
        Size gaussCopulaSteps = parseInteger(modelParameter("steps"));
        bool useQuadrature = parseBool(modelParameter("useQuadrature", {}, false, "false"));
        Size nBuckets = parseInteger(engineParameter("buckets"));
        bool homogeneousPoolWhenJustified = parseBool(engineParameter("homogeneousPoolWhenJustified"));

        homogeneous = homogeneous && homogeneousPoolWhenJustified;
        LOG("Use " << (homogeneous ? "" : "in") << "homogeneous pool loss model for qualifier " << qualifier);
        DLOG("useQuadrature is set to " << std::boolalpha << useQuadrature);
        return QuantLib::ext::make_shared<QuantExt::GaussPoolLossModel>(homogeneous, gaussLM, nBuckets, gaussCopulaMax,
                                                                gaussCopulaMin, gaussCopulaSteps, useQuadrature,
                                                                useStochasticRecovery);
    }

protected:
    virtual QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const Currency& ccy, bool isIndexCDS, const vector<string>& creditCurves,
               const QuantLib::ext::shared_ptr<QuantLib::SimpleQuote>& calibrationFactor,
               const QuantLib::Real fixedRecovery = QuantLib::Null<QuantLib::Real>()) override;
};

} // namespace data
} // namespace ore
