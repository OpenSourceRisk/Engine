/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/cpicashflow.hpp
    \brief An extended cpi cashflow
*/

#ifndef quantext_cpi_cashflow_hpp
#define quantext_cpi_cashflow_hpp

#include <ql/cashflows/cpicoupon.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Cash flow paying the performance of a CPI (zero inflation) index
/*! It is NOT a coupon, i.e. no accruals. */
/*!
 * Normal CPICashflow pays Notional * (I(t) / I(t0) - 1)
 * This CPICashflow pays partal redemptions of form
 *   Notional * (I(t) - I(t-1)) / I(t0)
 */
    
class CPICashFlow : public QuantLib::CPICashFlow {
public:
    CPICashFlow(Real notional,
                const boost::shared_ptr<ZeroInflationIndex>& index,
                Real baseFixing,
                const Date& fixingDate,     // Time t
                const Date& prevFixingDate, // Time t-1
                const Date& paymentDate,
                CPI::InterpolationType interpolation = CPI::AsIndex,
                const Frequency& frequency = QuantLib::NoFrequency)
    :  QuantLib::CPICashFlow (notional, index, Date(), baseFixing, fixingDate, paymentDate, false, interpolation, frequency),
       prevFixingDate_(prevFixingDate) {}

    Date prevFixingDate() { return prevFixingDate_; }

    //! \name CashFlow interface
    //@{
    Real amount() const;
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&);
    //@}

protected:
    Date prevFixingDate_;
};

// inline definitions

inline void CPICashFlow::accept(AcyclicVisitor& v) {
    Visitor<CPICashFlow>* v1 =
    dynamic_cast<Visitor<CPICashFlow>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        IndexedCashFlow::accept(v);
}
}

#endif
