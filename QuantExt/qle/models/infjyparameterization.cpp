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

#include <qle/models/infjyparameterization.hpp>
#include <ql/utilities/dataformatters.hpp>

using QuantLib::Array;
using QuantLib::Parameter;
using QuantLib::Real;
using QuantLib::Size;
using QuantLib::ZeroInflationIndex;
using QuantLib::ZeroInflationTermStructure;

namespace QuantExt {

InfJyParameterization::InfJyParameterization(
    QuantLib::ext::shared_ptr<Lgm1fParametrization<ZeroInflationTermStructure>> realRate,
    QuantLib::ext::shared_ptr<FxBsParametrization> index,
    QuantLib::ext::shared_ptr<ZeroInflationIndex> inflationIndex)
    : Parametrization(realRate->currency(), realRate->name()),
      realRate_(realRate), index_(index), inflationIndex_(inflationIndex) {}

const Array& InfJyParameterization::parameterTimes(const Size i) const {
    checkIndex(i);
    if (i < 2) {
        return realRate_->parameterTimes(i);
    } else {
        return index_->parameterTimes(0);
    }
}

const QuantLib::ext::shared_ptr<Parameter> InfJyParameterization::parameter(const Size i) const {
    checkIndex(i);
    if (i < 2) {
        return realRate_->parameter(i);
    } else {
        return index_->parameter(0);
    }
}

void InfJyParameterization::update() const {
    realRate_->update();
    index_->update();
}

QuantLib::ext::shared_ptr<Lgm1fParametrization<ZeroInflationTermStructure>> InfJyParameterization::realRate() const {
    return realRate_;
}

QuantLib::ext::shared_ptr<FxBsParametrization> InfJyParameterization::index() const {
    return index_;
}

QuantLib::ext::shared_ptr<ZeroInflationIndex> InfJyParameterization::inflationIndex() const {
    return inflationIndex_;
}

Real InfJyParameterization::direct(const Size i, const Real x) const {
    checkIndex(i);
    if (i < 2) {
        return realRate_->direct(i, x);
    } else {
        return index_->direct(0, x);
    }
}

Real InfJyParameterization::inverse(const Size i, const Real y) const {
    checkIndex(i);
    if (i < 2) {
        return realRate_->inverse(i, y);
    } else {
        return index_->inverse(i, y);
    }
}

void InfJyParameterization::checkIndex(Size i) const {
    QL_REQUIRE(i < 3, "InfJyParameterization has 3 parameters but has been asked for its " << io::ordinal(i + 1));
}

}

