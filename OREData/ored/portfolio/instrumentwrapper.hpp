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

/*! \file ored/portfolio/instrumentwrapper.hpp
    \brief Base class for wrapper of QL instrument, used to store "state" of trade under each scenario
    \ingroup tradedata
*/

#pragma once

#include <ql/instrument.hpp>
#include <ql/time/date.hpp>
#include <vector>

using QuantLib::Real;

namespace ore {
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
    InstrumentWrapper(const boost::shared_ptr<QuantLib::Instrument>& inst, const Real multiplier = 1.0,
                      const std::vector<boost::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                          std::vector<boost::shared_ptr<QuantLib::Instrument>>(),
                      const std::vector<Real>& additionalMultipliers = std::vector<Real>())
        : instrument_(inst), multiplier_(multiplier), additionalInstruments_(additionalInstruments),
          additionalMultipliers_(additionalMultipliers) {
        QL_REQUIRE(additionalInstruments_.size() == additionalMultipliers_.size(),
                   "vector size mismatch, instruments (" << additionalInstruments_.size() << ") vs multipliers ("
                                                         << additionalMultipliers_.size() << ")");
    }

    virtual ~InstrumentWrapper() {}

    //! Initialise with the given date grid
    virtual void initialise(const std::vector<QuantLib::Date>& dates) = 0;

    //! reset is called every time a new path is about to be priced.
    /*! For path dependent Wrappers, this is when internal state should be reset
     */
    virtual void reset() = 0;

    //! Return the NPV of this instrument
    virtual QuantLib::Real NPV() const = 0;

    QuantLib::Real additionalInstrumentsNPV() const {
        Real npv = 0.0;
        for (QuantLib::Size i = 0; i < additionalInstruments_.size(); ++i)
            npv += additionalInstruments_[i]->NPV() * additionalMultipliers_[i];
        return npv;
    }

    //! call update on enclosed instrument(s)
    virtual void updateQlInstruments() {
        instrument_->update();
        for (QuantLib::Size i = 0; i < additionalInstruments_.size(); ++i)
            additionalInstruments_[i]->update();
    }

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
    std::vector<boost::shared_ptr<QuantLib::Instrument>> additionalInstruments_;
    std::vector<Real> additionalMultipliers_;
};

//! Vanilla Instrument Wrapper
/*!
  Derived from InstrumentWrapper
  Used for any non path-dependent trades

  \ingroup tradedata
*/
class VanillaInstrument : public InstrumentWrapper {
public:
    VanillaInstrument(const boost::shared_ptr<QuantLib::Instrument>& inst, const Real multiplier = 1.0,
                      const std::vector<boost::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                          std::vector<boost::shared_ptr<QuantLib::Instrument>>(),
                      const std::vector<Real>& additionalMultipliers = std::vector<Real>())
        : InstrumentWrapper(inst, multiplier, additionalInstruments, additionalMultipliers) {}

    void initialise(const std::vector<QuantLib::Date>&) override{};
    void reset() override {}

    QuantLib::Real NPV() const override { return instrument_->NPV() * multiplier_ + additionalInstrumentsNPV(); }
};
} // namespace data
} // namespace ore
