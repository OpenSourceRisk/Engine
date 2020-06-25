/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/cashflows/commodityindexedaveragecashflow.hpp
    \brief Cash flow dependent on the average commodity spot price or future's settlement price over a period
 */

#ifndef quantext_commodity_indexed_average_cash_flow_hpp
#define quantext_commodity_indexed_average_cash_flow_hpp

#include <ql/cashflow.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/time/schedule.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/time/futureexpirycalculator.hpp>

namespace QuantExt {

/*! Cash flow dependent on the average of commodity spot prices or futures settlement prices over a period.

    The cash flow takes a start date and an end date. The set of valid pricing dates is determined from and including
    the start date to but excluding the end date. The cash flow amount is then the arithmetic average of the commodity
    spot prices or next commodity future settlement prices on each valid pricing date times the quantity. The next
    commodity future is determined relative to each pricing date so the settlement prices for multiple commodity
    contracts may be involved in the averaging.
*/
class CommodityIndexedAverageCashFlow : public CashFlow, public Observer {

public:
    //! Constructor taking an explicit \p paymentDate
    CommodityIndexedAverageCashFlow(QuantLib::Real quantity, const QuantLib::Date& startDate,
                                    const QuantLib::Date& endDate, const QuantLib::Date& paymentDate,
                                    const ext::shared_ptr<CommoditySpotIndex>& spotIndex,
                                    const QuantLib::Calendar& pricingCalendar = QuantLib::Calendar(),
                                    QuantLib::Real spread = 0.0, QuantLib::Real gearing = 1.0,
                                    bool useFuturePrice = false, QuantLib::Natural deliveryDateRoll = 0,
                                    QuantLib::Natural futureMonthOffset = 0,
                                    const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr,
                                    bool includeEndDate = true, bool excludeStartDate = true);

    //! Constructor that deduces payment date from \p endDate using payment conventions
    CommodityIndexedAverageCashFlow(
        QuantLib::Real quantity, const QuantLib::Date& startDate, const QuantLib::Date& endDate,
        QuantLib::Natural paymentLag, QuantLib::Calendar paymentCalendar,
        QuantLib::BusinessDayConvention paymentConvention, const ext::shared_ptr<CommoditySpotIndex>& spotIndex,
        const QuantLib::Calendar& pricingCalendar = QuantLib::Calendar(), QuantLib::Real spread = 0.0,
        QuantLib::Real gearing = 1.0, bool payInAdvance = false, bool useFuturePrice = false,
        QuantLib::Natural deliveryDateRoll = 0, QuantLib::Natural futureMonthOffset = 0,
        const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr, bool includeEndDate = true,
        bool excludeStartDate = true, const QuantLib::Date& paymentDateOverride = Date());

    //! \name Inspectors
    //@{
    QuantLib::Real quantity() const { return quantity_; }
    const QuantLib::Date& startDate() const { return startDate_; }
    const QuantLib::Date& endDate() const { return endDate_; }
    ext::shared_ptr<CommodityIndex> index() const { return spotIndex_; }
    QuantLib::Real spread() const { return spread_; }
    QuantLib::Real gearing() const { return gearing_; }
    bool useFuturePrice() const { return useFuturePrice_; }
    QuantLib::Natural deliveryDateRoll() const { return deliveryDateRoll_; }
    QuantLib::Natural futureMonthOffset() const { return futureMonthOffset_; }

    /*! Return the index used to get the price for each pricing date in the period. The map keys are the pricing dates.
        For a given key date, the map value holds the commodity index used to give the price on that date. If the
        averaging does not reference future contract settlement prices, i.e. \c useFirstFuture() is \c false, the
        commodity index is simply the commodity spot index passed in the constructor. If the averaging references
        future contract settlement prices, i.e. \c useFirstFuture() is \c true, the commodity index is the commodity
        future contract \em index relevant for that pricing date.
    */
    const std::map<QuantLib::Date, ext::shared_ptr<CommodityIndex> > indices() const { return indices_; }
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

private:
    QuantLib::Real quantity_;
    QuantLib::Date startDate_;
    QuantLib::Date endDate_;
    QuantLib::Date paymentDate_;
    ext::shared_ptr<CommodityIndex> spotIndex_;
    QuantLib::Calendar pricingCalendar_;
    QuantLib::Real spread_;
    QuantLib::Real gearing_;
    bool useFuturePrice_;
    QuantLib::Natural deliveryDateRoll_;
    QuantLib::Natural futureMonthOffset_;
    bool includeEndDate_;
    bool excludeStartDate_;
    std::map<QuantLib::Date, ext::shared_ptr<CommodityIndex> > indices_;

    //! Shared initialisation
    void init(const ext::shared_ptr<FutureExpiryCalculator>& calc);
};

//! Helper class building a sequence of commodity indexed average cashflows
class CommodityIndexedAverageLeg {

public:
    CommodityIndexedAverageLeg(const QuantLib::Schedule& schedule, const ext::shared_ptr<CommoditySpotIndex>& index);
    CommodityIndexedAverageLeg& withQuantities(QuantLib::Real quantity);
    CommodityIndexedAverageLeg& withQuantities(const std::vector<QuantLib::Real>& quantities);
    CommodityIndexedAverageLeg& withPaymentLag(QuantLib::Natural paymentLag);
    CommodityIndexedAverageLeg& withPaymentCalendar(const QuantLib::Calendar& paymentCalendar);
    CommodityIndexedAverageLeg& withPaymentConvention(QuantLib::BusinessDayConvention paymentConvention);
    CommodityIndexedAverageLeg& withPricingCalendar(const QuantLib::Calendar& pricingCalendar);
    CommodityIndexedAverageLeg& withSpreads(QuantLib::Real spread);
    CommodityIndexedAverageLeg& withSpreads(const std::vector<QuantLib::Real>& spreads);
    CommodityIndexedAverageLeg& withGearings(QuantLib::Real gearing);
    CommodityIndexedAverageLeg& withGearings(const std::vector<QuantLib::Real>& gearings);
    CommodityIndexedAverageLeg& payInAdvance(bool flag = false);
    CommodityIndexedAverageLeg& useFuturePrice(bool flag = false);
    CommodityIndexedAverageLeg& withDeliveryDateRoll(QuantLib::Natural deliveryDateRoll);
    CommodityIndexedAverageLeg& withFutureMonthOffset(QuantLib::Natural futureMonthOffset);
    CommodityIndexedAverageLeg&
    withFutureExpiryCalculator(const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr);
    CommodityIndexedAverageLeg& payAtMaturity(bool flag = false);
    CommodityIndexedAverageLeg& includeEndDate(bool flag = true);
    CommodityIndexedAverageLeg& excludeStartDate(bool flag = true);
    CommodityIndexedAverageLeg& withPricingDates(const std::vector<QuantLib::Date>& pricingDates);
    CommodityIndexedAverageLeg& quantityPerDay(bool flag = false);
    CommodityIndexedAverageLeg& withPaymentDates(const std::vector<QuantLib::Date>& paymentDates);

    operator Leg() const;

private:
    Schedule schedule_;
    ext::shared_ptr<CommoditySpotIndex> index_;
    std::vector<QuantLib::Real> quantities_;
    QuantLib::Natural paymentLag_;
    QuantLib::Calendar paymentCalendar_;
    QuantLib::BusinessDayConvention paymentConvention_;
    QuantLib::Calendar pricingCalendar_;
    std::vector<QuantLib::Real> spreads_;
    std::vector<QuantLib::Real> gearings_;
    bool payInAdvance_;
    bool useFuturePrice_;
    QuantLib::Natural deliveryDateRoll_;
    QuantLib::Natural futureMonthOffset_;
    ext::shared_ptr<FutureExpiryCalculator> calc_;
    bool payAtMaturity_;
    bool includeEndDate_;
    bool excludeStartDate_;
    std::vector<QuantLib::Date> pricingDates_;
    bool quantityPerDay_;
    std::vector<QuantLib::Date> paymentDates_;
};

} // namespace QuantExt

#endif
