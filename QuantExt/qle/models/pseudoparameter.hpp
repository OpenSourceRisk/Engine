/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file pseudoparameter.hpp
    \brief parameter giving access to calibration machinery

        \ingroup models
*/

#ifndef quantext_pseudoparameter_hpp
#define quantext_pseudoparameter_hpp

#include <ql/models/parameter.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Parameter that accesses CalibratedModel
/*! lightweight parameter, that gives access to the
    CalibratedModel calibration machinery, but without
    any own logic (if this is not needed anyway)

        \ingroup models
*/
class PseudoParameter : public Parameter {
private:
    class Impl : public Parameter::Impl {
    public:
        Impl() {}
        Real value(const Array&, Time) const { QL_FAIL("pseudo-parameter can not be asked to values"); }
    };

public:
    PseudoParameter(const Size size = 0, const Constraint& constraint = NoConstraint())
        : Parameter(size, boost::make_shared<PseudoParameter::Impl>(), constraint) {}
};

} // namespace QuantExt

#endif
