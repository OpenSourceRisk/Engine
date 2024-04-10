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

/*! \file qle/termstructures/crossccyfixfloatswaphelper.hpp
    \brief Cross currency fixed vs. float swap helper

    \ingroup termstructures
*/

#ifndef quantext_cross_ccy_fix_float_swap_helper_hpp
#define quantext_cross_ccy_fix_float_swap_helper_hpp

#include <ql/termstructures/yield/ratehelpers.hpp>

#include <qle/instruments/crossccyfixfloatswap.hpp>

namespace QuantExt {

//! Cross currency fix vs. float swap helper
/*! Rate helper for bootstrapping with fixed vs. float cross currency swaps

    \ingroup termstructures
*/
class CrossCcyFixFloatSwapHelper : public RelativeDateRateHelper {
public:
    CrossCcyFixFloatSwapHelper(const QuantLib::Handle<QuantLib::Quote>& rate,
                               const QuantLib::Handle<QuantLib::Quote>& spotFx, QuantLib::Natural settlementDays,
                               const QuantLib::Calendar& paymentCalendar,
                               QuantLib::BusinessDayConvention paymentConvention, const QuantLib::Period& tenor,
                               const QuantLib::Currency& fixedCurrency, QuantLib::Frequency fixedFrequency,
                               QuantLib::BusinessDayConvention fixedConvention,
                               const QuantLib::DayCounter& fixedDayCount,
                               const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index,
                               const QuantLib::Handle<QuantLib::YieldTermStructure>& floatDiscount,
                               const Handle<Quote>& spread = Handle<Quote>(), bool endOfMonth = false);

    //! \name Observer interface
    //@{
    void update() override;
    //@}

    //! \name BootstrapHelper interface
    //@{
    QuantLib::Real impliedQuote() const override;
    void setTermStructure(QuantLib::YieldTermStructure*) override;
    //@}

    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwap> swap() const { return swap_; }
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor&) override;
    //@}

private:
    //! \name RelativeDateBootstrapHelper interface
    //@{
    void initializeDates() override;
    //@}

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

    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwap> swap_;
    QuantLib::RelinkableHandle<QuantLib::YieldTermStructure> termStructureHandle_;
};

} // namespace QuantExt

#endif
