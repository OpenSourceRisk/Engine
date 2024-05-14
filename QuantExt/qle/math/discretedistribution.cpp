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

/*! \file discretedistribution.hpp
 \brief Discretized probability density and cumulative probability
 */

#include <qle/math/discretedistribution.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

DiscreteDistribution::DiscreteDistribution(const vector<Distributionpair>& data) : data_(data) {
    // do checks here
}

DiscreteDistribution::DiscreteDistribution() : data_(1, Distributionpair(0.0, 1.0)) {}

DiscreteDistribution::DiscreteDistribution(const vector<Real>& dataPoints, const vector<Real>& probabilities) {
    QL_REQUIRE(dataPoints.size() == probabilities.size(), "Must be the same number of data points and probabilities");

    for (Size i = 0; i < dataPoints.size(); i++) {
        data_.push_back(Distributionpair(dataPoints[i], probabilities[i]));
    }
}

Size DiscreteDistribution::size() const { return data_.size(); }

vector<Distributionpair> DiscreteDistribution::get() const { return data_; }

Distributionpair DiscreteDistribution::get(Size i) const {
    // checks here
    return data_[i];
}

Real DiscreteDistribution::probability(Size i) const {
    // Check that index i is in bounds
    QL_REQUIRE(0 <= i && i < data_.size(), "Asked for probability outside range of distribution");
    return data_[i].y_;
}

Real DiscreteDistribution::data(Size i) const {
    // Check that index i is in bounds
    QL_REQUIRE(0 <= i && i < data_.size(), "Asked for data point outside range of distribution");
    return data_[i].x_;
}

//-------------------------------------------
DiscreteDistribution MDD::convolve(const DiscreteDistribution& a, const DiscreteDistribution& b, Size buckets) {
    //-------------------------------------------
    vector<Distributionpair> result(buckets);

    vector<Distributionpair> x1pm1 = a.get();
    vector<Distributionpair> x2pm2 = b.get();
    vector<Distributionpair> xpconvtemp;

    for (Size i = 0; i < x1pm1.size(); i++) {
        for (Size j = 0; j < x2pm2.size(); j++) {
            Distributionpair xp(x1pm1[i].x_ + x2pm2[j].x_, x1pm1[i].y_ * x2pm2[j].y_);
            xpconvtemp.push_back(xp);
            // cout << i << " " << x1pm1[i].x_ << " " << x1pm1[i].y_ << " " << x2pm2[j].x_ << " " << x2pm2[j].y_ <<
            // endl;
        }
    }

    std::sort(xpconvtemp.begin(), xpconvtemp.end());
    //---------
    // rebucket
    //---------

    vector<Distributionpair> xpconv(buckets, Distributionpair(0.0, 0.0));

    Real xmin = xpconvtemp.front().x_;
    Real xmax = xpconvtemp.back().x_;
    Real Bucketsize = (xmax - xmin) / buckets;
    // cout << "Bucketsize " << Bucketsize << "xMax "<< xmax<< "xmin "<< xmin <<endl;

    if (xmin == xmax) {
        buckets = 1;
        Bucketsize = 1.;
    }

    for (Size i = 0; i < buckets; i++) {
        xpconv[i].x_ = xmin + i * Bucketsize;
    }

    for (Size j = 0; j < xpconvtemp.size(); j++) {
        Size bucket = static_cast<Size>((xpconvtemp[j].x_ - xmin) / Bucketsize);
        QL_REQUIRE(bucket <= buckets, "Number of buckets in Convolution incorrect");
        if (bucket == buckets) {
            bucket -= 1;
        }

        Real probs = xpconv[bucket].y_ + xpconvtemp[j].y_;

        // deal with zero probability

        if (probs < 1.0e-20) {
            xpconv[bucket].y_ = 0.0;
            xpconv[bucket].x_ += 0.0;
        } else {
            Real intermed = (xpconv[bucket].x_ * xpconv[bucket].y_ + xpconvtemp[j].x_ * xpconvtemp[j].y_) / probs;
            xpconv[bucket].y_ += xpconvtemp[j].y_;
            xpconv[bucket].x_ = intermed;
        }

        /*cout << j << " bucket " << bucket
          << " " << xpconvtemp[j].x_
          << " " << xpconvtemp[j].y_
          << " " << xpconv[bucket].x_
          << " " << xpconv[bucket].y_
          << " Bucketsize " << Bucketsize
          << " xmin "<< xmin
          <<" xmax " << xmax
          << " test "<<xpconv[bucket].x_ - xmin+(bucket)*Bucketsize
          << endl;*/

        /*QL_REQUIRE( (xpconv[bucket].x_ - xmin+(bucket)*Bucketsize > -1.0e-8) and
          ( xmin+(bucket+1)*Bucketsize - xpconv[bucket].x_ >  -1.0e-8 ),"Convolve:: Fell out of bucket "<< bucket)
        */
    }

    /* for (Size i = 0; i < xpconv.size() ;i++){
       cout << i << " " << i*Bucketsize << " " << xpconv[i].x_ << " "<< xpconv[i].y_ <<
       endl;
       }
    */
    return DiscreteDistribution(xpconv);
}

//-------------------------------------------
DiscreteDistribution MDD::rebucketfixednumber(const DiscreteDistribution& a, Size buckets) {
    //-------------------------------------------
    vector<Distributionpair> xptemp = a.get();

    std::sort(xptemp.begin(), xptemp.end());

    Real xmin = xptemp.front().x_;
    Real xmax = xptemp.back().x_;
    Real Bucketsize = (xmax - xmin) / buckets;

    if (xmin == xmax) {
        buckets = 1;
        Bucketsize = 1.;
    }

    //---------
    // rebucket
    //---------

    vector<Distributionpair> xp(buckets, Distributionpair(0.0, 0.0));

    // cout << "Bucketsize " << Bucketsize << "xMax "<< xmax<< "xmin "<< xmin <<endl;

    for (Size i = 0; i < buckets; i++) {
        xp[i].x_ = xmin + i * Bucketsize;
    }

    for (Size j = 0; j < xptemp.size(); j++) {
        Size bucket = static_cast<Size>((xptemp[j].x_ - xmin) / Bucketsize);
        QL_REQUIRE(bucket <= buckets, "Number of buckets in Rebucket incorrect");
        if (bucket == buckets) {
            bucket -= 1;
        }

        Real probs = xp[bucket].y_ + xptemp[j].y_;

        // deal with zero probability

        if (probs < 1.0e-30) {
            xp[bucket].y_ = 0.0;
            xp[bucket].x_ += 0.0;
        } else {
            Real intermed = (xp[bucket].x_ * xp[bucket].y_ + xptemp[j].x_ * xptemp[j].y_) / probs;
            xp[bucket].y_ += xptemp[j].y_;
            xp[bucket].x_ = intermed;
        }
    }

    return DiscreteDistribution(xp);
}

//-------------------------------------------
DiscreteDistribution MDD::rebucketfixedstep(const DiscreteDistribution& a, Real step) {
    //-------------------------------------------
    vector<Distributionpair> xptemp = a.get();

    std::sort(xptemp.begin(), xptemp.end());

    Size buckets;
    Real xmin = xptemp.front().x_;
    Real xmax = xptemp.back().x_;

    if (xmin == xmax) {
        buckets = 1;
    } else {
        buckets = static_cast<Size>(ceil((xmax - xmin) / step));
    }

    //---------
    // rebucket
    //---------

    vector<Distributionpair> xp(buckets, Distributionpair(0.0, 0.0));

    // cout << "Bucketsize " << Bucketsize << "xMax "<< xmax<< "xmin "<< xmin <<endl;

    for (Size i = 0; i < buckets; i++) {
        xp[i].x_ = xmin + i * step;
    }

    for (Size j = 0; j < xptemp.size(); j++) {
        Size bucket = static_cast<Size>((xptemp[j].x_ - xmin) / step);
        QL_REQUIRE(bucket <= buckets, "Number of buckets in Rebucket incorrect");
        if (bucket == buckets) {
            bucket -= 1;
        }

        Real probs = xp[bucket].y_ + xptemp[j].y_;

        // deal with zero probability

        if (probs < 1.0e-30) {
            xp[bucket].y_ = 0.0;
            xp[bucket].x_ += 0.0;
        } else {
            Real intermed = (xp[bucket].x_ * xp[bucket].y_ + xptemp[j].x_ * xptemp[j].y_) / probs;
            xp[bucket].y_ += xptemp[j].y_;
            xp[bucket].x_ = intermed;
        }
    }

    return DiscreteDistribution(xp);
}

//-------------------------------------------
DiscreteDistribution MDD::sum(const DiscreteDistribution& a, const DiscreteDistribution& b, Size buckets) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    vector<Distributionpair> x2pm2 = b.get();
    vector<Distributionpair> xpsumtemp;

    for (Size i = 0; i < x1pm1.size(); i++) {
        Distributionpair xp(x1pm1[i].x_, x1pm1[i].y_);
        xpsumtemp.push_back(xp);
    }

    for (Size i = 0; i < x2pm2.size(); i++) {
        Distributionpair xp(x2pm2[i].x_, x2pm2[i].y_);
        xpsumtemp.push_back(xp);
    }

    std::sort(xpsumtemp.begin(), xpsumtemp.end());
    //---------
    // rebucket
    //---------

    vector<Distributionpair> xpsum(buckets, Distributionpair(0.0, 0.0));

    Real xmin = xpsumtemp.front().x_;
    Real xmax = xpsumtemp.back().x_;
    Real Bucketsize = (xmax - xmin) / buckets;
    // cout << "Bucketsize " << Bucketsize << "xMax "<< xmax<< "xmin "<< xmin <<endl;

    if (xmin == xmax) {
        xpsum[0].x_ = xmin;
        xpsum[0].y_ = 1.0;
    } else {
        for (Size i = 0; i < buckets; i++) {
            xpsum[i].x_ = i * Bucketsize;
        }

        for (Size j = 0; j < xpsumtemp.size(); j++) {
            Size bucket = static_cast<Size>((xpsumtemp[j].x_ - xmin) / Bucketsize);
            QL_REQUIRE(bucket <= buckets, "Number of buckets in Convolution incorrect: " << bucket);
            if (bucket == buckets) {
                bucket -= 1;
            }

            Real probs = xpsum[bucket].y_ + xpsumtemp[j].y_;

            // deal with zero probability

            if (probs < 1.0e-30) {
                xpsum[bucket].y_ = 0.0;
                xpsum[bucket].x_ += 0.0;
            } else {
                Real intermed = (xpsum[bucket].x_ * xpsum[bucket].y_ + xpsumtemp[j].x_ * xpsumtemp[j].y_) / probs;
                xpsum[bucket].y_ += xpsumtemp[j].y_;
                xpsum[bucket].x_ = intermed;
            }

            /*cout << j << " bucket " << bucket << " " <<
             xpsumtemp[j].x_ << " " <<
             xpsumtemp[j].y_ << " " <<
             xpsum[bucket].x_ << " " <<
             xpsum[bucket].y_ <<
             endl; */
        }

        /* for (Size i = 0; i < xpsum.size() ;i++){
         cout << i << " " << i*Bucketsize << " " << xpsum[i].x_ << " "<< xpsum[i].y_ <<
         endl;
         }
         */
    }
    return DiscreteDistribution(xpsum);
}
//-------------------------------------------
Real MDD::probabilitymatch(const DiscreteDistribution& a, const DiscreteDistribution& b, Real c, bool forward) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    vector<Distributionpair> x2pm2 = b.get();
    Real c_ = c;
    Real target(0.);
    std::sort(x2pm2.begin(), x2pm2.end());
    if (forward) {
        std::sort(x1pm1.begin(), x1pm1.end());
    } else {
        std::sort(x1pm1.rbegin(), x1pm1.rend());
    }
    Real cuma = 0.0;
    Real cumb = 0.0;

    for (Size i = 0; i < x2pm2.size(); i++) {
        if (x2pm2[i].x_ <= c_) {
            cumb += x2pm2[i].y_;
        }
    }
    for (Size i = 0; i < x1pm1.size(); i++) {
        cuma += x1pm1[i].y_;
        if (cuma <= cumb) {
            target = x1pm1[i].x_;
        }
    }

    return target;
}

Real MDD::probabilitymatch(const DiscreteDistribution& a, const DiscreteDistribution& b, Real c) {

    // Sort both distributions and calculate their cumulative distributions
    vector<Distributionpair> aData = a.get();
    vector<Distributionpair> bData = b.get();
    sort(aData.begin(), aData.end());
    sort(bData.begin(), bData.end());

    // Find P_b(c)
    Real probability = 0.0;
    Distributionpair dummy(c);
    vector<Distributionpair>::iterator itVar = lower_bound(bData.begin(), bData.end(), dummy);
    if (itVar == bData.end()) {
        for (Size i = 0; i < bData.size(); i++)
            probability += bData[i].y_;
    } else if (itVar == bData.begin()) {
        probability = itVar->y_;
    } else {
        Size startIndex = itVar - bData.begin() - 1;
        for (Size i = 0; i <= startIndex; i++)
            probability += bData[i].y_;
        probability += (c - bData[startIndex].x_) * itVar->y_ / (itVar->x_ - bData[startIndex].x_);
    }

    // Find target such that P_a(target) = P_b(c)
    Real sum = 0.0;
    vector<Real> aCumulative(aData.size());
    vector<Real> aValues(aData.size());
    for (Size i = 0; i < aData.size(); i++) {
        sum += aData[i].y_;
        aCumulative[i] = sum;
        aValues[i] = aData[i].x_;
    }

    vector<Real>::iterator itProb = lower_bound(aCumulative.begin(), aCumulative.end(), probability);
    if (itProb == aCumulative.end()) {
        return aValues.back();
    } else if (itProb == aCumulative.begin()) {
        return aValues.front();
    } else {
        Size low = itProb - aCumulative.begin() - 1;
        Real target = aValues[low];
        target += (aValues[low + 1] - aValues[low]) * (probability - aCumulative[low]) /
                  (aCumulative[low + 1] - aCumulative[low]);
        return target;
    }
}

//-------------------------------------------
DiscreteDistribution MDD::sumspecialunsorted(const DiscreteDistribution& a, const DiscreteDistribution& b, Real c) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    vector<Distributionpair> x2pm2 = b.get();
    Real c_ = c;
    vector<Distributionpair> xpsumtemp;

    // std::sort (x1pm1.begin(), x1pm1.end());
    // std::sort (x2pm2.begin(), x2pm2.end());

    Real cumLast = 0.0;
    Real cumNow = 0.0;

    for (Size i = 0; i < x2pm2.size(); i++) {
        cumNow += x2pm2[i].y_;
        Real cumX1 = 0.0;
        for (Size j = 0; j < x1pm1.size(); j++) {
            cumX1 += x1pm1[j].y_;
            if ((cumX1 > cumLast) && (cumX1 <= cumNow)) {
                x1pm1[j].x_ += x2pm2[i].x_ * c_;
            }
        }
        // cout<<"Cum X1"<< cumX1<<" Cum now" << cumNow<<endl;
        // QL_REQUIRE(  cumNow <= cumX1, "Cumulative probability of Distribution b exceeds a");
        cumLast = cumNow;
    }
    return x1pm1;
}

//-------------------------------------------
DiscreteDistribution MDD::sumspecial(const DiscreteDistribution& a, const DiscreteDistribution& b, Real c) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    vector<Distributionpair> x2pm2 = b.get();
    Real c_ = c;
    vector<Distributionpair> xpsumtemp;

    std::sort(x1pm1.begin(), x1pm1.end());
    std::sort(x2pm2.begin(), x2pm2.end());

    Real cumLast = 0.0;
    Real cumNow = 0.0;

    for (Size i = 0; i < x2pm2.size(); i++) {
        cumNow += x2pm2[i].y_;
        Real cumX1 = 0.0;
        for (Size j = 0; j < x1pm1.size(); j++) {
            cumX1 += x1pm1[j].y_;
            if ((cumX1 >= cumLast) && (cumX1 < cumNow)) {
                x1pm1[j].x_ += x2pm2[i].x_ * c_;
            }
        }
        // cout<<"Cum X1"<< cumX1<<" Cum now" << cumNow<<endl;
        // QL_REQUIRE(  cumNow <= cumX1, "Cumulative probability of Distribution b exceeds a");
        cumLast = cumNow;
    }
    return x1pm1;
}

//-------------------------------------------
DiscreteDistribution MDD::sumspecialright(const DiscreteDistribution& a, const DiscreteDistribution& b, Real c) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    vector<Distributionpair> x2pm2 = b.get();
    Real c_ = c;
    vector<Distributionpair> xpsumtemp;

    std::sort(x1pm1.begin(), x1pm1.end());
    std::sort(x2pm2.begin(), x2pm2.end());

    Real cumLast = 0.0;
    Real cumNow = 0.0;

    for (Size i = x2pm2.size(); i > 0; i--) {
        cumNow += x2pm2[i - 1].y_;
        Real cumX1 = 0.0;
        for (Size j = x1pm1.size(); j > 0; j--) {
            cumX1 += x1pm1[j - 1].y_;
            if ((cumX1 >= cumLast) && (cumX1 < cumNow)) {
                x1pm1[j - 1].x_ += x2pm2[i - 1].x_ * c_;
            }
        }
        // cout<<"Cum X1"<< cumX1<<" Cum now" << cumNow<<endl;
        // QL_REQUIRE(  cumNow <= cumX1, "Cumulative probability of Distribution b exceeds a");
        cumLast = cumNow;
    }
    return x1pm1;
}

//-------------------------------------------
DiscreteDistribution MDD::splicemezz(const DiscreteDistribution& a, const DiscreteDistribution& b, Real kR) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    vector<Distributionpair> x2pm2 = b.get();
    vector<Distributionpair> xpsumtemp;
    Real probmNNzero = 0.;
    Real probKicker = 0.;

    // Splices together the mezz and equity distributions to form the kicker

    for (Size i = 0; i < x1pm1.size(); i++) {

        Distributionpair xp(x1pm1[i].x_, x1pm1[i].y_);
        if (x1pm1[i].x_ >= 0) {
            xpsumtemp.push_back(xp);
        } else {
            probmNNzero += x1pm1[i].y_;
        }
    }

    for (Size i = 0; i < x2pm2.size(); i++) {
        Distributionpair xp((1. - kR) * x2pm2[i].x_, x2pm2[i].y_);
        if (x2pm2[i].x_ < 0) {
            xpsumtemp.push_back(xp);
            probKicker += x2pm2[i].y_;
        }
    }
    Distributionpair xp(0.0, probmNNzero - probKicker);
    QL_REQUIRE(xp.y_ >= 0, "Problem with probabilities in Mezz Splice");
    xpsumtemp.push_back(xp);

    std::sort(xpsumtemp.begin(), xpsumtemp.end());

    return DiscreteDistribution(xpsumtemp);
}

//-------------------------------------------
DiscreteDistribution MDD::scalarmultprob(const DiscreteDistribution& a, const Real& b) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    Real b_ = b;
    vector<Distributionpair> scalarmult;

    for (Size i = 0; i < x1pm1.size(); i++) {

        Distributionpair xp(x1pm1[i].x_, x1pm1[i].y_ * b_);
        scalarmult.push_back(xp);
    }
    return DiscreteDistribution(scalarmult);
}

//-------------------------------------------
DiscreteDistribution MDD::scalarmultx(const DiscreteDistribution& a, const Real& b) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    Real b_ = b;
    vector<Distributionpair> scalarmult;

    for (Size i = 0; i < x1pm1.size(); i++) {

        Distributionpair xp(b_ * x1pm1[i].x_, x1pm1[i].y_);
        scalarmult.push_back(xp);
    }
    return DiscreteDistribution(scalarmult);
}

//-------------------------------------------
DiscreteDistribution MDD::scalarshiftx(const DiscreteDistribution& a, const Real& b) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    Real b_ = b;
    vector<Distributionpair> scalarmult;

    for (Size i = 0; i < x1pm1.size(); i++) {

        Distributionpair xp(x1pm1[i].x_ + b_, x1pm1[i].y_);
        scalarmult.push_back(xp);
    }
    return DiscreteDistribution(scalarmult);
}

//-------------------------------------------
DiscreteDistribution MDD::functionmax(const DiscreteDistribution& a, const Real& b) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    Real b_ = b;
    vector<Distributionpair> func;

    std::sort(x1pm1.begin(), x1pm1.end());

    Real temp = 0.;

    for (Size i = 0; i < x1pm1.size(); i++) {
        if (x1pm1[i].x_ <= b_) {
            temp += x1pm1[i].y_;
        }
    }
    Distributionpair xp(b_, temp);
    func.push_back(xp);

    for (Size i = 0; i < x1pm1.size(); i++) {
        if (x1pm1[i].x_ > b_) {
            Distributionpair xp(max(x1pm1[i].x_, b_), x1pm1[i].y_);
            func.push_back(xp);
        }
    }

    return DiscreteDistribution(func);
}

//-------------------------------------------
DiscreteDistribution MDD::functionmin(const DiscreteDistribution& a, const Real& b) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    Real b_ = b;
    vector<Distributionpair> func;

    std::sort(x1pm1.begin(), x1pm1.end());

    for (Size i = 0; i < x1pm1.size(); i++) {
        if (x1pm1[i].x_ < b_) {
            Distributionpair xp(min(x1pm1[i].x_, b_), x1pm1[i].y_);
            func.push_back(xp);
        }
    }

    Real temp = 0.;

    for (Size i = 0; i < x1pm1.size(); i++) {
        if (x1pm1[i].x_ >= b_) {
            temp += x1pm1[i].y_;
        }
    }
    Distributionpair xp(b_, temp);
    func.push_back(xp);

    return DiscreteDistribution(func);
}

//-------------------------------------------
Real MDD::expectation(const DiscreteDistribution& a) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    Real exp = 0.0;

    for (Size i = 0; i < x1pm1.size(); i++) {

        exp += x1pm1[i].x_ * x1pm1[i].y_;
    }
    return exp;
}

//-------------------------------------------
Real MDD::stdev(const DiscreteDistribution& a) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    Real mu = MDD::expectation(a);
    Real variance = 0.0;

    for (Size i = 0; i < x1pm1.size(); i++) {

        variance += pow((x1pm1[i].x_ - mu), 2) * x1pm1[i].y_;
    }
    Real stdev = sqrt(variance);
    return stdev;
}

//-------------------------------------------
Real MDD::leftstdev(const DiscreteDistribution& a) {
    //-------------------------------------------

    vector<Distributionpair> x1pm1 = a.get();
    Real mu = MDD::expectation(a);
    Real variance = 0.0;

    for (Size i = 0; i < x1pm1.size(); i++) {
        if (x1pm1[i].x_ - mu < 0.0)
            variance += pow((x1pm1[i].x_ - mu), 2) * x1pm1[i].y_;
    }
    Real stdev = sqrt(variance);
    return stdev;
}

//-------------------------------------------
Real MDD::print(const DiscreteDistribution& a, const ostringstream& o) {
    //-------------------------------------------

    ofstream file;
    file.open(o.str().c_str());
    if (!file.is_open())
        QL_FAIL("error opening file " << o.str());
    file.setf(ios::scientific, ios::floatfield);
    file.setf(ios::showpoint);
    file.precision(4);
    for (Size k = 0; k < a.size(); k++) {
        file << k << " " << a.get(k).x_ << " " << a.get(k).y_ << endl;
    }
    file.close();

    return 0;
}

} // namespace QuantExt
