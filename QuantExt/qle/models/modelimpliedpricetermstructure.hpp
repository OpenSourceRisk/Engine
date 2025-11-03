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

/*! \file modelimpliedpricetermstructure.hpp
    \brief price term structure implied by a COM model
    \ingroup models
*/

#pragma once

#include <qle/models/commoditymodel.hpp>

#include <qle/termstructures/pricetermstructure.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {
using namespace QuantLib;

//! COM Implied Price Term Structure
/*! The termstructure has the reference date of the model's
    termstructure at construction, but you can vary this
    as well as the state.
    The purely time based variant is mainly there for
    perfomance reasons, note that it does not provide the
    full term structure interface and does not send
    notifications on reference time updates.

        \ingroup models
 */

class ModelImpliedPriceTermStructure : public PriceTermStructure {
public:
    ModelImpliedPriceTermStructure(const QuantLib::ext::shared_ptr<CommodityModel>& model, const DayCounter& dc = DayCounter(),
                                   const bool purelyTimeBased = false);

    Date maxDate() const override;
    Time maxTime() const override;
    
    const Date& referenceDate() const override;

    //! Price term structure interface
    const QuantLib::Currency& currency() const override { return model_->currency(); }
    std::vector<QuantLib::Date> pillarDates() const override { return std::vector<QuantLib::Date>(); }

    virtual void referenceDate(const Date& d);
    virtual void referenceTime(const Time t);
    void state(const Array& s);
    void move(const Date& d, const Array& s);
    void move(const Time t, const Array& s);

    virtual void update() override;

protected:
    Real priceImpl(Time t) const override;

    const QuantLib::ext::shared_ptr<CommodityModel> model_;
    const bool purelyTimeBased_;
    Date referenceDate_;
    Real relativeTime_;
    Array state_;
};

// inline

inline Date ModelImpliedPriceTermStructure::maxDate() const {
    // we don't care - let the underlying classes throw
    // exceptions if applicable
    return Date::maxDate();
}

inline Time ModelImpliedPriceTermStructure::maxTime() const {
    // see maxDate
    return QL_MAX_REAL;
}

inline const Date& ModelImpliedPriceTermStructure::referenceDate() const {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    return referenceDate_;
}

inline void ModelImpliedPriceTermStructure::referenceDate(const Date& d) {
    QL_REQUIRE(!purelyTimeBased_, "reference date not available for purely "
                                  "time based term structure");
    referenceDate_ = d;
    update();
}


inline void ModelImpliedPriceTermStructure::referenceTime(const Time t) {
    QL_REQUIRE(purelyTimeBased_, "reference time can only be set for purely "
                                 "time based term structure");
    relativeTime_ = t;
    notifyObservers();
}

inline void ModelImpliedPriceTermStructure::state(const Array& s) {
    state_ = s;
    notifyObservers();
}

inline void ModelImpliedPriceTermStructure::move(const Date& d, const Array& s) {
    state_ = s;
    referenceDate(d);
}

inline void ModelImpliedPriceTermStructure::move(const Time t, const Array& s) {
    state_ = s;
    referenceTime(t);
    notifyObservers();
}

inline void ModelImpliedPriceTermStructure::update() {
    if (!purelyTimeBased_) {
        relativeTime_ = dayCounter().yearFraction(model_->termStructure()->referenceDate(), referenceDate_);
    }
    notifyObservers();
}

inline Real ModelImpliedPriceTermStructure::priceImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "negative time (" << t << ") given");
    return model_->forwardPrice(relativeTime_, relativeTime_ + t, state_);
}

} // namespace QuantExt
