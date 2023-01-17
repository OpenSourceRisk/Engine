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

#pragma once

#include <qle/instruments/bondrepo.hpp>

#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

using namespace QuantLib;

//! Discounting Bond Repo Engine
class DiscountingBondRepoEngine : public QuantExt::BondRepo::engine {
public:
    DiscountingBondRepoEngine(const Handle<YieldTermStructure>& repoCurve, const bool includeSecurityLeg = true);

    void calculate() const override;

    const Handle<YieldTermStructure>& repoCurve() const { return repoCurve_; }

private:
    const Handle<YieldTermStructure> repoCurve_;
    const bool includeSecurityLeg_;
};

} // namespace QuantExt
