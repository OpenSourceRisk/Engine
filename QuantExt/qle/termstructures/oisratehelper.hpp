/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/oisratehelper.hpp
    \brief Overnight Indexed Swap (aka OIS) rate helpers
    \ingroup termstructures
*/

#ifndef quantext_oisratehelper_hpp
#define quantext_oisratehelper_hpp

#include <ql/instruments/overnightindexedswap.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Rate helper for bootstrapping using Overnight Indexed Swaps
/*! \ingroup termstructures
 */
class OISRateHelper : public RelativeDateRateHelper {
public:
    OISRateHelper(Natural settlementDays, const Period& swapTenor, const Handle<Quote>& fixedRate,
                  const QuantLib::ext::shared_ptr<OvernightIndex>& overnightIndex, const DayCounter& fixedDayCounter,
                  const Calendar& fixedCalendar, Natural paymentLag = 0, bool endOfMonth = false,
                  Frequency paymentFrequency = Annual, BusinessDayConvention fixedConvention = Following,
                  BusinessDayConvention paymentAdjustment = Following,
                  DateGeneration::Rule rule = DateGeneration::Backward,
                  const Handle<YieldTermStructure>& discountingCurve = Handle<YieldTermStructure>(),
                  bool telescopicValueDates = false, Pillar::Choice pillar = Pillar::LastRelevantDate,
                  Date customPillarDate = Date());
    //! \name RateHelper interface
    //@{
    Real impliedQuote() const override;
    void setTermStructure(YieldTermStructure*) override;
    //@}
    //! \name inspectors
    //@{
    QuantLib::ext::shared_ptr<OvernightIndexedSwap> swap() const { return swap_; }
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}
protected:
    void initializeDates() override;

    Natural settlementDays_;
    Period swapTenor_;
    QuantLib::ext::shared_ptr<OvernightIndex> overnightIndex_;
    DayCounter fixedDayCounter_;
    Calendar fixedCalendar_;
    Natural paymentLag_;
    bool endOfMonth_;
    Frequency paymentFrequency_;
    BusinessDayConvention fixedConvention_;
    BusinessDayConvention paymentAdjustment_;
    DateGeneration::Rule rule_;

    QuantLib::ext::shared_ptr<OvernightIndexedSwap> swap_;
    RelinkableHandle<YieldTermStructure> termStructureHandle_;
    Handle<YieldTermStructure> discountHandle_;
    RelinkableHandle<YieldTermStructure> discountRelinkableHandle_;
    bool telescopicValueDates_;
    Pillar::Choice pillarChoice_;
};

//! Rate helper for bootstrapping using Overnight Indexed Swaps
/*! \ingroup termstructures
 */
class DatedOISRateHelper : public RateHelper {
public:
    DatedOISRateHelper(const Date& startDate, const Date& endDate, const Handle<Quote>& fixedRate,
                       const QuantLib::ext::shared_ptr<OvernightIndex>& overnightIndex, const DayCounter& fixedDayCounter,
                       const Calendar& fixedCalendar, Natural paymentLag = 0, Frequency paymentFrequency = Annual,
                       BusinessDayConvention fixedConvention = Following,
                       BusinessDayConvention paymentAdjustment = Following,
                       DateGeneration::Rule rule = DateGeneration::Backward,
                       const Handle<YieldTermStructure>& discountingCurve = Handle<YieldTermStructure>(),
                       bool telescopicValueDates = false, Pillar::Choice pillar = Pillar::LastRelevantDate,
                       Date customPillarDate = Date());

    //! \name RateHelper interface
    //@{
    Real impliedQuote() const override;
    void setTermStructure(YieldTermStructure*) override;
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}
protected:
    QuantLib::ext::shared_ptr<OvernightIndex> overnightIndex_;
    DayCounter fixedDayCounter_;
    Calendar fixedCalendar_;
    Natural paymentLag_;
    Frequency paymentFrequency_;
    BusinessDayConvention fixedConvention_;
    BusinessDayConvention paymentAdjustment_;
    DateGeneration::Rule rule_;

    QuantLib::ext::shared_ptr<OvernightIndexedSwap> swap_;
    RelinkableHandle<YieldTermStructure> termStructureHandle_;
    Handle<YieldTermStructure> discountHandle_;
    RelinkableHandle<YieldTermStructure> discountRelinkableHandle_;
    bool telescopicValueDates_;
    Pillar::Choice pillarChoice_;
};
} // namespace QuantExt

#endif
