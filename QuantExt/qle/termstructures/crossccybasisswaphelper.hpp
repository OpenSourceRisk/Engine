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
    be be quoted with either of these currencies, this is determined
    by the flatIsDomestic flag. The settlement date of the spot is
    assumed to be equal to the settlement date of the swap itself.

            \ingroup termstructures
*/
class CrossCcyBasisSwapHelper : public RelativeDateRateHelper {
public:
    CrossCcyBasisSwapHelper(const Handle<Quote>& spreadQuote, const Handle<Quote>& spotFX, Natural settlementDays,
                            const Calendar& settlementCalendar, const Period& swapTenor,
                            BusinessDayConvention rollConvention,
                            const boost::shared_ptr<QuantLib::IborIndex>& flatIndex,
                            const boost::shared_ptr<QuantLib::IborIndex>& spreadIndex,
                            const Handle<YieldTermStructure>& flatDiscountCurve,
                            const Handle<YieldTermStructure>& spreadDiscountCurve, bool eom = false,
                            bool flatIsDomestic = true);
    //! \name RateHelper interface
    //@{
    Real impliedQuote() const;
    void setTermStructure(YieldTermStructure*);
    //@}
    //! \name inspectors
    //@{
    boost::shared_ptr<CrossCcyBasisSwap> swap() const { return swap_; }
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
    boost::shared_ptr<QuantLib::IborIndex> flatIndex_;
    boost::shared_ptr<QuantLib::IborIndex> spreadIndex_;
    Handle<YieldTermStructure> flatDiscountCurve_;
    Handle<YieldTermStructure> spreadDiscountCurve_;
    bool eom_, flatIsDomestic_;

    Currency flatLegCurrency_;
    Currency spreadLegCurrency_;
    boost::shared_ptr<CrossCcyBasisSwap> swap_;

    RelinkableHandle<YieldTermStructure> termStructureHandle_;
    RelinkableHandle<YieldTermStructure> flatDiscountRLH_;
    RelinkableHandle<YieldTermStructure> spreadDiscountRLH_;
};
} // namespace QuantExt

#endif
