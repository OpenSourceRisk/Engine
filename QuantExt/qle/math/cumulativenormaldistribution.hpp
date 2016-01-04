/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file cumulativenormaldistribution.hpp
    \brief cumulative normal distribution based on std::erf (since C++11),
           the rationale is that some AD frameworks recognize std::erf
           as an intrinsic function.
*/

#ifndef quantext_cumulative_normal_hpp
#define quantext_cumulative_normal_hpp

#include <ql/mathconstants.hpp>
#include <ql/types.hpp>

#if !(defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L) ||       \
      (_MSC_VER >= 1600))
#include <boost/math/special_functions/erf.hpp>
#endif

using namespace QuantLib;

namespace QuantExt {

class CumulativeNormalDistribution {
  public:
    CumulativeNormalDistribution(Real average = 0.0, Real sigma = 1.0);
    Real operator()(Real x) const;

  private:
    Real average_, sigma_;
};

// inline

inline Real CumulativeNormalDistribution::operator()(Real z) const {
#if !(defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L) ||       \
      (_MSC_VER >= 1600))
    // in case C++11 is not supported we fall back on boost::erf
    return 0.5 * (1.0 + boost::erf((z - average_) / sigma_ * M_SQRT1_2));
#else
    return 0.5 * (1.0 + std::erf((z - average_) / sigma_ * M_SQRT1_2));
#endif
}

} // namespace QuantExt

#endif
