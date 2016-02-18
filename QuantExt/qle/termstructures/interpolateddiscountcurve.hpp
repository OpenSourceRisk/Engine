/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
 */

/*! \file interpolateddiscountcurve.hpp
    \brief interpolated discount term structure
*/

#ifndef quantext_interpolated_discount_curve_hpp
#define quantext_interpolated_discount_curve_hpp

#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/quotes/logquote.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

    /*! InterpolatedDiscountCurve based on loglinear interpolation of DiscountFactors,
        flat fwd extrapolation is always enabled, the term structure has always a
        floating reference date */
    class InterpolatedDiscountCurve : public YieldTermStructure {
    public:
        //! \name Constructors
        //@{
        //! default constructor
      InterpolatedDiscountCurve(const std::vector<Time> &times,
                                const std::vector<Handle<Quote> > &quotes,
                                const Natural settlementDays,
                                const Calendar &cal, const DayCounter &dc)
          : YieldTermStructure(settlementDays, cal, dc), times_(times) {
            initalise(quotes);
        }

        //! constructor that takes a vector of dates
        InterpolatedDiscountCurve(const std::vector<Date> &dates,
                                  const std::vector<Handle<Quote> > &quotes,
                                  const Natural settlementDays,
                                  const Calendar &cal, const DayCounter &dc)
            : YieldTermStructure(settlementDays, cal, dc),
              times_(dates.size()) {
            for (Size i = 0; i < dates.size(); ++i)
                times_[i] = timeFromReference(dates[i]);
            initalise(quotes);
        }
        //@}
        
    private:
        void initalise (std::vector<Handle<Quote>> quotes) {
            QL_REQUIRE(times_.size() > 1, "at least two times required");
            QL_REQUIRE(times_[0] == 0.0, "First time must be 0, got " << times_[0]); // or date=asof
            QL_REQUIRE(times_.size() == quotes.size(), "size of time and quote vectors do not match");
            for (Size i = 0; i < quotes.size(); ++i)
                quotes_.push_back(boost::make_shared<LogQuote>(quotes[i]));
            for (Size i = 0; i < times_.size() - 1; ++i)
                timeDiffs_.push_back(times_[i+1] - times_[i]);
        }

        //! \name TermStructure interface
        //@{
        Date maxDate() const { return Date::maxDate(); } // flat fwd extrapolation
        //@}
  
    protected:
        DiscountFactor discountImpl(Time t) const {
            std::vector<Time>::const_iterator it =
                std::upper_bound(times_.begin(), times_.end(), t);
            Size i = std::min<Size>(it - times_.begin(), times_.size() - 1);
            Real w = (times_[i] - t) / timeDiffs_[i - 1];
            Real value = (1.0 - w) * quotes_[i]->value() +
                w * quotes_[i - 1]->value();
            return ::exp(value);
        }

    private:
        std::vector<Time> times_;
        std::vector<Time> timeDiffs_;
        std::vector<boost::shared_ptr<Quote> > quotes_;
    };

}

#endif
