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

#include <qle/models/cirppimplieddefaulttermstructure.hpp>

namespace QuantExt {

CirppImpliedDefaultTermStructure::CirppImpliedDefaultTermStructure(
    const QuantLib::ext::shared_ptr<CrCirpp>& model, const Size index,
    const DayCounter& dc, const bool purelyTimeBased)
    : SurvivalProbabilityStructure(dc == DayCounter() ? model->defaultCurve()->dayCounter() : dc),
      model_(model), index_(index), purelyTimeBased_(purelyTimeBased),
      referenceDate_(purelyTimeBased ? Null<Date>() : model_->defaultCurve()->referenceDate()), y_(0.0) {
    registerWith(model_);
    update();
}

} // namespace QuantExt
