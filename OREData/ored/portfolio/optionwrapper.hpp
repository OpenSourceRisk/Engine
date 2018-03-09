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

/*! \file ored/portfolio/optionwrapper.hpp
    \brief Wrapper for option instruments, tracks whether option has been exercised or not
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/instrumentwrapper.hpp>

namespace ore {
namespace data {

//! Option Wrapper
/*!
  Wrapper Class for Options
  Prices underlying instrument if option has been exercised
  Handles Physical and Cash Settlement

  \ingroup tradedata
*/
class OptionWrapper : public InstrumentWrapper {
public:
    //! Constructor
    OptionWrapper(const boost::shared_ptr<QuantLib::Instrument>& inst, const bool isLongOption,
                  const std::vector<QuantLib::Date>& exerciseDate, const bool isPhysicalDelivery,
                  const std::vector<boost::shared_ptr<QuantLib::Instrument>>& undInst,
                  // multiplier as seen from the option holder
                  const Real multiplier = 1.0,
                  // undMultiplier w.r.t. underlying as seen from the option holder
                  const Real undMultiplier = 1.0,
                  const std::vector<boost::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                      std::vector<boost::shared_ptr<QuantLib::Instrument>>(),
                  const std::vector<Real>& additionalMultipliers = std::vector<Real>());
    //! \name InstrumentWrapper interface
    //@{
    void initialise(const std::vector<QuantLib::Date>& dates) override;
    void reset() override;
    QuantLib::Real NPV() const override;
    void updateQlInstruments() override {
        for (QuantLib::Size i = 0; i < underlyingInstruments_.size(); ++i)
            underlyingInstruments_[i]->update();
        InstrumentWrapper::updateQlInstruments();
    }
    bool isOption() override { return true; }
    //@}

    //! return the underlying instruments
    const std::vector<boost::shared_ptr<QuantLib::Instrument>>& underlyingInstruments() const {
        return underlyingInstruments_;
    }

    //! return the active underlying instrument
    const boost::shared_ptr<QuantLib::Instrument>& activeUnderlyingInstrument() const {
        return activeUnderlyingInstrument_;
    }

    //! return true if option is long, false if option is short
    bool isLong() const { return isLong_; }

    //! return true if option is exercised
    bool isExercised() const { return exercised_; }

    //! return true for physical delivery, false for cash settlement
    bool isPhysicalDelivery() const { return isPhysicalDelivery_; }

    //! the underlying multiplier
    Real underlyingMultiplier() const { return undMultiplier_; }

protected:
    bool isLong_;
    bool isPhysicalDelivery_;
    std::vector<QuantLib::Date> contractExerciseDates_;
    std::vector<QuantLib::Date> effectiveExerciseDates_;
    std::vector<boost::shared_ptr<QuantLib::Instrument>> underlyingInstruments_;
    mutable boost::shared_ptr<QuantLib::Instrument> activeUnderlyingInstrument_;
    Real undMultiplier_;
    mutable bool exercised_;
    mutable QuantLib::Date exerciseDate_;

    virtual bool exercise() const = 0;
};

//! European Option Wrapper
/*! A European Option Wrapper will exercise if the underlying NPV is positive
 */
class EuropeanOptionWrapper : public OptionWrapper {
public:
    EuropeanOptionWrapper(const boost::shared_ptr<QuantLib::Instrument>& inst, const bool isLongOption,
                          const QuantLib::Date& exerciseDate, const bool isPhysicalDelivery,
                          const boost::shared_ptr<QuantLib::Instrument>& undInst,
                          // multiplier as seen from the option holder
                          const Real multiplier = 1.0,
                          // undMultiplier w.r.t. underlying as seen from the option holder
                          const Real undMultiplier = 1.0,
                          const std::vector<boost::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                              std::vector<boost::shared_ptr<QuantLib::Instrument>>(),
                          const std::vector<Real>& additionalMultipliers = std::vector<Real>())
        : OptionWrapper(inst, isLongOption, std::vector<QuantLib::Date>(1, exerciseDate), isPhysicalDelivery,
                        std::vector<boost::shared_ptr<QuantLib::Instrument>>(1, undInst), multiplier, undMultiplier,
                        additionalInstruments, additionalMultipliers) {}

protected:
    bool exercise() const override;
};

//! American Option Wrapper
/*! An American Option Wrapper will exercise whenever the underlying NPV is greater than
  the option NPV. On the last date it will exercise if the underlying is positive.
 */
class AmericanOptionWrapper : public OptionWrapper {
public:
    AmericanOptionWrapper(const boost::shared_ptr<QuantLib::Instrument>& inst, const bool isLongOption,
                          const QuantLib::Date& exerciseDate, const bool isPhysicalDelivery,
                          const boost::shared_ptr<QuantLib::Instrument>& undInst,
                          // multiplier as seen from the option holder
                          const Real multiplier = 1.0,
                          // undMultiplier w.r.t. underlying as seen from the option holder
                          const Real undMultiplier = 1.0,
                          const std::vector<boost::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                              std::vector<boost::shared_ptr<QuantLib::Instrument>>(),
                          const std::vector<Real>& additionalMultipliers = std::vector<Real>())
        : OptionWrapper(inst, isLongOption, std::vector<QuantLib::Date>(1, exerciseDate), isPhysicalDelivery,
                        std::vector<boost::shared_ptr<QuantLib::Instrument>>(1, undInst), multiplier, undMultiplier,
                        additionalInstruments, additionalMultipliers) {}

protected:
    bool exercise() const override;
};

//! Bermudan Option Wrapper
/*! A Bermudan Option Wrapper will exercise when the relevant underlying's NPV exceeds the
 *  option NPV. If only one exercise date is remaining, an analytic European pricing engine is used.
 */
class BermudanOptionWrapper : public OptionWrapper {
public:
    BermudanOptionWrapper(const boost::shared_ptr<QuantLib::Instrument>& inst, const bool isLongOption,
                          const std::vector<QuantLib::Date>& exerciseDates, const bool isPhysicalDelivery,
                          const std::vector<boost::shared_ptr<QuantLib::Instrument>>& undInsts,
                          // multiplier as seen from the option holder
                          const Real multiplier = 1.0,
                          // undMultiplier w.r.t. underlying as seen from the option holder
                          const Real undMultiplier = 1.0,
                          const std::vector<boost::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                              std::vector<boost::shared_ptr<QuantLib::Instrument>>(),
                          const std::vector<Real>& additionalMultipliers = std::vector<Real>())
        : OptionWrapper(inst, isLongOption, exerciseDates, isPhysicalDelivery, undInsts, multiplier, undMultiplier,
                        additionalInstruments, additionalMultipliers) {
        QL_REQUIRE(exerciseDates.size() == undInsts.size(),
                   "sizes of exercise date and underlying instrument vectors do not match");
    }

protected:
    bool exercise() const override;

private:
    /*! Check if European engine can be used */
    bool convertToEuropean() const;
    boost::shared_ptr<QuantLib::Instrument> getUnderlying() const;
};
} // namespace data
} // namespace ore
