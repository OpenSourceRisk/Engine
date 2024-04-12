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

/*! \file qle/instruments/crossccyfixfloatswap.hpp
    \brief Cross currency fixed vs float swap instrument
    \ingroup instruments
*/

#ifndef quantext_cross_ccy_fix_float_swap_hpp
#define quantext_cross_ccy_fix_float_swap_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>
#include <qle/instruments/crossccyswap.hpp>

namespace QuantExt {

/*! Cross currency fixed vs float swap
    \ingroup instruments
*/
class CrossCcyFixFloatSwap : public CrossCcySwap {
public:
    enum Type { Receiver = -1, Payer = 1 };
    class arguments;
    class results;

    //! \name Constructors
    //@{
    //! Detailed constructor
    CrossCcyFixFloatSwap(Type type, QuantLib::Real fixedNominal, const QuantLib::Currency& fixedCurrency,
                         const QuantLib::Schedule& fixedSchedule, QuantLib::Rate fixedRate,
                         const QuantLib::DayCounter& fixedDayCount, QuantLib::BusinessDayConvention fixedPaymentBdc,
                         QuantLib::Natural fixedPaymentLag, const QuantLib::Calendar& fixedPaymentCalendar,
                         QuantLib::Real floatNominal, const QuantLib::Currency& floatCurrency,
                         const QuantLib::Schedule& floatSchedule,
                         const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& floatIndex, QuantLib::Spread floatSpread,
                         QuantLib::BusinessDayConvention floatPaymentBdc, QuantLib::Natural floatPaymentLag,
                         const QuantLib::Calendar& floatPaymentCalendar);
    //@}

    //! \name Instrument interface
    //@{
    void setupArguments(QuantLib::PricingEngine::arguments* a) const override;
    void fetchResults(const QuantLib::PricingEngine::results* r) const override;
    //@}

    //! \name Inspectors
    //@{
    Type type() const { return type_; }

    QuantLib::Real fixedNominal() const { return fixedNominal_; }
    const QuantLib::Currency& fixedCurrency() const { return fixedCurrency_; }
    const QuantLib::Schedule& fixedSchedule() const { return fixedSchedule_; }
    QuantLib::Rate fixedRate() const { return fixedRate_; }
    const QuantLib::DayCounter& fixedDayCount() const { return fixedDayCount_; }
    QuantLib::BusinessDayConvention fixedPaymentBdc() const { return fixedPaymentBdc_; }
    QuantLib::Natural fixedPaymentLag() const { return fixedPaymentLag_; }
    const QuantLib::Calendar& fixedPaymentCalendar() const { return fixedPaymentCalendar_; }

    QuantLib::Real floatNominal() const { return floatNominal_; }
    const QuantLib::Currency& floatCurrency() const { return floatCurrency_; }
    const QuantLib::Schedule& floatSchedule() const { return floatSchedule_; }
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& floatIndex() const { return floatIndex_; }
    QuantLib::Rate floatSpread() const { return floatSpread_; }
    QuantLib::BusinessDayConvention floatPaymentBdc() const { return floatPaymentBdc_; }
    QuantLib::Natural floatPaymentLag() const { return floatPaymentLag_; }
    const QuantLib::Calendar& floatPaymentCalendar() const { return floatPaymentCalendar_; }
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
    Type type_;

    QuantLib::Real fixedNominal_;
    QuantLib::Currency fixedCurrency_;
    QuantLib::Schedule fixedSchedule_;
    QuantLib::Rate fixedRate_;
    QuantLib::DayCounter fixedDayCount_;
    QuantLib::BusinessDayConvention fixedPaymentBdc_;
    QuantLib::Natural fixedPaymentLag_;
    QuantLib::Calendar fixedPaymentCalendar_;

    QuantLib::Real floatNominal_;
    QuantLib::Currency floatCurrency_;
    QuantLib::Schedule floatSchedule_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> floatIndex_;
    QuantLib::Spread floatSpread_;
    QuantLib::BusinessDayConvention floatPaymentBdc_;
    QuantLib::Natural floatPaymentLag_;
    QuantLib::Calendar floatPaymentCalendar_;

    mutable QuantLib::Rate fairFixedRate_;
    mutable QuantLib::Spread fairSpread_;
};

//! \ingroup instruments
class CrossCcyFixFloatSwap::arguments : public CrossCcySwap::arguments {
public:
    QuantLib::Rate fixedRate;
    QuantLib::Spread spread;
    void validate() const override;
};

//! \ingroup instruments
class CrossCcyFixFloatSwap::results : public CrossCcySwap::results {
public:
    QuantLib::Rate fairFixedRate;
    QuantLib::Spread fairSpread;
    void reset() override;
};

} // namespace QuantExt

#endif
