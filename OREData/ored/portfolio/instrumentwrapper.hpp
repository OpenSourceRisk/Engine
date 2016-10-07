/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file ored/portfolio/instrumentwrapper.hpp
    \brief Base class for wrapper of QL instrument, used to store "state" of trade under each scenario
    \ingroup tradedata
*/

#pragma once

#include <ql/instrument.hpp>
#include <ql/time/date.hpp>
#include <vector>

using QuantLib::Real;

namespace openriskengine {
namespace data {

//! Instrument Wrapper
/*!
  Wrap Instrument base class
  Derived classes should
  - store instrument "state" for each scenario
  - adjust the instrument pricing formula to account for state

  \ingroup tradedata
*/
class InstrumentWrapper {
public:
    InstrumentWrapper(const boost::shared_ptr<QuantLib::Instrument>& inst, const Real multiplier = 1.0)
        : instrument_(inst), multiplier_(multiplier) {}
    virtual ~InstrumentWrapper() {}

    //! Initialise with the given date grid
    virtual void initialise(const std::vector<QuantLib::Date>& dates) = 0;

    //! reset is called every time a new path is about to be priced.
    /*! For path dependent Wrappers, this is when internal state should be reset
     */
    virtual void reset() = 0;

    //! Return the NPV of this instrument
    virtual QuantLib::Real NPV() const = 0;

    //! call update on enclosed instrument(s)
    virtual void updateQlInstruments() { instrument_->update(); }

    //! is it an Option?
    virtual bool isOption() { return false; }

    //! Inspectors
    //@{
    /*! The "QuantLib" instrument */
    boost::shared_ptr<QuantLib::Instrument> qlInstrument() const { return instrument_; }
    /*! The multiplier */
    const Real& multiplier() const { return multiplier_; }
    //@}

protected:
    boost::shared_ptr<QuantLib::Instrument> instrument_;
    Real multiplier_;
};

//! Vanilla Instrument Wrapper
/*!
  Derived from Wrap::InstrumentWrapper
  Used for any non path-dependent trades

  \ingroup tradedata
*/
class VanillaInstrument : public InstrumentWrapper {
public:
    VanillaInstrument(const boost::shared_ptr<QuantLib::Instrument>& inst, const Real multiplier = 1.0)
        : InstrumentWrapper(inst, multiplier) {}

    void initialise(const std::vector<QuantLib::Date>&) override{};
    void reset() override {}

    QuantLib::Real NPV() const override { return instrument_->NPV() * multiplier_; }
};
}
}
