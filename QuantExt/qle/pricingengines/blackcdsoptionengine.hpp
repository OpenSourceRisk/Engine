/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*
 Copyright (C) 2008 Roland Stamm
 Copyright (C) 2009 Jose Aparicio

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file blackcdsoptionengine.hpp
    \brief Black credit default swap option engine, with handling
    of upfront amount and exercise before CDS start
    \ingroup engines
*/

#ifndef quantext_black_cds_option_engine_hpp
#define quantext_black_cds_option_engine_hpp

#include <qle/instruments/cdsoption.hpp>

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Black-formula CDS-option engine base class
//! \ingroup engines
class BlackCdsOptionEngineBase {
public:
    BlackCdsOptionEngineBase(const Handle<YieldTermStructure>& termStructure, const Handle<BlackVolTermStructure>& vol);
    virtual ~BlackCdsOptionEngineBase() {}
    void calculate(const CreditDefaultSwap& swap, const Date& exerciseDate, const bool knocksOut,
                   CdsOption::results& results) const;
    Handle<YieldTermStructure> termStructure();
    Handle<BlackVolTermStructure> volatility();

protected:
    virtual Real recoveryRate() const = 0;
    virtual Real defaultProbability(const Date& d) const = 0;

    const Handle<YieldTermStructure> termStructure_;
    const Handle<BlackVolTermStructure> volatility_;
};

//! Black-formula CDS-option engine
class BlackCdsOptionEngine : public QuantExt::CdsOption::engine, public BlackCdsOptionEngineBase {
public:
    BlackCdsOptionEngine(const Handle<DefaultProbabilityTermStructure>&, Real recoveryRate,
                         const Handle<YieldTermStructure>& termStructure, const Handle<BlackVolTermStructure>& vol);
    void calculate() const;

private:
    virtual Real recoveryRate() const;
    virtual Real defaultProbability(const Date& d) const;

    const Handle<DefaultProbabilityTermStructure> probability_;
    const Real recoveryRate_;
};
} // namespace QuantExt

#endif
