/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <ql/utilities/vectors.hpp>
#include <numeric>

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
using std::accumulate;
using std::count_if;
using std::pair;
using std::vector;

namespace QuantExt {

CommodityIndexedAverageCashFlow::CommodityIndexedAverageCashFlow(
    Real quantity, const Date& startDate, const Date& endDate, const Date& paymentDate,
    const ext::shared_ptr<CommodityIndex>& index, const Calendar& pricingCalendar, QuantLib::Real spread,
    QuantLib::Real gearing, bool useFuturePrice, Natural deliveryDateRoll, Natural futureMonthOffset,
    const ext::shared_ptr<FutureExpiryCalculator>& calc, bool includeEndDate, bool excludeStartDate,
    bool useBusinessDays, CommodityQuantityFrequency quantityFrequency, Natural hoursPerDay, Natural dailyExpiryOffset,
    bool unrealisedQuantity, const boost::optional<pair<Calendar, Real>>& offPeakPowerData,
    const ext::shared_ptr<FxIndex>& fxIndex)
    : CommodityCashFlow(quantity, spread, gearing, useFuturePrice, index), startDate_(startDate), endDate_(endDate),
      paymentDate_(paymentDate), pricingCalendar_(pricingCalendar), deliveryDateRoll_(deliveryDateRoll),
      futureMonthOffset_(futureMonthOffset), includeEndDate_(includeEndDate), excludeStartDate_(excludeStartDate),
      useBusinessDays_(useBusinessDays), quantityFrequency_(quantityFrequency), hoursPerDay_(hoursPerDay),
      dailyExpiryOffset_(dailyExpiryOffset), unrealisedQuantity_(unrealisedQuantity),
      offPeakPowerData_(offPeakPowerData), fxIndex_(fxIndex) {
    init(calc);
}

CommodityIndexedAverageCashFlow::CommodityIndexedAverageCashFlow(
    Real quantity, const Date& startDate, const Date& endDate, Natural paymentLag, Calendar paymentCalendar,
    BusinessDayConvention paymentConvention, const ext::shared_ptr<CommodityIndex>& index,
    const Calendar& pricingCalendar, QuantLib::Real spread, QuantLib::Real gearing, PaymentTiming paymentTiming,
    bool useFuturePrice, Natural deliveryDateRoll, Natural futureMonthOffset,
    const ext::shared_ptr<FutureExpiryCalculator>& calc, bool includeEndDate, bool excludeStartDate,
    const QuantLib::Date& paymentDateOverride, bool useBusinessDays, CommodityQuantityFrequency quantityFrequency,
    Natural hoursPerDay, Natural dailyExpiryOffset, bool unrealisedQuantity,
    const boost::optional<pair<Calendar, Real>>& offPeakPowerData, const ext::shared_ptr<FxIndex>& fxIndex)
    : CommodityCashFlow(quantity, spread, gearing, useFuturePrice, index), startDate_(startDate), endDate_(endDate),
      paymentDate_(paymentDateOverride), pricingCalendar_(pricingCalendar),
      deliveryDateRoll_(deliveryDateRoll), futureMonthOffset_(futureMonthOffset), includeEndDate_(includeEndDate),
      excludeStartDate_(excludeStartDate), useBusinessDays_(useBusinessDays), quantityFrequency_(quantityFrequency),
      hoursPerDay_(hoursPerDay), dailyExpiryOffset_(dailyExpiryOffset), unrealisedQuantity_(unrealisedQuantity),
      offPeakPowerData_(offPeakPowerData), fxIndex_(fxIndex) {

    // Derive the payment date
    if (paymentDate_ == Date()) {
        paymentDate_ = paymentTiming == PaymentTiming::InArrears ? endDate : startDate;
        paymentDate_ = paymentCalendar.advance(endDate, paymentLag, Days, paymentConvention);
    }

    init(calc);
}

Real CommodityIndexedAverageCashFlow::amount() const {

    // Calculate the average price
    Real averagePrice = 0.0;
    Real fxRate = 0.0;
    if (weights_.empty()) {
        for (const auto& kv : indices_) {
            fxRate = (fxIndex_)? this->fxIndex()->fixing(kv.first):1.0;
            averagePrice += fxRate * kv.second->fixing(kv.first);
        }
        averagePrice /= indices_.size();
    } else {
        // weights_ will be populated when offPeakPowerData_ is provided.
        for (const auto& kv : indices_) {
            fxRate = (fxIndex_)? this->fxIndex()->fixing(kv.first):1.0;
            averagePrice += fxRate * kv.second->fixing(kv.first) * weights_.at(kv.first);
        }
    }

    // Amount is just average price times quantity
    // In case of Foreign currency settlement, the spread must be expressed in Foreign currency units
    return periodQuantity_ * (gearing_ * averagePrice + spread_);
}

void CommodityIndexedAverageCashFlow::accept(AcyclicVisitor& v) {
    if (Visitor<CommodityIndexedAverageCashFlow>* v1 = dynamic_cast<Visitor<CommodityIndexedAverageCashFlow>*>(&v))
        v1->visit(*this);
    else
        CashFlow::accept(v);
}

void CommodityIndexedAverageCashFlow::update() {
    notifyObservers();
}

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
    auto pds = pricingDates(startDate_, endDate_, pricingCalendar_,
        excludeStartDate_, includeEndDate_, useBusinessDays_);

    QL_REQUIRE(!pds.empty(), "CommodityIndexedAverageCashFlow: found no pricing dates between "
        << io::iso_date(startDate_) << " and " << io::iso_date(endDate_) << ".");

    // Populate the indices_ map with the correct values.
    if (!useFuturePrice_) {

        // If not using future prices, just observe spot on every pricing date.
        for (const Date& pd : pds) {
            indices_[pd] = index_;
        }

    } else {

        // If using future prices, first fill indices assuming delivery date roll is 0.
        for (const Date& pd : pds) {
            auto expiry = calc->nextExpiry(true, pd, futureMonthOffset_);

            // If given an offset for each expiry, apply it now.
            if (dailyExpiryOffset_ != Null<Natural>()) {
                expiry = index_->fixingCalendar().advance(expiry, dailyExpiryOffset_ * Days);
            }

            indices_[pd] = index_->clone(expiry);
        }

        // Update indices_ where necessary if delivery date roll is greater than 0.
        if (deliveryDateRoll_ > 0) {

            Date expiry;
            Date prevExpiry;
            Date rollDate;

            for (auto& kv : indices_) {

                // If expiry is different from previous expiry, update the roll date.
                expiry = kv.second->expiryDate();
                if (expiry != prevExpiry) {
                    rollDate = pricingCalendar_.advance(expiry,
                        -static_cast<Integer>(deliveryDateRoll_), Days);
                }
                prevExpiry = expiry;

                // If the pricing date is after the roll date, we use the next expiry.
                if (kv.first > rollDate) {
                    expiry = calc->nextExpiry(false, expiry);
                    kv.second = index_->clone(expiry);
                }

            }
        }
    }

    // Register with each of the indices.
    for (auto& kv : indices_) {
        registerWith(kv.second);
    }

    // If offPeakPowerData_ is provided, populate weights_
    if (offPeakPowerData_) {
        Calendar peakCalendar = offPeakPowerData_->first;
        Real offPeakHours = offPeakPowerData_->second;
        Real total = 0;
        for (const auto& kv : indices_) {
            if (peakCalendar.isHoliday(kv.first))
                total += weights_[kv.first] = 24;
            else
                total += weights_[kv.first] = offPeakHours;
        }
        for (auto& kv : weights_)
            kv.second /= total;
    }

    // Must be called here after indices_ has been populated.
    updateQuantity();

}

void CommodityIndexedAverageCashFlow::updateQuantity() {

    using CQF = CommodityQuantityFrequency;
    switch (quantityFrequency_)
    {
    case CQF::PerCalculationPeriod:
        periodQuantity_ = quantity_;
        if (unrealisedQuantity_) {
            Date today = Settings::instance().evaluationDate();
            if (startDate_ <= today && today < endDate_) {
                // In both cases, unrealised equal to 0 should probably be an error but leave quantity unaltered.
                if (offPeakPowerData_) {
                    QL_REQUIRE(!weights_.empty(), "Expected to have weights when we have off-peak power data.");
                    auto unrealised = accumulate(weights_.begin(), weights_.end(), 0.0,
                        [&today](Real sum, const pair<Date, Real>& p) {
                            return p.first > today ? sum + p.second : sum; });
                    if (unrealised > 0) {
                        periodQuantity_ /= unrealised;
                    }
                } else {
                    auto unrealised = count_if(indices_.begin(), indices_.end(),
                        [&today](const pair<Date, ext::shared_ptr<CommodityIndex>>& p) { return p.first > today; });
                    if (unrealised > 0) {
                        periodQuantity_ = periodQuantity_ * indices_.size() / unrealised;
                    }
                }
            }
        }
        break;
    case CQF::PerPricingDay:
        periodQuantity_ = quantity_ * indices_.size();
        break;
    case CQF::PerHour:
        if (offPeakPowerData_) {
            Calendar peakCalendar = offPeakPowerData_->first;
            Real offPeakHours = offPeakPowerData_->second;
            periodQuantity_ = 0.0;
            for (const auto& kv : indices_) {
                if (peakCalendar.isHoliday(kv.first))
                    periodQuantity_ += 24.0 * quantity_;
                else
                    periodQuantity_ += offPeakHours * quantity_;
            }
        } else {
            QL_REQUIRE(hoursPerDay_ != Null<Natural>(), "If a commodity quantity frequency of PerHour is used, " <<
                "a valid hoursPerDay value should be supplied.");
            periodQuantity_ = quantity_ * indices_.size() * hoursPerDay_;
        }
        break;
    case CQF::PerCalendarDay:
        // Rarely used but kept because it has already been documented and released.
        periodQuantity_ = quantity_ * ((endDate_ - startDate_ - 1.0) + (!excludeStartDate_ ? 1.0 : 0.0) +
            (includeEndDate_ ? 1.0 : 0.0));
        break;
    default:
        // Do nothing
        break;
    }

}

CommodityIndexedAverageLeg::CommodityIndexedAverageLeg(const Schedule& schedule,
                                                       const ext::shared_ptr<CommodityIndex>& index)
    : schedule_(schedule), index_(index), paymentLag_(0), paymentCalendar_(NullCalendar()),
      paymentConvention_(Unadjusted), pricingCalendar_(Calendar()),
      paymentTiming_(CommodityIndexedAverageCashFlow::PaymentTiming::InArrears), useFuturePrice_(false),
      deliveryDateRoll_(0), futureMonthOffset_(0), payAtMaturity_(false), includeEndDate_(true),
      excludeStartDate_(true), useBusinessDays_(true),
      quantityFrequency_(CommodityQuantityFrequency::PerCalculationPeriod), hoursPerDay_(Null<Natural>()),
      dailyExpiryOffset_(Null<Natural>()), unrealisedQuantity_(false) {}

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

CommodityIndexedAverageLeg&
CommodityIndexedAverageLeg::paymentTiming(CommodityIndexedAverageCashFlow::PaymentTiming paymentTiming) {
    paymentTiming_ = paymentTiming;
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

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withPaymentDates(const vector<Date>& paymentDates) {
    paymentDates_ = paymentDates;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::useBusinessDays(bool flag) {
    useBusinessDays_ = flag;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withQuantityFrequency(
    CommodityQuantityFrequency quantityFrequency) {
    quantityFrequency_ = quantityFrequency;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withHoursPerDay(Natural hoursPerDay) {
    hoursPerDay_ = hoursPerDay;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withDailyExpiryOffset(Natural dailyExpiryOffset) {
    dailyExpiryOffset_ = dailyExpiryOffset;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::unrealisedQuantity(bool flag) {
    unrealisedQuantity_ = flag;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withFxIndex(const ext::shared_ptr<FxIndex>& fxIndex) {
    fxIndex_ = fxIndex;
    return *this;
}

CommodityIndexedAverageLeg& CommodityIndexedAverageLeg::withOffPeakPowerData(const boost::optional<pair<Calendar, Real> >& offPeakPowerData) {
    offPeakPowerData_ = offPeakPowerData;
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

        leg.push_back(ext::make_shared<CommodityIndexedAverageCashFlow>(
            quantity, start, end, paymentLag_, paymentCalendar_, paymentConvention_, index_, pricingCalendar_, spread,
            gearing, paymentTiming_, useFuturePrice_, deliveryDateRoll_, futureMonthOffset_, calc_, includeEnd,
            excludeStart, paymentDate, useBusinessDays_, quantityFrequency_, hoursPerDay_, dailyExpiryOffset_,
            unrealisedQuantity_, offPeakPowerData_, fxIndex_));
    }

    return leg;
}

} // namespace QuantExt
