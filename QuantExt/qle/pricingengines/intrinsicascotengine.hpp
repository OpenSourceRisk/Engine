/*
 Copyright (C) 2020 Quaternion Risk Managment Ltd
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

/*! \file qle/pricingengines/intrinsicascotengine.hpp
    \brief intrinsic engine for Ascots
*/

#pragma once

#include <qle/instruments/ascot.hpp>

#include <ql/instruments/payoffs.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

namespace QuantExt {

using namespace QuantLib;

//! Intrinsic engine for Ascots
/*  \ingroup engines

    \test the correctness of the returned value is tested by
          checking it against known results in a few corner cases.
*/

class IntrinsicAscotEngine : public Ascot::engine {
public:
    IntrinsicAscotEngine(const Handle<YieldTermStructure>& discountCurve);
    void calculate() const override;

private:
    Handle<YieldTermStructure> discountCurve_;

};

} // namespace QuantExt
