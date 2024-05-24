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

/*! \file crossccyfixfloatmtmresetswap.hpp
    \brief Cross currency fix float swap instrument with MTM reset
    \ingroup instruments
*/

#ifndef quantext_cross_ccy_fix_float_mtmreset_swap_hpp
#define quantext_cross_ccy_fix_float_mtmreset_swap_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>

#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/crossccyswap.hpp>

namespace QuantExt {

//! Cross currency fix float MtM resettable swap
/*! The foreign leg holds the pay currency cashflows and domestic leg holds
    the receive currency cashflows. The notional resets are applied to the
    domestic leg.

            \ingroup instruments
*/
class CrossCcyFixFloatMtMResetSwap : public CrossCcySwap {
public:
    class arguments;
    class results;
    //! \name Constructors
    //@{
    /*! First leg holds the pay currency cashflows and the second leg
        holds the receive currency cashflows.
    */
    CrossCcyFixFloatMtMResetSwap(QuantLib::Real nominal, const QuantLib::Currency& fixedCurrency, const QuantLib::Schedule& fixedSchedule,
        QuantLib::Rate fixedRate, const QuantLib::DayCounter& fixedDayCount, const QuantLib::BusinessDayConvention& fixedPaymentBdc,
        QuantLib::Natural fixedPaymentLag, const QuantLib::Calendar& fixedPaymentCalendar, const QuantLib::Currency& floatCurrency,
        const QuantLib::Schedule& floatSchedule, const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& floatIndex, QuantLib::Spread floatSpread,
        const QuantLib::BusinessDayConvention& floatPaymentBdc, QuantLib::Natural floatPaymentLag, const QuantLib::Calendar& floatPaymentCalendar,
        const QuantLib::ext::shared_ptr<FxIndex>& fxIdx, bool resetsOnFloatLeg = true, bool receiveFixed = true);

    //@}
    //! \name Instrument interface
    //@{
    void setupArguments(PricingEngine::arguments* args) const override;
    void fetchResults(const PricingEngine::results*) const override;
    //@}
    //! \name Inspectors
    //@{
    QuantLib::Real nominal() const { return nominal_; }
    const QuantLib::Currency& fixedCurrency() const { return fixedCurrency_; }
    const QuantLib::Schedule& fixedSchedule() const { return fixedSchedule_; }
    QuantLib::Rate fixedRate() const { return fixedRate_; }
    const QuantLib::DayCounter& fixedDayCount() const { return fixedDayCount_; }
    const QuantLib::BusinessDayConvention& fixedPaymentBdc() const { return fixedPaymentBdc_; }
    QuantLib::Natural fixedPaymentLag() const { return fixedPaymentLag_; }
    const QuantLib::Calendar& fixedPaymentCalendar() const { return fixedPaymentCalendar_; }

    const QuantLib::Currency& floatCurrency() const { return floatCurrency_; }
    const QuantLib::Schedule& floatSchedule() const { return floatSchedule_; }
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& floatIndex() const { return floatIndex_; }
    QuantLib::Spread floatSpread() const { return floatSpread_; }
    //@}

    //! \name Additional interface
    //@{
    QuantLib::Rate fairFixedRate() const {
        calculate();
        QL_REQUIRE(fairFixedRate_ != QuantLib::Null<QuantLib::Real>(), "Fair fixed rate is not available");
        return fairFixedRate_;
    }

    QuantLib::Spread fairSpread() const {
        calculate();
        QL_REQUIRE(fairSpread_ != QuantLib::Null<QuantLib::Real>(), "Fair spread is not available");
        return fairSpread_;
    }
    //@}

protected:
    //! \name Instrument interface
    //@{
    void setupExpired() const override;
    //@}

private:
    void initialize();

    //! nominal of non resetting leg
    Real nominal_;
    QuantLib::Currency fixedCurrency_;
    QuantLib::Schedule fixedSchedule_;
    QuantLib::Rate fixedRate_; 
    QuantLib::DayCounter fixedDayCount_;
    QuantLib::BusinessDayConvention fixedPaymentBdc_;
    QuantLib::Natural fixedPaymentLag_;
    QuantLib::Calendar fixedPaymentCalendar_;

    QuantLib::Currency floatCurrency_;
    QuantLib::Schedule floatSchedule_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> floatIndex_;
    QuantLib::Spread floatSpread_;
    QuantLib::BusinessDayConvention floatPaymentBdc_;
    QuantLib::Natural floatPaymentLag_;
    QuantLib::Calendar floatPaymentCalendar_;

    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    bool resetsOnFloatLeg_;
    bool receiveFixed_;

    mutable QuantLib::Spread fairSpread_;
    mutable QuantLib::Rate fairFixedRate_;
};

//! \ingroup instruments
class CrossCcyFixFloatMtMResetSwap::arguments : public CrossCcySwap::arguments {
public:
    QuantLib::Spread spread;
    QuantLib::Rate fixedRate;
    void validate() const override;
};

//! \ingroup instruments
class CrossCcyFixFloatMtMResetSwap::results : public CrossCcySwap::results {
public:
    QuantLib::Spread fairSpread;
    QuantLib::Rate fairFixedRate;
    void reset() override;
};
} // namespace QuantExt

#endif
