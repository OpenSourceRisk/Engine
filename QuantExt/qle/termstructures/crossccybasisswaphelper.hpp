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

/*! \file crossccybasisswaphelper.hpp
    \brief Cross currency basis swap helper
    \ingroup termstructures
*/

#ifndef quantext_cross_ccy_basis_swap_helper_hpp
#define quantext_cross_ccy_basis_swap_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

#include <qle/instruments/crossccybasisswap.hpp>

namespace QuantExt {

//! Cross Ccy Basis Swap Rate Helper
/*! Rate helper for bootstrapping over cross currency basis swap spreads

    Assumes that you have, at a minimum, either:
    - flatIndex with attached YieldTermStructure and flatDiscountCurve
    - spreadIndex with attached YieldTermStructure and spreadDiscountCurve

    The other leg is then solved for i.e. index curve (if no
    YieldTermStructure is attached to its index) or discount curve (if
    its Handle is empty) or both.

    The currencies are deduced from the ibor indexes. The spotFx
    to be quoted with either of these currencies, this is determined
    by the flatIsDomestic flag. The settlement date of the spot is
    assumed to be equal to the settlement date of the swap itself.

            \ingroup termstructures
*/
class CrossCcyBasisSwapHelper : public RelativeDateRateHelper {
public:
    CrossCcyBasisSwapHelper(
        const Handle<Quote>& spreadQuote, const Handle<Quote>& spotFX, Natural settlementDays,
        const Calendar& settlementCalendar, const Period& swapTenor, BusinessDayConvention rollConvention,
        const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& flatIndex,
        const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& spreadIndex, const Handle<YieldTermStructure>& flatDiscountCurve,
        const Handle<YieldTermStructure>& spreadDiscountCurve, bool eom = false, bool flatIsDomestic = true,
        boost::optional<QuantLib::Period> flatTenor = boost::none,
        boost::optional<QuantLib::Period> spreadTenor = boost::none, Real spreadOnFlatLeg = 0.0, Real flatGearing = 1.0,
        Real spreadGearing = 1.0, const Calendar& flatCalendar = Calendar(),
        const Calendar& spreadCalendar = Calendar(),
        const std::vector<Natural>& spotFXSettleDaysVec = std::vector<Natural>(),
        const std::vector<Calendar>& spotFXSettleCalendar = std::vector<Calendar>(), Size paymentLag = 0,
        Size flatPaymentLag = 0, boost::optional<bool> includeSpread = boost::none,
        boost::optional<Period> lookback = boost::none, boost::optional<Size> fixingDays = boost::none,
        boost::optional<Size> rateCutoff = boost::none, boost::optional<bool> isAveraged = boost::none,
        boost::optional<bool> flatIncludeSpread = boost::none, boost::optional<Period> flatLookback = boost::none,
        boost::optional<Size> flatFixingDays = boost::none, boost::optional<Size> flatRateCutoff = boost::none,
        boost::optional<bool> flatIsAveraged = boost::none, const bool telescopicValueDates = false);
    //! \name RateHelper interface
    //@{
    Real impliedQuote() const override;
    void setTermStructure(YieldTermStructure*) override;
    //@}
    //! \name inspectors
    //@{
    QuantLib::ext::shared_ptr<CrossCcyBasisSwap> swap() const { return swap_; }
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}

protected:
    void initializeDates() override;

    Handle<Quote> spotFX_;
    Natural settlementDays_;
    Calendar settlementCalendar_;
    Period swapTenor_;
    BusinessDayConvention rollConvention_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> flatIndex_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> spreadIndex_;
    Handle<YieldTermStructure> flatDiscountCurve_;
    Handle<YieldTermStructure> spreadDiscountCurve_;
    bool eom_;
    bool flatIsDomestic_;
    QuantLib::Period flatTenor_;
    QuantLib::Period spreadTenor_;
    Real spreadOnFlatLeg_;
    Real flatGearing_;
    Real spreadGearing_;
    Calendar flatCalendar_;
    Calendar spreadCalendar_;
    std::vector<Natural> spotFXSettleDaysVec_;
    std::vector<Calendar> spotFXSettleCalendarVec_;

    Size paymentLag_;
    Size flatPaymentLag_;
    // OIS only
    boost::optional<bool> includeSpread_;
    boost::optional<QuantLib::Period> lookback_;
    boost::optional<QuantLib::Size> fixingDays_;
    boost::optional<Size> rateCutoff_;
    boost::optional<bool> isAveraged_;
    boost::optional<bool> flatIncludeSpread_;
    boost::optional<QuantLib::Period> flatLookback_;
    boost::optional<QuantLib::Size> flatFixingDays_;
    boost::optional<Size> flatRateCutoff_;
    boost::optional<bool> flatIsAveraged_;

    Currency flatLegCurrency_;
    Currency spreadLegCurrency_;
    QuantLib::ext::shared_ptr<CrossCcyBasisSwap> swap_;

    RelinkableHandle<YieldTermStructure> termStructureHandle_;
    RelinkableHandle<YieldTermStructure> flatDiscountRLH_;
    RelinkableHandle<YieldTermStructure> spreadDiscountRLH_;

    bool telescopicValueDates_;
};
} // namespace QuantExt

#endif
