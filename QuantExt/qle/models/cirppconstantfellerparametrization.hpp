/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file cirppconstantfellerparametrization.hpp
    \brief constant CIR ++ parametrization
    \ingroup models
*/

#ifndef quantext_cirpp_constant_feller_parametrization_hpp
#define quantext_cirpp_constant_feller_parametrization_hpp

#include <qle/models/cirppparametrization.hpp>

#include <ql/errors.hpp>

namespace QuantExt {

//! CIR++ Constant Parametrization
/*! \ingroup models
 */
template <class TS> class CirppConstantWithFellerParametrization : public CirppParametrization<TS> {
public:
    // sigma^2 is set to 2 \kappa \theta / fellerFactor (relaxed = false)
    //            resp.  4 \kappa \theta / fellerFactor (relaxed = true)
    CirppConstantWithFellerParametrization(const Currency& currency, const Handle<TS>& termStructure,
                                           const Real kappa, const Real theta, const Real sigma, const Real y0,
                                           const bool shifted, bool relaxed = false,
                                           const Real fellerFactor = 2.0,
                                           const std::string& name = std::string());

    Real kappa(const Time t) const override;
    Real theta(const Time t) const override;
    Real sigma(const Time t) const override;
    Real y0(const Time t) const override;

    const QuantLib::ext::shared_ptr<Parameter> parameter(const Size) const override;
    const bool relaxed() const;

protected:
    Real direct(const Size i, const Real x) const override;
    Real inverse(const Size j, const Real y) const override;

private:
    const QuantLib::ext::shared_ptr<PseudoParameter> kappa_, theta_, sigma_, y0_;
    bool relaxed_;
    const Real fellerFactor_;
};

// implementation

template <class TS>
CirppConstantWithFellerParametrization<TS>::CirppConstantWithFellerParametrization(
    const Currency& currency, const Handle<TS>& termStructure, const Real kappa, const Real theta, const Real sigma,
    const Real y0, const bool shifted, bool relaxed, const Real fellerFactor, const std::string& name)
    : CirppParametrization<TS>(currency, termStructure, shifted, name),
      kappa_(QuantLib::ext::make_shared<PseudoParameter>(1)),
      theta_(QuantLib::ext::make_shared<PseudoParameter>(1)), sigma_(QuantLib::ext::make_shared<PseudoParameter>(1)),
      y0_(QuantLib::ext::make_shared<PseudoParameter>(1)), relaxed_(relaxed), fellerFactor_(fellerFactor) {
    QL_REQUIRE((relaxed_ ? 4.0 : 2.0) * kappa * theta > sigma * sigma,
               "CirppConstantWithFellerParametrization: Feller constraint violated (kappa="
                   << kappa << ", theta=" << theta << ", sigma=" << sigma << " (relaxed=" << std::boolalpha << relaxed_
                   << ")");
    QL_REQUIRE(fellerFactor_ > 1.0, "CirppConstantWithFellerParametrization: Feller factor ("
                                        << fellerFactor_ << ") should be greater than 1.0");
    kappa_->setParam(0, inverse(0, kappa));
    theta_->setParam(0, inverse(1, theta));
    sigma_->setParam(0, inverse(2, sigma));
    y0_->setParam(0, inverse(3, y0));
}

template <class TS>
inline Real CirppConstantWithFellerParametrization<TS>::direct(const Size i, const Real x) const {
    constexpr Real eps = 1E-10;
    switch (i) {
    case 0:
    case 1:
    case 3:
        return x * x + eps;
    case 2: {
        const Real fellerBound =
            std::sqrt((relaxed_ ? 4.0 : 2.0) * direct(0, kappa_->params()[0]) * direct(1, theta_->params()[0]));
        // return fellerBound * (std::atan(x) + M_PI_2 + eps) / (M_PI - 2.0 * eps); // "free sigma"
        return fellerBound / std::sqrt(fellerFactor_); // tie sigma to kappa/theta on feller boundary
    }
    default:
        QL_FAIL("Index is not defined!");
    }
}

template <class TS>
inline Real CirppConstantWithFellerParametrization<TS>::inverse(const Size i, const Real y) const {
    constexpr Real eps = 1E-10;
    switch (i) {
    case 0:
    case 1:
    case 3:
        return std::sqrt(y - eps);
    case 2: {
        const Real fellerBound =
            std::sqrt((relaxed_ ? 4.0 : 2.0) * direct(0, kappa_->params()[0]) * direct(1, theta_->params()[0]));
        // return std::tan(y / fellerBound * (M_PI - 2.0 * eps) - M_PI_2 - eps); // see above
        return fellerBound / std::sqrt(fellerFactor_); // see above
    }
    default:
        QL_FAIL("Index is not defined!");
    }
}

template <class TS> inline Real CirppConstantWithFellerParametrization<TS>::kappa(const Time) const {
    return direct(0, kappa_->params()[0]);
}

template <class TS> inline Real CirppConstantWithFellerParametrization<TS>::theta(const Time) const {
    return direct(1, theta_->params()[0]);
}

template <class TS> inline Real CirppConstantWithFellerParametrization<TS>::sigma(const Time) const {
    return direct(2, sigma_->params()[0]);
}

template <class TS> inline Real CirppConstantWithFellerParametrization<TS>::y0(const Time) const {
    return direct(3, y0_->params()[0]);
}

template <class TS> inline const bool CirppConstantWithFellerParametrization<TS>::relaxed() const {
    return relaxed_;
}

template <class TS>
inline const QuantLib::ext::shared_ptr<Parameter>
CirppConstantWithFellerParametrization<TS>::parameter(const Size i) const {
    QL_REQUIRE(i < 4, "parameter " << i << " does not exist, only have 0..3");
    if (i == 0)
        return kappa_;
    else if (i == 1)
        return theta_;
    else if (i == 2)
        return sigma_;
    else
        return y0_;
}

// typedef
typedef CirppConstantWithFellerParametrization<YieldTermStructure> IrCirppConstantWithFellerParametrization;
typedef CirppConstantWithFellerParametrization<DefaultProbabilityTermStructure> CrCirppConstantWithFellerParametrization;

} // namespace QuantExt

#endif
