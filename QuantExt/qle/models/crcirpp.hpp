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

/*!
  \file crcirpp.hpp
  \brief CIR++ credit model class
  \ingroup models
 */

#ifndef quantext_crcirpp_hpp
#define quantext_crcirpp_hpp

#include <ql/experimental/credit/cdsoption.hpp>
#include <ql/stochasticprocess.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>
#include <qle/models/cirppparametrization.hpp>
#include <qle/processes/crcirppstateprocess.hpp>

#include <ql/math/distributions/chisquaredistribution.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Cox-Ingersoll-Ross ++  credit model class.
/*! This class implements the Cox-Ingersoll-Ross model defined by
  \f[
  \lambda(t) = y(t) + \psi (t) \\
  dy(t) = a(\theta - y(t)) dt + \sigma \, \sqrt{y(t)} \, dW
  \f]
*/

class CrCirpp : public LinkableCalibratedModel {

public:
    CrCirpp(const QuantLib::ext::shared_ptr<CrCirppParametrization>& parametrization);

    Real zeroBond(Real t, Real T, Real y) const;
    Real survivalProbability(Real t, Real T, Real y) const;

    // density of y(t) incl required change of measure
    Real densityForwardMeasure(Real x, Real t);
    Real cumulativeForwardMeasure(Real x, Real t);
    // density of y(t) in the w/o change of measure
    Real density(Real x, Real t);
    Real cumulative(Real x, Real t);
    Real zeroBondOption(Real eval_t, Real expiry_T, Real maturity_tau, Real strike_k, Real y_t, Real w);

    Handle<DefaultProbabilityTermStructure> defaultCurve(std::vector<Date> dateGrid = std::vector<Date>()) const;

    const QuantLib::ext::shared_ptr<CrCirppParametrization> parametrization() const;
    const QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const;

    Real A(Real t, Real T) const;
    Real B(Real t, Real T) const;

    /*! observer and linked calibrated model interface */
    void update() override;
    void generateArguments() override;

private:
    QuantLib::ext::shared_ptr<CrCirppParametrization> parametrization_;
    QuantLib::ext::shared_ptr<CrCirppStateProcess> stateProcess_;
}; // class CrCirpp

inline void CrCirpp::update() {
    notifyObservers();
    parametrization_->update();
}

inline void CrCirpp::generateArguments() { update(); }

inline const QuantLib::ext::shared_ptr<CrCirppParametrization> CrCirpp::parametrization() const {
    return parametrization_;
}

inline const QuantLib::ext::shared_ptr<StochasticProcess> CrCirpp::stateProcess() const {
    QL_REQUIRE(stateProcess_ != NULL, "stateProcess has null pointer in CIR++ stateProcess!");
    return stateProcess_;
}

} // end namespace QuantExt

#endif
