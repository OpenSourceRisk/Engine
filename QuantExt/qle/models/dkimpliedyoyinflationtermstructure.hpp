/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file dkimpliedyoyinflationtermstructure.hpp
    \brief yoy inflation term structure implied by a DK model
    \ingroup models
*/

#ifndef quantext_dk_implied_yyts_hpp
#define quantext_dk_implied_yyts_hpp

#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/models/crossassetmodel.hpp>

#include <ql/instruments/yearonyearinflationswap.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Dk Implied yoy Inflation Term Structure
/*! The termstructure has the reference date of the model's
    termstructure at construction, but you can vary this
    as well as the state.
    The purely time based variant is mainly there for
    perfomance reasons, note that it does not provide the
    full term structure interface and does not send
    notifications on reference time updates.

    \ingroup models
*/

class DkImpliedYoYInflationTermStructure : public YoYInflationTermStructure {
public:
    DkImpliedYoYInflationTermStructure(const boost::shared_ptr<CrossAssetModel>& model, Size index);

    Date maxDate() const;
    Time maxTime() const;
    Date baseDate() const;

    const Date& referenceDate() const;

    void referenceDate(const Date& d);
    void state(const Real s_z, const Real s_y, const Real s_irz);
    void move(const Date& d, const Real s_z, const Real s_y, const Real s_irz);
    Real yoyRate(const Date& d, const Period& instObsLag = Period(-1, Days)) const;
    std::map<Date, Real> yoyRates(const std::vector<Date>& dts, const Period& instObsLag = Period(-1, Days));

    void update();

protected:
    Real yoySwapletRate(Time S, Time T) const;
    Real yoyRateImpl(Time t) const;
    const boost::shared_ptr<CrossAssetModel> model_;
    Size index_;

    Date referenceDate_;
    Real relativeTime_, state_z_, state_y_, state_irz_;
};

// inline

inline Date DkImpliedYoYInflationTermStructure::maxDate() const {
    // we don't care - let the underlying classes throw
    // exceptions if applicable
    return Date::maxDate();
}

inline Time DkImpliedYoYInflationTermStructure::maxTime() const {
    // see maxDate
    return QL_MAX_REAL;
}

inline Date DkImpliedYoYInflationTermStructure::baseDate() const {
    if (model_->infdk(index_)->termStructure()->indexIsInterpolated()) {
        return referenceDate_ - observationLag_;
    } else {
        return inflationPeriod(referenceDate_ - observationLag_, frequency()).first;
    }
}

inline const Date& DkImpliedYoYInflationTermStructure::referenceDate() const { return referenceDate_; }

inline void DkImpliedYoYInflationTermStructure::referenceDate(const Date& d) {
    referenceDate_ = d;
    relativeTime_ = dayCounter().yearFraction(model_->infdk(index_)->termStructure()->referenceDate(), referenceDate_);
    update();
}

inline void DkImpliedYoYInflationTermStructure::state(const Real s_z, const Real s_y, const Real s_irz) {
    state_z_ = s_z;
    state_y_ = s_y;
    state_irz_ = s_irz;
    notifyObservers();
}

inline void DkImpliedYoYInflationTermStructure::move(const Date& d, const Real s_z, const Real s_y, const Real s_irz) {
    state(s_z, s_y, s_irz);
    referenceDate(d);
}

inline void DkImpliedYoYInflationTermStructure::update() { notifyObservers(); }

inline Real DkImpliedYoYInflationTermStructure::yoySwapletRate(Time S, Time T) const {
    return model_->infdkYY(index_, relativeTime_, relativeTime_ + S, relativeTime_ + T, state_z_, state_y_, state_irz_);
}

inline Real DkImpliedYoYInflationTermStructure::yoyRateImpl(Time t) const {
    QL_FAIL("DkImpliedYoYInflationTermStructure yoyRate requires a date");
}

inline Real DkImpliedYoYInflationTermStructure::yoyRate(const Date& d, const Period& instObsLag) const {

    Period useLag = instObsLag;
    if (instObsLag == Period(-1, Days)) {
        useLag = observationLag();
    }

    Calendar cal = model_->infdk(index_)->termStructure()->calendar();
    DayCounter dc = model_->infdk(index_)->termStructure()->dayCounter();

    Date maturity;
    if (model_->infdk(index_)->termStructure()->indexIsInterpolated()) {
        maturity = d - useLag;
    } else {
        maturity = inflationPeriod(d - useLag, frequency()).first;
    }

    Schedule schedule = MakeSchedule()
                            .from(baseDate())
                            .to(maturity)
                            .withTenor(1 * Years)
                            .withConvention(Unadjusted)
                            .withCalendar(cal)
                            .backwards();

    Real yoyLegRate = 0.0;
    Real fixedDiscounts = 0.0;
    for (Size i = 1; i < schedule.dates().size(); i++) {
        if (schedule.dates()[i - 1] < baseDate()) {
            // fot the first YoY swaplet, I(T_i-1) is known, obtained from a fixing. I(T_i) comes from the model
            // directly - I(t) * Itilde(t,T).
            Time t1 =
                dayCounter().yearFraction(model_->infdk(index_)->termStructure()->baseDate(), schedule.dates()[i - 1]);
            Real I1 = model_->infdkI(index_, t1, t1, state_z_, state_y_).first;
            Time t2 = dc.yearFraction(baseDate(), schedule.dates()[i]);
            std::pair<Real, Real> II2 = model_->infdkI(index_, relativeTime_, relativeTime_ + t2, state_z_, state_y_);
            Real I2 = II2.first * II2.second;
            Real discount = model_->discountBond(model_->ccyIndex(model_->infdk(index_)->currency()), relativeTime_,
                                                 relativeTime_ + t2, state_irz_);
            fixedDiscounts += discount;
            yoyLegRate += discount * ((I2 / I1) - 1);

        } else {
            Time t1 = dc.yearFraction(baseDate(), schedule.dates()[i - 1]);
            Time t2 = dc.yearFraction(baseDate(), schedule.dates()[i]);
            Real discount = model_->discountBond(model_->ccyIndex(model_->infdk(index_)->currency()), relativeTime_,
                                                 relativeTime_ + t2, state_irz_);
            fixedDiscounts += discount;
            yoyLegRate += yoySwapletRate(t1, t2);
        }
    }
    Real yoyRate = (yoyLegRate / fixedDiscounts);

    if (hasSeasonality()) {
        yoyRate = seasonality()->correctYoYRate(d - useLag, yoyRate, *this);
    }
    return yoyRate;
}

inline std::map<Date, Real> DkImpliedYoYInflationTermStructure::yoyRates(const std::vector<Date>& dts,
                                                                         const Period& instObsLag) {
    std::map<Date, Real> yoys, yoyswaplet, yoydiscount;

    Period useLag = instObsLag;
    if (instObsLag == Period(-1, Days)) {
        useLag = observationLag();
    }

    Calendar cal = model_->infdk(index_)->termStructure()->calendar();
    DayCounter dc = model_->infdk(index_)->termStructure()->dayCounter();
    Date maturity;

    for (Size j = 0; j < dts.size(); j++) {
        if (model_->infdk(index_)->termStructure()->indexIsInterpolated()) {
            maturity = dts[j] - useLag;
        } else {
            maturity = inflationPeriod(dts[j] - useLag, frequency()).first;
        }

        Schedule schedule = MakeSchedule()
                                .from(baseDate())
                                .to(maturity)
                                .withTenor(1 * Years)
                                .withConvention(Unadjusted)
                                .withCalendar(cal)
                                .backwards();

        Real yoyLegRate = 0.0;
        Real fixedDiscounts = 0.0;
        for (Size i = 1; i < schedule.dates().size(); i++) {
            std::map<Date, Real>::const_iterator it = yoyswaplet.find(schedule.dates()[i]);
            Real swapletPrice, discount;
            if (it == yoyswaplet.end()) {
                if (schedule.dates()[i - 1] < baseDate()) {
                    // for the first YoY swaplet, I(T_i-1) is known, obtained from a fixing. I(T_i) comes from the model
                    // directly - I(t) * Itilde(t,T).
                    Time t1 = dayCounter().yearFraction(model_->infdk(index_)->termStructure()->baseDate(),
                                                        schedule.dates()[i - 1]);
                    Real I1 = model_->infdkI(index_, t1, t1, state_z_, state_y_).first;
                    Time t2 = dc.yearFraction(baseDate(), schedule.dates()[i]);
                    std::pair<Real, Real> II2 =
                        model_->infdkI(index_, relativeTime_, relativeTime_ + t2, state_z_, state_y_);
                    Real I2 = II2.first * II2.second;
                    discount = model_->discountBond(model_->ccyIndex(model_->infdk(index_)->currency()), relativeTime_,
                                                    relativeTime_ + t2, state_irz_);
                    swapletPrice = discount * ((I2 / I1) - 1);
                } else {
                    Time t1 = dc.yearFraction(baseDate(), schedule.dates()[i - 1]);
                    Time t2 = dc.yearFraction(baseDate(), schedule.dates()[i]);
                    discount = model_->discountBond(model_->ccyIndex(model_->infdk(index_)->currency()), relativeTime_,
                                                    relativeTime_ + t2, state_irz_);
                    swapletPrice = yoySwapletRate(t1, t2);
                }
                yoyswaplet[schedule.dates()[i]] = swapletPrice;
                yoydiscount[schedule.dates()[i]] = discount;
            } else {
                swapletPrice = yoyswaplet[schedule.dates()[i]];
                discount = yoydiscount[schedule.dates()[i]];
            }
            yoyLegRate += swapletPrice;
            fixedDiscounts += discount;
        }
        Real yoyRate = (yoyLegRate / fixedDiscounts);

        if (hasSeasonality()) {
            yoyRate = seasonality()->correctYoYRate(dts[j] - useLag, yoyRate, *this);
        }
        yoys[dts[j]] = yoyRate;
    }
    return yoys;
}

} // namespace QuantExt

#endif
