/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/averagefuturepricehelper.hpp
    \brief Price helper for average of future settlement prices over a period.
    \ingroup termstructures
*/

#ifndef quantext_averagefuturepricehelper_hpp
#define quantext_averagefuturepricehelper_hpp

#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
#include <qle/time/futureexpirycalculator.hpp>
#include <ql/termstructures/bootstraphelper.hpp>

namespace QuantExt {

typedef QuantLib::BootstrapHelper<PriceTermStructure> PriceHelper;

//! Helper for bootstrapping using prices that are the average of future settlement prices over a period.
//! \ingroup termstructures
class AverageFuturePriceHelper : public PriceHelper {
public:
    //! \name Constructors
    //@{
    /*! \param price             The average price quote.
        \param index             The commodity index. Used to convey the commodity's name and calendar. The underlying 
                                 averaging cashflow may reference more than one commodity future indices.
        \param start             The start date of the averaging period. The averaging period includes the
                                 start date if it is a pricing date according to the \p calendar.
        \param end               The end date of the averaging period. The averaging period includes the 
                                 end date if it is a pricing date according to the \p calendar.
        \param calc              A FutureExpiryCalculator instance.
        \param calendar          The calendar used to determine pricing dates in the averaging period. If not 
                                 provided, the \p index calendar is used.
        \param deliveryDateRoll  The number of pricing days before the prompt future expiry date on which to roll to 
                                 using the next future contract in the averaging.
        \param futureMonthOffset Use a positive integer to select a non-prompt future contract in the averaging.
        \param useBusinessDays   If set to \c false, the averaging happens on the complement of the pricing calendar
                                 dates in the period. This is useful for some electricity futures.
        \param dailyExpiryOffset If set to \c Null<Natural>(), this is ignored. If set to a positive integer, it is 
                                 the number of business days on the \c index calendar to offset each daily expiry date 
                                 on each pricing date.
    */
    AverageFuturePriceHelper(const QuantLib::Handle<QuantLib::Quote>& price,
        const QuantLib::ext::shared_ptr<CommodityIndex>& index,
        const QuantLib::Date& start,
        const QuantLib::Date& end,
        const ext::shared_ptr<FutureExpiryCalculator>& calc,
        const QuantLib::Calendar& calendar = QuantLib::Calendar(),
        QuantLib::Natural deliveryDateRoll = 0,
        QuantLib::Natural futureMonthOffset = 0,
        bool useBusinessDays = true,
        QuantLib::Natural dailyExpiryOffset = QuantLib::Null<QuantLib::Natural>());

    /*! \param price             The average price.
        \param index             The commodity index. Used to convey the commodity's name and calendar. The underlying 
                                 averaging cashflow may reference more than one commodity future indices.
        \param start             The start date of the averaging period. The averaging period includes the
                                 start date if it is a pricing date according to the \p calendar.
        \param end               The end date of the averaging period. The averaging period includes the 
                                 end date if it is a pricing date according to the \p calendar.
        \param calc              A FutureExpiryCalculator instance.
        \param calendar          The calendar used to determine pricing dates in the averaging period. If not 
                                 provided, the \p index calendar is used.
        \param deliveryDateRoll  The number of pricing days before the prompt future expiry date on which to roll to 
                                 using the next future contract in the averaging.
        \param futureMonthOffset Use a positive integer to select a non-prompt future contract in the averaging.
        \param useBusinessDays   If set to \c false, the averaging happens on the complement of the pricing calendar
                                 dates in the period. This is useful for some electricity futures.
        \param dailyExpiryOffset If set to \c Null<Natural>(), this is ignored. If set to a positive integer, it is
                                 the number of business days on the \c index calendar to offset each daily expiry date
                                 on each pricing date.
    */
    AverageFuturePriceHelper(QuantLib::Real price,
        const QuantLib::ext::shared_ptr<CommodityIndex>& index,
        const QuantLib::Date& start,
        const QuantLib::Date& end,
        const ext::shared_ptr<FutureExpiryCalculator>& calc,
        const QuantLib::Calendar& calendar = QuantLib::Calendar(),
        QuantLib::Natural deliveryDateRoll = 0,
        QuantLib::Natural futureMonthOffset = 0,
        bool useBusinessDays = true,
        QuantLib::Natural dailyExpiryOffset = QuantLib::Null<QuantLib::Natural>());
    //@}

    //! \name PriceHelper interface
    //@{
    QuantLib::Real impliedQuote() const override;
    void setTermStructure(PriceTermStructure* ts) override;
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor& v) override;
    //@}

    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> averageCashflow() const;
    //@}

    void deepUpdate() override;

private:
    //! Shared initialisation method.
    void init(const QuantLib::ext::shared_ptr<CommodityIndex>& index,
        const QuantLib::Date& start,
        const QuantLib::Date& end,
        const ext::shared_ptr<FutureExpiryCalculator>& calc,
        const QuantLib::Calendar& calendar,
        QuantLib::Natural deliveryDateRoll,
        QuantLib::Natural futureMonthOffset,
        bool useBusinessDays,
        QuantLib::Natural dailyExpiryOffset);

    QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> averageCashflow_;
    QuantLib::RelinkableHandle<PriceTermStructure> termStructureHandle_;
};

}

#endif
