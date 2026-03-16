/*
  Copyright (C) 2020 Quaternion Risk Management Ltd.
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

#include <iostream>

#include <boost/math/distributions.hpp>
#include <boost/math/distributions/non_central_chi_squared.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/termstructures/credit/interpolatedsurvivalprobabilitycurve.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <qle/models/crcirpp.hpp>

namespace QuantExt {

namespace {
Real nccs(const Real df, const Real ncp, const Real x, const bool cumulative) {
    QL_REQUIRE(std::isfinite(df) && df > 0, "CrCirpp::density(): illegal df=" << df);
    // boost
    boost::math::non_central_chi_squared_distribution<> d(df, ncp);
    return cumulative ? cdf(d, x) : pdf(d, x);
    // ql (use density from boost, there is none in ql?)
    // return cumulative ? NonCentralCumulativeChiSquareDistribution(df, ncp)(x) : pdf(d, x);
}
} // namespace

CrCirpp::CrCirpp(
    const QuantLib::ext::shared_ptr<CrCirppParametrization>& parametrization)
    : parametrization_(parametrization) {

    // FIXME hardcoded discretisation schema
    stateProcess_ =
        QuantLib::ext::make_shared<CrCirppStateProcess>(this, CrCirppStateProcess::Discretization::BrigoAlfonsi);
    QL_REQUIRE(stateProcess_ != NULL, "stateProcess has null pointer in CrCirpp ctor!");

    arguments_.resize(4);
    arguments_[0] = parametrization_->parameter(0);
    arguments_[1] = parametrization_->parameter(1);
    arguments_[2] = parametrization_->parameter(2);
    arguments_[3] = parametrization_->parameter(3);
    registerWith(parametrization_->termStructure());
}

Handle<DefaultProbabilityTermStructure> CrCirpp::defaultCurve(std::vector<Date> dateGrid) const {
    if (parametrization_->shifted()) {
        QL_REQUIRE(!parametrization_->termStructure().empty(), "default curve not set");
        QL_REQUIRE(dateGrid.size() == 0, "dateGrid without effect for shifted model");
        return parametrization_->termStructure();
    } else {
        // build one on the fly
        Date today = Settings::instance().evaluationDate();
        std::vector<Real> survivalProbabilities(1, 1.0);
        std::vector<Date> dates;
        DayCounter tsDayCounter = Actual365Fixed();
        if (dateGrid.size() > 0) {
            QL_REQUIRE(dateGrid.front() == today, "front date must be today");
            dates = dateGrid;
        } else {
            dates.push_back(today);
            // up to one year in monthly steps
            for (Size i = 1; i <= 12; i++)
                dates.push_back(today + i * Months);
            // starts in year two annually
            for (Size i = 2; i <= 10; i++)
                dates.push_back(today + i * Years);
        }
        for (Size i = 1; i < dates.size(); i++) {
            Real t = tsDayCounter.yearFraction(today, dates[i]);
            survivalProbabilities.push_back(survivalProbability(0.0, t, parametrization_->y0(t)));
        }
        QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> defaultCurve(
            new InterpolatedSurvivalProbabilityCurve<LogLinear>(dates, survivalProbabilities, tsDayCounter));
        defaultCurve->enableExtrapolation();
        return Handle<DefaultProbabilityTermStructure>(defaultCurve);
    }
}

Real CrCirpp::A(Real t, Real T) const {
    Real kappa = parametrization_->kappa(t);
    Real theta = parametrization_->theta(t);
    Real sigma = parametrization_->sigma(t);
    Real sigma2 = sigma * sigma;
    Real h = sqrt(kappa * kappa + 2.0 * sigma2);

    Real nominator = 2.0 * h * exp((kappa + h) * (T - t) / 2.0);
    Real denominator = 2.0 * h + (kappa + h) * (exp((T - t) * h) - 1.0);
    Real exponent = 2.0 * kappa * theta / sigma2;

    Real value = std::pow((nominator / denominator), exponent);

    return value;
}

Real CrCirpp::B(Real t, Real T) const {

    Real kappa = parametrization_->kappa(t);
    Real sigma = parametrization_->sigma(t);
    Real sigma2 = sigma * sigma;
    Real h = sqrt(kappa * kappa + 2.0 * sigma2);

    Real nominator = 2.0 * (exp((T - t) * h) - 1.0);
    Real denominator = 2.0 * h + (kappa + h) * (exp((T - t) * h) - 1.0);

    Real value = nominator / denominator;

    return value;
}

Real CrCirpp::zeroBond(Real t, Real T, Real y) const { return A(t, T) * exp(-B(t, T) * y); }

Real CrCirpp::survivalProbability(Real t, Real T, Real y) const {
    Real P_Cir = zeroBond(t, T, y); // A(t,T) * exp(-B(t,T) * y);
    if (parametrization_->shifted()) {
        Probability SP_t = parametrization_->termStructure()->survivalProbability(t);
        // std::cout<<" SP_t  " << SP_t << std::endl;
        Probability SP_T = parametrization_->termStructure()->survivalProbability(T);
        // std::cout<<" SP_T " << SP_T << std::endl;
        // std::cout<<" y0  " << parametrization_->y0(t) << std::endl;
        Real A_bar = (SP_T * A(0, t) * exp(-B(0, t) * parametrization_->y0(t))) /
                     (SP_t * A(0, T) * exp(-B(0, T) * parametrization_->y0(t)));
        // std::cout<<"A_bar " <<A_bar<< std::endl;
        return A_bar * P_Cir;
    } else
        return P_Cir;
}

Real CrCirpp::density(Real x, Real t) {

    Real kappa = parametrization_->kappa(t);
    Real theta = parametrization_->theta(t);
    Real sigma = parametrization_->sigma(t);
    Real y0 = parametrization_->y0(t);
    Real sigma2 = sigma * sigma;

    Real c = 4 * kappa / (sigma2 * (1 - exp(-kappa * t)));
    // degrees of freedom
    Real df = 4 * kappa * theta / (sigma2);
    // non-central parameter
    Real ncp = c * y0 * exp(-kappa * t);

    return c * nccs(df, ncp, c * x, false);
}

Real CrCirpp::cumulative(Real x, Real t) {

    Real kappa = parametrization_->kappa(t);
    Real theta = parametrization_->theta(t);
    Real sigma = parametrization_->sigma(t);
    Real y0 = parametrization_->y0(t);
    Real sigma2 = sigma * sigma;

    Real c = 4 * kappa / (sigma2 * (1 - exp(-kappa * t)));
    // degrees of freedom
    Real df = 4 * kappa * theta / (sigma2);
    // non-central parameter
    Real ncp = c * y0 * exp(-kappa * t);

    return c * nccs(df, ncp, c * x, true);
}

Real CrCirpp::densityForwardMeasure(Real x, Real t) {

    Real kappa = parametrization_->kappa(t);
    Real theta = parametrization_->theta(t);
    Real sigma = parametrization_->sigma(t);
    Real y0 = parametrization_->y0(t);
    Real sigma2 = sigma * sigma;

    Real h = sqrt(kappa * kappa + 2.0 * sigma2);
    Real rho = 2 * h / (sigma2 * (exp(h * t) - 1));
    // q(t,s) in Brigo-Mercuri in (3.28)
    Real c = 2 * (rho + (kappa + h) / sigma2 + 0.0); // 0.0 stands for the B term which is neglectible
    // degrees of freedom
    Real df = 4 * kappa * theta / (sigma2);
    // non-central parameter
    // delta(t,s) in Brigo-Mercurio in (3.28)
    Real ncp = 4 * (rho * rho * y0 * exp(h * t)) / c;

    return c * nccs(df, ncp, c * x, false);
}

// Brigo-Mercurio 2nd Edition, page 67
Real CrCirpp::cumulativeForwardMeasure(Real x, Real t) {

    Real kappa = parametrization_->kappa(t);
    Real theta = parametrization_->theta(t);
    Real sigma = parametrization_->sigma(t);
    Real y0 = parametrization_->y0(t);
    Real sigma2 = sigma * sigma;
    Real h = sqrt(kappa * kappa + 2.0 * sigma2);
    Real rho = 2 * h / (sigma2 * (exp(h * t) - 1));

    // q(t,s) in Brigo-Mercurii (3.28)
    Real c = 2 * (rho + (kappa + h) / sigma2 + 0.0); // 0.0 stands for the B term which is neglectible
    // degrees of freedom
    Real df = 4 * kappa * theta / (sigma2);
    // non-central parameter
    // delta(t,s) in Brigo-Mercurio in (3.28)
    Real ncp = 4 * (rho * rho * y0 * exp(h * t)) / c;

    return c * nccs(df, ncp, c * x, true);
}

// Brigo-Mercurio (3.26) and (3.78)
Real CrCirpp::zeroBondOption(Real eval_t, Real expiry_T, Real maturity_tau, Real strike_k, Real y_t,
                                              Real w) {

    Real value = 0.0;
    Real kappa = parametrization_->kappa(eval_t);
    Real theta = parametrization_->theta(eval_t);
    Real sigma = parametrization_->sigma(eval_t);
    Real y0 = parametrization_->y0(eval_t);
    Real sigma2 = sigma * sigma;

    Real h = sqrt(kappa * kappa + 2.0 * sigma2);
    Real rho = 2.0 * h / (sigma2 * (exp(h * (expiry_T - eval_t)) - 1.0));
    Real Psi = (kappa + h) / sigma2;

    Probability SP_T;
    Probability SP_tau;
    if (parametrization_->shifted()) {
        SP_T = parametrization()->termStructure()->survivalProbability(expiry_T);
        SP_tau = parametrization()->termStructure()->survivalProbability(maturity_tau);
    } else {
        SP_T = survivalProbability(0.0, expiry_T, y0);
        SP_tau = survivalProbability(0.0, maturity_tau, y0);
    }

    Real r_hat = 1.0 / B(expiry_T, maturity_tau) *
                 (log(A(expiry_T, maturity_tau) / strike_k) -
                  log((SP_T * A(0.0, maturity_tau) * exp(-B(0.0, maturity_tau) * y0)) /
                      (SP_tau * A(0.0, expiry_T) * exp(-B(0.0, expiry_T) * y0))));

    Real denom = rho + Psi + B(expiry_T, maturity_tau);
    Real df = 4.0 * kappa * theta / sigma2;
    Real ncp = 2.0 * rho * rho * y_t * exp(h * (expiry_T - eval_t)) / denom;
    QL_REQUIRE(std::isfinite(df) && df > 0, "CrCirpp::zeroBondOption(): illegal df="
                                                << df << ", kappa=" << kappa << ", theta= " << theta
                                                << ", sigma=" << sigma);

    value += nccs(df, ncp, 2 * r_hat * denom, true) * SP_tau;

    denom = rho + Psi;
    // df unchanged
    ncp = 2 * rho * rho * y_t * exp(h * (expiry_T - eval_t)) / denom;

    value -= nccs(df, ncp, 2 * r_hat * denom, true) * SP_T * strike_k;

    if (w < 0.0) {
        // PUT via put-call-paritiy
        // C - P = S_M(T) - S_M(t)*K
        // P = C - S_M(T) + S_M(t)*K
        value -= SP_tau - SP_T * strike_k;
    }

    return value;
}

} // namespace QuantExt
