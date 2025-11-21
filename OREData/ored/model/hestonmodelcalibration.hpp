/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file ored/model/hestonmodelcalibration.hpp
    \brief build and calibrate a single Heston model
    \ingroup utilities
*/

#pragma once

#include <ored/model/assetmodelbuilderbase.hpp>
#include <qle/pricingengines/varianceswapgeneralreplicationengine.hpp>
#include <ql/models/equity/hestonmodel.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

  /* The naive calibration of all Heston parameters (theta, kappa, sigma, rho, v0) to the European Option surface
     is very sensitive to the choice of initial values. We therefore calibrate in two steps:
     
     1) Set initial values of theta, kappa and v0 using the variance curve implied from the option
     surface. Heston model implied expected variance:

              E[v(T)] = theta * T + (v0 - theta) * (1 - exp(-kappa * T)) / kappa 

     The market variance curve is constructed using the method inherited from the variance swap replication engine.
     See https:://ssrn.com//abstract=2255550
     
     2) With these starting points we calibrate the full Heston model as usual.

   */
class HestonModelCalibration {
public:
    HestonModelCalibration(const ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                           const std::vector<Period>& expiries = {3 * Months, 6 * Months, 1 * Years, 2 * Years,
                                                                  3 * Years, 5 * Years},
                           const std::vector<Real>& moneyness = {-2.0, -1.0, 0.0, 1.0, 2.0},
                           const std::vector<Period>& varianceTerms = {1 * Months, 2 * Months, 3 * Months, 6 * Months,
                                                                       9 * Months, 1 * Years, 15 * Months, 18 * Months,
                                                                       21 * Months, 2 * Years, 4 * Years, 5 * Years,
                                                                       6 * Years, 7 * Years, 10 * Years},
                           // theta, kappa, sigma, rho, v0 (same order as in the Heston model, not the Heston process)
                           const std::vector<Real>& initialValues = {0.04, 1.0, 0.5, -0.9, 0.04},
                           const std::vector<bool>& fixedValues = {false, false, false, false, false},
                           Real relaxedFellerConstraint = 1.0, Size restarts = 0, Real tolerance = 0.001, const bool dontCalibrate = false)
        : process_(process), expiries_(expiries), moneyness_(moneyness), varianceTerms_(varianceTerms),
          initialValues_(initialValues), fixedValues_(fixedValues), relaxedFellerConstraint_(relaxedFellerConstraint),
          restarts_(restarts), tolerance_(tolerance), dontCalibrate_(dontCalibrate) {}

    ext::shared_ptr<HestonModel> model();

    const CalibrationResults& results() const { return results_; }

private:
    class VarianceCalculator : public GeneralisedReplicatingVarianceSwapEngine {
    public:
        VarianceCalculator(const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process)
            : GeneralisedReplicatingVarianceSwapEngine(QuantLib::ext::shared_ptr<QuantLib::Index>(), process,
                                                       process->riskFreeRate()) {
	    settings_.bounds = VarSwapSettings::Bounds::Fixed;
            settings_.fixedMinStdDevs = -3.0;
            settings_.fixedMaxStdDevs = +3.0;
            settings_.priceThreshold = 1e-10;
        }

        Real futureVariance(const Date& maturity) const { return calculateFutureVariance(maturity); }
    };

    // 1) calibrate theta, kappa and v0 to the variance curve, use as initial values for step 2)
    // 2) calibrate the full model  
    ext::shared_ptr<HestonModel> model1();

    // Randomised initial values, calibrate the model repeatedly and keep best result  
    ext::shared_ptr<HestonModel> model2();

    std::vector<Real> buildHelpers(const QuantLib::ext::shared_ptr<PricingEngine>& engine);
    void buildExpectedVariances();
    void logCalibration(const std::vector<Real>& m);
    Real getRMSE();
  
    // Inputs
    ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
    std::vector<Period> expiries_;
    std::vector<Real> moneyness_;
    std::vector<Period> varianceTerms_;
    std::vector<Real> initialValues_;
    std::vector<bool> fixedValues_;
    Real relaxedFellerConstraint_;
    Size restarts_;
    Real tolerance_;
    bool dontCalibrate_;

    // Results
    std::vector<ext::shared_ptr<CalibrationHelper>> helpers_;
    std::vector<Time> varianceTimes_;
    std::vector<Real> annualisedVariances_;

    CalibrationResults results_;
};

class RelaxedFellerConstraint : public Constraint {
private:
    class Impl final : public Constraint::Impl {
    public:
        Impl(Real epsilon) : epsilon_(epsilon) {}
        bool test(const Array& params) const override {
            const Real theta = params[0];
            const Real kappa = params[1];
            const Real sigma = params[2];

            // return (sigma >= 0.0 && sigma*sigma < 2.0*kappa*theta);
            return (sigma >= 0.0 && epsilon_ * sigma * sigma < 2.0 * kappa * theta);
        }

    private:
        Real epsilon_;
    };

public:
    RelaxedFellerConstraint(Real epsilon = 1.0)
        : Constraint(ext::shared_ptr<Constraint::Impl>(new RelaxedFellerConstraint::Impl(epsilon))) {
        QL_REQUIRE(epsilon > 0.0 && epsilon <= 1.0, "epsilon " << epsilon << " out of range [0,1]");
    }
};
  
} // namespace data
} // namespace ore
