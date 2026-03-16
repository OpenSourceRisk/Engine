/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/averageoffpeakpowerhelper.hpp
    \brief Price helper for average of off-peak electricity prices over a period.
    \ingroup termstructures
*/

#ifndef quantext_average_off_peak_power_helper_hpp
#define quantext_average_off_peak_power_helper_hpp

#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
#include <qle/time/futureexpirycalculator.hpp>
#include <ql/termstructures/bootstraphelper.hpp>

namespace QuantExt {

typedef QuantLib::BootstrapHelper<PriceTermStructure> PriceHelper;

//! Helper for bootstrapping using prices that are the average of future settlement prices over a period.
//! \ingroup termstructures
class AverageOffPeakPowerHelper : public PriceHelper {
public:
    //! \name Constructors
    //@{
    /*! \param price           The average price quote.
        \param index           The commodity index. Used to convey the off-peak commodity's name and calendar. The 
                               underlying averaging cashflow may reference more than one commodity future indices.
        \param start           The start date of the averaging period. The averaging period includes the
                               start date if it is a pricing date according to the \p calendar.
        \param end             The end date of the averaging period. The averaging period includes the 
                               end date if it is a pricing date according to the \p calendar.
        \param calc            A FutureExpiryCalculator instance.
        \param peakIndex       The commodity index for the peak electricity prices.
        \param peakCalendar    The calendar used to determine peak dates in the averaging period.
        \param peakHoursPerDay The number of peak hours per day.
    */
    AverageOffPeakPowerHelper(const QuantLib::Handle<QuantLib::Quote>& price,
        const QuantLib::ext::shared_ptr<CommodityIndex>& index,
        const QuantLib::Date& start,
        const QuantLib::Date& end,
        const ext::shared_ptr<FutureExpiryCalculator>& calc,
        const QuantLib::ext::shared_ptr<CommodityIndex>& peakIndex,
        const QuantLib::Calendar& peakCalendar,
        QuantLib::Natural peakHoursPerDay = 16);

    /*! \param price           The average price.
        \param index           The commodity index. Used to convey the off-peak commodity's name and calendar. The
                               underlying averaging cashflow may reference more than one commodity future indices.
        \param start           The start date of the averaging period. The averaging period includes the
                               start date if it is a pricing date according to the \p calendar.
        \param end             The end date of the averaging period. The averaging period includes the
                               end date if it is a pricing date according to the \p calendar.
        \param calc            A FutureExpiryCalculator instance.
        \param peakIndex       The commodity index for the peak electricity prices.
        \param peakCalendar    The calendar used to determine peak dates in the averaging period.
        \param peakHoursPerDay The number of peak hours per day.
    */
    AverageOffPeakPowerHelper(QuantLib::Real price,
        const QuantLib::ext::shared_ptr<CommodityIndex>& index,
        const QuantLib::Date& start,
        const QuantLib::Date& end,
        const ext::shared_ptr<FutureExpiryCalculator>& calc,
        const QuantLib::ext::shared_ptr<CommodityIndex>& peakIndex,
        const QuantLib::Calendar& peakCalendar,
        QuantLib::Natural peakHoursPerDay = 16);
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

    void deepUpdate() override;

private:
    //! Shared initialisation method.
    void init(const QuantLib::ext::shared_ptr<CommodityIndex>& index,
        const QuantLib::Date& start,
        const QuantLib::Date& end,
        const ext::shared_ptr<FutureExpiryCalculator>& calc,
        const QuantLib::ext::shared_ptr<CommodityIndex>& peakIndex,
        const QuantLib::Calendar& peakCalendar,
        QuantLib::Natural peakHoursPerDay);

    QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> businessOffPeak_;
    QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> holidayOffPeak_;
    QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> holidayPeak_;
    QuantLib::Natural peakDays_;
    QuantLib::Natural nonPeakDays_;

    QuantLib::RelinkableHandle<PriceTermStructure> termStructureHandle_;
};

}

#endif
