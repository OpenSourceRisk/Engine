/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*
 Copyright (C) 2009 Roland Lichters
 Copyright (C) 2009 Ferdinando Ametrano
 Copyright (C) 2014 Peter Caspers
 Copyright (C) 2017 Joseph Jeisman
 Copyright (C) 2017 Fabrice Lecuyer

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/cashflows/overnightindexedcoupon.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/utilities/vectors.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using std::vector;

namespace QuantExt {

    namespace {

        class OvernightIndexedCouponPricer : public FloatingRateCouponPricer {
          public:
            void initialize(const FloatingRateCoupon& coupon) {
                coupon_ = dynamic_cast<const OvernightIndexedCoupon*>(&coupon);
                QL_ENSURE(coupon_, "wrong coupon type");
            }
            Rate swapletRate() const {

                ext::shared_ptr<OvernightIndex> index =
                    ext::dynamic_pointer_cast<OvernightIndex>(coupon_->index());

                const vector<Date>& fixingDates = coupon_->fixingDates();
                const vector<Time>& dt = coupon_->dt();

                Size n = dt.size(),
                     i = 0;

                Real compoundFactor = 1.0;

                // already fixed part
                Date today = Settings::instance().evaluationDate();
                while (i<n && fixingDates[i]<today) {
                    // rate must have been fixed
                    Rate pastFixing = IndexManager::instance().getHistory(
                                                index->name())[fixingDates[i]];
                    QL_REQUIRE(pastFixing != Null<Real>(),
                               "Missing " << index->name() <<
                               " fixing for " << fixingDates[i]);
                    if(coupon_->includeSpread())
                        pastFixing += coupon_->spread();
                    compoundFactor *= (1.0 + pastFixing*dt[i]);
                    ++i;
                }

                // today is a border case
                if (i<n && fixingDates[i] == today) {
                    // might have been fixed
                    try {
                        Rate pastFixing = IndexManager::instance().getHistory(
                                                index->name())[fixingDates[i]];
                        if (pastFixing != Null<Real>()) {
                            if(coupon_->includeSpread())
                                pastFixing += coupon_->spread();
                            compoundFactor *= (1.0 + pastFixing*dt[i]);
                            ++i;
                        } else {
                            ;   // fall through and forecast
                        }
                    } catch (Error&) {
                        ;       // fall through and forecast
                    }
                }

                // forward part using telescopic property in order
                // to avoid the evaluation of multiple forward fixings
                const vector<Date>& dates = coupon_->valueDates();
                if (i<n) {
                    Handle<YieldTermStructure> curve =
                        index->forwardingTermStructure();
                    QL_REQUIRE(!curve.empty(),
                               "null term structure set to this instance of "<<
                               index->name());

                    DiscountFactor startDiscount = curve->discount(dates[i]);
                    DiscountFactor endDiscount = curve->discount(dates[n]);

                    compoundFactor *= startDiscount/endDiscount;

                    if(coupon_->includeSpread()) {
                        // this is an approximation, see "Ester / Daily Spread Curve Setup in ORE":
                        // set tau to an average value
                        Real tau =
                            coupon_->dayCounter().yearFraction(dates[i], dates.back()) / (dates.back() - dates[i]);
                        // now use formula (4) from the paper
                        compoundFactor *=
                            std::pow(1.0 + tau * coupon_->spread(), static_cast<int>(dates.back() - dates[i]));
                    }
                }

                Rate tau = coupon_->lookback() == 0 * Days
                               ? coupon_->accrualPeriod()
                               : coupon_->dayCounter().yearFraction(dates.front(), dates.back());
                Rate rate = (compoundFactor - 1.0) / tau;
                Rate result = coupon_->gearing() * rate;
                if(!coupon_->includeSpread())
                    result += coupon_->spread();
                return result;
            }

            Real swapletPrice() const { QL_FAIL("swapletPrice not available");  }
            Real capletPrice(Rate) const { QL_FAIL("capletPrice not available"); }
            Rate capletRate(Rate) const { QL_FAIL("capletRate not available"); }
            Real floorletPrice(Rate) const { QL_FAIL("floorletPrice not available"); }
            Rate floorletRate(Rate) const { QL_FAIL("floorletRate not available"); }
          protected:
            const OvernightIndexedCoupon* coupon_;
        };
    }

    OvernightIndexedCoupon::OvernightIndexedCoupon(
                    const Date& paymentDate,
                    Real nominal,
                    const Date& startDate,
                    const Date& endDate,
                    const ext::shared_ptr<OvernightIndex>& overnightIndex,
                    Real gearing,
                    Spread spread,
                    const Date& refPeriodStart,
                    const Date& refPeriodEnd,
                    const DayCounter& dayCounter,
                    bool telescopicValueDates,
                    bool includeSpread,
                    const Period& lookback,
                    const Natural rateCutoff,
                    const Natural fixingDays)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate,
                         fixingDays,
                         overnightIndex,
                         gearing, spread,
                         refPeriodStart, refPeriodEnd,
                         dayCounter, false), includeSpread_(includeSpread),
                         lookback_(lookback), rateCutoff_(rateCutoff) {

        Date valueStart = startDate;
        Date valueEnd = endDate;
        if (lookback != 0 * Days) {
            BusinessDayConvention bdc = lookback.length() > 0 ? Preceding : Following;
            valueStart = overnightIndex->fixingCalendar().advance(valueStart, -lookback, bdc);
            valueEnd = overnightIndex->fixingCalendar().advance(valueEnd, -lookback, bdc);
        }

        // value dates
        Date tmpEndDate = valueEnd;

        /* For the coupon's valuation only the first and last future valuation
           dates matter, therefore we can avoid to construct the whole series
           of valuation dates, a front and back stub will do. However notice
           that if the global evaluation date moves forward it might run past
           the front stub of valuation dates we build here (which incorporates
           a grace period of 7 business after the evluation date). This will
           lead to false coupon projections (see the warning the class header). */

        if (telescopicValueDates) {
            // build optimised value dates schedule: front stub goes
            // from start date to max(evalDate,valueStart) + 7bd
            Date evalDate = Settings::instance().evaluationDate();
            tmpEndDate = overnightIndex->fixingCalendar().advance(
                std::max(valueStart, evalDate), 7, Days, Following);
            tmpEndDate = std::min(tmpEndDate, valueEnd);
        }
        Schedule sch =
            MakeSchedule()
                .from(valueStart)
                // .to(valueEnd)
                .to(tmpEndDate)
                .withTenor(1 * Days)
                .withCalendar(overnightIndex->fixingCalendar())
                .withConvention(overnightIndex->businessDayConvention())
                .backwards();
        valueDates_ = sch.dates();

        if (telescopicValueDates) {
            // build optimised value dates schedule: back stub
            // contains at least two dates
            Date tmp = overnightIndex->fixingCalendar().advance(
                valueEnd, -1, Days, Preceding);
            if (tmp != valueDates_.back())
                valueDates_.push_back(tmp);
            tmp = overnightIndex->fixingCalendar().adjust(
                valueEnd, overnightIndex->businessDayConvention());
            if (tmp != valueDates_.back())
                valueDates_.push_back(tmp);
        }

        QL_ENSURE(valueDates_.size() >= 2 + rateCutoff_, "degenerate schedule");

        // fixing dates
        n_ = valueDates_.size()-1;
        if (FloatingRateCoupon::fixingDays() == 0) {
            fixingDates_ = vector<Date>(valueDates_.begin(),
                                             valueDates_.end()-1);
        } else {
            fixingDates_.resize(n_);
            for (Size i = 0; i < n_; ++i)
                fixingDates_[i] = overnightIndex->fixingCalendar().advance(
                    valueDates_[i], -static_cast<Integer>(FloatingRateCoupon::fixingDays()), Days, Preceding);
        }

        // accrual (compounding) periods
        dt_.resize(n_);
        const DayCounter& dc = overnightIndex->dayCounter();
        for (Size i=0; i<n_; ++i)
            dt_[i] = dc.yearFraction(valueDates_[i], valueDates_[i+1]);

        setPricer(ext::shared_ptr<FloatingRateCouponPricer>(new
                                            OvernightIndexedCouponPricer));
    }

    const vector<Rate>& OvernightIndexedCoupon::indexFixings() const {
        fixings_.resize(n_);
        Size i;
        for (i = 0; i < n_ - rateCutoff_; ++i) {
            fixings_[i] = index_->fixing(fixingDates_[i]);
        }
        Rate cutoffFixing = fixings_[i - 1];
        while (i < n_) {
            fixings_[i] = cutoffFixing;
            i++;
        }
        return fixings_;
    }

    void OvernightIndexedCoupon::accept(AcyclicVisitor& v) {
        Visitor<OvernightIndexedCoupon>* v1 =
            dynamic_cast<Visitor<OvernightIndexedCoupon>*>(&v);
        if (v1 != 0) {
            v1->visit(*this);
        } else {
            FloatingRateCoupon::accept(v);
        }
    }

    OvernightLeg::OvernightLeg(const Schedule& schedule, const ext::shared_ptr<OvernightIndex>& i)
        : schedule_(schedule), overnightIndex_(i), paymentCalendar_(schedule.calendar()), paymentAdjustment_(Following),
          paymentLag_(0), telescopicValueDates_(false), includeSpread_(false), lookback_(0 * Days), rateCutoff_(0),
          fixingDays_(Null<Size>()) {}

    OvernightLeg& OvernightLeg::withNotionals(Real notional) {
        notionals_ = vector<Real>(1, notional);
        return *this;
    }

    OvernightLeg& OvernightLeg::withNotionals(const vector<Real>& notionals) {
        notionals_ = notionals;
        return *this;
    }

    OvernightLeg& OvernightLeg::withPaymentDayCounter(const DayCounter& dc) {
        paymentDayCounter_ = dc;
        return *this;
    }

    OvernightLeg&
    OvernightLeg::withPaymentAdjustment(BusinessDayConvention convention) {
        paymentAdjustment_ = convention;
        return *this;
    }

    OvernightLeg& OvernightLeg::withPaymentCalendar(const Calendar& cal) {
        paymentCalendar_ = cal;
        return *this;
    }

    OvernightLeg& OvernightLeg::withPaymentLag(Natural lag) {
        paymentLag_ = lag;
        return *this;
    }

    OvernightLeg& OvernightLeg::withGearings(Real gearing) {
        gearings_ = vector<Real>(1,gearing);
        return *this;
    }

    OvernightLeg& OvernightLeg::withGearings(const vector<Real>& gearings) {
        gearings_ = gearings;
        return *this;
    }

    OvernightLeg& OvernightLeg::withSpreads(Spread spread) {
        spreads_ = vector<Spread>(1,spread);
        return *this;
    }

    OvernightLeg& OvernightLeg::withSpreads(const vector<Spread>& spreads) {
        spreads_ = spreads;
        return *this;
    }

    OvernightLeg& OvernightLeg::withTelescopicValueDates(bool telescopicValueDates) {
        telescopicValueDates_ = telescopicValueDates;
        return *this;
    }

    OvernightLeg& OvernightLeg::includeSpread(bool includeSpread) {
        includeSpread_ = includeSpread;
        return *this;
    }

    OvernightLeg& OvernightLeg::withLookback(const Period& lookback) {
        lookback_ = lookback;
        return *this;
    }

    OvernightLeg& OvernightLeg::withRateCutoff(const Natural rateCutoff) {
        rateCutoff_ = rateCutoff;
        return *this;
    }

    OvernightLeg& OvernightLeg::withFixingDays(const Natural fixingDays) {
        fixingDays_ = fixingDays;
        return *this;
    }

    OvernightLeg::operator Leg() const {

        QL_REQUIRE(!notionals_.empty(), "no notional given");

        Leg cashflows;

        // the following is not always correct
        Calendar calendar = schedule_.calendar();

        Date refStart, start, refEnd, end;
        Date paymentDate;

        Size n = schedule_.size()-1;
        for (Size i=0; i<n; ++i) {
            refStart = start = schedule_.date(i);
            refEnd   =   end = schedule_.date(i+1);
            paymentDate = paymentCalendar_.advance(end, paymentLag_, Days, paymentAdjustment_);

            if (i == 0 && schedule_.hasIsRegular() && !schedule_.isRegular(i+1))
                refStart = calendar.adjust(end - schedule_.tenor(),
                                           paymentAdjustment_);
            if (i == n-1 && schedule_.hasIsRegular() && !schedule_.isRegular(i+1))
                refEnd = calendar.adjust(start + schedule_.tenor(),
                                         paymentAdjustment_);

            cashflows.push_back(ext::shared_ptr<CashFlow>(new
                OvernightIndexedCoupon(paymentDate,
                                       detail::get(notionals_, i,
                                                   notionals_.back()),
                                       start, end,
                                       overnightIndex_,
                                       detail::get(gearings_, i, 1.0),
                                       detail::get(spreads_, i, 0.0),
                                       refStart, refEnd,
                                       paymentDayCounter_,
                                       telescopicValueDates_,
                                       includeSpread_,
                                       lookback_,
                                       rateCutoff_,
                                       fixingDays_)));
        }
        return cashflows;
    }

}
