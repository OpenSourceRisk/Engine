/*
 Copyright (C) 2024 AcadiaSoft Inc.
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

#include <ql/experimental/inflation/interpolatedyoyoptionletstripper.hpp>
namespace QuantExt {

class YoYOptionletStripperSolverWithFallBack : public QuantLib::YoYOptionletBaseSolver {
public:
    YoYOptionletStripperSolverWithFallBack(double minVol = 1e-8, double maxVol = 0.3, size_t steps = 10);

    double solveForImpliedVol(QuantLib::YoYInflationCapFloor::Type type, QuantLib::Real slope, QuantLib::Rate K,
                              QuantLib::Period& lag, QuantLib::Natural fixingDays,
                              const QuantLib::ext::shared_ptr<QuantLib::YoYInflationIndex>& anIndex,
                              const QuantLib::ext::shared_ptr<QuantLib::YoYCapFloorTermPriceSurface>& surf,
                              QuantLib::ext::shared_ptr<QuantLib::YoYInflationCapFloorEngine> p,
                              QuantLib::Real priceToMatch) const override;

private:
    QuantLib::YoYOptionletSolver solver_;
    double minVol_;
    double maxVol_;
    size_t steps_;
};
} // namespace QuantExt