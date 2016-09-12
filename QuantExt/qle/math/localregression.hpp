/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#ifndef quantext_local_regression__hpp
#define quantext_local_regression__hpp

#include <ql/math/interpolations/linearinterpolation.hpp>

#include <boost/make_shared.hpp>

#include <iostream>

namespace QuantExt {

class LocalRegression {
  public:
    LocalRegression(const std::vector<Real> &x, const std::vector<Real> &y, const Real xmin, const Real xmax,
                    const Size buckets, const Size minSize) {

        QL_REQUIRE(x.size() == y.size(), "LocalRegression: x size (" << x.size() << ") must be equal to y size ("
                                                                     << y.size() << ")");

        Real dx = (xmax - xmin) / static_cast<Real>(buckets);

        std::vector<Real> xTmp(buckets), yTmp(buckets, 0.0), yTmp2(buckets, 0.0);
        std::vector<Size> ySize(buckets, 0);

        for (Size idx = 0; idx < buckets; ++idx) {
            xTmp[idx] = xmin + dx * (static_cast<Real>(idx) + 0.5);
        }

        for (Size i = 0; i < y.size(); ++i) {
            int idx = std::min<int>(std::max<int>(static_cast<int>((x[i] - xmin) / dx), 0), buckets - 1);
            yTmp[idx] += y[i];
	    yTmp2[idx] += y[i]*y[i];
            ++ySize[idx];
        }

        x_.clear();
        y_.clear();
	stdev_.clear();
        Size idx = 0;
        while (idx < xTmp.size()) {
            Size finalSize = 0, buckets = 0;
            Real finalY = 0.0, finalX = 0.0, finalY2 = 0.0;
            do {
                finalSize += ySize[idx];
                finalY += yTmp[idx];
                finalX += xTmp[idx];
                finalY2 += yTmp2[idx];
                ++idx;
                ++buckets;
            } while (idx < xTmp.size() && finalSize < minSize);
            if(finalSize >= minSize) {
                x_.push_back(finalX / static_cast<Real>(buckets));
                y_.push_back(finalY / static_cast<Real>(finalSize));
                stdev_.push_back(sqrt(finalY2 / static_cast<Real>(finalSize)
				      - pow(finalY / static_cast<Real>(finalSize), 2)));
            }
        }

        interpolator_ = boost::make_shared<LinearInterpolation>(x_.begin(), x_.end(), y_.begin());
        interpolatorStdev_ = boost::make_shared<LinearInterpolation>(x_.begin(), x_.end(), stdev_.begin());
    }

    Real operator()(const Real x) const {
        Real tmp = std::min(std::max(x, x_.front()), x_.back()); // flat extrapolation
        return interpolator_->operator()(tmp);
    }

    Real stdev(const Real x) const {
        Real tmp = std::min(std::max(x, x_.front()), x_.back()); // flat extrapolation
        return interpolatorStdev_->operator()(tmp);
    }

  private:
    boost::shared_ptr<Interpolation> interpolator_, interpolatorStdev_;
    std::vector<Real> x_, y_, stdev_;
};

} // namespace QuantExt

#endif
