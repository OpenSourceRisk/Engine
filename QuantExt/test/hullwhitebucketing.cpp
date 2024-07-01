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

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <oret/datapaths.hpp>

#include <qle/models/hullwhitebucketing.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/integrals/all.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/experimental/credit/lossdistribution.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/experimental/credit/distribution.hpp>
#include <boost/math/distributions/binomial.hpp>
#include <iostream>
#include <fstream>
#include <boost/timer/timer.hpp>

using namespace QuantLib;
using namespace QuantExt;

using std::fixed;
using std::setprecision;
using std::string;
using std::vector;

namespace test_hullwhitebucketing {

void computeDiscreteDistribution(std::vector<std::vector<double>>::iterator beginPDs,
                    std::vector<std::vector<double>>::iterator endPDs,
                    std::vector<std::vector<double>>::iterator beginLGDs, double runningDensity, double runningLoss,
                    std::map<double, double>& dist) {
    if (beginPDs == endPDs) {
        dist[runningLoss] = dist[runningLoss]  + runningDensity;
    } else {
        for (size_t eventId = 0; eventId < beginPDs->size(); eventId++) {
            double loss = runningLoss + beginLGDs->at(eventId);
            double density = runningDensity * beginPDs->at(eventId);
            computeDiscreteDistribution(beginPDs + 1, endPDs, beginLGDs + 1, density, loss, dist);
        }
    }
}

std::map<double, double> lossDistribution(const std::vector<double>& pds, const std::vector<double>& lgds) {

    QL_REQUIRE(pds.size() == lgds.size(), "Mismatch number of pds and lgds");

    vector<vector<double>> statePDs;
    vector<vector<double>> stateLGDs;
    for (const auto& pd : pds) {
        statePDs.push_back({pd, 1 - pd});
    }
    for (const auto& lgd : lgds) {
        stateLGDs.push_back({lgd, 0});
    }
    std::map<double, double> dist;
    computeDiscreteDistribution(statePDs.begin(), statePDs.end(), stateLGDs.begin(), 1.0, 0.0, dist);
    return dist;
}

std::map<double, double> lossDistribution(const std::vector<vector<double>>& pds,
                                      const std::vector<vector<double>>& lgds) {

    QL_REQUIRE(pds.size() == lgds.size(), "Mismatch number of pds and lgds");

    vector<vector<double>> statePDs;
    vector<vector<double>> stateLGDs;
    for (const auto& pd : pds) {
        statePDs.push_back(pd);
        statePDs.back().push_back(1.0-std::accumulate(pd.begin(), pd.end(), 0.0));
    }
    for (const auto& lgd : lgds) {
        stateLGDs.push_back(lgd);
        stateLGDs.back().push_back(0);
    }

    std::map<double, double> dist;
    computeDiscreteDistribution(statePDs.begin(), statePDs.end(), stateLGDs.begin(), 1.0, 0.0, dist);
    return dist;
}

struct BucketedDistribution {
    BucketedDistribution(const HullWhiteBucketing& hwb, const std::map<double, double>& dist) 
        : upperBound(hwb.upperBucketBound()), p(hwb.buckets(), 0.0), A(hwb.buckets(), 0.0){
        lowerBound.push_back(QL_MIN_REAL);
        lowerBound.insert(lowerBound.end(), upperBound.begin(), upperBound.end()-1);
        
        for (const auto& [l, d] : dist) {
            size_t idx = hwb.index(l);
            if (!close_enough(d, 0.0)) {
                A[idx] = (A[idx] * p[idx] + d * l) / (d + p[idx]);
                p[idx] = p[idx] + d;
            }
        }
    }

    BucketedDistribution(const HullWhiteBucketing& hwb)
        : upperBound(hwb.upperBucketBound()), p(hwb.probability().begin(), hwb.probability().end()),
          A(hwb.averageLoss().begin(), hwb.averageLoss().end()) {
        lowerBound.push_back(QL_MIN_REAL);
        lowerBound.insert(lowerBound.end(), upperBound.begin(), upperBound.end()-1);
    }
    
    std::vector<double> lowerBound;
    std::vector<double> upperBound;
    vector<double> p;
    vector<double> A;

    QuantLib::Distribution lossDistribution(size_t nBuckets, Real minLoss, Real maxLoss) {
        QuantLib::Distribution dist(nBuckets, minLoss, maxLoss); 
        // Add from pd and losses [min, min+dx), ... [max-dx, max)
        for (size_t i = 0; i < nBuckets; i++) {
            dist.addDensity(i, p[i+1] / dist.dx(i));
            dist.addAverage(i,A[i+1]);
        }
        return dist;
    }
    friend std::ostream& operator<<(std::ostream& os, const BucketedDistribution& dist);
};

std::ostream& operator<<(std::ostream& os, const BucketedDistribution& dist) {
    os << "#\tLB\tUB\tPD\tAvg" << std::endl;
    for (Size i = 0; i < dist.A.size(); ++i) {

        os << i << "\t" << dist.lowerBound[i] << "\t" << dist.upperBound[i] << "\t" << dist.p[i] << "\t"
           << dist.A[i] << std::endl;
    }
    return os;
}

double expectedTrancheLoss(const std::map<double, double>& dist, double detachmentPoint) {
    double expectedLoss = 0.0;
    for (const auto& [L, pd] : dist) {
        if (L <= detachmentPoint) {
            expectedLoss += L * pd;
        } else {
            expectedLoss += detachmentPoint * pd;
        }
    }
    return expectedLoss;
}

double expectedTrancheLoss(QuantLib::Distribution dist, double attachmentAmount, double detachmentAmount) {
    QuantLib::Real expectedLoss = 0;
    dist.normalize();
    for (QuantLib::Size i = 0; i < dist.size(); i++) {
        // Real x = dist.x(i) + dist.dx(i)/2; // in QL distribution.cpp
        QuantLib::Real x = dist.average(i);
        if (x < attachmentAmount)
            continue;
        if (x > detachmentAmount)
            break;
        expectedLoss += (x - attachmentAmount) * dist.dx(i) * dist.density(i);
    }
    expectedLoss += (detachmentAmount - attachmentAmount) * (1.0 - dist.cumulativeDensity(detachmentAmount));
    return expectedLoss;
}


double expectedTrancheLoss(const vector<double>& prob, const vector<double>& loss, double detachment) {
    QuantLib::Real expectedLoss = 0;
    for (size_t i = 0; i < prob.size(); ++i) {
        expectedLoss += std::min(loss[i], detachment) * prob[i];
    }
    return expectedLoss;
}


}
BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(HullWhiteBucketingTestSuite)

BOOST_AUTO_TEST_CASE(testHullWhiteBucketing) {

    BOOST_TEST_MESSAGE("Testing Hull White Bucketing...");

    boost::timer::cpu_timer timer;
    
    Size N = 100; // buckets
    Size L = 100; // obligors
    Real pd = 0.01;

    std::vector<Real> buckets;
    for (Size i = 0; i <= N; ++i) {
        buckets.push_back(0.5 + static_cast<Real>(i));
    }

    std::vector<Real> pds(L, pd), losses(L, 1.0);

    HullWhiteBucketing hw(buckets.begin(), buckets.end());
    hw.compute(pds.begin(), pds.end(), losses.begin());

    const Array& p = hw.probability();
    const Array& A = hw.averageLoss();

    boost::math::binomial ref(L, pd);

    for (Size i = 0; i < Size(std::min(Size(p.size()),Size(15))); ++i) {
        if (i < p.size() - 1) {
            BOOST_TEST_MESSAGE("Bucket " << i << " ..." << hw.upperBucketBound()[i] << ": p = " << p[i]
                                         << " A = " << A[i] << " ref = " << boost::math::pdf(ref, i));
            BOOST_CHECK_CLOSE(p[i], boost::math::pdf(ref, i), 1E-10);
            BOOST_CHECK_CLOSE(A[i], static_cast<Real>(i), 1E-10);
        } else {
            BOOST_TEST_MESSAGE("Bucket " << i << " ..." << hw.upperBucketBound()[i] << ": p = " << p[i]
                                         << " A = " << A[i] << " ref = " << 1.0 - boost::math::cdf(ref, i - 1));
            BOOST_CHECK_CLOSE(p[i], 1.0 - boost::math::cdf(ref, i - 1), 1E-6);
        }
    }

    BOOST_TEST_MESSAGE("Elapsed: " << timer.elapsed().wall * 1e-9);
}

BOOST_AUTO_TEST_CASE(testHullWhiteBucketingQuantLib) {

    BOOST_TEST_MESSAGE("Testing Hull White Bucketing in QuantLib...");

    boost::timer::cpu_timer timer;
    
    Size N = 100; // buckets
    Size L = 100; // obligors
    Real pd = 0.01;

    // L buckets of width 1
    QuantLib::ext::shared_ptr<QuantLib::LossDist> bucketing = QuantLib::ext::make_shared<QuantLib::LossDistBucketing>(N+1, N+1);
    
    std::vector<Real> pds(L, pd), losses(L, 1.0);

    QuantLib::Distribution dist = (*bucketing)(losses, pds);

    boost::math::binomial ref(L, pd);

    for (Size i = 0; i <= Size(std::min(Size(N),Size(15))); ++i) {
        Real p = dist.density(i) * dist.dx(i);
        Real A = dist.average(i);
        Real x = dist.x(i);
        BOOST_TEST_MESSAGE("Bucket " << i << " ..." << x << ": p = " << p
                           << " A = " << A << " ref = " << boost::math::pdf(ref, i));
        BOOST_CHECK_CLOSE(p, boost::math::pdf(ref, i), 1E-10);
        BOOST_CHECK_CLOSE(A, static_cast<Real>(i), 1E-10);
    }

    BOOST_TEST_MESSAGE("Elapsed: " << timer.elapsed().wall * 1e-9);
}

BOOST_AUTO_TEST_CASE(testHullWhiteBucketingMultiState) {
    BOOST_TEST_MESSAGE("Testing Multi State Hull White Bucketing...");

    Size N = 10;  // buckets
    Size L = 100; // obligors

    std::vector<Real> buckets;
    for (Size i = 0; i <= N; ++i) {
        buckets.push_back(0.5 + static_cast<Real>(i));
    }

    std::vector<Real> pd = {0.01, 0.02};
    std::vector<Real> l = {1.0, 2.0};
    std::vector<std::vector<Real>> pds(L, pd), losses(L, l);

    HullWhiteBucketing hw(buckets.begin(), buckets.end());
    hw.computeMultiState(pds.begin(), pds.end(), losses.begin());

    const Array& p = hw.probability();
    const Array& A = hw.averageLoss();

    // generate reference results with monte carlo
    Array ref(p.size(), 0.0);
    MersenneTwisterUniformRng mt(42);
    Size paths = 1000000;
    for (Size k = 0; k < paths; ++k) {
        Real loss = 0.0;
        for (Size ll = 0; ll < L; ++ll) {
            Real r = mt.nextReal();
            if (r < pd[0])
                loss += l[0];
            else if (r < pd[0] + pd[1])
                loss += l[1];
        }
        Size idx = hw.index(loss);
        ref[idx] += 1.0;
    }
    ref /= paths;

    // check against reference results
    for (Size i = 0; i < p.size(); ++i) {
        Real diff = p[i] - ref[i];
        BOOST_TEST_MESSAGE("Bucket " << i << " ..." << hw.upperBucketBound()[i] << ": p = " << p[i] << " A = " << A[i]
                           << " ref = " << ref[i] << " pdiff " << std::scientific << diff);
        BOOST_CHECK_CLOSE(p[i], ref[i], 1.5);
        if (i < p.size() - 1)
            BOOST_CHECK_CLOSE(A[i], static_cast<Real>(i), 1E-10);
    }
}

BOOST_AUTO_TEST_CASE(testHullWhiteBucketingMultiStateEdgeCase) {
    BOOST_TEST_MESSAGE("Testing Multi State Hull White Bucketing, edge case with different probabilities and identical losses...");

    Size N = 10;  // buckets
    Size L = 100; // obligors

    std::vector<Real> buckets;
    for (Size i = 0; i <= N; ++i) {
        buckets.push_back(0.5 + static_cast<Real>(i));
    }

    std::vector<Real> pd = {0.005, 0.01, 0.005};
    std::vector<Real> l = {1.0, 1.0, 1.0};
    std::vector<std::vector<Real>> pds(L, pd), losses(L, l);

    HullWhiteBucketing hw(buckets.begin(), buckets.end());
    hw.computeMultiState(pds.begin(), pds.end(), losses.begin());

    const Array& p = hw.probability();
    const Array& A = hw.averageLoss();

    // Equivalent single state
    std::vector<Real> pds1(L, 0.02);
    std::vector<Real> losses1(L, 1.0);

    HullWhiteBucketing hw1(buckets.begin(), buckets.end());
    hw1.compute(pds1.begin(), pds1.end(), losses1.begin());

    const Array& p1 = hw1.probability();
    const Array& A1 = hw1.averageLoss();

    // check consistency
    for (Size i = 0; i < p.size(); ++i) {
        Real diffp = p[i] - p1[i];
        Real diffA = A[i] - A1[i];
        BOOST_TEST_MESSAGE("Bucket " << i << " ..." << hw.upperBucketBound()[i] << ": p = " << p[i] << " A = " << A[i]
                           << " pdiff " << std::scientific << diffp  << " Adiff = " << diffA);
        BOOST_CHECK_CLOSE(p[i], p1[i], 0.01);
        if (i < p.size() - 1) {
            BOOST_CHECK_CLOSE(A[i], static_cast<Real>(i), 1E-10);
            BOOST_CHECK_CLOSE(A1[i], static_cast<Real>(i), 1E-10);
        }
    }
}

BOOST_AUTO_TEST_CASE(testHullWhiteBucketingMultiStateExpectedLoss) {
    BOOST_TEST_MESSAGE("Testing Multi State Hull White Bucketing, expected loss...");

    Size N = 80;  // buckets
    Size L = 100; // obligors

    std::vector<Real> buckets;
    for (Size i = 0; i <= N; ++i) {
        buckets.push_back(0.5 + static_cast<Real>(i));
    }

    std::vector<Real> pd = {0.02, 0.01, 0.02};
    std::vector<Real> l1 = {2.0, 2.0, 2.0};
    std::vector<Real> l2 = {1.0, 2.0, 3.0};
    std::vector<std::vector<Real>> pds(L, pd), losses1(L, l1), losses2(L, l2);

    HullWhiteBucketing hw1(buckets.begin(), buckets.end());
    hw1.computeMultiState(pds.begin(), pds.end(), losses1.begin());
    const Array& p1 = hw1.probability();
    const Array& A1 = hw1.averageLoss();

    HullWhiteBucketing hw2(buckets.begin(), buckets.end());
    hw2.computeMultiState(pds.begin(), pds.end(), losses2.begin());
    const Array& p2 = hw2.probability();
    const Array& A2 = hw2.averageLoss();

    Real el1 = 0.0, el2 = 0.0;
    for (Size i = 0; i < p1.size(); ++i) {
        el1 += p1[i] * A1[i];
        el2 += p2[i] * A2[i];
    }
    
    for (Size i = 0; i < p1.size(); ++i) {
        BOOST_TEST_MESSAGE("Bucket " << i << " ..." << hw1.upperBucketBound()[i] << ":"
                           << std::scientific << " p = " << p1[i] << " " << p2[i] << " " << p1[i] - p2[i]);
    }
    // check consistency
    // the different loss levels change the loss distribution, but the balanced setup should keep expected losses unchanged
    BOOST_TEST_MESSAGE("Expected loss: " << std::scientific << el1 << " " << el2 << " " << el1 - el2);
    BOOST_CHECK_CLOSE(el1, el2, 1e-12);
}

Real expectedTrancheLoss(Real attach, Real detach, const Array& p, const Array& cumu, const Array& loss) {
    Real expectedLoss = 0;
    QL_REQUIRE((p.size() == cumu.size()) && (p.size() == loss.size()), "array size mismatch");
    Size i;
    for (i = 0; i < p.size(); ++i) {
        Real x = loss[i];
        if (x < attach)
            continue;
        if (x > detach)
            break;
        expectedLoss += (x - attach) * p[i];
    }
    expectedLoss += (detach - attach) * (1.0 - cumu[i-1]);
    return expectedLoss;
}

void run_case(std::vector<Real> l, std::string fileName, double detachmentRatio = 0.2) {
    QL_REQUIRE(l.size() == 3, "three losses required");
    // loss sizes in increasing order
    for (Size i = 1; i < l.size(); ++i)
        QL_REQUIRE(l[i] >= l[i-1], "increasing losses required");

    BOOST_TEST_MESSAGE("Testing Multi State Hull White Bucketing, expected tranche loss for stylized CDX: " << l[0] << " " << l[1] << " " << l[2]);
        
    Size bucketsFullBasket = 400; // buckets
    Size bucketsTranche = 100;
    Size L = 100; // obligors with notional 1 each
    Real rho = 0.75;
    Real attachmentRatio = 0.0;    
    Real a = attachmentRatio * L;
    Real d = detachmentRatio * L;
    Real pd0 = 0.04;                                            // same PD across all entities, 0.01 for IG, 0.04 for HY    
    Real cutoff = 1.0 * L;                                      // just beyond detachment point is sufficient for tranche expectations only
    std::vector<Real> pd = {pd0 * 0.35, pd0 * 0.3, pd0 * 0.35}; // Markit 2020 grid
    // std::vector<Real> l_1 = {0.6, 0.6, 0.6};                    // rr 0.4
    // std::vector<Real> l_2 = {0.9, 0.6, 0.3};                    // rr 0.1, 0.4, 0.7
    // std::vector<Real> l_3 = {0.3, 0.6, 0.9};                    // rr 0.7, 0.4, 0.1 (to be used according to Krekel & Markit)
    // std::vector<Real> l = l_1;                                  // pick one of the loss vectors
    std::vector<std::vector<Real>> pds(L, pd), losses(L, l);
   
    // Marginal loss distribution, i.e. rho = 0
    
    HullWhiteBucketing hw0(0.0, cutoff, bucketsFullBasket);
    hw0.computeMultiState(pds.begin(), pds.end(), losses.begin());
    Array p0 = hw0.probability();
    Array A0 = hw0.averageLoss();

    // Compute thresholds of the Gaussian Copula model

    InverseCumulativeNormal ICN;
    CumulativeNormalDistribution CN;
    std::vector<std::vector<Real>> c(L, std::vector<Real>(pd.size() + 1, 0.0));
    std::vector<std::vector<Real>> q(L, std::vector<Real>(pd.size() + 1, pd0));
    for (Size i = 0; i < c.size(); ++i) {
        c[i][0] = ICN(q[i][0]);
        Real sum = 0.0;
        // Size np = pd.size();
        for (Size j = 0; j < pd.size(); ++j) {
            sum += pd[j] / pd0;
            q[i][j+1] = q[i][0] * (1.0 - sum);
            if (close_enough(q[i][j+1], 0.0))
                    c[i][j+1] = QL_MIN_REAL;
                else
                    c[i][j+1] = ICN(q[i][j+1]);
        }
        QL_REQUIRE(fabs(q[i].back()) < 1e-10, "expected zero qij, but found " << q[i].back() << " for i=" << i); 
    }

    Size mSteps = 63;
    Real mMin = -5.0;
    Real mMax = 5.0;
    Real dm = (mMax - mMin) / mSteps;
    std::vector<std::vector<Real>> cpds = pds; // just to allocate the correct size

    Array p(p0.size(), 0.0);
    Array A(A0.size(), 0.0);

    Array pTranche(bucketsTranche + 2, 0.0);
    Array ATranche(bucketsTranche + 2, 0.0);

    Array pref(p0.size(), 0.0);
    Real norm = 0.0;
    double trancheLossMC = 0; 
    for (Size k = 0; k <= mSteps; ++k) { // copula loop
        BOOST_TEST_MESSAGE("Copula loop " << k << "/" << mSteps); 
        Real m = mMin + dm * k;
        Real mDensity = exp(-m * m / 2) / sqrt(2.0 * M_PI);
        norm += mDensity * dm;
        
        // Compute conditional PDs

        for (Size i = 0; i < c.size(); i++) {
            Real cpd0 = CN((c[i][0] - sqrt(rho) * m) / sqrt(1.0 - rho));
            Real sum = 0.0;
            for (Size j = 1; j < c[i].size(); ++j) {
                // this ordering is consistent also assumes that cpds[i][0] is associated with the largest loss
                cpds[i][j-1] = CN((c[i][j-1]-sqrt(rho)*m)/sqrt(1.0-rho)) - CN((c[i][j]-sqrt(rho)*m)/sqrt(1.0-rho));
                sum += cpds[i][j-1];
            }
            QL_REQUIRE(fabs(sum - cpd0) < 1e-10, "probability check failed for factor " << m << ": " << sum << " vs " << cpd0);
        }

        // Loss distribution conditional on m - bucketing

        HullWhiteBucketing hwm(0.0, cutoff, bucketsFullBasket);
        hwm.computeMultiState(cpds.begin(), cpds.end(), losses.begin());
        Array pm = hwm.probability();
        Array Am = hwm.averageLoss();
        
        HullWhiteBucketing hwmTranche(0.0, d, bucketsTranche);
        hwmTranche.computeMultiState(cpds.begin(), cpds.end(), losses.begin());
        Array pmTranche = hwmTranche.probability();
        Array AmTranche = hwmTranche.averageLoss();


        // Loss distribution conditional on m - Monte Carlo
        Array pmc(pm.size(), 0.0);
        MersenneTwisterUniformRng mt(42);
        Size paths = 50000;
        double mLossMC = 0;
        for (Size kk = 0; kk < paths; ++kk) {
            Real loss = 0.0;
            for (Size ll = 0; ll < L; ++ll) {
                Real r = mt.nextReal();
                Real sum = 0.0;
                Size n = cpds[ll].size();
                for (Size mm = 0; mm < cpds[ll].size(); ++mm) {
                    sum += cpds[ll][n - 1 - mm];
                    if (r < sum) {
                        loss += losses[ll][n - 1 - mm];
                        break;
                    }
                }
            }
            mLossMC += std::min(loss, d);
            Size idx = hwm.index(loss);
            pmc[idx] += 1.0;
        }
        pmc /= paths;
        trancheLossMC += mLossMC * dm * mDensity / paths;
        // Aggregate
        for (Size j = 0; j < p.size(); j++) {
            BOOST_CHECK_MESSAGE(Am[j] >= 0.0, "averageLoss[" << j << "] " <<  Am[j] << " at k=" << k);
            BOOST_CHECK_MESSAGE(pm[j] >= 0.0 && pm[j] <= 1.0, "probability[" << j << "] " <<  pm[j] << " at k=" << k);                
            p[j] += pm[j] * mDensity * dm;
            A[j] += Am[j] * mDensity * dm;
           
            pref[j] += pmc[j] * mDensity * dm;
        }
        for (Size j = 0; j < pTranche.size(); ++j) {
            pTranche[j] += pmTranche[j] * mDensity * dm;
            ATranche[j] += AmTranche[j] * mDensity * dm;
        }

    }
    BOOST_CHECK_CLOSE(norm, 1.0, 0.1);
    



    Real el0 = 0.0, el = 0.0, /*elref = 0.0, */sum0 = 0.0, sum = 0.0;
    
    Distribution refDistribution(bucketsFullBasket, 0, cutoff);
    Distribution hwDistribution(bucketsFullBasket, 0, cutoff);
    for (Size i = 0; i < bucketsFullBasket; i++) {
        hwDistribution.addDensity(i, p[i + 1] / hwDistribution.dx(i));
        hwDistribution.addAverage(i, A[i + 1]);
        refDistribution.addDensity(i, pref[i + 1] / hwDistribution.dx(i));
        refDistribution.addAverage(i, A[i + 1]);
    }
    
    Distribution hwDistributionTranche(bucketsTranche, a, d);
    for (Size i = 0; i < bucketsTranche; i++) {
        hwDistributionTranche.addDensity(i, pTranche[i + 1] / hwDistributionTranche.dx(i));
        hwDistributionTranche.addAverage(i, ATranche[i + 1]);
    }

    double calculatedLossTrancheHWFullBucketing = test_hullwhitebucketing::expectedTrancheLoss(hwDistribution, a, d);
    double calculatedLossTrancheHWTrancheBucketing =
        test_hullwhitebucketing::expectedTrancheLoss(hwDistributionTranche, a, d);

    BOOST_TEST_MESSAGE("Expected tranche loss (MC) " << trancheLossMC);
    BOOST_TEST_MESSAGE("Calculated tranche loss (HW bucketing over full basket) "
                       << calculatedLossTrancheHWFullBucketing);
    BOOST_TEST_MESSAGE("Calculated tranche loss (HW bucketing of the tranche) "
                       << calculatedLossTrancheHWTrancheBucketing);

    BOOST_CHECK_CLOSE(trancheLossMC, calculatedLossTrancheHWFullBucketing, 0.25);
    BOOST_CHECK_CLOSE(trancheLossMC, calculatedLossTrancheHWTrancheBucketing, 0.25);
    
    // Calculate full basket expected loss
    for (Size i = 0; i < p.size(); ++i) {  
        el0 += p0[i] * A0[i];
        el += p[i] * A[i];
        sum0 += p0[i];
        sum += p[i];
    }

    std::ofstream file;
    if (fileName != "")
        file.open(fileName.c_str());

    Array cumu0(p0.size(), 0.0);
    Array cumu(p.size(), 0.0);
    Array cumuref(p.size(), 0.0);

    for (Size i = 0; i < p.size(); ++i) {
        cumu0[i] = (i == 0 ? p0[0] : p0[i] + cumu0[i-1]);
        cumu[i] = (i == 0 ? p[0] : p[i] + cumu[i-1]);
        cumuref[i] = (i == 0 ? pref[0] : pref[i] + cumuref[i-1]);
        if (file.is_open())
            file << i << " " << std::scientific
                 << A0[i] << " " << p0[i] << " " << cumu0[i] << " "
                 << A[i] << " " << p[i] << " " << cumu[i] << " "
                 << A[i] << " " << pref[i] << " " << cumuref[i] << std::endl;
    }
    if (file.is_open()) {
        file << "# pd0: " << pd0 << std::endl
             << "# losses: " << l[0] << " " << l[1] << " " << l[2] << std::endl
             << "# attachment point: " << attachmentRatio << std::endl
             << "# detachment point: " << detachmentRatio << std::endl
             << "# correlation: " << rho << std::endl
             << "# Expected basket loss, marginal:            " << el0 << std::endl
             << "# Expected basket loss, correlated:          " << el << std::endl
             << "# Expected tranche loss, marginal:           " << expectedTrancheLoss(a, d, p0, cumu0, A0) << std::endl
             << "# Expected tranche loss, correlated (full):  " << calculatedLossTrancheHWFullBucketing << std::endl 
             << "# Expected tranche loss, correlated:          " << calculatedLossTrancheHWTrancheBucketing << std::endl
             << "# Expected tranche loss, correlated, ref:    " << trancheLossMC << std::endl;
        file.close();
    }
    
    BOOST_TEST_MESSAGE("pd: " << pd0);
    BOOST_TEST_MESSAGE("losses: " << l[0] << " " << l[1] << " " << l[2]);
    BOOST_TEST_MESSAGE("attachment point: " << attachmentRatio);
    BOOST_TEST_MESSAGE("detachment point: " << detachmentRatio);
    BOOST_TEST_MESSAGE("correlation: " << rho);
    BOOST_TEST_MESSAGE("Expected basket loss, marginal:         " << el0);
    BOOST_TEST_MESSAGE("Expected basket loss, correlated:       " << el);
    BOOST_TEST_MESSAGE("# Expected tranche loss, correlated (full):  " << calculatedLossTrancheHWFullBucketing);
    BOOST_TEST_MESSAGE("# Expected tranche loss, correlated:          " << calculatedLossTrancheHWTrancheBucketing);
    BOOST_TEST_MESSAGE("# Expected tranche loss, correlated, ref:    " << trancheLossMC);
    BOOST_CHECK_CLOSE(sum0, 1.0, 1e-4);
    BOOST_CHECK_CLOSE(sum, 1.0, 1e-4);
    BOOST_CHECK_CLOSE(el0, el, 1.0);
}

BOOST_AUTO_TEST_CASE(testHullWhiteBucketingMultiStateExpectedTrancheLoss) {
    
    run_case({0.6, 0.6, 0.6}, "data_1.txt", 0.03);
    run_case({0.3, 0.6, 0.9}, "data_2.txt", 0.07); // 40% recovery CDX/iTraxx IG
    run_case({0.3, 0.6, 0.9}, "data_3.txt", 0.15); // 40% recovery CDX/iTraxx IG
    run_case({0.5, 0.7, 0.9}, "data_5.txt", 0.35); // 30% recovery CDX HY
    //run_case({0.76, 0.6, 0.2}, "data_4.txt"); // asymmetric
}

BOOST_AUTO_TEST_CASE(testHullWhiteBucketingNonEqualPDs) {
    BOOST_TEST_MESSAGE("Testing Hull White Bucketing with different PDs...");

    Size N = 5; // buckets

    double lowerlimit = 0;
    double upperlimit = 5;

    std::vector<Real> pds{0.1, 0.1, 0.05, 0.1, 0.05};
    std::vector<Real> losses{1.0, 1.0, 1.0, 1.0, 1.0};

    HullWhiteBucketing hw(lowerlimit, upperlimit, N);
    hw.compute(pds.begin(), pds.end(), losses.begin());

    std::vector<Real> expectedPDs;
    auto p = hw.probability();
    auto A = hw.averageLoss();
    std::vector<double> ref = {0., 0.6579225000000, 0.2885625000000, 0.0492750000000, 0.0040750000000, 0.0001625000000,
                               0.0000025000000};
    std::vector<double> refA = {0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
    for (Size i = 0; i < hw.buckets(); ++i) {
        BOOST_TEST_MESSAGE("Bucket " << (i==0 ? QL_MIN_REAL : hw.upperBucketBound()[i-1]) << " ..." << hw.upperBucketBound()[i] << ": p = " << p[i] << " A = " << A[i]
                                     << " ref = " << ref[i]);
        BOOST_CHECK_CLOSE(p[i], ref[i], 0.01);
        BOOST_CHECK_CLOSE(A[i], refA[i], 0.01);
    }
}

BOOST_AUTO_TEST_CASE(testHullWhiteBucketingSingleStateExpectedLossNonHomogenous) {
    using namespace test_hullwhitebucketing;
    BOOST_TEST_MESSAGE("Testing Multistate Hull White Bucketing Inhomogeneous Portfolio");
    Size N = 20; // buckets
    double lowerlimit = 0;
    std::vector<Real> pds {
        0.0125, 0.0093, 0.0106, 0.0095, 0.0077, 0.0104, 0.0075, 0.0117, 0.0078, 0.009,
        0.0092, 0.0088, 0.0107, 0.0085, 0.0089, 0.0115, 0.0092, 0.0093, 0.012,  0.0102
    };
    std::vector<Real> lgds{
        0.45, 0.41, 0.35, 0.39, 0.39, 0.35, 0.42, 0.39, 0.45, 0.37,
        0.4,  0.39, 0.42, 0.37, 0.36, 0.44, 0.44, 0.42, 0.38, 0.42
    };
    size_t nObligors = pds.size();
    BOOST_TEST_MESSAGE("number of obligors " << nObligors);
    BOOST_TEST_MESSAGE("number of Buckets " << N);
    std::map<double, double> excactDistribution = lossDistribution(pds, lgds);
    for (auto detachmentPoint : {0.03, 0.07, 0.15, 0.35}) {
        double upperlimit = static_cast<double>(nObligors) * detachmentPoint;
        BOOST_TEST_MESSAGE("detachment point " << detachmentPoint);
        BOOST_TEST_MESSAGE("upperLimit " << upperlimit);
        HullWhiteBucketing hw(lowerlimit, upperlimit, N);
        double expectedLoss = test_hullwhitebucketing::expectedTrancheLoss(excactDistribution, upperlimit);
        hw.compute(pds.begin(), pds.end(), lgds.begin());
        BucketedDistribution hwBucketing(hw);
        double calculatedLoss = test_hullwhitebucketing::expectedTrancheLoss(hwBucketing.p, hwBucketing.A, upperlimit);
        BOOST_TEST_MESSAGE("Expected Loss " << expectedLoss << " and calculated loss " << calculatedLoss);
        BOOST_CHECK_CLOSE(expectedLoss, calculatedLoss, 1e-4);
    }
}

BOOST_AUTO_TEST_CASE(testHullWhiteBucketingMultiStateExpectedLossNonHomogenous) {

    using namespace test_hullwhitebucketing;

    BOOST_TEST_MESSAGE("Testing Multistate Hull White Bucketing Inhomogeneous Portfolio");
    Size N = 20; // buckets
    double lowerlimit = 0.0;
    std::vector<std::vector<Real>> pds{
        {0.0238, 0.0079}, {0.0223, 0.0074}, {0.0293, 0.0098}, {0.0106, 0.0035}, {0.012, 0.004},
        {0.0175, 0.0058}, {0.0129, 0.0043}, {0.0091, 0.003},  {0.014, 0.0047},  {0.0138, 0.0046},
        {0.023, 0.0077},  {0.0299, 0.01},   {0.0183, 0.0061}, {0.0291, 0.0097}
    };
    std::vector<std::vector<Real>> lgds{
        {0.44, 0.48}, {0.34, 0.37}, {0.46, 0.51}, {0.47, 0.52}, {0.3, 0.33},  {0.42, 0.46}, {0.33, 0.36},
        {0.3, 0.33},  {0.3, 0.33},  {0.42, 0.46}, {0.38, 0.42}, {0.4, 0.44},  {0.38, 0.42}, {0.44, 0.48}
      
    };
    size_t nObligors = pds.size();
    BOOST_TEST_MESSAGE("number of obligors " << nObligors);
    BOOST_TEST_MESSAGE("number of Buckets " << N);
    std::map<double, double> excactDistribution = lossDistribution(pds, lgds);
    for (auto detachmentPoint : {0.03, 0.07, 0.15, 0.35}) {
        double upperlimit = static_cast<double>(nObligors) * detachmentPoint;
        BOOST_TEST_MESSAGE("detachment point " << detachmentPoint);
        BOOST_TEST_MESSAGE("upperLimit " << upperlimit);
        HullWhiteBucketing hw(lowerlimit, upperlimit, N);
        double expectedLoss = test_hullwhitebucketing::expectedTrancheLoss(excactDistribution, upperlimit);
        hw.computeMultiState(pds.begin(), pds.end(), lgds.begin());
        BucketedDistribution hwBucketing(hw);
        double calculatedLoss = test_hullwhitebucketing::expectedTrancheLoss(hwBucketing.p, hwBucketing.A, upperlimit);
        BOOST_TEST_MESSAGE("Expected Loss " << expectedLoss << " and calculated loss " << calculatedLoss);
        BOOST_CHECK_CLOSE(expectedLoss, calculatedLoss, 1e-4);
    }
}

BOOST_AUTO_TEST_CASE(testBucketingIndex) {
    Size N = 5; // buckets
    double upperlimit = 5;
    BOOST_TEST_MESSAGE("Testing uniform bucket indexing");
    HullWhiteBucketing hw(0.0, upperlimit, N);
    std::map<double, size_t> testCases1{{-1, 0}, {0., 1}, {0.99, 1}, {1.0, 2}, {1.75, 2}, {2.0, 3}};

    for (const auto& [value, expectedIndex] : testCases1) {
        BOOST_CHECK_EQUAL(expectedIndex, hw.index(value));
    }

    // non uniform buckets
    BOOST_TEST_MESSAGE("Testing non uniform bucket indexing");
    std::vector<Real> buckets{0., 0.25, 0.3, 0.5, 1};

    HullWhiteBucketing hw2(buckets.begin(), buckets.end());

    std::map<double, size_t> testCases2{{-0.01, 0}, {0., 1}, {0.125, 1}, {0.25, 2}, {0.275, 2}, {0.3, 3}, {1.1, 5}};

    for (const auto& [value, expectedIndex] : testCases2) {
        BOOST_CHECK_EQUAL(expectedIndex, hw2.index(value));
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
