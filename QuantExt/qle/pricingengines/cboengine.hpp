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

/*! \file cboengine.hpp
    \brief collateralized bond obligation pricing engine
*/

#pragma once

#include <qle/instruments/cbo.hpp>
#include <ql/experimental/credit/distribution.hpp>
#include <ql/experimental/credit/randomdefaultmodel.hpp>
#include <ql/math/distributions/bivariatenormaldistribution.hpp>
#include <ql/math/distributions/normaldistribution.hpp>

namespace QuantExt {
using namespace QuantLib;

//--------------------------------------------------------------------------
//! CBO base engine
class CBO::engine : public GenericEngine<CBO::arguments, CBO::results> {
protected:
    // virtual Real expectedTrancheLoss(const Date&) const = 0;
    virtual void initialize() const { remainingBasket_ = this->arguments_.basket; }
    mutable QuantLib::ext::shared_ptr<BondBasket> remainingBasket_;
};

//--------------------------------------------------------------------------
//! helper class for the MonteCarloCBOEngine
class Stats {
public:
    Stats(std::vector<Real> data);
    Real mean();
    Real std();
    Real max();
    Real min();
    std::vector<Real>& data();
    Distribution histogram(Size bins, Real xmin = -QL_MAX_REAL, Real xmax = QL_MAX_REAL);

private:
    std::vector<Real> data_;
    Real mean_;
    Real std_;
    Real max_;
    Real min_;
};

inline Real Stats::mean() { return mean_; }

inline Real Stats::std() { return std_; }

inline Real Stats::max() { return max_; }

inline Real Stats::min() { return min_; }

inline std::vector<Real>& Stats::data() { return data_; }

} // namespace QuantExt
