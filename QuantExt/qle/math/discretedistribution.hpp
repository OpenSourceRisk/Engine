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

/*! \file qle/math/discretedistribution.hpp
 \brief Discretized probability density and cumulative probability
 */

#pragma once

#include <fstream>
#include <vector>

#include <ql/errors.hpp>
#include <ql/instruments/payoffs.hpp>

namespace QuantExt {
using namespace std;
using namespace QuantLib;

//! Distributionpair is a helper class for DiscretDistribution
/*!
     The Distributionpair holds a single point of a one-domensional
     discrete distribution, i.e. coordinate x and ordinate (density) y.

     \ingroup math
 */

class Distributionpair {
public:
    Distributionpair(Real x = 0, Real y = 0) : x_(x), y_(y) {}
    Real x_, y_;
};

inline bool operator<(const Distributionpair& p1, const Distributionpair& p2) { return p1.x_ < p2.x_; }

inline bool operator>(const Distributionpair& p1, const Distributionpair& p2) { return p1.x_ > p2.x_; }

class MDD;
//! Discrete Distribution
/*!
     This class implements a one-dimensional distribution in terms of a
     vector of Distributionpairs.

     \ingroup math
 */
class DiscreteDistribution {
public:
    DiscreteDistribution(const vector<Distributionpair>&);
    //! Default constructor with probability 1.0 at 0.0
    DiscreteDistribution();
    //! Construct a discrete probability distribution by giving the points and probabilities
    DiscreteDistribution(const vector<Real>& dataPoints, const vector<Real>& probabilities);
    // inspectors
    virtual Size size() const;
    virtual vector<Distributionpair> get() const;
    virtual Distributionpair get(Size i) const;
    //! Return probability for data at index \p i
    Real probability(Size i) const;
    //! Return data at index \p i
    Real data(Size i) const;

    virtual ~DiscreteDistribution() {}

    friend class MDD;

protected:
    vector<Distributionpair> data_;
};
//! Modify Distrete Distribution
/*!
     This class implements a set of operations on discrete disctributions, that
     involve one or two distributions.

     \todo Complete and check the member function documentation (Donal)
     \ingroup math
 */

class MDD {
public:
    /*!
     Convolution of two discrete distribution
     */
    static DiscreteDistribution convolve(const DiscreteDistribution& a, const DiscreteDistribution& b, Size buckets);
    /*!
     Amend the discretization of the distribution such that the number of
     buckets is reduced to the given number.
     */
    static DiscreteDistribution rebucketfixednumber(const DiscreteDistribution& a, Size buckets);
    /*!
     Amend the discretization of the distribution such that the distance
     of adjacent buckets is reduced to the given number.
     */
    static DiscreteDistribution rebucketfixedstep(const DiscreteDistribution& a, Real step);
    /*!
     Add two discrete distributions while introducing a desired number
     of buckets.
     */
    static DiscreteDistribution sum(const DiscreteDistribution& a, const DiscreteDistribution& b, Size buckets);
    /*!
     Add c * distribution b to distribution a, starting from the left.
     */
    static DiscreteDistribution sumspecialunsorted(const DiscreteDistribution& a, const DiscreteDistribution& b,
                                                   Real c);
    /*!
     Add c * distribution b to distribution a, starting from the right.
     */
    static DiscreteDistribution sumspecial(const DiscreteDistribution& a, const DiscreteDistribution& b, Real c);
    /*!
     TODO
     */
    static DiscreteDistribution sumspecialright(const DiscreteDistribution& a, const DiscreteDistribution& b, Real c);
    /*!
     TODO
     */
    static DiscreteDistribution splicemezz(const DiscreteDistribution& a, const DiscreteDistribution& b, Real c);
    /*!
     Scale each density by factor b.
     */
    static DiscreteDistribution scalarmultprob(const DiscreteDistribution& a, const Real& b);
    /*!
     Scale each coordinate by factor x.
     */
    static DiscreteDistribution scalarmultx(const DiscreteDistribution& a, const Real& b);
    /*!
     Shift each coordinate by amount b.
     */
    static DiscreteDistribution scalarshiftx(const DiscreteDistribution& a, const Real& b);
    /*!
     CHECK:
     Cut off the branch of the distribution to the left of coordinate b and
     subsitute it with a single oint at coordinate b holding the
     cumulative probability up to b.
     */
    static DiscreteDistribution functionmax(const DiscreteDistribution& a, const Real& b);
    /*!
     Apply function F to each coordinate.
     */
    template <class F> static DiscreteDistribution function(F&, const DiscreteDistribution& a);
    /*!
     TODO
     */
    static DiscreteDistribution functionmin(const DiscreteDistribution& a, const Real& b);
    /*!
     Return the expected coordinate value.
     */
    static Real expectation(const DiscreteDistribution& a);
    /*!
     Return the standard deviation of the discrete distribution.
*/
    static Real stdev(const DiscreteDistribution& a);
    /*!
     TODO
     */
    static Real leftstdev(const DiscreteDistribution& a);
    /*!
     Print the distribution the the provided stream.
     */
    static Real print(const DiscreteDistribution& a, const ostringstream& o);
    /*!
     Probability matching:

     Compute the cumulative probability P_b(c) of distribution b up to
     the provided coordintae c.

     Compute the coordinate c* of distribution a where its cumulative
     probability equals P_b(c), i.e. P_a(c*) = P_b(c).

     Return coordinate c*.
     */
    static Real probabilitymatch(const DiscreteDistribution& a, const DiscreteDistribution& b, Real c, bool forward);

    //! Probability matching with linear interpolation
    static Real probabilitymatch(const DiscreteDistribution& a, const DiscreteDistribution& b, Real c);
};

template <class F> inline DiscreteDistribution MDD::function(F& f, const DiscreteDistribution& a) {
    vector<Distributionpair> x1pm1 = a.get();
    vector<Distributionpair> func;

    for (Size i = 0; i < x1pm1.size(); i++) {
        Distributionpair xp(f(x1pm1[i].x_), x1pm1[i].y_);
        func.push_back(xp);
    }
    return DiscreteDistribution(func);
}

} // namespace QuantExt

