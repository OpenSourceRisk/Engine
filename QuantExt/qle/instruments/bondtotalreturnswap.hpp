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

#ifndef quantext_bondtrs_hpp
#define quantext_bondtrs_hpp

#include <ql/handle.hpp>
#include <ql/instrument.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/interestrate.hpp>
#include <ql/position.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>
#include <ql/types.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Bond TRS class
class BondTRS : public Instrument {
public:
    class arguments;
    class engine;
    class results;
    //! Constructor
    BondTRS(const boost::shared_ptr<QuantLib::Bond>&, const QuantLib::Leg&, const QuantLib::Schedule&, const Date&,
            const Date&, const bool&);

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
    QuantLib::Leg fundingLeg_;
    QuantLib::Schedule compensationPaymentsSchedule_;
    Date bondTRSStartDate_;
    Date bondTRSMaturityDate_;
    bool longInBond_;
    mutable Real underlyingSpotValue_;
    mutable Real fundingLegSpotValue_;
    mutable Real compensationPaymentsSpotValue_;
};

//! \ingroup instruments
class BondTRS::arguments : public virtual PricingEngine::arguments {
public:
    boost::shared_ptr<QuantLib::Bond> underlying;
    QuantLib::Leg fundingLeg;
    QuantLib::Schedule compensationPaymentsSchedule;
    Date bondTRSMaturityDate;
    Date bondTRSStartDate;
    bool longInBond;
    void validate() const;
};

//! \ingroup instruments
class BondTRS::results : public Instrument::results {
public:
    Real underlyingSpotValue;
    Real fundingLegSpotValue;
    Real compensationPaymentsSpotValue;
    void reset();
};

//! \ingroup instruments
class BondTRS::engine : public GenericEngine<BondTRS::arguments, BondTRS::results> {};

} // namespace QuantExt

#endif
