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

    //! InterpolatedDiscountCurve based on interpolation of DiscountFactors
    /*!
      TermStructure can do both Linear and LogLinear interpolation.
      \ingroup correlationtermstructures
     */
    class InterpolatedDiscountCurve : public YieldTermStructure {
    public:
        /*! InterpolatedDiscountCurve has floating referenceDate (Settings::evaluationDate())
         */
        //! \name Constructors
        //@{
        //! default constructor
        InterpolatedDiscountCurve
        (const std::vector<Time>& times, const std::vector<Handle<Quote> >& quotes,
         const Calendar& cal, const DayCounter& dc, bool logLinear)
        : YieldTermStructure(0, cal, dc), times_(times), logLinear_(logLinear) {
            initalise(quotes);
        }

        //! constructor that takes a vector of dates
        InterpolatedDiscountCurve
        (const std::vector<Date>& dates, const std::vector<Handle<Quote> >& quotes,
         const Calendar& cal, const DayCounter& dc, bool logLinear)
        : YieldTermStructure(0, cal, dc), times_(dates.size()), logLinear_(logLinear) {
            for (Size i = 0; i < dates.size(); ++i)
                times_[i] = timeFromReference(dates[i]);
            initalise(quotes);
        }
        
    private:
        void initalise (const std::vector<Handle<Quote> >& quotes) {
            QL_REQUIRE(!times_.empty(), "No times provided");
            QL_REQUIRE(times_[0] == 0.0, "First time must be 0, got " << times_[0]); // or date=asof
            QL_REQUIRE(times_.size() == quotes.size(), "size of time and quote vectors do not match");

            for (Size i = 0; i < quotes.size(); ++i)
                quotes_.push_back(logLinear_ ? Handle<Quote>(boost::make_shared<LogQuote>(quotes[i])) : quotes[i]);

            for (Size i = 0; i < times_.size() - 1; ++i)
                timeDiffs_.push_back(times_[i+1] - times_[i]);
        }

        //@}
        //! \name TermStructure interface
        //@{
        Date maxDate() const { return Date::maxDate(); } // flat extrapolation of zero rate
        //@}
  
    protected:
        DiscountFactor discountImpl(Time t) const {
            QL_REQUIRE(t > 0, "Time " << t << " must be positive");
            if (t >= times_.back()) {

                // extrapolate using flat zero rate from final discount
                Real r = 0.0;
                if (logLinear_)
                    r = -quotes_.back()->value() / times_.back();
                else
                    r = -::log(quotes_.back()->value()) / times_.back();
                return ::exp(-r*t);
            } else {
                std::vector<Time>::const_iterator it = std::lower_bound(times_.begin(), times_.end(), t);
                Size i = it - times_.begin();
                QL_REQUIRE(i > 0, "Invalid i, Time " << t << " should be greater than first point");
                // times_[i-1] < t < times_[i]
                Real weight1 = (t - times_[i-1]) / timeDiffs_[i-1];
                Real weight2 = 1.0 - weight1;
                Real value = (weight1 * quotes_[i]->value()) + (weight2 * quotes_[i-1]->value());
                return logLinear_ ? ::exp(value) : value;
            }
        }

    private:
        std::vector<Time> times_;
        std::vector<Time> timeDiffs_;
        std::vector<Handle<Quote> > quotes_;
        bool logLinear_;
    };

}

#endif
