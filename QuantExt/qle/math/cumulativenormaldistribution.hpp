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

/*! \file cumulativenormaldistribution.hpp
    \brief cumulative normal distribution based on std::erf (since C++11),
    \ingroup math
*/

#ifndef quantext_cumulative_normal_hpp
#define quantext_cumulative_normal_hpp

#include <ql/mathconstants.hpp>
#include <ql/types.hpp>

#if !(defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L) || (_MSC_VER >= 1600))
#include <boost/math/special_functions/erf.hpp>
#endif

using namespace QuantLib;

namespace QuantExt {

/*! Cumulative normal distribution
    This implementation relies on std::erf if c++11 is supported,
    otherwise falls back on boost::math::erf.
 \ingroup math
*/
class CumulativeNormalDistribution {
public:
    CumulativeNormalDistribution(Real average = 0.0, Real sigma = 1.0);
    Real operator()(Real x) const;

private:
    Real average_, sigma_;
};

// inline

inline Real CumulativeNormalDistribution::operator()(Real z) const {
#if !(defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L) || (_MSC_VER >= 1600))
    // in case C++11 is not supported we fall back on boost::math::erf
    return 0.5 * (1.0 + boost::math::erf((z - average_) / sigma_ * M_SQRT1_2));
#else
    return 0.5 * (1.0 + std::erf((z - average_) / sigma_ * M_SQRT1_2));
#endif
}

} // namespace QuantExt

#endif
