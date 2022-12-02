/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file analyticdoublebarrierengine.hpp
    \brief Analytic barrier option engines
*/

#ifndef quantext_analytic_double_barrier_engine_hpp
#define quantext_analytic_double_barrier_engine_hpp

#include <ql/experimental/barrieroption/analyticdoublebarrierengine.hpp>
#include <ql/instruments/barrieroption.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantExt {

using namespace QuantLib;

//! Wrapper engine for the QuantLib engine to take settlement delay into account
class AnalyticDoubleBarrierEngine : public QuantLib::AnalyticDoubleBarrierEngine {
public:
    AnalyticDoubleBarrierEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process, const Date& paymentDate,
                                int series = 5);
    void calculate() const override;

private:
    ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
    Date paymentDate_;
};

} // namespace QuantExt

#endif
