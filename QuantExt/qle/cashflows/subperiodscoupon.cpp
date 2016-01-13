/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <ql/time/schedule.hpp>
#include <ql/indexes/interestrateindex.hpp>
#include <ql/utilities/vectors.hpp>

#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/cashflows/subperiodscouponpricer.hpp>
#include <qle/cashflows/couponpricer.hpp>

using namespace QuantLib;

namespace QuantExt {

    SubPeriodsCoupon::SubPeriodsCoupon(const Date& paymentDate,
        Real nominal,
        const Date& startDate,
        const Date& endDate,
        const boost::shared_ptr<InterestRateIndex>& index,
        Type type,
        BusinessDayConvention convention,
        Spread spread,
        const DayCounter& dayCounter,
        bool includeSpread,
        Real gearing)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate, 
      index->fixingDays(), index, gearing, spread, Date(), Date(),
      dayCounter, false), type_(type), includeSpread_(includeSpread) {
            
        // Populate the value dates.     
        Schedule sch = MakeSchedule().from(startDate)
            .to(endDate)
            .withTenor(index->tenor())
            .withCalendar(index->fixingCalendar())
            .withConvention(convention)
            .withTerminationDateConvention(convention)
            .backwards();
        valueDates_ = sch.dates();
        QL_ENSURE(valueDates_.size() >= 2, "Degenerate schedule.");

        // Populate the fixing dates.
        numPeriods_ = valueDates_.size() - 1;
        if (index->fixingDays() == 0) {
            fixingDates_ = std::vector<Date>(valueDates_.begin(),
                valueDates_.end() - 1);
        } else {
            fixingDates_.resize(numPeriods_);
            for (Size i = 0; i < numPeriods_; ++i)
                fixingDates_[i] = index->fixingDate(valueDates_[i]);
        }

        // Populate the accrual periods.
        accrualFractions_.resize(numPeriods_);
        for (Size i = 0; i < numPeriods_; ++i) {
            accrualFractions_[i] =
                dayCounter.yearFraction(valueDates_[i], valueDates_[i+1]);
        }
    }

    const std::vector<Rate>& 
        SubPeriodsCoupon::indexFixings() const {
        
        fixings_.resize(numPeriods_);
        
        for (Size i = 0; i < numPeriods_; ++i) {
            fixings_[i] = index_->fixing(fixingDates_[i]);
        }

        return fixings_;
    }

    void SubPeriodsCoupon::accept(AcyclicVisitor& v) {
        Visitor<SubPeriodsCoupon>* v1 =
            dynamic_cast<Visitor<SubPeriodsCoupon>*>(&v);
        if (v1 != 0) {
            v1->visit(*this);
        } else {
            FloatingRateCoupon::accept(v);
        }
    }

    SubPeriodsLeg::SubPeriodsLeg(const Schedule& schedule,
        const boost::shared_ptr<InterestRateIndex>& index)
    : schedule_(schedule), index_(index),
      notionals_(std::vector<Real>(1, 1.0)),
      paymentAdjustment_(Following),
      paymentCalendar_(Calendar()),
      type_(SubPeriodsCoupon::Compounding) {}

    SubPeriodsLeg& SubPeriodsLeg::withNotional(Real notional) {
        notionals_ = std::vector<Real>(1, notional);
        return *this;
    }

    SubPeriodsLeg& SubPeriodsLeg::
        withNotionals(const std::vector<Real>& notionals) {
        notionals_ = notionals;
        return *this;
    }

    SubPeriodsLeg& SubPeriodsLeg::
        withPaymentDayCounter(const DayCounter& dayCounter) {
        paymentDayCounter_ = dayCounter;
        return *this;
    }

    SubPeriodsLeg& SubPeriodsLeg::
        withPaymentAdjustment(BusinessDayConvention convention) {
        paymentAdjustment_ = convention;
        return *this;
    }

    SubPeriodsLeg& SubPeriodsLeg::withGearing(Real gearing) {
        gearings_ = std::vector<Real>(1, gearing);
        return *this;
    }

    SubPeriodsLeg& SubPeriodsLeg::
        withGearings(const std::vector<Real>& gearings) {
        gearings_ = gearings;
        return *this;
    }

    SubPeriodsLeg& SubPeriodsLeg::withSpread(Spread spread) {
        spreads_ = std::vector<Spread>(1, spread);
        return *this;
    }

    SubPeriodsLeg& SubPeriodsLeg::
        withSpreads(const std::vector<Spread>& spreads) {
        spreads_ = spreads;
        return *this;
    }
    
    SubPeriodsLeg& SubPeriodsLeg::
        withPaymentCalendar(const Calendar& calendar) {
        paymentCalendar_ = calendar;
        return *this;
    }

    SubPeriodsLeg& SubPeriodsLeg::
        withType(SubPeriodsCoupon::Type type) {
        type_ = type;
        return *this;
    }
    
    SubPeriodsLeg& SubPeriodsLeg::
        includeSpread(bool includeSpread) {
        includeSpread_ = includeSpread;
        return *this;
    }

    SubPeriodsLeg::operator Leg() const {

        Leg cashflows;
        Date startDate;
        Date endDate;
        Date paymentDate;

        Calendar calendar;
        if (!paymentCalendar_.empty()) {
            calendar = paymentCalendar_;
        } else {
            calendar = schedule_.calendar();
        }

        Size numPeriods = schedule_.size() - 1;
        for (Size i = 0; i < numPeriods; ++i) {
            startDate = schedule_.date(i);
            endDate = schedule_.date(i + 1);
            paymentDate = calendar.adjust(endDate, paymentAdjustment_);

            boost::shared_ptr<SubPeriodsCoupon> cashflow(new 
                SubPeriodsCoupon(paymentDate,
                    detail::get(notionals_, i, notionals_.back()), startDate,
                    endDate, index_, type_, paymentAdjustment_,
                    detail::get(spreads_, i, 0.0),
                    paymentDayCounter_, includeSpread_,
                    detail::get(gearings_, i, 1.0)));

            cashflows.push_back(cashflow);
        }

        boost::shared_ptr<SubPeriodsCouponPricer> 
            pricer(new SubPeriodsCouponPricer);
        QuantExt::setCouponPricer(cashflows, pricer);

        return cashflows;
    }
}
