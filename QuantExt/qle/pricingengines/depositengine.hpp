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

/*! \file depositengine.hpp
    \brief deposit engine
    \ingroup engines
*/

#ifndef quantext_deposit_engine_hpp
#define quantext_deposit_engine_hpp

#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/instruments/deposit.hpp>

namespace QuantExt {
//! Deposit engine
//! \ingroup engines
class DepositEngine : public Deposit::engine {
public:
    DepositEngine(const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                  boost::optional<bool> includeSettlementDateFlows = boost::none, Date settlementDate = Date(),
                  Date npvDate = Date());
    void calculate() const;
    Handle<YieldTermStructure> discountCurve() const { return discountCurve_; }

private:
    Handle<YieldTermStructure> discountCurve_;
    boost::optional<bool> includeSettlementDateFlows_;
    Date settlementDate_, npvDate_;
};
} // namespace QuantExt

#endif
