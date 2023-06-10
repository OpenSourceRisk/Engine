/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
 Copyright (C) 2023 Oleg Kulkov
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

/*! \file bondyieldshiftedcurvetermstructure.hpp
    \brief term structure provided yield curve shifted by bond spread
*/

#include <ql/termstructures/yieldtermstructure.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

namespace QuantExt {
using namespace QuantLib;
using namespace boost::accumulators;

/*! The given date will be the implied reference date.

  \note This term structure will be linked to the original
        curve and the bond spread, i.e., any changes in the latter will be
        reflected in this structure as well.
*/
class BondYieldShiftedCurveTermStructure : public QuantLib::YieldTermStructure {
public:
    BondYieldShiftedCurveTermStructure(const QuantLib::Handle<YieldTermStructure>& originalCurve,
                                       const Real& bondSpread,
                                       const Real& duration
                                       )
        : YieldTermStructure(originalCurve->dayCounter()), originalCurve_(originalCurve), bondSpread_(bondSpread), duration_(duration) {
        registerWith(originalCurve);
    }
    BondYieldShiftedCurveTermStructure(const QuantLib::Handle<YieldTermStructure>& originalCurve,
                                       const std::vector<Real>& bondYields,
                                       const std::vector<Real>& bondDurations
    )
        : YieldTermStructure(originalCurve->dayCounter()), originalCurve_(originalCurve) {
        registerWith(originalCurve);

        QL_REQUIRE(bondYields.size() == bondDurations.size(),
                   "BondYieldShiftedCurveTermStructure: inconsistent lengths of yield and duration vectors ("
                       << bondYields.size() << " vs. " << bondDurations.size() << ")");

        QL_REQUIRE(bondYields.size() > 0, "at least one bondYield for shifting of the reference curve required.");

        accumulator_set<Real, stats<tag::mean>> spreadMean, durationMean;

        for (Size i = 0; i < bondYields.size(); ++i)  {
            //estimate spread at duration
            Real thisCrvRate = -std::log(originalCurve_->discount(static_cast<Real>(bondDurations[i])))/bondDurations[i];
            Real thisSpread = static_cast<Real>(bondYields[i]) - thisCrvRate;

            spreadMean(thisSpread);
            durationMean(bondDurations[i]);
        }

        bondSpread_ = mean(spreadMean);
        duration_ = mean(durationMean);
    }

    //! \name BondYieldShiftedCurveTermStructure interface
    //@{
    DayCounter dayCounter() const override;
    Calendar calendar() const override;
    Natural settlementDays() const override;
    const Date& referenceDate() const override;
    Date maxDate() const override;
    Real bondSpread() const;
    Real duration() const;

protected:
    DiscountFactor discountImpl(Time) const override;
    //@}
private:
    Handle<YieldTermStructure> originalCurve_;
    Real bondSpread_;
    Real duration_;
};

// inline definitions

inline DayCounter BondYieldShiftedCurveTermStructure::dayCounter() const { return originalCurve_->dayCounter(); }

inline Calendar BondYieldShiftedCurveTermStructure::calendar() const { return originalCurve_->calendar(); }

inline Natural BondYieldShiftedCurveTermStructure::settlementDays() const { return originalCurve_->settlementDays(); }

inline Date BondYieldShiftedCurveTermStructure::maxDate() const { return originalCurve_->maxDate(); }

inline Real BondYieldShiftedCurveTermStructure::bondSpread() const { return bondSpread_; }

inline Real BondYieldShiftedCurveTermStructure::duration() const { return duration_; }

inline const Date& BondYieldShiftedCurveTermStructure::referenceDate() const {
    return originalCurve_->referenceDate();
}

inline DiscountFactor BondYieldShiftedCurveTermStructure::discountImpl(Time t) const {

    if ((duration_ != Null<Real>()) && (bondSpread_ != Null<Real>())) {

        Real df = originalCurve_->discount(t) * std::exp(-t * bondSpread_);

        return df;

    } else {
            return originalCurve_->discount(t);
        }

    }

} // namespace QuantExt