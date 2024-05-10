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

/*! \file qle/pricingengines/analyticoutperformanceoptionengine.hpp
    \brief Analytic European engine for outperformance options
*/

#ifndef quantext_analytic_outperformance_option_engine_hpp
#define quantext_analytic_outperformance_option_engine_hpp

#include <qle/instruments/outperformanceoption.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <qle/termstructures/correlationtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Pricing engine for European outperformance options using analytical formulae

class AnalyticOutperformanceOptionEngine : public OutperformanceOption::engine {
public:
    AnalyticOutperformanceOptionEngine( const ext::shared_ptr<GeneralizedBlackScholesProcess>& process1,
                const ext::shared_ptr<GeneralizedBlackScholesProcess>& process2,
                const Handle<CorrelationTermStructure>& correlation, QuantLib::Size integrationPoints);
    void calculate() const override;

private:
    Real correlation(Time t, Real strike = 1) const { return correlationCurve_->correlation(t, strike); }
    Real rho(Time t) const { return std::max(std::min(correlation(t), 0.9999), -0.9999); }
    Real integrand(const Real x, Real phi, Real k, Real m1, Real m2, Real v1, Real v2, Real s1, Real s2, Real i1, Real i2, Real fixingTime) const;
    Real getTodaysFxConversionRate(const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex) const;

    class integrand_f;
    friend class integrand_f;

    ext::shared_ptr<GeneralizedBlackScholesProcess> process1_;
    ext::shared_ptr<GeneralizedBlackScholesProcess> process2_;
    
    Handle<CorrelationTermStructure> correlationCurve_;
    QuantLib::Size integrationPoints_;
};
} // namespace QuantExt

#endif
