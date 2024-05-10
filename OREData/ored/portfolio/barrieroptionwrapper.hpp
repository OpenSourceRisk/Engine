/*
  Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/barrieroptionwrapper.hpp
    \brief Wrapper for option instruments, tracks whether option has been exercised or not
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/instrumentwrapper.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ql/instruments/doublebarriertype.hpp>
#include <ql/instruments/barriertype.hpp>
#include <ql/index.hpp>
#include <qle/instruments/payment.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! Barrier Option Wrapper
/*! An Barrier Option Wrapper will exercise whenever the barrier is touched.
    If it is an 'out' option it will pay the rebate value when exercised
 */
class BarrierOptionWrapper : public OptionWrapper {
public:
    BarrierOptionWrapper(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& inst, const bool isLongOption,
                         const QuantLib::Date& exerciseDate, const bool isPhysicalDelivery,
                         const QuantLib::ext::shared_ptr<QuantLib::Instrument>& undInst, Barrier::Type barrierType,
                         Handle<Quote> spot, Real rebate, const QuantLib::Currency ccy,
                         const QuantLib::Date& startDate, const QuantLib::ext::shared_ptr<QuantLib::Index>& index,
                         const QuantLib::Calendar& calendar,
                         // multiplier as seen from the option holder
                         const Real multiplier = 1.0,
                         // undMultiplier w.r.t. underlying as seen from the option holder
                         const Real undMultiplier = 1.0,
                         const std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                             std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>(),
                         const std::vector<Real>& additionalMultipliers = std::vector<Real>())
        : OptionWrapper(inst, isLongOption, std::vector<QuantLib::Date>(1, exerciseDate), isPhysicalDelivery,
                        std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>(1, undInst), multiplier, undMultiplier,
                        additionalInstruments, additionalMultipliers),
          spot_(spot), barrierType_(barrierType), rebate_(rebate), ccy_(ccy), startDate_(startDate), 
          index_(index) {
        calendar_ = index ? index->fixingCalendar() : calendar;
        reset();
    }

    const std::map<std::string, boost::any>& additionalResults() const override;
    virtual bool checkBarrier(Real, bool) const = 0;

protected:
    QuantLib::Real NPV() const override;
    Handle<Quote> spot_;
    Barrier::Type barrierType_;
    Real rebate_;
    const QuantLib::Currency ccy_;
    const QuantLib::Date startDate_;
    QuantLib::ext::shared_ptr<QuantLib::Index> index_;
    QuantLib::Calendar calendar_;
};

class SingleBarrierOptionWrapper : public BarrierOptionWrapper {
public:
    SingleBarrierOptionWrapper(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& inst, const bool isLongOption,
                               const QuantLib::Date& exerciseDate, const bool isPhysicalDelivery,
                               const QuantLib::ext::shared_ptr<QuantLib::Instrument>& undInst, Barrier::Type barrierType,
                               Handle<Quote> spot, Real barrier, Real rebate, const QuantLib::Currency ccy,
                               const QuantLib::Date& startDate, const QuantLib::ext::shared_ptr<QuantLib::Index>& index,
                               const QuantLib::Calendar& calendar,
                               // multiplier as seen from the option holder
                               const Real multiplier = 1.0,
                               // undMultiplier w.r.t. underlying as seen from the option holder
                               const Real undMultiplier = 1.0,
                               const std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                                   std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>(),
                               const std::vector<Real>& additionalMultipliers = std::vector<Real>())
        : BarrierOptionWrapper(inst, isLongOption, exerciseDate, isPhysicalDelivery, undInst, barrierType, spot, rebate,
                               ccy, startDate, index, calendar, multiplier, undMultiplier, additionalInstruments, additionalMultipliers),
          barrier_(barrier) {}

    bool checkBarrier(Real spot, bool isTouchingOnly) const override;
    bool exercise() const override;

protected:
    Real barrier_;
};

class DoubleBarrierOptionWrapper : public BarrierOptionWrapper {
public:
    DoubleBarrierOptionWrapper(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& inst, const bool isLongOption,
                               const QuantLib::Date& exerciseDate, const bool isPhysicalDelivery,
                               const QuantLib::ext::shared_ptr<QuantLib::Instrument>& undInst, DoubleBarrier::Type barrierType,
                               Handle<Quote> spot, Real barrierLow, Real barrierHigh, Real rebate,
                               const QuantLib::Currency ccy, const QuantLib::Date& startDate,
                               const QuantLib::ext::shared_ptr<QuantLib::Index>& index, const QuantLib::Calendar& calendar,
                               // multiplier as seen from the option holder
                               const Real multiplier = 1.0,
                               // undMultiplier w.r.t. underlying as seen from the option holder
                               const Real undMultiplier = 1.0,
                               const std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>& additionalInstruments =
                                   std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>(),
                               const std::vector<Real>& additionalMultipliers = std::vector<Real>())
        : BarrierOptionWrapper(
              inst, isLongOption, exerciseDate, isPhysicalDelivery, undInst,
              (barrierType == DoubleBarrier::Type::KnockOut ? Barrier::Type::UpOut : Barrier::Type::UpIn), spot, rebate,
              ccy, startDate, index, calendar, multiplier, undMultiplier, additionalInstruments, additionalMultipliers),
          barrierLow_(barrierLow), barrierHigh_(barrierHigh) {
        QL_REQUIRE(barrierType == DoubleBarrier::Type::KnockOut || barrierType == DoubleBarrier::Type::KnockIn,
                   "Invalid barrier type " << barrierType << ". Only KnockOut and KnockIn are supported.");
        QL_REQUIRE(barrierLow < barrierHigh, "barrierLow has to be less than barrierHigh");
    }
        
    bool checkBarrier(Real spot, bool isTouchingOnly) const override;
    bool exercise() const override;

protected:
    Real barrierLow_;
    Real barrierHigh_;
};

} // namespace data
} // namespace oreplus
