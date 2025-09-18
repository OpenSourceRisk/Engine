/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file forwardbond.hpp
    \brief Forward bond class
*/

#pragma once

#include <ql/handle.hpp>
#include <ql/instrument.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/interestrate.hpp>
#include <ql/payoff.hpp>
#include <ql/position.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/types.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Forward Bond class
class ForwardBond : public Instrument {
public:
    class arguments;
    class engine;
    class results;
    //! Constructor vanilla forward bond
    ForwardBond(const QuantLib::ext::shared_ptr<QuantLib::Bond>& underlying, const Real strikeAmount,
                const Date& fwdMaturityDate, const Date& fwdSettlementDate, const bool isPhysicallySettled,
                const bool settlementDirty, const Real compensationPayment, const Date compensationPaymentDate,
                const bool isLong, const Real bondNotional = 1.0);
    //! Constructor for tlocks with lock rate
    ForwardBond(const QuantLib::ext::shared_ptr<QuantLib::Bond>& underlying, const Real lockRate,
                const DayCounter& lockRateDayCounter, const bool longInForward, const Date& fwdMaturityDate,
                const Date& fwdSettlementDate, const bool isPhysicallySettled, const bool settlementDirty,
                const Real compensationPayment, const Date compensationPaymentDate, 
                const bool isLong,
                const Real bondNotional = 1.0,
                const Real dv01 = Null<Real>());

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override;
    void fetchResults(const PricingEngine::results*) const override;
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<QuantLib::Bond>& underlying() { return underlying_; }
    //@}

private:
    QuantLib::ext::shared_ptr<QuantLib::Bond> underlying_;
    Real strikeAmount_;                        // Null<Real>() for tlocks
    Real lockRate_;                            // Null<Real>() for vanilla forwards
    DayCounter lockRateDayCounter_;            // filled only for tlocks
    Date fwdMaturityDate_;
    Date fwdSettlementDate_;
    bool isPhysicallySettled_;
    bool settlementDirty_;
    Real compensationPayment_;
    Date compensationPaymentDate_;
    bool isLong_;
    Real bondNotional_;
    Real dv01_;
};

//! \ingroup instruments
class ForwardBond::arguments : public virtual PricingEngine::arguments {
public:
    QuantLib::ext::shared_ptr<QuantLib::Bond> underlying;
    Real strikeAmount;                        // Null<Real>() for tlocks
    Real lockRate;                            // Null<Real>() for vanilla forwards
    DayCounter lockRateDayCounter;            // filled only for tlocks
    Date fwdMaturityDate;
    Date fwdSettlementDate;
    bool isPhysicallySettled;
    bool settlementDirty;
    Real compensationPayment;
    Date compensationPaymentDate;
    bool isLong;
    Real bondNotional;
    Real dv01;
    void validate() const override;
};

//! \ingroup instruments
class ForwardBond::results : public Instrument::results {
public:
    void reset() override;
};

//! \ingroup instruments
class ForwardBond::engine : public GenericEngine<ForwardBond::arguments, ForwardBond::results> {};

} // namespace QuantExt
