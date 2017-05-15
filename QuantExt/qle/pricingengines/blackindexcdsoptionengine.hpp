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

/*! \file blackindexcdsoptionengine.hpp
    \brief Black credit default swap option engine, with handling
    of upfront amount and exercise before CDS start
*/

#ifndef quantext_black_index_cds_option_engine_hpp
#define quantext_black_index_cds_option_engine_hpp

#include <qle/instruments/indexcdsoption.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Black-formula CDS-option engine
class BlackIndexCdsOptionEngine : public QuantExt::IndexCdsOption::engine {
public:
    // use index curve for front end protection calculation
    BlackIndexCdsOptionEngine(const Handle<DefaultProbabilityTermStructure>&, Real recoveryRate,
                              const Handle<YieldTermStructure>& termStructure, const Handle<Quote>& vol);
    // use underlying curves for front end protection calculation
    BlackIndexCdsOptionEngine(const std::vector<Handle<DefaultProbabilityTermStructure> >&,
                              const std::vector<Real> recoveryRate, const Handle<YieldTermStructure>& termStructure,
                              const Handle<Quote>& vol);
    void calculate() const;
    Handle<YieldTermStructure> termStructure();
    Handle<Quote> volatility();

private:
    Real defaultProbability(const Date& d1, const Date& d2) const;
    Real recoveryRate() const;

    const Handle<DefaultProbabilityTermStructure> probability_;
    const std::vector<Handle<DefaultProbabilityTermStructure> > underlyingProbability_;
    const Real recoveryRate_;
    const std::vector<Real> underlyingRecoveryRate_;
    const Handle<YieldTermStructure> termStructure_;
    const Handle<Quote> volatility_;
    const bool useUnderlyingCurves_;
};
} // namespace QuantExt

#endif
