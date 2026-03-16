/*
 Copyright (C) 2005, 2006 Theo Boafo
 Copyright (C) 2006, 2007 StatPro Italia srl
 Copyright (C) 2020 Quaternion Risk Managment Ltd

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

/*! \file qle/pricingengines/binomialconvertibleengine.hpp
    \brief binomial engine for convertible bonds
*/

#pragma once

#include <qle/instruments/convertiblebond.hpp>
#include <qle/pricingengines/discretizedconvertible.hpp>
#include <qle/pricingengines/tflattice.hpp>

#include <ql/instruments/payoffs.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

namespace QuantExt {

using namespace QuantLib;

//! Binomial Tsiveriotis-Fernandes engine for convertible bonds
/*  \ingroup hybridengines

    \test the correctness of the returned value is tested by
          checking it against known results in a few corner cases.
*/

template <class T> class BinomialConvertibleEngine : public ConvertibleBond::option::engine {
public:
    BinomialConvertibleEngine(const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                              const Handle<YieldTermStructure>& referenceCurve, const Handle<Quote>& creditSpread,
                              const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                              const Handle<Quote>& recoveryRate, Size timeSteps);
    void calculate() const override;

private:
    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
    Handle<YieldTermStructure> referenceCurve_;
    Handle<Quote> creditSpread_;
    Handle<DefaultProbabilityTermStructure> defaultCurve_;
    Handle<Quote> recoveryRate_;
    Size timeSteps_;
};

} // namespace QuantExt
