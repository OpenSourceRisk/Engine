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

/*! \file cirppparametrization.hpp
    \brief CIR ++ parametrisation
    \ingroup models
*/

#ifndef quantext_cirpp_parametrization_hpp
#define quantext_cirpp_parametrization_hpp

#include <ql/handle.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/models/parametrization.hpp>

namespace QuantExt {

//! CIR++ Parametrization
/*! \ingroup models
 */
template <class TS> class CirppParametrization : public Parametrization {
public:
    CirppParametrization(const Currency& currency, const Handle<TS>& termStructure, bool shifted,
                         const std::string& name = std::string());

    virtual Real kappa(const Time t) const = 0;
    virtual Real theta(const Time t) const = 0;
    virtual Real sigma(const Time t) const = 0;
    virtual Real y0(const Time t) const = 0;

    const Handle<TS> termStructure() const;
    const bool shifted() const;

    Size numberOfParameters() const override { return 4; }

private:
    const Handle<TS> termStructure_;
    const bool shifted_;
};

template <class TS> inline const Handle<TS> CirppParametrization<TS>::termStructure() const { return termStructure_; }
template <class TS> inline const bool CirppParametrization<TS>::shifted() const { return shifted_; }

// typedef
typedef CirppParametrization<DefaultProbabilityTermStructure> CrCirppParametrization;
typedef CirppParametrization<YieldTermStructure> IrCirppParametrization;

template <class TS>
CirppParametrization<TS>::CirppParametrization(const Currency& currency, const Handle<TS>& termStructure, bool shifted,
                                               const std::string& name)
    : Parametrization(currency, name.empty() ? currency.code() : name), termStructure_(termStructure),
      shifted_(shifted) {}

} // namespace QuantExt

#endif
