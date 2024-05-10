/*
Copyright (C) 2024 Oleg Kulkov
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

/*! \file qle/termstructures/cheapesttodelivercurve.hpp
    \brief cheapest to deliver curve helper
    \ingroup termstructures
*/

#ifndef AGILIB_CHEAPESTTODELIVERCURVE_HPP
#define AGILIB_CHEAPESTTODELIVERCURVE_HPP

#include <ql/time/date.hpp>
#include <ql/currency.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

#include <qle/quotes/logquote.hpp>

#include <boost/optional.hpp>


namespace QuantExt {
using namespace QuantLib;

//! cheapest to deliver term structure
/*! this yield term structure is defined by discount factors given by a maximum forward daily rates of n yield term structures
    and a spread over the resulting curve
 */
class CheapestToDeliverTermStructure : public YieldTermStructure {
public:
    enum class Interpolation { logLinear, linearZero };
    enum class Extrapolation { flatZero, flatFwd };

    CheapestToDeliverTermStructure(const std::vector<Handle<YieldTermStructure>>& yts,
                                   const std::vector<QuantLib::Date>& dts,
                                   const std::vector<Handle<Quote>>& dfs,
                                   Interpolation interpol = Interpolation::logLinear,
                                   Extrapolation extrapol = Extrapolation::flatZero)
        : YieldTermStructure(yts[0]->dayCounter()), altYts_(yts), dts_(dts),
          interpolation_(interpol), extrapolation_(extrapol){
        //register alternative collateral curves
        for (const Handle<YieldTermStructure>& yts_i : altYts_) {
            if (!yts_i.empty()) {
                registerWith(yts_i);
            }
        }
        //initialize log quotes for interpolation
        for (auto const & df: dfs) {
            dfs_.push_back(boost::make_shared<LogQuote>(df));
        }
        //initialize time grid for interpolation
        boost::optional<QuantLib::Time> lastTime = boost::none;
        for (auto const & date: dts_) {
            QuantLib::Time time = timeFromReference(date);
            times_.push_back(time);
            if (lastTime)
                timeDiffs_.push_back(time - *lastTime);
            lastTime = time;
        }
    }

    Date maxDate() const override { return dts_.back(); }
    const Date& referenceDate() const override;

    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) ;
    //@}

protected:
    Real discountImpl(Time t) const override;
    const std::vector<Handle<YieldTermStructure>> altYts_;

private:
    std::vector<QuantLib::Date> dts_;
    std::vector<boost::shared_ptr<QuantExt::LogQuote>> dfs_;
    std::vector<Time> times_;
    std::vector<Time> timeDiffs_;
    Interpolation interpolation_;
    Extrapolation extrapolation_;
};

// inline

inline const Date& CheapestToDeliverTermStructure::referenceDate() const {

    for (Size i = 1; i < altYts_.size(); i++) {
        QL_REQUIRE(altYts_[i-1]->referenceDate() == altYts_[i]->referenceDate(),
                   "CheapestToDeliverTermStructure::referenceDate(): inconsistent reference dates in sources ("
                       << altYts_[i-1]->referenceDate() << " vs. " << altYts_[i]->referenceDate() << ")");
    }

    return altYts_[0]->referenceDate();
}

inline Real CheapestToDeliverTermStructure::discountImpl(Time t) const {

    if (t > this->times_.back() && extrapolation_ == Extrapolation::flatZero) {
        Real tMax = this->times_.back();
        Real dMax = dfs_.back()->quote();

        return std::pow(dMax, t / tMax);
    }
    auto it = std::upper_bound(times_.begin(), times_.end(), t) - times_.begin();
    Size i = std::min<Size>(it, times_.size() - 1);
    Real weight = (times_[i] - t) / timeDiffs_[i - 1];
    if (interpolation_ == Interpolation::logLinear || extrapolation_ == Extrapolation::flatFwd) {
        // the formula handles flat forward extrapolation as well
        Real value = (1.0 - weight) * dfs_[i]->value() + weight * dfs_[i - 1]->value();
        return std::exp(value);
    } else {
        Real value =
            (1.0 - weight) * dfs_[i]->value() / times_[i] + weight * dfs_[i - 1]->value() / times_[i - 1];
        return std::exp(t * value);
    }
}

} // namespace QuantExt

#endif // AGILIB_CHEAPESTTODELIVERCURVE_HPP