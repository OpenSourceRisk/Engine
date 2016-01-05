/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file pseudoparameter.hpp
    \brief parameter giving access to calibration machinery
*/

#ifndef quantext_pseudoparameter_hpp
#define quantext_pseudoparameter_hpp

#include <ql/models/parameter.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! lightweight parameter, that gives access to the
    CalibratedModel calibration machinery, but without
    any own logic (if this is not needed anyway) */

class PseudoParameter : public Parameter {
  private:
    class Impl : public Parameter::Impl {
      public:
        Impl() {}
        Real value(const Array &, Time ) const {
            QL_FAIL("pseudo-parameter can not be asked to values");
        }
    };

  public:
    PseudoParameter(const Size size = 0,
                    const Constraint &constraint = NoConstraint())
        : Parameter(size, boost::make_shared<PseudoParameter::Impl>(),
                    constraint) {}
};

} // namespace QuantExt

#endif
