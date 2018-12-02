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

/*! \file crossccybasisswaphelper.hpp
    \brief Cross currency basis swap helper
    \ingroup termstructures
*/

#ifndef quantext_cross_ccy_basis_mtmreset_swap_helper_hpp
#define quantext_cross_ccy_basis_mtmreset_swap_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

#include <qle/instruments/crossccybasismtmresetswap.hpp>

namespace QuantExt {

//! Cross Ccy Basis MtM Reset Swap Rate Helper
/*! Rate helper for bootstrapping over cross currency basis (MtM reset) swap spreads

    Assumes that you have, at a minimum, either:
    - flatIndex with attached YieldTermStructure and flatDiscountCurve
    - spreadIndex with attached YieldTermStructure and spreadDiscountCurve

    The other leg is then solved for i.e. index curve (if no
    YieldTermStructure is attached to its index) or discount curve (if
    its Handle is empty) or both.

    The currencies are deduced from the ibor indexes. The spotFx
    be be quoted with either of these currencies, this is determined
    by the flatIsDomestic flag. The settlement date of the spot is
    assumed to be equal to the settlement date of the swap itself.

            \ingroup termstructures
*/
class CrossCcyBasisMtMResetSwapHelper : public RelativeDateRateHelper {
public:
    CrossCcyBasisMtMResetSwapHelper(const Handle<Quote>& spreadQuote, const Handle<Quote>& spotFX, Natural settlementDays,
                            const Calendar& settlementCalendar, const Period& swapTenor,
                            BusinessDayConvention rollConvention,
                            const boost::shared_ptr<QuantLib::IborIndex>& fgnCcyIndex,
                            const boost::shared_ptr<QuantLib::IborIndex>& domCcyIndex,
                            const Handle<YieldTermStructure>& fgnCcyDiscountCurve,
                            const Handle<YieldTermStructure>& domCcyDiscountCurve,
                            const Handle<YieldTermStructure>& fgnCcyFxFwdRateCurve = Handle<YieldTermStructure>(),
                            const Handle<YieldTermStructure>& domCcyFxFwdRateCurve = Handle<YieldTermStructure>(),
                            bool eom = false,
                            bool spreadOnFgnCcy = true);
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
    boost::shared_ptr<QuantLib::IborIndex> fgnCcyIndex_;
    boost::shared_ptr<QuantLib::IborIndex> domCcyIndex_;
    Handle<YieldTermStructure> fgnCcyDiscountCurve_;
    Handle<YieldTermStructure> domCcyDiscountCurve_;
    Handle<YieldTermStructure> fgnCcyFxFwdRateCurve_;
    Handle<YieldTermStructure> domCcyFxFwdRateCurve_;
    bool eom_, spreadOnFgnCcy_;

    Currency fgnCurrency_;
    Currency domCurrency_;
    boost::shared_ptr<CrossCcyBasisMtMResetSwap> swap_;

    RelinkableHandle<YieldTermStructure> termStructureHandle_;
    RelinkableHandle<YieldTermStructure> fgnDiscountRLH_;
    RelinkableHandle<YieldTermStructure> domDiscountRLH_;
    RelinkableHandle<YieldTermStructure> fgnCcyFxFwdRateCurveRLH_;
    RelinkableHandle<YieldTermStructure> domCcyFxFwdRateCurveRLH_;
};
} // namespace QuantExt

#endif
