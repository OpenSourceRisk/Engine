/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <orepbase/qle/models/annuitymapping.hpp>

namespace QuantExt {

Real AnnuityMapping::mapPrime(const Real S) const { return (map(S + h_) - map(S - h_)) / (2.0 * h_); }
Real AnnuityMapping::mapPrime2(const Real S) const { return (map(S + h_) - 2.0 * map(S) + map(S - h_)) / (h_ * h_); }

void AnnuityMappingBuilder::update() { notifyObservers(); }

} // namespace QuantExt
