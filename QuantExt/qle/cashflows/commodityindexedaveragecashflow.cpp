/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <ql/utilities/vectors.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>

using QuantLib::AcyclicVisitor;
using QuantLib::BusinessDayConvention;
using QuantLib::Calendar;
using QuantLib::CashFlow;
using QuantLib::Date;
using QuantLib::Integer;
using QuantLib::MakeSchedule;
using QuantLib::Real;
using QuantLib::Schedule;
using QuantLib::Visitor;
using QuantLib::io::iso_date;
using std::vector;

namespace QuantExt {

CommodityIndexedAverageCashFlow::CommodityIndexedAverageCashFlow(
    Real quantity, const Date& startDate, const Date& endDate, const Date& paymentDate,
    const ext::shared_ptr<CommodityIndex>& index, const Calendar& pricingCalendar, QuantLib::Real spread,
    QuantLib::Real gearing, bool useFuturePrice, Natural deliveryDateRoll, Natural futureMonthOffset,
    const ext::shared_ptr<FutureExpiryCalculator>& calc, bool includeEndDate, bool excludeStartDate,
    bool useBusinessDays)
    : quantity_(quantity), startDate_(startDate), endDate_(endDate), paymentDate_(paymentDate), index_(index),
      pricingCalendar_(pricingCalendar), spread_(spread), gearing_(gearing), useFuturePrice_(useFuturePrice),
      deliveryDateRoll_(deliveryDateRoll), futureMonthOffset_(futureMonthOffset), includeEndDate_(includeEndDate),
      excludeStartDate_(excludeStartDate), useBusinessDays_(useBusinessDays) {
    init(calc);
}

CommodityIndexedAverageCashFlow::CommodityIndexedAverageCashFlow(
    Real quantity, const Date& startDate, const Date& endDate, Natural paymentLag, Calendar paymentCalendar,
    BusinessDayConvention paymentConvention, const ext::shared_ptr<CommodityIndex>& index,
    const Calendar& pricingCalendar, QuantLib::Real spread, QuantLib::Real gearing, bool payInAdvance,
    bool useFuturePrice, Natural deliveryDateRoll, Natural futureMonthOffset,
    const ext::shared_ptr<FutureExpiryCalculator>& calc, bool includeEndDate, bool excludeStartDate,
    const QuantLib::Date& paymentDateOverride, bool useBusinessDays)
    : quantity_(quantity), startDate_(startDate), endDate_(endDate), paymentDate_(paymentDateOverride),
      index_(index), pricingCalendar_(pricingCalendar), spread_(spread), gearing_(gearing),
      useFuturePrice_(useFuturePrice), deliveryDateRoll_(deliveryDateRoll), futureMonthOffset_(futureMonthOffset),
      includeEndDate_(includeEndDate), excludeStartDate_(excludeStartDate), useBusinessDays_(useBusinessDays) {

    // Derive the payment date
    if (paymentDate_ == Date()) {
        paymentDate_ = payInAdvance ? startDate : endDate;
        paymentDate_ = paymentCalendar.advance(endDate, paymentLag, Days, paymentConvention);
    }

    init(calc);
}

Real CommodityIndexedAverageCashFlow::amount() const {

    // Calculate the average price
    Real averagePrice = 0.0;
    for (const auto& kv : indices_) {
        averagePrice += kv.second->fixing(kv.first);
    }
    averagePrice /= indices_.size();

    // Amount is just average price times quantity
    return quantity_ * (gearing_ * averagePrice + spread_);
}

void CommodityIndexedAverageCashFlow::accept(AcyclicVisitor& v) {
    if (Visitor<CommodityIndexedAverageCashFlow>* v1 = dynamic_cast<Visitor<CommodityIndexedAverageCashFlow>*>(&v))
        v1->visit(*this);
    else
        CashFlow::accept(v);
}

void CommodityIndexedAverageCashFlow::update() { notifyObservers(); }

void CommodityIndexedAverageCashFlow::init(const ext::shared_ptr<FutureExpiryCalculator>& calc) {

    // If pricing calendar is not set, use the index fixing calendar
    if (pricingCalendar_ == Calendar()) {
        pricingCalendar_ = index_->fixingCalendar();
    }

    // If we are going to reference a future settlement price, check that we have a valid expiry calculator
    if (useFuturePrice_) {
        QL_REQUIRE(calc, "CommodityIndexedCashFlow needs a valid future expiry calculator when using first future");
    }

    // Store the relevant index for each pricing date taking account of the flags and the pricing calendar
    Date pdStart = startDate_;
    Date pdEnd = endDate_;

    // Cover the possible exclusion of the start date
    if ((useBusinessDays_ && pricingCalendar_.isBusinessDay(pdStart)) && excludeStartDate_) {
        pdStart = pricingCalendar_.advance(pdStart, 1, Days);
    }

    if ((!useBusinessDays_ && pricingCalendar_.isHoliday(pdStart)) && excludeStartDate_) {
        while (pricingCalendar_.isHoliday(pdStart) && pdStart <= pdEnd)
            pdStart++;
    }
 
    // Cover the possible exclusion of the end date
    if ((useBusinessDays_ && pricingCalendar_.isBusinessDay(pdEnd)) && !includeEndDate_) {
        pdEnd = pricingCalendar_.advance(pdEnd, -1, Days);
    }

    if ((!useBusinessDays_ && pricingCalendar_.isHoliday(pdEnd)) && !includeEndDate_) {
        while (pricingCalendar_.isHoliday(pdEnd) && pdStart <= pdEnd)
            pdEnd--;
    }

    QL_REQUIRE(pdEnd > pdStart, "CommodityIndexedAverageCashFlow: end date, "
                                    << io::iso_date(pdEnd) << ", must be greater than start date, "
                                    << io::iso_date(pdStart) << ", for a valid coupon.");

    // Populate first expiry date and roll date if averaging a future settlement price
    Date expiry;
    Date roll;
    if (useFuturePrice_) {
        expiry = calc->nextExpiry(deliveryDateRoll_ == 0, pdStart, futureMonthOffset_);
        roll = deliveryDateRoll_ == 0
                   ? expiry
                   : pricingCalendar_.advance(expiry, -static_cast<Integer>(deliveryDateRoll_), Days);
        QL_REQUIRE(expiry >= pdStart, "Expected the future's expiry date ("
                                          << io::iso_date(expiry) << ") to be on or after the first pricing date ("
                                          << io::iso_date(pdStart) << ") in the averaging period");
    }

    for (; pdStart <= pdEnd; pdStart++) {

        if ((useBusinessDays_ && pricingCalendar_.isHoliday(pdStart)) ||
            (!useBusinessDays_ && pricingCalendar_.isBusinessDay(pdStart)))
            continue;

        if (useFuturePrice_) {
            // On the first date after the roll date, use the next future contract and update the roll date
            if (pdStart > roll) {
                expiry = calc->nextExpiry(false, expiry);
                roll = deliveryDateRoll_ == 0
                           ? expiry
                           : pricingCalendar_.advance(expiry, -static_cast<Integer>(deliveryDateRoll_), Days);
            }
            indices_[pdStart] = boost::make_shared<CommodityFuturesIndex>(index_, expiry);
        } else {
            indices_[pdStart] = index_;
        }
        registerWith(indices_[pdStart]);
    }
}

CommodityIndexedAverageLeg::CommodityIndexedAverageLeg(const Schedule& schedule,
                                                       const ext::shared_ptr<CommodityIndex>& index)
    : schedule_(schedule), index_(index), paymentLag_(0), paymentCalendar_(NullCalendar()),
      paymentConvention_(Unadjusted), pricingCalendar_(Calendar()), payInAdvance_(false), useFuturePrice_(false),
      deliveryDateRoll_(0), futureMonthOffset_(0), payAtMaturity_(false), includeEndDate_(true),
      excludeStartDate_(true), quantityPerDay_(false), useBusinessDays_(true) {}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withQuantities(Real quantity) {
    quantities_ = vector<Real>(1, quantity);
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withQuantities(const vector<Real>& quantities) {
    quantities_ = quantities;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withPaymentLag(Natural paymentLag) {
    paymentLag_ = paymentLag;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withPaymentCalendar(const Calendar& paymentCalendar) {
    paymentCalendar_ = paymentCalendar;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withPaymentConvention(BusinessDayConvention paymentConvention) {
    paymentConvention_ = paymentConvention;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withPricingCalendar(const Calendar& pricingCalendar) {
    pricingCalendar_ = pricingCalendar;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withSpreads(Real spread) {
    spreads_ = vector<Real>(1, spread);
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withSpreads(const vector<Real>& spreads) {
    spreads_ = spreads;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withGearings(Real gearing) {
    gearings_ = vector<Real>(1, gearing);
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withGearings(const vector<Real>& gearings) {
    gearings_ = gearings;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::payInAdvance(bool flag) {
    payInAdvance_ = flag;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::useFuturePrice(bool flag) {
    useFuturePrice_ = flag;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withDeliveryDateRoll(Natural deliveryDateRoll) {
    deliveryDateRoll_ = deliveryDateRoll;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withFutureMonthOffset(Natural futureMonthOffset) {
    futureMonthOffset_ = futureMonthOffset;
    return *this;
}

CommodityIndexedAverageLeg&
CommodityIndexedAverageLeg::withFutureExpiryCalculator(const ext::shared_ptr<FutureExpiryCalculator>& calc) {
    calc_ = calc;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::payAtMaturity(bool flag) {
    payAtMaturity_ = flag;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::includeEndDate(bool flag) {
    includeEndDate_ = flag;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::excludeStartDate(bool flag) {
    excludeStartDate_ = flag;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withPricingDates(const vector<Date>& pricingDates) {
    pricingDates_ = pricingDates;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::quantityPerDay(bool flag) {
    quantityPerDay_ = flag;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withPaymentDates(const vector<Date>& paymentDates) {
    paymentDates_ = paymentDates;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::useBusinessDays(bool flag) {
    useBusinessDays_ = flag;
    return *this;
}

CommodityIndexedAverageLeg::operator Leg() const {

    // Number of commodity indexed average cashflows
    Size numberCashflows = schedule_.size() - 1;

    // Initial consistency checks
    QL_REQUIRE(!quantities_.empty(), "No quantities given");
    QL_REQUIRE(quantities_.size() <= numberCashflows,
               "Too many quantities (" << quantities_.size() << "), only " << numberCashflows << " required");
    if (useFuturePrice_) {
        QL_REQUIRE(calc_, "CommodityIndexedCashFlow needs a valid future expiry calculator when using first future");
    }
    if (!pricingDates_.empty()) {
        QL_REQUIRE(pricingDates_.size() == numberCashflows, "Expected the number of explicit pricing dates ("
                                                                << pricingDates_.size()
                                                                << ") to equal the number of calculation periods ("
                                                                << numberCashflows << ")");
    }
    if (!paymentDates_.empty()) {
        QL_REQUIRE(paymentDates_.size() == numberCashflows, "Expected the number of explicit payment dates ("
                                                                << paymentDates_.size()
                                                                << ") to equal the number of calculation periods ("
                                                                << numberCashflows << ")");
    }

    // If pay at maturity, populate payment date.
    Date paymentDate;
    if (payAtMaturity_) {
        paymentDate = paymentCalendar_.advance(schedule_.dates().back(), paymentLag_ * Days, paymentConvention_);
    }

    // Leg to hold the result
    // We always include the schedule start and schedule termination date in the averaging so the first and last
    // coupon have special treatment here that overrides the includeEndDate_ and excludeStartDate_ flags
    Leg leg;
    leg.reserve(numberCashflows);
    for (Size i = 0; i < numberCashflows; ++i) {

        Date start = schedule_.date(i);
        Date end = schedule_.date(i + 1);
        bool excludeStart = i == 0 ? false : excludeStartDate_;
        bool includeEnd = i == numberCashflows - 1 ? true : includeEndDate_;
        Real quantity = detail::get(quantities_, i, 1.0);
        Real spread = detail::get(spreads_, i, 0.0);
        Real gearing = detail::get(gearings_, i, 1.0);

        // If explicit payment dates provided, use them.
        if (!paymentDates_.empty()) {
            paymentDate = paymentDates_[i];
        }

        if (quantityPerDay_) {
            Natural factor = end - start - 1;
            if (!excludeStart)
                factor++;
            if (includeEnd)
                factor++;
            quantity *= factor;
        }

        leg.push_back(ext::make_shared<CommodityIndexedAverageCashFlow>(
            quantity, start, end, paymentLag_, paymentCalendar_, paymentConvention_, index_, pricingCalendar_, spread,
            gearing, payInAdvance_, useFuturePrice_, deliveryDateRoll_, futureMonthOffset_, calc_, includeEnd,
            excludeStart, paymentDate, useBusinessDays_));
    }

    return leg;
}

} // namespace QuantExt
