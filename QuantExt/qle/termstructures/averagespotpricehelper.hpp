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

/*! \file qle/termstructures/averagespotpricehelper.hpp
    \brief Price helper for average of spot price over a period.
    \ingroup termstructures
*/

#ifndef quantext_averagespotpricehelper_hpp
#define quantext_averagespotpricehelper_hpp

#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
#include <ql/termstructures/bootstraphelper.hpp>

namespace QuantExt {

typedef QuantLib::BootstrapHelper<PriceTermStructure> PriceHelper;

//! Helper for bootstrapping using prices that are the average of a spot price over a period.
//! \ingroup termstructures
class AverageSpotPriceHelper : public PriceHelper {
public:
    //! \name Constructors
    //@{
    /*! \param price           The average price quote.
        \param index           The commodity spot index.
        \param start           The start date of the averaging period. The averaging period includes the
                               start date if it is a pricing date according to the \p calendar.
        \param end             The end date of the averaging period. The averaging period includes the 
                               end date if it is a pricing date according to the \p calendar.
        \param calendar        The calendar used to determine pricing dates in the averaging period. If not provided, 
                               the \p index calendar is used.
        \param useBusinessDays If set to \c false, the averaging happens on the complement of the pricing calendar
                               dates in the period.
    */
    AverageSpotPriceHelper(const QuantLib::Handle<QuantLib::Quote>& price,
        const QuantLib::ext::shared_ptr<CommoditySpotIndex>& index,
        const QuantLib::Date& start,
        const QuantLib::Date& end,
        const QuantLib::Calendar& calendar = QuantLib::Calendar(),
        bool useBusinessDays = true);

    /*! \param price           The average price.
        \param index           The commodity spot index.
        \param start           The start date of the averaging period. The averaging period includes the
                               start date if it is a pricing date according to the \p calendar.
        \param end             The end date of the averaging period. The averaging period includes the
                               end date if it is a pricing date according to the \p calendar.
        \param calendar        The calendar used to determine pricing dates in the averaging period. If not provided, 
                               the \p index calendar is used.
        \param useBusinessDays If set to \c false, the averaging happens on the complement of the pricing calendar
                               dates in the period.
    */
    AverageSpotPriceHelper(QuantLib::Real price,
        const QuantLib::ext::shared_ptr<CommoditySpotIndex>& index,
        const QuantLib::Date& start,
        const QuantLib::Date& end,
        const QuantLib::Calendar& calendar = QuantLib::Calendar(),
        bool useBusinessDays = true);
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

private:
    //! Shared initialisation method.
    void init(const QuantLib::ext::shared_ptr<CommoditySpotIndex>& index,
        const QuantLib::Date& start,
        const QuantLib::Date& end,
        const QuantLib::Calendar& calendar,
        bool useBusinessDays);

    QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> averageCashflow_;
    QuantLib::RelinkableHandle<PriceTermStructure> termStructureHandle_;
};

}

#endif
