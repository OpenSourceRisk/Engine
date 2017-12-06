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
 Copyright (C) 2008 Jose Aparicio
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2008, 2009 StatPro Italia srl

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

/*! \file midpointcdsengine.hpp
    \brief Mid-point engine for credit default swaps
    \ingroup engines
*/

#ifndef quantext_mid_point_cds_engine_hpp
#define quantext_mid_point_cds_engine_hpp

#include <qle/instruments/creditdefaultswap.hpp>

using namespace QuantLib;

namespace QuantExt {
//! Mid point CDS engine base
//! \ingroup engines
class MidPointCdsEngineBase {
public:
    MidPointCdsEngineBase(const Handle<YieldTermStructure>& discountCurve,
                          boost::optional<bool> includeSettlementDateFlows)
        : discountCurve_(discountCurve), includeSettlementDateFlows_(includeSettlementDateFlows) {}
    virtual ~MidPointCdsEngineBase() {}

protected:
    virtual Real survivalProbability(const Date& d) const = 0;
    virtual Real defaultProbability(const Date& d1, const Date& d2) const = 0;
    virtual Real expectedLoss(const Date& defaultDate, const Date& d1, const Date& d2, const Real notional) const = 0;
    void calculate(const Date& refDate, const CreditDefaultSwap::arguments& arguments,
                   CreditDefaultSwap::results& results) const;

    Handle<YieldTermStructure> discountCurve_;
    boost::optional<bool> includeSettlementDateFlows_;
};

//! Mid point CDS engine
//! \ingroup engines
class MidPointCdsEngine : public CreditDefaultSwap::engine, public MidPointCdsEngineBase {
public:
    MidPointCdsEngine(const Handle<DefaultProbabilityTermStructure>&, Real recoveryRate,
                      const Handle<YieldTermStructure>& discountCurve,
                      boost::optional<bool> includeSettlementDateFlows = boost::none);
    void calculate() const;

protected:
    virtual Real survivalProbability(const Date& d) const;
    virtual Real defaultProbability(const Date& d1, const Date& d2) const;
    virtual Real expectedLoss(const Date& defaultDate, const Date& d1, const Date& d2, const Real notional) const;

    Handle<DefaultProbabilityTermStructure> probability_;
    Real recoveryRate_;
};
} // namespace QuantExt

#endif
