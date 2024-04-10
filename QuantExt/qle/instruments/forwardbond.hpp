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

#ifndef quantext_forwardbond_hpp
#define quantext_forwardbond_hpp

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
    ForwardBond(const QuantLib::ext::shared_ptr<QuantLib::Bond>& underlying, const QuantLib::ext::shared_ptr<Payoff>& payoff,
                const Date& fwdMaturityDate, const Date& fwdSettlementDate, const bool isPhysicallySettled,
                const bool settlementDirty, const Real compensationPayment, const Date compensationPaymentDate,
                const Real bondNotional = 1.0);
    //! Constructor for tlocks with lock rate
    ForwardBond(const QuantLib::ext::shared_ptr<QuantLib::Bond>& underlying, const Real lockRate,
                const DayCounter& lockRateDayCounter, const bool longInForward, const Date& fwdMaturityDate,
                const Date& fwdSettlementDate, const bool isPhysicallySettled, const bool settlementDirty,
                const Real compensationPayment, const Date compensationPaymentDate, const Real bondNotional = 1.0,
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
    QuantLib::ext::shared_ptr<Payoff> payoff_;    // nullptr for tlocks
    Real lockRate_;                       // Null<Real>() for vanilla forwards
    DayCounter lockRateDayCounter_;       // empty dc for vanilla forwards
    boost::optional<bool> longInForward_; // only filled for tlocks
    Date fwdMaturityDate_;
    Date fwdSettlementDate_;
    bool isPhysicallySettled_;
    bool settlementDirty_;
    Real compensationPayment_;
    Date compensationPaymentDate_;
    Real bondNotional_;
    Real dv01_;
    mutable Real underlyingIncome_;
    mutable Real underlyingSpotValue_;
    mutable Real forwardValue_;
};

//! \ingroup instruments
class ForwardBond::arguments : public virtual PricingEngine::arguments {
public:
    QuantLib::ext::shared_ptr<QuantLib::Bond> underlying;
    QuantLib::ext::shared_ptr<Payoff> payoff;    // nullptr for tlocks
    Real lockRate;                       // Null<Real>() for vanilla forwards
    boost::optional<bool> longInForward; // only filled for tlocks
    DayCounter lockRateDayCounter;       // empty dc for vanilla forwards
    Date fwdMaturityDate;
    Date fwdSettlementDate;
    bool isPhysicallySettled;
    bool settlementDirty;
    Real compensationPayment;
    Date compensationPaymentDate;
    Real bondNotional;
    Real dv01;
    void validate() const override;
};

//! \ingroup instruments
class ForwardBond::results : public Instrument::results {
public:
    Real forwardValue;
    Real underlyingSpotValue;
    Real underlyingIncome;
    void reset() override;
};

//! Class for forward type payoffs
class ForwardBondTypePayoff : public Payoff {
public:
    ForwardBondTypePayoff(Position::Type type, Real strike) : type_(type), strike_(strike) {
        QL_REQUIRE(strike >= 0.0, "negative strike given");
    }
    Position::Type forwardType() const { return type_; };
    Real strike() const { return strike_; };
    //! \name Payoff interface
    //@{
    std::string name() const override { return "ForwardBond"; }
    std::string description() const override;
    Real operator()(Real price) const override;
    //@}
protected:
    Position::Type type_;
    Real strike_;
};

// inline definitions
inline std::string ForwardBondTypePayoff::description() const {
    std::ostringstream result;
    result << name() << ", " << strike() << " strike";
    return result.str();
}

inline Real ForwardBondTypePayoff::operator()(Real price) const {
    switch (type_) {
    case Position::Long:
        return (price - strike_);
    case Position::Short:
        return (strike_ - price);
    default:
        QL_FAIL("unknown/illegal position type");
    }
}

//! \ingroup instruments
class ForwardBond::engine : public GenericEngine<ForwardBond::arguments, ForwardBond::results> {};

} // namespace QuantExt

#endif
