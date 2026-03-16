/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/equitycashflow.hpp
    \brief casflow paying an equity price
    \ingroup cashflows
*/

#pragma once

#include <qle/indexes/equityindex.hpp>

#include <ql/cashflow.hpp>

namespace QuantExt {
using namespace QuantLib;

class EquityCashFlow : public CashFlow {   
public:
    EquityCashFlow(const Date& paymentDate, Real quantity, const Date& fixingDate,
                   const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve)
        : paymentDate_(paymentDate), quantity_(quantity), fixingDate_(fixingDate), equityCurve_(equityCurve) {
        registerWith(equityCurve_);
    }

    Date date() const override { return paymentDate_; }
    Real amount() const override { return quantity_ * equityCurve_->fixing(fixingDate_); }
    Date fixingDate() const { return fixingDate_; }
    Real quantity() const { return quantity_; }
    const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equityCurve() const { return equityCurve_; }
    void update() override { notifyObservers(); }

private:
    Date paymentDate_;
    Real quantity_;
    Date fixingDate_;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equityCurve_;
};

} // namespace QuantExt
