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
    InstrumentWrapper() : multiplier_(1.0) {}
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

    //! Return the additional results of this instrument
    virtual const std::map<std::string, boost::any>& additionalResults() const = 0;

    QuantLib::Real additionalInstrumentsNPV() const {
        Real npv = 0.0;
        for (QuantLib::Size i = 0; i < additionalInstruments_.size(); ++i)
            npv += additionalInstruments_[i]->NPV() * additionalMultipliers_[i];
        return npv;
    }

    //! call update on enclosed instrument(s)
    virtual void updateQlInstruments() {
        // the instrument might contain nested lazy objects which we also want to be updated
        instrument_->deepUpdate();
        for (QuantLib::Size i = 0; i < additionalInstruments_.size(); ++i)
            additionalInstruments_[i]->deepUpdate();
    }

    //! is it an Option?
    virtual bool isOption() { return false; }

    //! Inspectors
    //@{
    /*! The "QuantLib" instrument
        Pass true if you trigger a calculation on the returned instrument and want to record
        the timing for that calculation. If in doubt whether a calculation is triggered, pass false. */
    boost::shared_ptr<QuantLib::Instrument> qlInstrument(const bool calculate = false) const {
        if (calculate && instrument_ != nullptr) {
            getTimedNPV(instrument_);
        }
        return instrument_;
    }
    /*! The multiplier */
    const Real& multiplier() const { return multiplier_; }
    /*! additional instruments */
    const std::vector<boost::shared_ptr<QuantLib::Instrument>>& additionalInstruments() const {
        return additionalInstruments_;
    }
    /*! multipliers for additional instruments */
    const std::vector<Real>& additionalMultipliers() const { return additionalMultipliers_; }
    //@}

    //! Get cumulative timing spent on pricing
    boost::timer::nanosecond_type getCumulativePricingTime() const { return cumulativePricingTime_; }

    //! Get number of pricings
    std::size_t getNumberOfPricings() const { return numberOfPricings_; }

    //! Reset pricing statistics
    void resetPricingStats() const {
        numberOfPricings_ = 0;
        cumulativePricingTime_ = 0;
    }

protected:
    boost::shared_ptr<QuantLib::Instrument> instrument_;
    Real multiplier_;
    std::vector<boost::shared_ptr<QuantLib::Instrument>> additionalInstruments_;
    std::vector<Real> additionalMultipliers_;

    // all NPV calls to be logged in the timings should go through this method
    Real getTimedNPV(const boost::shared_ptr<QuantLib::Instrument>& instr) const {
        if (instr == nullptr)
            return 0.0;
        if (instr->isCalculated() || instr->isExpired())
            return instr->NPV();
        boost::timer::cpu_timer timer_;
        Real tmp = instr->NPV();
        cumulativePricingTime_ += timer_.elapsed().wall;
        ++numberOfPricings_;
        return tmp;
    }

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
    VanillaInstrument(const boost::shared_ptr<QuantLib::Instrument>& inst, const Real multiplier = 1.0,
                      const std::vector<boost::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                          std::vector<boost::shared_ptr<QuantLib::Instrument>>(),
                      const std::vector<Real>& additionalMultipliers = std::vector<Real>())
        : InstrumentWrapper(inst, multiplier, additionalInstruments, additionalMultipliers) {}

    void initialise(const std::vector<QuantLib::Date>&) override{};
    void reset() override {}

    QuantLib::Real NPV() const override { return getTimedNPV(instrument_) * multiplier_ + additionalInstrumentsNPV(); }
    const std::map<std::string, boost::any>& additionalResults() const override {
        static std::map<std::string, boost::any> empty;
        if (instrument_ == nullptr)
            return empty;
        getTimedNPV(instrument_);
        return instrument_->additionalResults();
    }
};
} // namespace data
} // namespace ore
