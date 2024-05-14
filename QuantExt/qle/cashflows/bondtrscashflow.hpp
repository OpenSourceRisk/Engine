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

/*! \file qle/cashflows/bondtrscashflow.hpp
    \brief cashflow paying the total return of a bond
    \ingroup cashflows
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/handle.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounter.hpp>
#include <qle/cashflows/trscashflow.hpp>
#include <qle/indexes/bondindex.hpp>
#include <qle/indexes/fxindex.hpp>

namespace QuantExt {
using namespace QuantLib;

//! bond trs cashflow
class BondTRSCashFlow : public TRSCashFlow {
public:
    BondTRSCashFlow(const Date& paymentDate, const Date& fixingStartDate, const Date& fixingEndDate,
                    const Real bondNotional, const QuantLib::ext::shared_ptr<BondIndex>& bondIndex,
                    const Real initialPrice = Null<Real>(), const QuantLib::ext::shared_ptr<FxIndex>& fxIndex = nullptr);

    const Real notional(Date date) const override;
    const Real notional() const override { return TRSCashFlow::notional(); };

    void setFixingStartDate(QuantLib::Date fixingDate);
};

//! helper class building a sequence of bond trs cashflows
/*! \ingroup cashflows
 */
class BondTRSLeg {
public:
    BondTRSLeg(const std::vector<Date>& valuationDates, const std::vector<Date>& paymentDates, const Real bondNotional,
               const QuantLib::ext::shared_ptr<BondIndex>& bondIndex, const QuantLib::ext::shared_ptr<FxIndex>& fxIndex = nullptr);
    BondTRSLeg& withInitialPrice(Real);
    operator Leg() const;

private:
    std::vector<Date> valuationDates_;
    std::vector<Date> paymentDates_;
    Real bondNotional_;
    QuantLib::ext::shared_ptr<BondIndex> bondIndex_;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    Real initialPrice_ = QuantLib::Null<QuantLib::Real>();
};

} // namespace QuantExt
