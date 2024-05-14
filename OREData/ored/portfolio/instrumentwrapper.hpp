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

#include <boost/timer/timer.hpp>

namespace ore {
namespace data {

using QuantLib::Real;

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
    InstrumentWrapper();
    InstrumentWrapper(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& inst, const Real multiplier = 1.0,
                      const std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                          std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>(),
                      const std::vector<Real>& additionalMultipliers = std::vector<Real>());

    virtual ~InstrumentWrapper() {}

    //! Initialise with the given date grid
    virtual void initialise(const std::vector<QuantLib::Date>& dates) = 0;

    //! reset is called every time a new path is about to be priced.
    /*! For path dependent Wrappers, this is when internal state should be reset
     */
    virtual void reset() = 0;

    //! Return the NPV of this instrument
    virtual QuantLib::Real NPV() const = 0;

    //! Return the additional results of this instrument
    virtual const std::map<std::string, boost::any>& additionalResults() const = 0;

    QuantLib::Real additionalInstrumentsNPV() const;

    //! call update on enclosed instrument(s)
    virtual void updateQlInstruments();

    //! is it an Option?
    virtual bool isOption();

    //! Inspectors
    //@{
    /*! The "QuantLib" instrument
        Pass true if you trigger a calculation on the returned instrument and want to record
        the timing for that calculation. If in doubt whether a calculation is triggered, pass false. */
    QuantLib::ext::shared_ptr<QuantLib::Instrument> qlInstrument(const bool calculate = false) const;

    /*! The multiplier */
    Real multiplier() const;

    /*! multiplier to be applied on top of multiplier(), e.g. -1 for short options  */
    virtual Real multiplier2() const;

    /*! additional instruments */
    const std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>& additionalInstruments() const;

    /*! multipliers for additional instruments */
    const std::vector<Real>& additionalMultipliers() const;
    //@}

    //! Get cumulative timing spent on pricing
    boost::timer::nanosecond_type getCumulativePricingTime() const;

    //! Get number of pricings
    std::size_t getNumberOfPricings() const;

    //! Reset pricing statistics
    void resetPricingStats() const;

protected:
    QuantLib::ext::shared_ptr<QuantLib::Instrument> instrument_;
    Real multiplier_;
    std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>> additionalInstruments_;
    std::vector<Real> additionalMultipliers_;

    // all NPV calls to be logged in the timings should go through this method
    Real getTimedNPV(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& instr) const;

    mutable std::size_t numberOfPricings_ = 0;
    mutable boost::timer::nanosecond_type cumulativePricingTime_ = 0;
};

//! Vanilla Instrument Wrapper
/*!
  Derived from InstrumentWrapper
  Used for any non path-dependent trades

  \ingroup tradedata
*/
class VanillaInstrument : public InstrumentWrapper {
public:
    VanillaInstrument(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& inst, const Real multiplier = 1.0,
                      const std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                          std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>(),
                      const std::vector<Real>& additionalMultipliers = std::vector<Real>());

    void initialise(const std::vector<QuantLib::Date>&) override{};
    void reset() override {}

    QuantLib::Real NPV() const override;
    const std::map<std::string, boost::any>& additionalResults() const override;
};

} // namespace data
} // namespace ore
