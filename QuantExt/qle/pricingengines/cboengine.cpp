/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file cbo.cpp
    \brief collateralized bond obligation pricing engine
*/

#include <fstream>
#include <iostream>
#include <stdio.h>

#include <qle/pricingengines/cboengine.hpp>

namespace QuantExt {

Stats::Stats(std::vector<Real> data) : data_(data), mean_(0.0), std_(0.0), max_(0.0), min_(0.0) {
    for (Size i = 0; i < data_.size(); i++) {
        mean_ += data_[i];
        std_ += data_[i] * data_[i];
    }
    mean_ /= data_.size();
    std_ /= data_.size();
    std_ = sqrt(std_ - mean_ * mean_);
    max_ = *(std::max_element(data_.begin(), data_.end()));
    min_ = *(std::min_element(data_.begin(), data_.end()));
}

Distribution Stats::histogram(Size bins, Real xmin, Real xmax) {
    Distribution dist(bins, std::max(min_, xmin), std::min(max_, xmax));
    for (Size i = 0; i < data_.size(); i++) {
        if (data_[i] > xmax)
            data_[i] = xmax;
        if (data_[i] < xmin)
            data_[i] = xmin;
        dist.add(data_[i]);
    }
    dist.normalize();
    return dist;
}

void print(Distribution& dist, std::string fileName) {
    std::ofstream file;
    file.open(fileName.c_str());
    if (!file.is_open())
        QL_FAIL("error opening file " << fileName);
    file.setf(std::ios::scientific, std::ios::floatfield);
    file.setf(std::ios::showpoint);
    file.precision(4);
    for (Size i = 0; i < dist.size(); i++)
        file << i << " " << dist.x(i) << " " << dist.density(i) << std::endl;
    file.close();
}

} // namespace QuantExt
