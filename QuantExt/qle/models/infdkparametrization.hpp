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

/*! \file qle/models/infdkparametrization.hpp
    \brief Inflation Dodgson Kainth parametrization
*/

#ifndef quantext_infdklgm1f_parametrization_hpp
#define quantext_infdklgm1f_parametrization_hpp

#include <ql/handle.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/models/parametrization.hpp>

namespace QuantExt {
using namespace QuantLib;
using namespace QuantExt;

class InfDkParametrization : public Parametrization {
public:
    InfDkParametrization(QuantLib::ext::shared_ptr<Lgm1fParametrization<ZeroInflationTermStructure>> dkLgmParam,
                         const Handle<ZeroInflationIndex>& index)
        : Parametrization(dkLgmParam->currency(), dkLgmParam->name()), dkLgmParam_(dkLgmParam), index_(index) {}
    const QuantLib::ext::shared_ptr<Lgm1fParametrization<ZeroInflationTermStructure>> dkLgmParam() const {
        return dkLgmParam_;
    }
    const Handle<ZeroInflationIndex>& inflationIndex() const { return index_; }

    const Currency& currency() const override { return dkLgmParam_->currency(); }

    const Array& parameterTimes(const Size i) const override { return dkLgmParam_->parameterTimes(i); }

    virtual Size numberOfParameters() const override { return dkLgmParam_->numberOfParameters(); }

    virtual Array parameterValues(const Size i) const override { return dkLgmParam_->parameterValues(i); }

    virtual const QuantLib::ext::shared_ptr<Parameter> parameter(const Size i) const override {
        return dkLgmParam_->parameter(i);
    }

    virtual void update() const override { dkLgmParam_->update(); }

private:
    QuantLib::ext::shared_ptr<Lgm1fParametrization<ZeroInflationTermStructure>> dkLgmParam_;
    Handle<ZeroInflationIndex> index_;
};

class InfDkConstantParametrization : public InfDkParametrization {
public:
    InfDkConstantParametrization(const Currency& currency, const Handle<ZeroInflationTermStructure>& termStructure,
                                 const Real alpha, const Real kappa, const Handle<ZeroInflationIndex>& index,
                                 const std::string& name = std::string())
        : InfDkParametrization(ext::make_shared<Lgm1fConstantParametrization<ZeroInflationTermStructure>>(
                                   currency, termStructure, alpha, kappa, name),
                               index) {}
};

class InfDkPiecewiseConstantHullWhiteAdaptor : public InfDkParametrization {
public:
    InfDkPiecewiseConstantHullWhiteAdaptor(const Currency& currency,
                                           const Handle<ZeroInflationTermStructure>& termStructure,
                                           const Array& sigmaTimes, const Array& sigma, const Array& kappaTimes,
                                           const Array& kappa, const Handle<ZeroInflationIndex>& index,
                                           const std::string& name = std::string(),
                                           const QuantLib::ext::shared_ptr<QuantLib::Constraint>& sigmaConstraint =
                                               QuantLib::ext::make_shared<QuantLib::NoConstraint>(),
                                           const QuantLib::ext::shared_ptr<QuantLib::Constraint>& kappaConstraint =
                                               QuantLib::ext::make_shared<QuantLib::NoConstraint>())
        : InfDkParametrization(ext::make_shared<Lgm1fPiecewiseConstantHullWhiteAdaptor<ZeroInflationTermStructure>>(
                                   currency, termStructure, sigmaTimes, sigma, kappaTimes, kappa, name, sigmaConstraint,
                                   kappaConstraint),
                               index) {}
};

class InfDkPiecewiseConstantParametrization : public InfDkParametrization {
public:
    InfDkPiecewiseConstantParametrization(const Currency& currency,
                                          const Handle<ZeroInflationTermStructure>& termStructure,
                                          const Array& alphaTimes, const Array& alpha, const Array& kappaTimes,
                                          const Array& kappa, const Handle<ZeroInflationIndex>& index,
                                          const std::string& name = std::string(),
                                          const QuantLib::ext::shared_ptr<QuantLib::Constraint>& alphaConstraint =
                                              QuantLib::ext::make_shared<QuantLib::NoConstraint>(),
                                          const QuantLib::ext::shared_ptr<QuantLib::Constraint>& kappaConstraint =
                                              QuantLib::ext::make_shared<QuantLib::NoConstraint>())
        : InfDkParametrization(ext::make_shared<Lgm1fPiecewiseConstantParametrization<ZeroInflationTermStructure>>(
                                   currency, termStructure, alphaTimes, alpha, kappaTimes, kappa, name, alphaConstraint,
                                   kappaConstraint),
                               index) {}
};

class InfDkPiecewiseLinearParametrization : public InfDkParametrization {
public:
    InfDkPiecewiseLinearParametrization(const Currency& currency,
                                        const Handle<ZeroInflationTermStructure>& termStructure,
                                        const Array& alphaTimes, const Array& alpha, const Array& hTimes,
                                        const Array& h, const Handle<ZeroInflationIndex>& index,
                                        const std::string& name = std::string(),
                                        const QuantLib::ext::shared_ptr<QuantLib::Constraint>& alphaConstraint =
                                            QuantLib::ext::make_shared<QuantLib::NoConstraint>(),
                                        const QuantLib::ext::shared_ptr<QuantLib::Constraint>& hConstraint =
                                            QuantLib::ext::make_shared<QuantLib::NoConstraint>())
        : InfDkParametrization(
              ext::make_shared<Lgm1fPiecewiseLinearParametrization<ZeroInflationTermStructure>>(
                  currency, termStructure, alphaTimes, alpha, hTimes, h, name, alphaConstraint, hConstraint),
              index) {}
};

} // namespace QuantExt

#endif
