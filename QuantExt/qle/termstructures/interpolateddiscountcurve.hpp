/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
 */

/*! \file interpolateddiscountcurve.hpp
    \brief interpolated discount term structure
*/

#ifndef quantext_interpolated_discount_curve_hpp
#define quantext_interpolated_discount_curve_hpp

#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! InterpolatedDiscountCurve as in QuantLib, but with
  floating discount quotes and floating reference date,
  reference date is always the global evaluation date,
  i.e. settlement days are zero and calendar is NullCalendar() */
class InterpolatedDiscountCurve : public YieldTermStructure, public LazyObject {
  public:
    //! \name Constructors
    //@{
    //! default constructor
    InterpolatedDiscountCurve(const std::vector<Time> &times,
                              const std::vector<Handle<Quote> > &quotes,
                              const DayCounter &dc)
        : YieldTermStructure(dc), times_(times), quotes_(quotes),
          data_(times_.size(), 1.0), today_(Settings::instance().evaluationDate()) {
        for (Size i = 0; i < quotes.size(); ++i) {
            QL_REQUIRE(times_.size() > 1, "at least two times required");
            QL_REQUIRE(times_.size() == quotes.size(),
                       "size of time and quote vectors do not match");
            QL_REQUIRE(times_[0] == 0.0, "First time must be 0, got "
                                             << times_[0]);
            QL_REQUIRE(!quotes[i].empty(), "quote at index " << i
                                                             << " is empty");
            registerWith(quotes[i]);
        }
        interpolation_ = boost::make_shared<LogLinearInterpolation>(
            times_.begin(), times_.end(), data_.begin());
        registerWith(Settings::instance().evaluationDate());
    }
    //@}

    Date maxDate() const { return Date::maxDate(); }
    void update() {
        LazyObject::update();
        updated_ = false;
    }
    const Date &referenceDate() const {
        calculate();
        return today_;
    }

    Calendar calendar() const { return NullCalendar(); }
    Natural settlementDays() const { return 0; }

  protected:
    void performCalculations() const {
        today_ = Settings::instance().evaluationDate();
        for (Size i = 0; i < times_.size(); ++i) {
            data_[i] = quotes_[i]->value();
        }
        interpolation_->update();
    }

    DiscountFactor discountImpl(Time t) const {
        calculate();
        if (t <= this->times_.back())
            return (*interpolation_)(t, true);
        // flat fwd extrapolation
        Time tMax = this->times_.back();
        DiscountFactor dMax = this->data_.back();
        Rate instFwdMax = -(*interpolation_).derivative(tMax) / dMax;
        return dMax * std::exp(-instFwdMax * (t - tMax));
    }

  private:
    std::vector<Time> times_;
    std::vector<Handle<Quote> > quotes_;
    mutable std::vector<Real> data_;
    mutable Date today_;
    boost::shared_ptr<Interpolation> interpolation_;
};

} // namespace QuantExt

#endif
