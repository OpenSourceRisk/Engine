/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file crossccybasismtmresetswaphelper.hpp
    \brief Cross currency basis swap helper with MTM reset
    \ingroup termstructures
*/

#ifndef quantext_cross_ccy_basis_mtmreset_swap_helper_hpp
#define quantext_cross_ccy_basis_mtmreset_swap_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

#include <qle/instruments/crossccybasismtmresetswap.hpp>

namespace QuantExt {

//! Cross Ccy Basis MtM Reset Swap Rate Helper
/*! Rate helper for bootstrapping over cross currency basis (MtM reset) swap spreads

    The resets are applied to the domestic leg (foreign currency is constant notional)

    Assumes that you have, at a minimum, either:
    - foreign ccy index with attached YieldTermStructure and discountCurve
    - domestic ccy with attached YieldTermStructure and discountCurve

    The other leg is then solved for i.e. index curve (if no
    YieldTermStructure is attached to its index) or discount curve (if
    its Handle is empty) or both.

    The currencies are deduced from the ibor indexes. The spotFx
    be be quoted with either of these currencies, this is determined
    by the invertFxIndex flag. The settlement date of the spot is
    assumed to be equal to the settlement date of the swap itself.

    \ingroup termstructures
*/
class CrossCcyBasisMtMResetSwapHelper : public RelativeDateRateHelper {
public:
    CrossCcyBasisMtMResetSwapHelper(
        const Handle<Quote>& spreadQuote, const Handle<Quote>& spotFX, Natural settlementDays,
        const Calendar& settlementCalendar, const Period& swapTenor, BusinessDayConvention rollConvention,
        const boost::shared_ptr<QuantLib::IborIndex>& foreignCcyIndex,
        const boost::shared_ptr<QuantLib::IborIndex>& domesticCcyIndex,
        const Handle<YieldTermStructure>& foreignCcyDiscountCurve,
        const Handle<YieldTermStructure>& domesticCcyDiscountCurve,
        const Handle<YieldTermStructure>& foreignCcyFxFwdRateCurve = Handle<YieldTermStructure>(),
        const Handle<YieldTermStructure>& domesticCcyFxFwdRateCurve = Handle<YieldTermStructure>(), bool eom = false,
        bool spreadOnForeignCcy = true, boost::optional<QuantLib::Period> foreignTenor = boost::none,
        boost::optional<QuantLib::Period> domesticTenor = boost::none);
    //! \name RateHelper interface
    //@{
    Real impliedQuote() const;
    void setTermStructure(YieldTermStructure*);
    //@}
    //! \name inspectors
    //@{
    boost::shared_ptr<CrossCcyBasisMtMResetSwap> swap() const { return swap_; }
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&);
    //@}

protected:
    void initializeDates();

    Handle<Quote> spotFX_;
    Natural settlementDays_;
    Calendar settlementCalendar_;
    Period swapTenor_;
    BusinessDayConvention rollConvention_;
    boost::shared_ptr<QuantLib::IborIndex> foreignCcyIndex_;
    boost::shared_ptr<QuantLib::IborIndex> domesticCcyIndex_;
    Handle<YieldTermStructure> foreignCcyDiscountCurve_;
    Handle<YieldTermStructure> domesticCcyDiscountCurve_;
    Handle<YieldTermStructure> foreignCcyFxFwdRateCurve_;
    Handle<YieldTermStructure> domesticCcyFxFwdRateCurve_;
    bool eom_, spreadOnForeignCcy_;
    QuantLib::Period foreignTenor_;
    QuantLib::Period domesticTenor_;

    Currency foreignCurrency_;
    Currency domesticCurrency_;
    boost::shared_ptr<CrossCcyBasisMtMResetSwap> swap_;

    RelinkableHandle<YieldTermStructure> termStructureHandle_;
    RelinkableHandle<YieldTermStructure> foreignDiscountRLH_;
    RelinkableHandle<YieldTermStructure> domesticDiscountRLH_;
    RelinkableHandle<YieldTermStructure> foreignCcyFxFwdRateCurveRLH_;
    RelinkableHandle<YieldTermStructure> domesticCcyFxFwdRateCurveRLH_;
};
} // namespace QuantExt

#endif
