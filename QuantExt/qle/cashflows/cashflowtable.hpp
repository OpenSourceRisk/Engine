/*
 Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
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

/*! \file qle/cashflows/cashflowtable.hpp
    \brief Cashflow table to store cashflow calculation results
*/

#ifndef quantext_cashflow_table_hpp
#define quantext_cashflow_table_hpp

#include <vector>

#include <ql/qldefines.hpp>
#include <ql/time/date.hpp>

namespace QuantExt {

//! Class representing the row of a cashflow table
class CashflowRow {
public:
    //! Default constructor sets all variables to Null value
    CashflowRow();
    /** \name Initialisers
     *  Functions to initialise the members
     */
    //@{
    CashflowRow& withStartDate(const QuantLib::Date& startDate);
    CashflowRow& withEndDate(const QuantLib::Date& endDate);
    CashflowRow& withStartNotional(QuantLib::Real startNotional);
    CashflowRow& withEndNotional(QuantLib::Real endNotional);
    CashflowRow& withCouponAmount(QuantLib::Real couponAmount);
    CashflowRow& withAllInRate(QuantLib::Rate allInRate);
    CashflowRow& withRate(QuantLib::Rate rate);
    CashflowRow& withSpread(QuantLib::Spread spread);
    CashflowRow& withDiscount(QuantLib::DiscountFactor discount);
    //@}
    /** \name Inspectors
     *  Functions to retrieve the members
     */
    //@{
    const QuantLib::Date& startDate() const { return startDate_; }
    const QuantLib::Date& endDate() const { return endDate_; }
    QuantLib::Real startNotional() const { return startNotional_; }
    QuantLib::Real endNotional() const { return endNotional_; }
    QuantLib::Real couponAmount() const { return couponAmount_; }
    QuantLib::Rate allInRate() const { return allInRate_; }
    QuantLib::Rate rate() const { return rate_; }
    QuantLib::Spread spread() const { return spread_; }
    QuantLib::DiscountFactor discount() const { return discount_; }
    //@}
private:
    QuantLib::Date startDate_;
    QuantLib::Date endDate_;
    QuantLib::Real startNotional_;
    QuantLib::Real endNotional_;
    QuantLib::Real couponAmount_;
    QuantLib::Rate allInRate_;
    QuantLib::Rate rate_;
    QuantLib::Spread spread_;
    QuantLib::DiscountFactor discount_;
};

//! Class representing the contents of a cashflow table
class CashflowTable {
public:
    //! Default constructor to create empty table
    CashflowTable() {}
    //! Add a row to the cashflow table
    void add(const CashflowRow& cashflowRow);
    //! Return the number of rows in the cashflow table
    QuantLib::Size size() const;
    //! Retrieve row \p i from the cashflow table
    const CashflowRow& operator[](QuantLib::Size i) const;
    CashflowRow& operator[](QuantLib::Size i);

private:
    std::vector<CashflowRow> rows_;
};
} // namespace QuantExt

#endif
