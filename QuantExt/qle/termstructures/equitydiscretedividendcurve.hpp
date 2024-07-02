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

/*! \file qle/termstructures/equitydiscretedividendcurve.hpp
    \brief holds future discrete dividends
    \ingroup termstructures
*/

#ifndef quantext_equity_discrete_dividend_curve_hpp
#define quantext_equity_discrete_dividend_curve_hpp

#include <ql/termstructure.hpp>

namespace QuantExt {

class EquityDiscreteDividendCurve : public QuantLib::TermStructure {

public:
    EquityDiscreteDividendCurve(const Date& referenceDate, const std::set<QuantExt::Dividend>& dividends,
                                const QuantLib::Calendar& cal = QuantLib::Calendar(),
                                const QuantLib::DayCounter& dc = QuantLib::DayCounter())
        : QuantLib::TermStructure(referenceDate, cal, dc) {
        times_.push_back(0.0);
        accumlatedDivs_.push_back(0.0);
        for (auto& d : dividends) {
            Time time = timeFromReference(d.exDate);
            times_.push_back(time);
            accumlatedDivs_.push_back(accumlatedDivs_.back() + d.rate);
        }
    }

    //! \name TermStructure interface
    //@{
    Date maxDate() const override { return Date::maxDate(); }
    //@}

    Real accumulatedDividends(Time t) {
        if (accumlatedDivs_.size() > 1) {
            std::vector<Time>::const_iterator it = std::upper_bound(times_.begin(), times_.end(), t);
            Size i = std::min<Size>(it - times_.begin(), times_.size());
            return accumlatedDivs_[i-1];
        } else
            return accumlatedDivs_[0];
    }

private:
    std::vector<Time> times_;
    std::vector<Real> accumlatedDivs_;
};

} // namespace QuantExt
#endif
