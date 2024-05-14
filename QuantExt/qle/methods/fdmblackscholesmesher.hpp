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

/*! \file fdmblackscholesmesher.hpp
    \brief extended version of the QuantLib class, see the documentation for details

*/

#pragma once

#include <ql/instruments/dividendschedule.hpp>
#include <ql/methods/finitedifferences/meshers/fdm1dmesher.hpp>

#include <ql/handle.hpp>
#include <ql/quote.hpp>

#include <ql/tuple.hpp>

namespace QuantLib {
class FdmQuantoHelper;
class YieldTermStructure;
class GeneralizedBlackScholesProcess;
} // namespace QuantLib

namespace QuantExt {
using namespace QuantLib;

/* 1-d mesher for the Black-Scholes process (in ln(S)), extension of ql class
   - allowing for several cPoints
   - cPoints outside constructed grid are ignored */

class FdmBlackScholesMesher : public Fdm1dMesher {
public:
    FdmBlackScholesMesher(Size size, const ext::shared_ptr<GeneralizedBlackScholesProcess>& process, Time maturity,
                          Real strike, Real xMinConstraint = Null<Real>(), Real xMaxConstraint = Null<Real>(),
                          Real eps = 0.0001, Real scaleFactor = 1.5,
                          const std::vector<QuantLib::ext::tuple<Real, Real, bool>>& cPoints = {},
                          const DividendSchedule& dividendSchedule = DividendSchedule(),
                          const ext::shared_ptr<FdmQuantoHelper>& fdmQuantoHelper = ext::shared_ptr<FdmQuantoHelper>(),
                          Real spotAdjustment = 0.0);

    static ext::shared_ptr<GeneralizedBlackScholesProcess> processHelper(const Handle<Quote>& s0,
                                                                         const Handle<YieldTermStructure>& rTS,
                                                                         const Handle<YieldTermStructure>& qTS,
                                                                         Volatility vol);
};

} // namespace QuantExt
