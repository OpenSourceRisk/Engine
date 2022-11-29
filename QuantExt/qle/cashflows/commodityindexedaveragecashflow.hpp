/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/cashflows/commodityindexedaveragecashflow.hpp
    \brief Cash flow dependent on the average commodity spot price or future's settlement price over a period.
    If settled in a foreign currency (domestic: currency on which the underlying curve is traded, foreing: settlement currency)
    the FX is applied day by day. This approach cannot be appied to averaged underlying curves.
 */

#ifndef quantext_commodity_indexed_average_cash_flow_hpp
#define quantext_commodity_indexed_average_cash_flow_hpp

#include <ql/cashflow.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/time/schedule.hpp>
#include <qle/cashflows/commoditycashflow.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/time/futureexpirycalculator.hpp>
#include <qle/indexes/fxindex.hpp>

namespace QuantExt {

/*! Cash flow dependent on the average of commodity spot prices or futures settlement prices over a period.

    The cash flow takes a start date and an end date. The set of valid pricing dates is determined from and including
    the start date to but excluding the end date. The cash flow amount is then the arithmetic average of the commodity
    spot prices or next commodity future settlement prices on each valid pricing date times the quantity. The next
    commodity future is determined relative to each pricing date so the settlement prices for multiple commodity
    contracts may be involved in the averaging.
*/
class CommodityIndexedAverageCashFlow : public CommodityCashFlow {

public:
    enum class PaymentTiming { InAdvance, InArrears };

    //! Constructor taking an explicit \p paymentDate
    CommodityIndexedAverageCashFlow(QuantLib::Real quantity, const QuantLib::Date& startDate,
                                    const QuantLib::Date& endDate, const QuantLib::Date& paymentDate,
                                    const ext::shared_ptr<CommodityIndex>& index,
                                    const QuantLib::Calendar& pricingCalendar = QuantLib::Calendar(),
                                    QuantLib::Real spread = 0.0, QuantLib::Real gearing = 1.0,
                                    bool useFuturePrice = false, QuantLib::Natural deliveryDateRoll = 0,
                                    QuantLib::Natural futureMonthOffset = 0,
                                    const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr,
                                    bool includeEndDate = true, bool excludeStartDate = true,
                                    bool useBusinessDays = true,
                                    CommodityQuantityFrequency quantityFrequency =
                                        CommodityQuantityFrequency::PerCalculationPeriod,
                                    QuantLib::Natural hoursPerDay = QuantLib::Null<QuantLib::Natural>(),
                                    QuantLib::Natural dailyExpiryOffset = QuantLib::Null<QuantLib::Natural>(),
                                    bool unrealisedQuantity = false,
                                    const boost::optional<std::pair<QuantLib::Calendar, QuantLib::Real>>& offPeakPowerData = boost::none,
                                    const ext::shared_ptr<FxIndex>& fxIndex = nullptr);

    //! Constructor that deduces payment date from \p endDate using payment conventions
    CommodityIndexedAverageCashFlow(
        QuantLib::Real quantity, const QuantLib::Date& startDate, const QuantLib::Date& endDate,
        QuantLib::Natural paymentLag, QuantLib::Calendar paymentCalendar,
        QuantLib::BusinessDayConvention paymentConvention, const ext::shared_ptr<CommodityIndex>& index,
        const QuantLib::Calendar& pricingCalendar = QuantLib::Calendar(), QuantLib::Real spread = 0.0,
        QuantLib::Real gearing = 1.0, PaymentTiming paymentTiming = PaymentTiming::InArrears,
        bool useFuturePrice = false, QuantLib::Natural deliveryDateRoll = 0, QuantLib::Natural futureMonthOffset = 0,
        const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr, bool includeEndDate = true,
        bool excludeStartDate = true, const QuantLib::Date& paymentDateOverride = Date(),
        bool useBusinessDays = true, CommodityQuantityFrequency quantityFrequency =
        CommodityQuantityFrequency::PerCalculationPeriod, QuantLib::Natural hoursPerDay =
        QuantLib::Null<QuantLib::Natural>(), QuantLib::Natural dailyExpiryOffset =
        QuantLib::Null<QuantLib::Natural>(), bool unrealisedQuantity = false,
        const boost::optional<std::pair<QuantLib::Calendar, QuantLib::Real>>& offPeakPowerData = boost::none,
        const ext::shared_ptr<FxIndex>& fxIndex = nullptr);


    //! \name Inspectors
    //@{
    
    const QuantLib::Date& startDate() const { return startDate_; }
    const QuantLib::Date& endDate() const { return endDate_; }
    ext::shared_ptr<CommodityIndex> index() const { return index_; }
    QuantLib::Natural deliveryDateRoll() const { return deliveryDateRoll_; }
    QuantLib::Natural futureMonthOffset() const { return futureMonthOffset_; }
    bool useBusinessDays() const { return useBusinessDays_; }
    CommodityQuantityFrequency quantityFrequency() const { return quantityFrequency_; }
    QuantLib::Natural hoursPerDay() const { return hoursPerDay_; }
    QuantLib::Natural dailyExpiryOffset() const { return dailyExpiryOffset_; }
    bool unrealisedQuantity() const { return unrealisedQuantity_; }
    const boost::optional<std::pair<QuantLib::Calendar, QuantLib::Real>>& offPeakPowerData() const {
        return offPeakPowerData_;
    }
    ext::shared_ptr<FxIndex> fxIndex() const { return fxIndex_; }

    /*! Return the index used to get the price for each pricing date in the period. The map keys are the pricing dates.
        For a given key date, the map value holds the commodity index used to give the price on that date. If the
        averaging does not reference future contract settlement prices, i.e. \c useFirstFuture() is \c false, the
        commodity index is simply the commodity spot index passed in the constructor. If the averaging references
        future contract settlement prices, i.e. \c useFirstFuture() is \c true, the commodity index is the commodity
        future contract \em index relevant for that pricing date.
    */
    const std::map<QuantLib::Date, ext::shared_ptr<CommodityIndex> >& indices() const { return indices_; }

    /*! Quantity for the full calculation period i.e. the effective quantity after taking into account the 
        quantity frequency setting.
    */
    QuantLib::Real periodQuantity() const { return periodQuantity_; }
    //@}

    //! \name Event interface
    //@{
    QuantLib::Date date() const override { return paymentDate_; }
    //@}

    //! \name CashFlow interface
    //@{
    QuantLib::Real amount() const override;
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor& v) override;
    //@}

        //@}
    //! \name CommodityCashFlow interface
    //@{
    QuantLib::Date lastPricingDate() const override {
        if (indices_.empty()) {
            return Date();
        } else {
            return indices_.rbegin()->first;
        }
    }
    //@}
    

    //! \name Observer interface
    //@{
    void update() override;
    //@}

private:
    QuantLib::Date startDate_;
    QuantLib::Date endDate_;
    QuantLib::Date paymentDate_;
    QuantLib::Calendar pricingCalendar_;
    QuantLib::Natural deliveryDateRoll_;
    QuantLib::Natural futureMonthOffset_;
    bool includeEndDate_;
    bool excludeStartDate_;
    std::map<QuantLib::Date, ext::shared_ptr<CommodityIndex> > indices_;
    bool useBusinessDays_;
    CommodityQuantityFrequency quantityFrequency_;
    QuantLib::Natural hoursPerDay_;
    QuantLib::Natural dailyExpiryOffset_;
    bool unrealisedQuantity_;
    QuantLib::Real periodQuantity_;
    boost::optional<std::pair<QuantLib::Calendar, QuantLib::Real>> offPeakPowerData_;
    ext::shared_ptr<FxIndex> fxIndex_;

    // Populated only when offPeakPowerData_ is provided.
    std::map<QuantLib::Date, QuantLib::Real> weights_;

    //! Shared initialisation
    void init(const ext::shared_ptr<FutureExpiryCalculator>& calc);

    //! Set the period quantity based on the quantity and quantity frequency parameter
    void updateQuantity();
};

//! Helper class building a sequence of commodity indexed average cashflows
class CommodityIndexedAverageLeg {

public:
    CommodityIndexedAverageLeg(const QuantLib::Schedule& schedule, const ext::shared_ptr<CommodityIndex>& index);
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
    CommodityIndexedAverageLeg& paymentTiming(CommodityIndexedAverageCashFlow::PaymentTiming paymentTiming);
    CommodityIndexedAverageLeg& useFuturePrice(bool flag = false);
    CommodityIndexedAverageLeg& withDeliveryDateRoll(QuantLib::Natural deliveryDateRoll);
    CommodityIndexedAverageLeg& withFutureMonthOffset(QuantLib::Natural futureMonthOffset);
    CommodityIndexedAverageLeg&
    withFutureExpiryCalculator(const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr);
    CommodityIndexedAverageLeg& payAtMaturity(bool flag = false);
    CommodityIndexedAverageLeg& includeEndDate(bool flag = true);
    CommodityIndexedAverageLeg& excludeStartDate(bool flag = true);
    CommodityIndexedAverageLeg& withPaymentDates(const std::vector<QuantLib::Date>& paymentDates);
    CommodityIndexedAverageLeg& useBusinessDays(bool flag = true);
    CommodityIndexedAverageLeg& withQuantityFrequency(CommodityQuantityFrequency quantityFrequency);
    CommodityIndexedAverageLeg& withHoursPerDay(QuantLib::Natural hoursPerDay);
    CommodityIndexedAverageLeg& withDailyExpiryOffset(QuantLib::Natural dailyExpiryOffset);
    CommodityIndexedAverageLeg& unrealisedQuantity(bool flag = false);
    CommodityIndexedAverageLeg& withOffPeakPowerData(const boost::optional<std::pair<QuantLib::Calendar, QuantLib::Real>>& offPeakPowerData);
    CommodityIndexedAverageLeg& withFxIndex(const ext::shared_ptr<FxIndex>& fxIndex);

    operator Leg() const;

private:
    Schedule schedule_;
    ext::shared_ptr<CommodityIndex> index_;
    std::vector<QuantLib::Real> quantities_;
    QuantLib::Natural paymentLag_;
    QuantLib::Calendar paymentCalendar_;
    QuantLib::BusinessDayConvention paymentConvention_;
    QuantLib::Calendar pricingCalendar_;
    std::vector<QuantLib::Real> spreads_;
    std::vector<QuantLib::Real> gearings_;
    CommodityIndexedAverageCashFlow::PaymentTiming paymentTiming_;
    bool useFuturePrice_;
    QuantLib::Natural deliveryDateRoll_;
    QuantLib::Natural futureMonthOffset_;
    ext::shared_ptr<FutureExpiryCalculator> calc_;
    bool payAtMaturity_;
    bool includeEndDate_;
    bool excludeStartDate_;
    std::vector<QuantLib::Date> paymentDates_;
    bool useBusinessDays_;
    CommodityQuantityFrequency quantityFrequency_;
    QuantLib::Natural hoursPerDay_;
    QuantLib::Natural dailyExpiryOffset_;
    bool unrealisedQuantity_;
    boost::optional<std::pair<QuantLib::Calendar, QuantLib::Real>> offPeakPowerData_;
    ext::shared_ptr<FxIndex> fxIndex_;
};

} // namespace QuantExt

#endif
