/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/cashflows/commodityindexedcashflow.hpp
    \brief Cash flow dependent on a single commodity spot price or future's settlement price
 */

#ifndef quantext_commodity_indexed_cash_flow_hpp
#define quantext_commodity_indexed_cash_flow_hpp

#include <boost/optional.hpp>
#include <ql/cashflow.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/time/schedule.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/time/futureexpirycalculator.hpp>

namespace QuantExt {

//! Cash flow dependent on a single commodity spot price or futures settlement price on a given pricing date
class CommodityIndexedCashFlow : public CashFlow, public Observer {

public:
    //! Constructor taking an explicit \p pricingDate and \p paymentDate
    CommodityIndexedCashFlow(QuantLib::Real quantity, const QuantLib::Date& pricingDate,
                             const QuantLib::Date& paymentDate, const ext::shared_ptr<CommodityIndex>& index,
                             QuantLib::Real spread = 0.0, QuantLib::Real gearing = 1.0, bool useFuturePrice = false,
                             const Date& contractDate = Date(),
                             const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr);

    /*! Constructor taking a period \p startDate, \p endDate and some conventions. The pricing date and payment date
        are derived from the start date and end date using the conventions.
    */
    CommodityIndexedCashFlow(QuantLib::Real quantity, const QuantLib::Date& startDate, const QuantLib::Date& endDate,
                             const ext::shared_ptr<CommodityIndex>& index, QuantLib::Natural paymentLag,
                             const QuantLib::Calendar& paymentCalendar,
                             QuantLib::BusinessDayConvention paymentConvention, QuantLib::Natural pricingLag,
                             const QuantLib::Calendar& pricingLagCalendar, QuantLib::Real spread = 0.0,
                             QuantLib::Real gearing = 1.0, bool payInAdvance = false, bool isInArrears = true,
                             bool useFuturePrice = false, bool useFutureExpiryDate = true,
                             QuantLib::Natural futureMonthOffset = 0,
                             const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr,
                             const QuantLib::Date& paymentDateOverride = Date(),
                             const QuantLib::Date& pricingDateOverride = Date());

    //! \name Inspectors
    //@{
    QuantLib::Real quantity() const { return quantity_; }
    const QuantLib::Date& pricingDate() const { return pricingDate_; }
    ext::shared_ptr<CommodityIndex> index() const { return index_; }
    QuantLib::Real spread() const { return spread_; }
    QuantLib::Real gearing() const { return gearing_; }
    bool useFuturePrice() const { return useFuturePrice_; }
    bool useFutureExpiryDate() const { return useFutureExpiryDate_; }
    QuantLib::Natural futureMonthOffset() const { return futureMonthOffset_; }
    QuantLib::Real periodQuantity() const { return periodQuantity_; }
    //@}

    //! \name Event interface
    //@{
    QuantLib::Date date() const { return paymentDate_; }
    //@}

    //! \name CashFlow interface
    //@{
    QuantLib::Real amount() const;
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor& v);
    //@}

    //! \name Observer interface
    //@{
    void update();
    //@}

    //! Allow the full calculation period quantity to be updated.
    void setPeriodQuantity(QuantLib::Real periodQuantity);

private:
    QuantLib::Real quantity_;
    QuantLib::Date pricingDate_;
    QuantLib::Date paymentDate_;
    ext::shared_ptr<CommodityIndex> index_;
    QuantLib::Real spread_;
    QuantLib::Real gearing_;
    bool useFuturePrice_;
    bool useFutureExpiryDate_;
    QuantLib::Natural futureMonthOffset_;
    QuantLib::Real periodQuantity_;

    //! Shared initialisation
    void init(const ext::shared_ptr<FutureExpiryCalculator>& calc,
        const QuantLib::Date& contractDate = QuantLib::Date());
};

//! Helper class building a sequence of commodity indexed cashflows
class CommodityIndexedLeg {

public:
    CommodityIndexedLeg(const QuantLib::Schedule& schedule, const ext::shared_ptr<CommodityIndex>& index);
    CommodityIndexedLeg& withQuantities(QuantLib::Real quantity);
    CommodityIndexedLeg& withQuantities(const std::vector<QuantLib::Real>& quantities);
    CommodityIndexedLeg& withPaymentLag(QuantLib::Natural paymentLag);
    CommodityIndexedLeg& withPaymentCalendar(const QuantLib::Calendar& paymentCalendar);
    CommodityIndexedLeg& withPaymentConvention(QuantLib::BusinessDayConvention paymentConvention);
    CommodityIndexedLeg& withPricingLag(QuantLib::Natural pricingLag);
    CommodityIndexedLeg& withPricingLagCalendar(const QuantLib::Calendar& pricingLagCalendar);
    CommodityIndexedLeg& withSpreads(QuantLib::Real spread);
    CommodityIndexedLeg& withSpreads(const std::vector<QuantLib::Real>& spreads);
    CommodityIndexedLeg& withGearings(QuantLib::Real gearing);
    CommodityIndexedLeg& withGearings(const std::vector<QuantLib::Real>& gearings);
    CommodityIndexedLeg& payInAdvance(bool flag = false);
    CommodityIndexedLeg& inArrears(bool flag = true);
    CommodityIndexedLeg& useFuturePrice(bool flag = false);
    CommodityIndexedLeg& useFutureExpiryDate(bool flag = true);
    CommodityIndexedLeg& withFutureMonthOffset(QuantLib::Natural futureMonthOffset);
    CommodityIndexedLeg& withFutureExpiryCalculator(const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr);
    CommodityIndexedLeg& payAtMaturity(bool flag = false);
    CommodityIndexedLeg& withPricingDates(const std::vector<QuantLib::Date>& pricingDates);
    CommodityIndexedLeg& withPaymentDates(const std::vector<QuantLib::Date>& paymentDates);

    operator Leg() const;

private:
    Schedule schedule_;
    ext::shared_ptr<CommodityIndex> index_;
    std::vector<QuantLib::Real> quantities_;
    QuantLib::Natural paymentLag_;
    QuantLib::Calendar paymentCalendar_;
    QuantLib::BusinessDayConvention paymentConvention_;
    QuantLib::Natural pricingLag_;
    QuantLib::Calendar pricingLagCalendar_;
    std::vector<QuantLib::Real> spreads_;
    std::vector<QuantLib::Real> gearings_;
    bool payInAdvance_;
    bool inArrears_;
    bool useFuturePrice_;
    bool useFutureExpiryDate_;
    QuantLib::Natural futureMonthOffset_;
    ext::shared_ptr<FutureExpiryCalculator> calc_;
    bool payAtMaturity_;
    std::vector<QuantLib::Date> pricingDates_;
    std::vector<QuantLib::Date> paymentDates_;
};

} // namespace QuantExt

#endif
