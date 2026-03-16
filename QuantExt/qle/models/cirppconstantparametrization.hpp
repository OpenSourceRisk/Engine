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

/*! \file cirppconstantparametrization.hpp
    \brief constant CIR ++ parametrization
    \ingroup models
*/

#ifndef quantext_cirpp_constant_parametrization_hpp
#define quantext_cirpp_constant_parametrization_hpp

#include <qle/models/cirppparametrization.hpp>

#include <ql/errors.hpp>

namespace QuantExt {

//! CIR++ Constant Parametrization
/*! \ingroup models
 */
template <class TS> class CirppConstantParametrization : public CirppParametrization<TS> {
public:
    CirppConstantParametrization(const Currency& currency, const Handle<TS>& termStructure, const Real kappa,
                                 const Real theta, const Real sigma, const Real y0, const bool shifted,
                                 const std::string& name = std::string());

    Real kappa(const Time t) const;
    Real theta(const Time t) const;
    Real sigma(const Time t) const;
    Real y0(const Time t) const;

    const QuantLib::ext::shared_ptr<Parameter> parameter(const Size) const;

protected:
    Real direct(const Size i, const Real x) const;
    Real inverse(const Size j, const Real y) const;

private:
    const QuantLib::ext::shared_ptr<PseudoParameter> kappa_, theta_, sigma_, y0_;
};

// implementation

template <class TS>
CirppConstantParametrization<TS>::CirppConstantParametrization(const Currency& currency,
                                                               const Handle<TS>& termStructure,
                                                               const Real kappa, const Real theta,
                                                               const Real sigma, const Real y0,
                                                               const bool shifted,
                                                               const std::string& name)
    : CirppParametrization<TS>(currency, termStructure, shifted, name),
      kappa_(QuantLib::ext::make_shared<PseudoParameter>(1)),
      theta_(QuantLib::ext::make_shared<PseudoParameter>(1)), sigma_(QuantLib::ext::make_shared<PseudoParameter>(1)),
      y0_(QuantLib::ext::make_shared<PseudoParameter>(1)) {
    kappa_->setParam(0, inverse(0, kappa));
    theta_->setParam(0, inverse(1, theta));
    sigma_->setParam(0, inverse(2, sigma));
    y0_->setParam(0, inverse(3, y0));
}

template <class TS> inline Real CirppConstantParametrization<TS>::direct(const Size i, const Real x) const {
    constexpr Real eps = 1E-10;
    return x * x + eps;
}

template <class TS> inline Real CirppConstantParametrization<TS>::inverse(const Size i, const Real y) const {
    constexpr Real eps = 1E-10;
    return std::sqrt(y - eps);
}

template <class TS> inline Real CirppConstantParametrization<TS>::kappa(const Time) const {
    return direct(0, kappa_->params()[0]);
}

template <class TS> inline Real CirppConstantParametrization<TS>::theta(const Time) const {
    return direct(1, theta_->params()[0]);
}

template <class TS> inline Real CirppConstantParametrization<TS>::sigma(const Time) const {
    return direct(2, sigma_->params()[0]);
}

template <class TS> inline Real CirppConstantParametrization<TS>::y0(const Time) const {
    return direct(3, y0_->params()[0]);
}

template <class TS>
inline const QuantLib::ext::shared_ptr<Parameter> CirppConstantParametrization<TS>::parameter(const Size i) const {
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
typedef CirppConstantParametrization<YieldTermStructure> IrCirppConstantParametrization;
typedef CirppConstantParametrization<DefaultProbabilityTermStructure> CrCirppConstantParametrization;

} // namespace QuantExt

#endif
