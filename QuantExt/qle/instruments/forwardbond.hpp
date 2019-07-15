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
    //! Constructor
    ForwardBond(const boost::shared_ptr<QuantLib::Bond>& underlying, const boost::shared_ptr<Payoff>& payoff,
                const Date& fwdMaturityDate, const bool settlementDirty, const Real compensationPayment,
                const Date compensationPaymentDate);

    //! \name Instrument interface
    //@{
    bool isExpired() const;
    void setupArguments(PricingEngine::arguments*) const;
    void fetchResults(const PricingEngine::results*) const;
    //@}

    //! \name Inspectors
    //@{
    const boost::shared_ptr<QuantLib::Bond>& underlying();
    //@}

private:
    boost::shared_ptr<QuantLib::Bond> underlying_;
    boost::shared_ptr<Payoff> payoff_;
    Date fwdMaturityDate_;
    bool settlementDirty_;
    Real compensationPayment_;
    Date compensationPaymentDate_;
    mutable Real underlyingIncome_;
    mutable Real underlyingSpotValue_;
    mutable Real forwardValue_;
};

//! \ingroup instruments
class ForwardBond::arguments : public virtual PricingEngine::arguments {
public:
    boost::shared_ptr<Payoff> payoff;
    boost::shared_ptr<QuantLib::Bond> underlying;
    Date fwdMaturityDate;
    bool settlementDirty;
    Real compensationPayment;
    Date compensationPaymentDate;
    void validate() const;
};

//! \ingroup instruments
class ForwardBond::results : public Instrument::results {
public:
    Real forwardValue;
    Real underlyingSpotValue;
    Real underlyingIncome;
    void reset();
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
    std::string name() const { return "ForwardBond"; }
    std::string description() const;
    Real operator()(Real price) const;
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
