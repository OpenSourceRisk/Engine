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

/*! \file crossassetanalyticsbase.hpp
    \brief basic functions for analytics in the cross asset model
    \ingroup crossassetmodel
*/

#ifndef quantext_crossasset_analytics_base_hpp
#define quantext_crossasset_analytics_base_hpp

#include <boost/bind/bind.hpp>

#include <ql/types.hpp>

#include <qle/models/crossassetmodel.hpp>

namespace QuantExt {
using namespace QuantLib;

namespace CrossAssetAnalytics {

/*! \addtogroup crossassetmodel
    @{
*/

/*! generic integrand */
template <class E> Real integral_helper(const CrossAssetModel& x, const E& e, const Real t);

/*! generic integral calculation */
template <typename E>
Real integral(const CrossAssetModel& model, const E& e, const Real a, const Real b);

/*! product expression, 2 factors */
template <typename E1, typename E2> struct P2_ {
    P2_(const E1& e1, const E2& e2) : e1_(e1), e2_(e2) {}
    Real eval(const CrossAssetModel& x, const Real t) const {
        return e1_.eval(x, t) * e2_.eval(x, t);
    }
    const E1& e1_;
    const E2& e2_;
};

/*! product expression, 3 factors */
template <typename E1, typename E2, typename E3> struct P3_ {
    P3_(const E1& e1, const E2& e2, const E3& e3) : e1_(e1), e2_(e2), e3_(e3) {}
    Real eval(const CrossAssetModel& x, const Real t) const {
        return e1_.eval(x, t) * e2_.eval(x, t) * e3_.eval(x, t);
    }
    const E1& e1_;
    const E2& e2_;
    const E3& e3_;
};

/*! product expression, 4 factors */
template <typename E1, typename E2, typename E3, typename E4> struct P4_ {
    P4_(const E1& e1, const E2& e2, const E3& e3, const E4& e4) : e1_(e1), e2_(e2), e3_(e3), e4_(e4) {}
    Real eval(const CrossAssetModel& x, const Real t) const {
        return e1_.eval(x, t) * e2_.eval(x, t) * e3_.eval(x, t) * e4_.eval(x, t);
    }
    const E1& e1_;
    const E2& e2_;
    const E3& e3_;
    const E4& e4_;
};

/*! product expression, 5 factors */
template <typename E1, typename E2, typename E3, typename E4, typename E5> struct P5_ {
    P5_(const E1& e1, const E2& e2, const E3& e3, const E4& e4, const E5& e5)
        : e1_(e1), e2_(e2), e3_(e3), e4_(e4), e5_(e5) {}
    Real eval(const CrossAssetModel& x, const Real t) const {
        return e1_.eval(x, t) * e2_.eval(x, t) * e3_.eval(x, t) * e4_.eval(x, t) * e5_.eval(x, t);
    }
    const E1& e1_;
    const E2& e2_;
    const E3& e3_;
    const E4& e4_;
    const E5& e5_;
};

/*! creator function for product expression, 2 factors */
template <class E1, class E2> const P2_<E1, E2> P(const E1& e1, const E2& e2) { return P2_<E1, E2>(e1, e2); }

/*! creator function for product expression, 3 factors */
template <class E1, class E2, class E3> const P3_<E1, E2, E3> P(const E1& e1, const E2& e2, const E3& e3) {
    return P3_<E1, E2, E3>(e1, e2, e3);
}

/*! creator function for product expression, 4 factors */
template <class E1, class E2, class E3, class E4>
const P4_<E1, E2, E3, E4> P(const E1& e1, const E2& e2, const E3& e3, const E4& e4) {
    return P4_<E1, E2, E3, E4>(e1, e2, e3, e4);
}

/*! creator function for product expression, 5 factors */
template <class E1, class E2, class E3, class E4, class E5>
const P5_<E1, E2, E3, E4, E5> P(const E1& e1, const E2& e2, const E3& e3, const E4& e4, const E5& e5) {
    return P5_<E1, E2, E3, E4, E5>(e1, e2, e3, e4, e5);
}

/*! linear combination, 1 factor i.e. c + c1 * factor1. */
template <typename E1> struct LC1_ {

    LC1_(QuantLib::Real c, QuantLib::Real c1, const E1& e1) : c_(c), c1_(c1), e1_(e1) {}

    Real eval(const CrossAssetModel& x, const Real t) const { return c_ + c1_ * e1_.eval(x, t); }

    QuantLib::Real c_;
    QuantLib::Real c1_;
    E1 e1_;
};

/*! linear combination, 2 factors i.e. c + c1 * factor1 + c2 * factor2. */
template <typename E1, typename E2> struct LC2_ {

    LC2_(QuantLib::Real c, QuantLib::Real c1, const E1& e1, QuantLib::Real c2, const E2& e2)
        : c_(c), c1_(c1), e1_(e1), c2_(c2), e2_(e2) {}

    Real eval(const CrossAssetModel& x, const Real t) const {
        return c_ + c1_ * e1_.eval(x, t) + c2_ * e2_.eval(x, t);
    }

    QuantLib::Real c_;
    QuantLib::Real c1_;
    E1 e1_;
    QuantLib::Real c2_;
    E2 e2_;
};

/*! linear combination, 3 factors. */
template <typename E1, typename E2, typename E3> struct LC3_ {

    LC3_(QuantLib::Real c, QuantLib::Real c1, const E1& e1, QuantLib::Real c2, const E2& e2, QuantLib::Real c3,
         const E3& e3)
        : c_(c), c1_(c1), e1_(e1), c2_(c2), e2_(e2), c3_(c3), e3_(e3) {}

    Real eval(const CrossAssetModel& x, const Real t) const {
        return c_ + c1_ * e1_.eval(x, t) + c2_ * e2_.eval(x, t) + c3_ * e3_.eval(x, t);
    }

    QuantLib::Real c_;
    QuantLib::Real c1_;
    E1 e1_;
    QuantLib::Real c2_;
    E2 e2_;
    QuantLib::Real c3_;
    E3 e3_;
};

/*! linear combination, 4 factors. */
template <typename E1, typename E2, typename E3, typename E4> struct LC4_ {

    LC4_(QuantLib::Real c, QuantLib::Real c1, const E1& e1, QuantLib::Real c2, const E2& e2, QuantLib::Real c3,
         const E3& e3, QuantLib::Real c4, const E4& e4)
        : c_(c), c1_(c1), e1_(e1), c2_(c2), e2_(e2), c3_(c3), e3_(e3), c4_(c4), e4_(e4) {}

    Real eval(const CrossAssetModel& x, const Real t) const {
        return c_ + c1_ * e1_.eval(x, t) + c2_ * e2_.eval(x, t) + c3_ * e3_.eval(x, t) + c4_ * e4_.eval(x, t);
    }

    QuantLib::Real c_;
    QuantLib::Real c1_;
    E1 e1_;
    QuantLib::Real c2_;
    E2 e2_;
    QuantLib::Real c3_;
    E3 e3_;
    QuantLib::Real c4_;
    E4 e4_;
};

/*! creator function for linear combination, 1 factor */
template <class E1> const LC1_<E1> LC(QuantLib::Real c, QuantLib::Real c1, const E1& e1) { return LC1_<E1>(c, c1, e1); }

/*! creator function for linear combination, 2 factors */
template <class E1, class E2>
const LC2_<E1, E2> LC(QuantLib::Real c, QuantLib::Real c1, const E1& e1, QuantLib::Real c2, const E2& e2) {
    return LC2_<E1, E2>(c, c1, e1, c2, e2);
}

/*! creator function for linear combination, 3 factors */
template <class E1, class E2, class E3>
const LC3_<E1, E2, E3> LC(QuantLib::Real c, QuantLib::Real c1, const E1& e1, QuantLib::Real c2, const E2& e2,
                          QuantLib::Real c3, const E3& e3) {
    return LC3_<E1, E2, E3>(c, c1, e1, c2, e2, c3, e3);
}

/*! creator function for linear combination, 4 factors */
template <class E1, class E2, class E3, class E4>
const LC4_<E1, E2, E3, E4> LC(QuantLib::Real c, QuantLib::Real c1, const E1& e1, QuantLib::Real c2, const E2& e2,
                              QuantLib::Real c3, const E3& e3, QuantLib::Real c4, const E4& e4) {
    return LC4_<E1, E2, E3, E4>(c, c1, e1, c2, e2, c3, e3, c4, e4);
}

// inline

template <class E> inline Real integral_helper(const CrossAssetModel& x, const E& e, const Real t) {
    return e.eval(x, t);
}

template <class E>
inline Real integral(const CrossAssetModel& x, const E& e, const Real a, const Real b) {
    return x.integrator()->operator()(QuantLib::ext::bind(&integral_helper<E>, x, e, QuantLib::ext::placeholders::_1), a, b);
}

/*! @} */

} // namespace CrossAssetAnalytics
} // namespace QuantExt

#endif
