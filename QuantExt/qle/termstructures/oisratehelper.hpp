/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/termstructures/oisratehelper.hpp
    \brief Overnight Indexed Swap (aka OIS) rate helpers
*/

#ifndef quantext_oisratehelper_hpp
#define quantext_oisratehelper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/instruments/overnightindexedswap.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! Rate helper for bootstrapping using Overnight Indexed Swaps
    class OISRateHelper : public RelativeDateRateHelper {
      public:
        OISRateHelper(Natural settlementDays,
            const Period& swapTenor,
            const Handle<Quote>& fixedRate,
            const boost::shared_ptr<OvernightIndex>& overnightIndex,
            const DayCounter& fixedDayCounter,
            Natural paymentLag = 0,
            bool endOfMonth = false,
            Frequency paymentFrequency = Annual,
            BusinessDayConvention fixedConvention = Following,
            BusinessDayConvention paymentAdjustment = Following,
            DateGeneration::Rule rule = DateGeneration::Backward,
            const Handle<YieldTermStructure>& discountingCurve
                = Handle<YieldTermStructure>());
        //! \name RateHelper interface
        //@{
        Real impliedQuote() const;
        void setTermStructure(YieldTermStructure*);
        //@}
        //! \name inspectors
        //@{
        boost::shared_ptr<OvernightIndexedSwap> swap() const { return swap_; }
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&);
        //@}
    protected:
        void initializeDates();

        Natural settlementDays_;
        Period swapTenor_;
        boost::shared_ptr<OvernightIndex> overnightIndex_;
        DayCounter fixedDayCounter_;
        Natural paymentLag_;
        bool endOfMonth_;
        Frequency paymentFrequency_;
        BusinessDayConvention fixedConvention_;
        BusinessDayConvention paymentAdjustment_;
        DateGeneration::Rule rule_;

        boost::shared_ptr<OvernightIndexedSwap> swap_;
        RelinkableHandle<YieldTermStructure> termStructureHandle_;
        Handle<YieldTermStructure> discountHandle_;
        RelinkableHandle<YieldTermStructure> discountRelinkableHandle_;
    };

    //! Rate helper for bootstrapping using Overnight Indexed Swaps
    class DatedOISRateHelper : public RateHelper {
      public:
        DatedOISRateHelper(const Date& startDate,
            const Date& endDate,
            const Handle<Quote>& fixedRate,
            const boost::shared_ptr<OvernightIndex>& overnightIndex,
            const DayCounter& fixedDayCounter,
            Natural paymentLag = 0,
            Frequency paymentFrequency = Annual,
            BusinessDayConvention fixedConvention = Following,
            BusinessDayConvention paymentAdjustment = Following,
            DateGeneration::Rule rule = DateGeneration::Backward,
            const Handle<YieldTermStructure>& discountingCurve
                = Handle<YieldTermStructure>());
        //! \name RateHelper interface
        //@{
        Real impliedQuote() const;
        void setTermStructure(YieldTermStructure*);
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&);
        //@}
    protected:
        boost::shared_ptr<OvernightIndex> overnightIndex_;
        DayCounter fixedDayCounter_;
        Natural paymentLag_;
        Frequency paymentFrequency_;
        BusinessDayConvention fixedConvention_;
        BusinessDayConvention paymentAdjustment_;
        DateGeneration::Rule rule_;

        boost::shared_ptr<OvernightIndexedSwap> swap_;
        RelinkableHandle<YieldTermStructure> termStructureHandle_;
        Handle<YieldTermStructure> discountHandle_;
        RelinkableHandle<YieldTermStructure> discountRelinkableHandle_;
    };

}

#endif
