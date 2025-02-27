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
#include <qle/models/mcdefaultlossmodel.hpp>
#include <qle/pricingengines/indexcdstrancheengine.hpp>
#include <ored/portfolio/creditdefaultswapdata.hpp>
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
    : public CachingPricingEngineBuilder<vector<string>, const Currency&, bool, 
                                         const std::string&,
                                         const std::vector<std::string>&,
                                         const std::vector<Handle<DefaultProbabilityTermStructure>> &,
                                         const std::vector<double>&, const bool, const Real> {
public:
    CdoEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"SyntheticCDO"}) {}

    virtual QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel>
    lossModel(const string& qualifier, const vector<Real>& recoveryRates, const Real& detachmentPoint,
              const QuantLib::Date& trancheMaturity, bool homogeneous, const std::vector<CdsTier>& seniorities) = 0;

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
    virtual vector<string> keyImpl(const Currency& ccy, bool isIndex, 
                                   const std::string& qualifier,
                                   const std::vector<std::string>& creditCurves,
                                   const std::vector<Handle<DefaultProbabilityTermStructure>>&,
                                   const std::vector<double>&,
                                   const bool calibrated = false,
                                   const Real fixedRecovery = Null<Real>()) override {
        vector<string> res;
        res.reserve(creditCurves.size() + 5);
        res.emplace_back(ccy.code());
        res.emplace_back(qualifier);
        if (isIndex) {
            res.emplace_back("_indexCDS");
        }
        res.insert(res.end(), creditCurves.begin(), creditCurves.end());
        res.emplace_back(to_string(calibrated));
        if(fixedRecovery != Null<Real>())
            res.emplace_back(to_string(fixedRecovery));
        return res;
    }
    virtual QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const Currency&, bool isIndex, const std::string& qualifier,
               const std::vector<std::string>& creditCurves,
               const std::vector<Handle<DefaultProbabilityTermStructure>> &, const std::vector<double>&,
               const bool calibrated = false, const Real fixedRecovery = Null<Real>()) override;
};

class LossModelBuilder {
public:
    virtual ~LossModelBuilder() {}
    virtual QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel>
    lossModel(const vector<Real>& recoveryRates, const QuantLib::RelinkableHandle<Quote>& baseCorrelation,
              bool poolIsHomogenous, const std::vector<CdsTier>& seniorities) const = 0;
};

class GaussCopulaBucketingLossModelBuilder : public LossModelBuilder{
public:
    GaussCopulaBucketingLossModelBuilder(double gaussCopulaMin, double gaussCopulaMax, Size gaussCopulaSteps,
                                         bool useQuadrature, Size nBuckets, bool homogeneousPoolWhenJustified,
                                         bool useStochasticRecovery, const std::vector<double>& rrProbs,
                                         const std::string& recoveryRateGrid, bool isCDXHYNA)
        : gaussCopulaMin_(gaussCopulaMin), gaussCopulaMax_(gaussCopulaMax), gaussCopulaSteps_(gaussCopulaSteps),
          useQuadrature_(useQuadrature), nBuckets_(nBuckets),
          homogeneousPoolWhenJustified_(homogeneousPoolWhenJustified), useStochasticRecovery_(useStochasticRecovery),
          rrProbs_(rrProbs), recoveryRateGrid_(recoveryRateGrid), isCDXHYNA_(isCDXHYNA) {}

    QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel>
    lossModel(const vector<Real>& recoveryRates, const QuantLib::RelinkableHandle<Quote>& baseCorrelation,
              bool poolIsHomogenous, const std::vector<CdsTier>& seniorities) const override {
        Size poolSize = recoveryRates.size();
        std::vector<std::vector<Real>> recoveryProbabilities, recoveryGrids;
        if (useStochasticRecovery_) {
            for (Size i = 0; i < recoveryRates.size(); ++i) {
                // Use the same recovery rate probabilities across all entites
                recoveryProbabilities.push_back(rrProbs_);                 
                // recovery rate grid dependent on market recovery rate
                std::vector<Real> rrGrid(3, recoveryRates[i]); // constant recovery by default
                QL_REQUIRE(rrProbs_.size() == rrGrid.size(), "recovery grid size mismatch");
                if (recoveryRateGrid_ == "Markit2020") {
                    if (isCDXHYNA_ && (seniorities[i] == CdsTier::SNRFOR || seniorities[i] == CdsTier::SECDOM)) {
                        rrGrid[0] = 0.5;
                        rrGrid[1] = 0.3;
                        rrGrid[2] = 0.1;
                    } else if (seniorities[i] == CdsTier::SNRFOR) {
                        rrGrid[0] = 0.6;
                        rrGrid[1] = 0.4;
                        rrGrid[2] = 0.1;
                    }
                } else if (recoveryRateGrid_ != "Constant") {
                    QL_FAIL("recovery rate model code " << recoveryRateGrid_ << " not recognized");
                }
                recoveryGrids.push_back(rrGrid);
            }
        }
        
        DLOG("Build ExtendedGaussianConstantLossLM");
        /*
        TCopulaPolicy::initTraits initTraits;
        initTraits.tOrders = {3, 3};

        QuantLib::ext::shared_ptr<QuantExt::ExtendedTCopulaConstantLossLM>studentLM(new
        QuantExt::ExtendedTCopulaConstantLossLM( baseCorrelation, recoveryRates, recoveryProbabilities, recoveryGrids,
                LatentModelIntegrationType::GaussianQuadrature, poolSize, initTraits));

        bool homogeneous = poolIsHomogenous && homogeneousPoolWhenJustified_;

        return QuantLib::ext::make_shared<QuantExt::StudentPoolLossModel>(
            homogeneous, studentLM, nBuckets_, gaussCopulaMax_, gaussCopulaMin_, gaussCopulaSteps_, useQuadrature_,
            useStochasticRecovery_);
        */
        QuantLib::ext::shared_ptr<QuantExt::ExtendedGaussianConstantLossLM> gaussLM(
            new QuantExt::ExtendedGaussianConstantLossLM(baseCorrelation, recoveryRates, recoveryProbabilities,
                                                         recoveryGrids, LatentModelIntegrationType::GaussianQuadrature,
                                                         poolSize, GaussianCopulaPolicy::initTraits()));

        
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
    std::vector<double> rrProbs_;
    std::string recoveryRateGrid_;
    bool isCDXHYNA_;
    
};

class GaussCopulaBucketingCdoEngineBuilder : public CdoEngineBuilder {
public:
    GaussCopulaBucketingCdoEngineBuilder() : CdoEngineBuilder("GaussCopula", "Bucketing") {}

    QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel>
    lossModel(const string& qualifier, const vector<Real>& recoveryRates, const Real& detachmentPoint,
              const QuantLib::Date& trancheMaturity, bool homogeneous,
              const std::vector<CdsTier>& seniorities) override {

        Real gaussCopulaMin = parseReal(modelParameter("min"));
        Real gaussCopulaMax = parseReal(modelParameter("max"));
        Size gaussCopulaSteps = parseInteger(modelParameter("steps"));
        bool useQuadrature = parseBool(modelParameter("useQuadrature", {}, false, "false"));
        bool homogeneousPoolWhenJustified = parseBool(engineParameter("homogeneousPoolWhenJustified", {}, false, "false"));
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
        vector<Real> rrProb;
        string rrGridString;
        if (useStochasticRecovery) {
            string rrProbString = modelParameter("recoveryRateProbabilities");
            rrGridString= modelParameter("recoveryRateGrid");
            vector<string> rrProbStringTokens;
            boost::split(rrProbStringTokens, rrProbString, boost::is_any_of(","));
            rrProb = parseVectorOfValues<Real>(rrProbStringTokens, &parseReal);
        }
        GaussCopulaBucketingLossModelBuilder modelbuilder(gaussCopulaMin, gaussCopulaMax, gaussCopulaSteps,
                                                          useQuadrature, nBuckets, homogeneousPoolWhenJustified,
                                                          useStochasticRecovery, rrProb, rrGridString, false);
        return modelbuilder.lossModel(recoveryRates, correlation, homogeneous, seniorities);
    }
};


class GaussCopulaMonteCarloCdoEngineBuilder : public CdoEngineBuilder {
public:
    GaussCopulaMonteCarloCdoEngineBuilder() : CdoEngineBuilder("GaussCopula", "MonteCarlo") {}

    QuantLib::ext::shared_ptr<QuantExt::DefaultLossModel>
    lossModel(const string& qualifier, const vector<Real>& recoveryRates, const Real& detachmentPoint,
              const QuantLib::Date& trancheMaturity, bool homogeneous,
              const std::vector<CdsTier>& seniorities) override {

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
        //bool useStochasticRecovery = parseBool(modelParameter("useStochasticRecovery", {}, false, "false"));

        // Compile default recovery rate grids and probabilities for each name:
        // Recovery rate grids have three pillars, centered around market recovery, DECREASING order : [ 2*RR - 0.1, RR,
        // 0.1 ] Probabilities for the three pillars are symmetric around the center of the distribution and independent
        // of the concrete rate grid
        bool useStochasticRecovery = parseBool(modelParameter("useStochasticRecovery", {}, false, "false"));
        std::vector<std::vector<Real>> recoveryGrids;
        vector<Real> rrProb(11, 1.0);
        if (useStochasticRecovery) {
            for (Size i = 0; i < recoveryRates.size(); ++i) {
                // Use the same recovery rate probabilities across all entites
                // recoveryProbabilities.push_back(rrProbs_);
                // recovery rate grid dependent on market recovery rate
                std::vector<Real> rrGrid(11, recoveryRates[i]); // constant recovery by default
                //std::vector<Real> rrProb(21, 0.0);              // constant recovery by default
                // QL_REQUIRE(rrProbs_.size() == rrGrid.size(), "recovery grid size mismatch");

                rrGrid[10] = 0.025;
                rrGrid[9] = 0.1;
                rrGrid[8] = 0.2;
                rrGrid[7] = 0.3;
                rrGrid[6] = 0.4;
                rrGrid[5] = 0.5;
                rrGrid[4] = 0.6;
                rrGrid[3] = 0.7;
                rrGrid[2] = 0.8;
                rrGrid[1] = 0.9;
                rrGrid[0] = 0.975;
                if (seniorities[i] == CdsTier::SUBLT2) {
                    TLOG("Constituent " << i << " is snrfor and use stochastic lgd");
                    rrProb[10] = 0.325118277844636;
                    rrProb[9] = 0.224476678356585;
                    rrProb[8] = 0.137505881380772;
                    rrProb[7] = 0.0976082381433515;
                    rrProb[6] = 0.0720651798043243;
                    rrProb[5] = 0.0533735316069257;
                    rrProb[4] = 0.0386620690651551;
                    rrProb[3] = 0.0265532021173571;
                    rrProb[2] = 0.0162995635078089;
                    rrProb[1] = 0.00748608812541296;
                    rrProb[0] = 0.000851290047671927;
                } else {
                    rrProb[10] = 0.0215505369655808;
                    rrProb[9] = 0.108383283087955;
                    rrProb[8] = 0.150968176350172;
                    rrProb[7] = 0.164610171389183;
                    rrProb[6] = 0.159045332991804;
                    rrProb[5] = 0.140043519486056;
                    rrProb[4] = 0.112108402294552;
                    rrProb[3] = 0.0793324094757484;
                    rrProb[2] = 0.0459235225768655;
                    rrProb[1] = 0.0169370460512352;
                    rrProb[0] = 0.00109759933084774;
                }
                recoveryGrids.push_back(rrGrid);
            }
        }
            DLOG("Build MonteCarloModel");
            Size nSimulations = parseInteger(engineParameter("nSimulations"));
            auto model = ext::make_shared<QuantExt::GaussianOneFactorMonteCarloLossModel>(correlation, recoveryGrids,
                                                                                          rrProb, nSimulations);
            return model;
    }
};

} // namespace data
} // namespace ore
