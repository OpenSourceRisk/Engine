/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/math/randomvariablelsmbasissystem.hpp>

#include <ql/math/integrals/gaussianquadratures.hpp>

#include <boost/math/special_functions/binomial.hpp>

#include <numeric>
#include <set>

namespace QuantExt {

namespace {

// makes typing a little easier
typedef std::vector<std::function<RandomVariable(const RandomVariable&)>> VF_R;
typedef std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>> VF_A;
typedef std::vector<std::vector<Size>> VV;

// pow(x, order)
class MonomialFct {
public:
    explicit MonomialFct(Size order) : order_(order) {}
    inline RandomVariable operator()(const RandomVariable& x) const { return compute(x, order_); }

private:
    inline RandomVariable compute(const RandomVariable& x, Size order) const {
        switch (order) {
        case 0:
            return RandomVariable(x.size(), 1.0);
        case 1:
            return x;
        case 2:
            return x * x;
        case 3:
            return x * x * x;
        case 4: {
            auto y = x * x;
            return y * y;
        }
        case 5: {
            auto y = x * x;
            return y * y * x;
        }
        case 6: {
            auto y = x * x;
            return y * y * y;
        }
        case 7: {
            auto y = x * x;
            return y * y * y * x;
        }
        case 8: {
            auto y = x * x;
            y *= y;
            return y * y;
        }
        default: {
            Size m = order / 2, r = order % 2;
            auto y = compute(x, m);
            if (r > 0)
                return y * y * x;
            else
                return y * y;
        }
        }
    }
    const Size order_;
};

/* multiplies [RV -> RV] functors
   to create [Vec<RV> -> RV] functor */
class MultiDimFct {
public:
    explicit MultiDimFct(const VF_R& b) : b_(b) { QL_REQUIRE(b_.size() > 0, "zero size basis"); }
    inline RandomVariable operator()(const std::vector<const RandomVariable*>& a) const {
#if defined(QL_EXTRA_SAFETY_CHECKS)
        QL_REQUIRE(b_.size() == a.size(), "wrong argument size");
#endif
        RandomVariable ret = b_[0](*a[0]);
        for (Size i = 1; i < b_.size(); ++i)
            ret *= b_[i](*a[i]);
        return ret;
    }

private:
    const VF_R b_;
};

// check size and order of tuples
void check_tuples(const VV& v, Size dim, Size order) {
    for (Size i = 0; i < v.size(); ++i) {
        QL_REQUIRE(dim == v[i].size(), "wrong tuple size");
        QL_REQUIRE(order == std::accumulate(v[i].begin(), v[i].end(), 0u), "wrong tuple order");
    }
}

// build order N+1 tuples from order N tuples
VV next_order_tuples(const VV& v) {
    const Size order = std::accumulate(v[0].begin(), v[0].end(), 0u);
    const Size dim = v[0].size();

    check_tuples(v, dim, order);

    // the set of unique tuples
    std::set<std::vector<Size>> tuples;
    std::vector<Size> x;
    for (Size i = 0; i < dim; ++i) {
        // increase i-th value in every tuple by 1
        for (Size j = 0; j < v.size(); ++j) {
            x = v[j];
            x[i] += 1;
            tuples.insert(x);
        }
    }

    VV ret(tuples.begin(), tuples.end());
    return ret;
}
} // namespace

// LsmBasisSystem static methods

VF_R RandomVariableLsmBasisSystem::pathBasisSystem(Size order, QuantLib::LsmBasisSystem::PolynomialType type) {
    VF_R ret(order + 1);
    for (Size i = 0; i <= order; ++i) {
        switch (type) {
        case QuantLib::LsmBasisSystem::Monomial:
            ret[i] = MonomialFct(i);
            break;
        case QuantLib::LsmBasisSystem::Laguerre: {
            GaussLaguerrePolynomial p;
            ret[i] = [=](RandomVariable x) {
                for (std::size_t i = 0; i < x.size(); ++i) {
                    x.set(i, p.weightedValue(i, x[i]));
                }
                return x;
            };
        } break;
        case QuantLib::LsmBasisSystem::Hermite: {
            GaussHermitePolynomial p;
            ret[i] = [=](RandomVariable x) {
                for (std::size_t i = 0; i < x.size(); ++i) {
                    x.set(i, p.weightedValue(i, x[i]));
                }
                return x;
            };
        } break;
        case QuantLib::LsmBasisSystem::Hyperbolic: {
            GaussHyperbolicPolynomial p;
            ret[i] = [=](RandomVariable x) {
                for (std::size_t i = 0; i < x.size(); ++i) {
                    x.set(i, p.weightedValue(i, x[i]));
                }
                return x;
            };
        } break;
        case QuantLib::LsmBasisSystem::Legendre: {
            GaussLegendrePolynomial p;
            ret[i] = [=](RandomVariable x) {
                for (std::size_t i = 0; i < x.size(); ++i) {
                    x.set(i, p.weightedValue(i, x[i]));
                }
                return x;
            };
        } break;
        case QuantLib::LsmBasisSystem::Chebyshev: {
            GaussChebyshevPolynomial p;
            ret[i] = [=](RandomVariable x) {
                for (std::size_t i = 0; i < x.size(); ++i) {
                    x.set(i, p.weightedValue(i, x[i]));
                }
                return x;
            };
        } break;
        case QuantLib::LsmBasisSystem::Chebyshev2nd: {
            GaussChebyshev2ndPolynomial p;
            ret[i] = [=](RandomVariable x) {
                for (std::size_t i = 0; i < x.size(); ++i) {
                    x.set(i, p.weightedValue(i, x[i]));
                }
                return x;
            };
        } break;
        default:
            QL_FAIL("unknown regression type");
        }
    }
    return ret;
}

VF_A RandomVariableLsmBasisSystem::multiPathBasisSystem(Size dim, Size order,
                                                        QuantLib::LsmBasisSystem::PolynomialType type) {
    QL_REQUIRE(dim > 0, "zero dimension");
    // get single factor basis
    VF_R pathBasis = pathBasisSystem(order, type);
    VF_A ret;
    // 0-th order term
    VF_R term(dim, pathBasis[0]);
    ret.push_back(MultiDimFct(term));
    // start with all 0 tuple
    VV tuples(1, std::vector<Size>(dim));
    // add multi-factor terms
    for (Size i = 1; i <= order; ++i) {
        tuples = next_order_tuples(tuples);
        // now we have all tuples of order i
        // for each tuple add the corresponding term
        for (Size j = 0; j < tuples.size(); ++j) {
            for (Size k = 0; k < dim; ++k)
                term[k] = pathBasis[tuples[j][k]];
            ret.push_back(MultiDimFct(term));
        }
    }
    return ret;
}

Real RandomVariableLsmBasisSystem::size(Size dim, Size order) {
    // see e.g. proposition 3 in https://murphmath.wordpress.com/2012/08/22/counting-monomials/
    return boost::math::binomial_coefficient<Real>(
        dim + order, order,
        boost::math::policies::make_policy(
            boost::math::policies::overflow_error<boost::math::policies::ignore_error>()));
}

} // namespace QuantExt
