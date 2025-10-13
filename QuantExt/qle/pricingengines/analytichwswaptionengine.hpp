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

/*! \file analytichwswaptionengine.hpp
    \brief Analytic hw swaption engine
*/

#pragma once

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/instruments/swaption.hpp>
#include <qle/models/crossassetmodel.hpp>

namespace QuantExt {

class AnalyticHwSwaptionEngine : public GenericEngine<Swaption::arguments, Swaption::results> {

public:
    AnalyticHwSwaptionEngine(const QuantLib::ext::shared_ptr<HwModel>& model,
                             const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>());

    void calculate() const;

private:
    double time(const QuantLib::Date& d) const;
    Array q(const QuantLib::Time t) const;

    QuantLib::ext::shared_ptr<HwModel> model_;
    QuantLib::ext::shared_ptr<IrHwParametrization> p_;
    Handle<YieldTermStructure> c_;

    mutable std::vector<double> fixedAccrualTimes_, fixedAccrualFractions_, fixedPaymentTimes_;
};

} // namespace QuantExt
