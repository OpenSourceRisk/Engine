/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <ql/utilities/vectors.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>

using namespace QuantLib;
using std::vector;

namespace QuantExt {

CommodityIndexedCashFlow::CommodityIndexedCashFlow(Real quantity, const Date& pricingDate, const Date& paymentDate,
                                                   const ext::shared_ptr<CommoditySpotIndex>& index, Real spread,
                                                   Real gearing, bool useFuturePrice,
                                                   boost::optional<QuantLib::Month> contractMonth,
                                                   boost::optional<QuantLib::Year> contractYear,
                                                   const ext::shared_ptr<FutureExpiryCalculator>& calc)
    : quantity_(quantity), pricingDate_(pricingDate), paymentDate_(paymentDate), index_(index), spread_(spread),
      gearing_(gearing), useFuturePrice_(useFuturePrice), futureMonthOffset_(0) {

    init(calc, contractMonth, contractYear);
}

CommodityIndexedCashFlow::CommodityIndexedCashFlow(
    Real quantity, const Date& startDate, const Date& endDate, const ext::shared_ptr<CommoditySpotIndex>& index,
    Natural paymentLag, const Calendar& paymentCalendar, BusinessDayConvention paymentConvention, Natural pricingLag,
    const Calendar& pricingLagCalendar, Real spread, Real gearing, bool payInAdvance, bool isInArrears,
    bool useFuturePrice, bool useFutureExpiryDate, Natural futureMonthOffset,
    const ext::shared_ptr<FutureExpiryCalculator>& calc, const QuantLib::Date& paymentDateOverride,
    const QuantLib::Date& pricingDateOverride)
    : quantity_(quantity), pricingDate_(pricingDateOverride), paymentDate_(paymentDateOverride), index_(index),
      spread_(spread), gearing_(gearing), useFuturePrice_(useFuturePrice), useFutureExpiryDate_(useFutureExpiryDate),
      futureMonthOffset_(futureMonthOffset) {

    // Derive the pricing date if an explicit override has not been provided
    if (pricingDate_ == Date()) {
        pricingDate_ = isInArrears ? endDate : startDate;
        if (!useFuturePrice_ || !useFutureExpiryDate_) {
            // We just use the pricing date rules to get the pricing date
            pricingDate_ = pricingLagCalendar.advance(pricingDate_, -static_cast<Integer>(pricingLag), Days, Preceding);
        } else {
            // We need to use the expiry date of the future contract
            QL_REQUIRE(calc, "CommodityIndexedCashFlow needs a valid future "
                                 << "expiry calculator when using first future");
            pricingDate_ = calc->expiryDate(pricingDate_.month(), pricingDate_.year(), futureMonthOffset_);
        }
    }

    // Derive the payment date if an explicit override has not been provided
    if (paymentDate_ == Date()) {
        paymentDate_ = payInAdvance ? startDate : endDate;
        paymentDate_ = paymentCalendar.advance(paymentDate_, paymentLag, Days, paymentConvention);
    }

    // We may not need the month and year if we are not using a future settlement price but get them
    // and pass them here in any case to init
    Date ref = isInArrears ? endDate : startDate;

    init(calc, ref.month(), ref.year());
}

Real CommodityIndexedCashFlow::amount() const {
    return quantity_ * gearing_ * (index_->fixing(pricingDate_) + spread_);
}

void CommodityIndexedCashFlow::accept(AcyclicVisitor& v) {
    if (Visitor<CommodityIndexedCashFlow>* v1 = dynamic_cast<Visitor<CommodityIndexedCashFlow>*>(&v))
        v1->visit(*this);
    else
        CashFlow::accept(v);
}

void CommodityIndexedCashFlow::update() { notifyObservers(); }

void CommodityIndexedCashFlow::init(const ext::shared_ptr<FutureExpiryCalculator>& calc,
                                    boost::optional<Month> contractMonth, boost::optional<Year> contractYear) {

    QL_REQUIRE(paymentDate_ >= pricingDate_, "Expected that the payment date ("
                                                 << io::iso_date(paymentDate_)
                                                 << ") would be on or after the pricing date ("
                                                 << io::iso_date(pricingDate_) << ")");
    QL_REQUIRE(index_->isValidFixingDate(pricingDate_), "Pricing date " << io::iso_date(pricingDate_)
                                                                        << " is not valid for commodity "
                                                                        << index_->underlyingName());

    // If we are using the future settlement price as the reference price, then we need to create the
    // relevant "future index" here and update the cashflow's index with it.
    if (useFuturePrice_) {
        QL_REQUIRE(calc, "CommodityIndexedCashFlow needs a valid future expiry calculator when using "
                             << "the future settlement price as reference price");
        QL_REQUIRE(contractMonth, "Need a valid contract month if referencing a future settlement price");
        QL_REQUIRE(contractYear, "Need a valid contract year if referencing a future settlement price");
        Date expiry = calc->expiryDate(*contractMonth, *contractYear, futureMonthOffset_);
        QL_REQUIRE(expiry >= pricingDate_,
                   "Expected that the expiry date ("
                       << io::iso_date(expiry) << ") for commodity " << index_->underlyingName() << " future "
                       << *contractYear << "-" << *contractMonth << " with month offset of " << futureMonthOffset_
                       << " would be on or after the pricing date (" << io::iso_date(pricingDate_) << ")");
        index_ = boost::make_shared<CommodityFuturesIndex>(index_, expiry);
    }

    registerWith(index_);
}

CommodityIndexedLeg::CommodityIndexedLeg(const Schedule& schedule, const ext::shared_ptr<CommoditySpotIndex>& index)
    : schedule_(schedule), index_(index), paymentLag_(0), paymentCalendar_(NullCalendar()),
      paymentConvention_(Unadjusted), pricingLag_(0), pricingLagCalendar_(NullCalendar()), payInAdvance_(false),
      inArrears_(true), useFuturePrice_(false), useFutureExpiryDate_(true), futureMonthOffset_(0),
      payAtMaturity_(false), quantityPerDay_(false) {}

CommodityIndexedLeg& CommodityIndexedLeg::withQuantities(Real quantity) {
    quantities_ = vector<Real>(1, quantity);
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withQuantities(const vector<Real>& quantities) {
    quantities_ = quantities;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPaymentLag(Natural paymentLag) {
    paymentLag_ = paymentLag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPaymentCalendar(const Calendar& paymentCalendar) {
    paymentCalendar_ = paymentCalendar;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPaymentConvention(BusinessDayConvention paymentConvention) {
    paymentConvention_ = paymentConvention;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPricingLag(Natural pricingLag) {
    pricingLag_ = pricingLag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPricingLagCalendar(const Calendar& pricingLagCalendar) {
    pricingLagCalendar_ = pricingLagCalendar;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withSpreads(Real spread) {
    spreads_ = vector<Real>(1, spread);
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withSpreads(const vector<Real>& spreads) {
    spreads_ = spreads;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withGearings(Real gearing) {
    gearings_ = vector<Real>(1, gearing);
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withGearings(const vector<Real>& gearings) {
    gearings_ = gearings;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::payInAdvance(bool flag) {
    payInAdvance_ = flag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::inArrears(bool flag) {
    inArrears_ = flag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::useFuturePrice(bool flag) {
    useFuturePrice_ = flag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::useFutureExpiryDate(bool flag) {
    useFutureExpiryDate_ = flag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withFutureMonthOffset(Natural futureMonthOffset) {
    futureMonthOffset_ = futureMonthOffset;
    return *this;
}

CommodityIndexedLeg&
CommodityIndexedLeg::withFutureExpiryCalculator(const ext::shared_ptr<FutureExpiryCalculator>& calc) {
    calc_ = calc;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::payAtMaturity(bool flag) {
    payAtMaturity_ = flag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPricingDates(const vector<Date>& pricingDates) {
    pricingDates_ = pricingDates;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::quantityPerDay(bool flag) {
    quantityPerDay_ = flag;
    return *this;
}

CommodityIndexedLeg& CommodityIndexedLeg::withPaymentDates(const vector<Date>& paymentDates) {
    paymentDates_ = paymentDates;
    return *this;
}

CommodityIndexedLeg::operator Leg() const {

    // Number of commodity indexed cashflows
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
    Leg leg;
    leg.reserve(numberCashflows);
    for (Size i = 0; i < numberCashflows; ++i) {

        Date start = schedule_.date(i);
        Date end = schedule_.date(i + 1);
        Real quantity = detail::get(quantities_, i, 1.0);
        Real spread = detail::get(spreads_, i, 0.0);
        Real gearing = detail::get(gearings_, i, 1.0);
        Date pricingDate = detail::get(pricingDates_, i, Date());

        if (quantityPerDay_) {
            Natural factor = end - start;
            quantity = quantity * (i == 0 ? factor + 1 : factor);
        }

        // If explicit payment dates provided, use them.
        if (!paymentDates_.empty()) {
            paymentDate = paymentDates_[i];
        }

        leg.push_back(ext::make_shared<CommodityIndexedCashFlow>(
            quantity, start, end, index_, paymentLag_, paymentCalendar_, paymentConvention_, pricingLag_,
            pricingLagCalendar_, spread, gearing, payInAdvance_, inArrears_, useFuturePrice_, useFutureExpiryDate_,
            futureMonthOffset_, calc_, paymentDate, pricingDate));
    }

    return leg;
}

} // namespace QuantExt
