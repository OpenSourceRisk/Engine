/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/termstructures/creditcurve.hpp>
#include <boost/algorithm/string.hpp>
namespace QuantExt {
using namespace QuantLib;

CreditCurve::CreditCurve(const Handle<DefaultProbabilityTermStructure>& curve,
                         const QuantLib::Handle<QuantLib::YieldTermStructure>& rateCurve,
                         const QuantLib::Handle<QuantLib::Quote>& recovery, const RefData& refData)
    : curve_(curve), rateCurve_(rateCurve), recovery_(recovery), refData_(refData) {
    registerWith(curve_);
    registerWith(rateCurve_);
    registerWith(recovery_);
}

void CreditCurve::update() { notifyObservers(); }
const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& CreditCurve::curve() const { return curve_; }
const QuantLib::Handle<QuantLib::YieldTermStructure>& CreditCurve::rateCurve() const { return rateCurve_; }
const QuantLib::Handle<QuantLib::Quote>& CreditCurve::recovery() const { return recovery_; }
const CreditCurve::RefData& CreditCurve::refData() const { return refData_; }


} // namespace QuantExt
