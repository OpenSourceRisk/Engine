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

/*! \file crossccybasismtmresetswaphelper.hpp
    \brief Cross currency basis swap helper with MTM reset
    \ingroup termstructures
*/

#ifndef quantext_cross_ccy_fix_float_mtmreset_swap_helper_hpp
#define quantext_cross_ccy_fix_float_mtmreset_swap_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

#include <qle/instruments/crossccyfixfloatmtmresetswap.hpp>

namespace QuantExt {

//! Cross Ccy Fix Float MtM Reset Swap Rate Helper
/*! Rate helper for bootstrapping over cross currency fix float (MtM reset) swap 

    The resets are applied to the domestic leg (foreign currency is constant notional)

    Assumes that you have, at a minimum, either:
    - foreign ccy index with attached YieldTermStructure and discountCurve
    - domestic ccy with attached YieldTermStructure and discountCurve

    The other leg is then solved for i.e. index curve (if no
    YieldTermStructure is attached to its index) or discount curve (if
    its Handle is empty) or both.

    The currencies are deduced from the ibor indexes. The spotFx
    to be quoted with either of these currencies, this is determined
    by the invertFxIndex flag. The settlement date of the spot is
    assumed to be equal to the settlement date of the swap itself.

    \ingroup termstructures
*/
class CrossCcyFixFloatMtMResetSwapHelper : public RelativeDateRateHelper {
public:
    CrossCcyFixFloatMtMResetSwapHelper(
        const QuantLib::Handle<QuantLib::Quote>& rate,
        const QuantLib::Handle<QuantLib::Quote>& spotFx, QuantLib::Natural settlementDays,
        const QuantLib::Calendar& paymentCalendar,
        QuantLib::BusinessDayConvention paymentConvention, const QuantLib::Period& tenor,
        const QuantLib::Currency& fixedCurrency, QuantLib::Frequency fixedFrequency,
        QuantLib::BusinessDayConvention fixedConvention,
        const QuantLib::DayCounter& fixedDayCount,
        const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& floatDiscount,
        const Handle<Quote>& spread = Handle<Quote>(), bool endOfMonth = false, 
        bool resetsOnFloatLeg = true);
    //! \name RateHelper interface
    //@{
    Real impliedQuote() const override;
    void setTermStructure(YieldTermStructure*) override;
    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
    //! \name inspectors
    //@{
    QuantLib::ext::shared_ptr<CrossCcyFixFloatMtMResetSwap> swap() const { return swap_; }
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}

protected:
    void initializeDates() override;

    QuantLib::Handle<QuantLib::Quote> spotFx_;
    QuantLib::Natural settlementDays_;
    QuantLib::Calendar paymentCalendar_;
    QuantLib::BusinessDayConvention paymentConvention_;
    QuantLib::Period tenor_;
    QuantLib::Currency fixedCurrency_;
    QuantLib::Frequency fixedFrequency_;
    QuantLib::BusinessDayConvention fixedConvention_;
    QuantLib::DayCounter fixedDayCount_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> index_;
    QuantLib::Handle<QuantLib::YieldTermStructure> floatDiscount_;
    QuantLib::Handle<QuantLib::Quote> spread_;
    bool endOfMonth_;
    bool resetsOnFloatLeg_;

    QuantLib::ext::shared_ptr<CrossCcyFixFloatMtMResetSwap> swap_;

    RelinkableHandle<YieldTermStructure> termStructureHandle_;
};
} // namespace QuantExt

#endif
