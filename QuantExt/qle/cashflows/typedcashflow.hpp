/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/typedcashflow.hpp
    \brief simple cashflow with a type
    \ingroup cashflows
*/

#pragma once

#include <ql/cashflows/simplecashflow.hpp>

namespace QuantExt {
using namespace QuantLib;

//! cashflow with the type
class TypedCashFlow : public SimpleCashFlow {
public:
    enum class Type { Interest, Notional, Fee, Premium, Unspecified };

    TypedCashFlow(Real amount, const Date& date, const Type type = Type::Unspecified) : SimpleCashFlow(amount, date), type_(type) {};

    //! \name Inspectors
    //@{
    //! Return the moneyness type
    Type type() const;

private:
    Type type_;
};

//! Enable writing of Type
std::ostream& operator<<(std::ostream& out, const TypedCashFlow::Type& type);

} // namespace QuantExt
