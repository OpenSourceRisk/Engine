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

#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/instrumentwrapper.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/log.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/indexes/eqfxindexbase.hpp>
#include <qle/indexes/fxindex.hpp>
#include <ql/instruments/barriertype.hpp>
#include <ql/settings.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

Real BarrierOptionWrapper::NPV() const {
    Real addNPV = additionalInstrumentsNPV();
    
    Date today = Settings::instance().evaluationDate();

    // check the trigger on the first run, we only need to re check it if the 
    // instrument becomes un calculated. The trigger should only need to be rechecked
    // for a change in valuation date or spot, and this ensures
    if (!exercised_ || !instrument_->isCalculated()) {
        exercise();
    }

    if (exercised_) {
        Real npv;
        if (barrierType_ == Barrier::DownOut || barrierType_ == Barrier::UpOut)
            npv = (today == exerciseDate_) ? (isLong_ ? 1.0 : -1.0) * rebate_ * undMultiplier_ : 0.0;
        else
            npv = (isLong_ ? 1.0 : -1.0) * getTimedNPV(activeUnderlyingInstrument_) * undMultiplier_;
        
        return npv + addNPV;
    } else {
        // if not exercised we just return the original option's NPV
        Real npv = (isLong_ ? 1.0 : -1.0) * getTimedNPV(instrument_) * multiplier_;
        return npv + addNPV;
    }
}

const std::map<std::string, boost::any>& BarrierOptionWrapper::additionalResults() const {
    static std::map<std::string, boost::any> emptyMap;
    NPV();
    if (exercised_) {
        if (!(barrierType_ == Barrier::DownOut || barrierType_ == Barrier::UpOut))
            return activeUnderlyingInstrument_->additionalResults();
        else
            return emptyMap;
    } else {
        return instrument_->additionalResults();
    }
}

bool SingleBarrierOptionWrapper::checkBarrier(Real spot, Barrier::Type type, Real barrier) const {
    switch (type) {
    case Barrier::DownIn:
    case Barrier::DownOut:
        return spot <= barrier;
    case Barrier::UpIn:
    case Barrier::UpOut:
        return spot >= barrier;
    default:
        QL_FAIL("unknown barrier type " << type);
    }
}

bool SingleBarrierOptionWrapper::exercise() const {
    bool trigger = false;
    Date today = Settings::instance().evaluationDate();

    // check historical fixings - only check if the instrument is not calculated
    // really only needs to be checked if evaluation date changed
    if (!instrument_->isCalculated()) {
        if (startDate_ != Date() && startDate_ < today) {
            QL_REQUIRE(index_, "no index provided");
            QL_REQUIRE(calendar_ != Calendar(), "no calendar provided");

            boost::shared_ptr<QuantExt::EqFxIndexBase> eqfxIndex =
                boost::dynamic_pointer_cast<QuantExt::EqFxIndexBase>(index_);

            if (eqfxIndex) {
                Date d = calendar_.adjust(startDate_);
                while (d < today && !trigger) {
                    Real fixing = eqfxIndex->pastFixing(d);                    
                    if (fixing == 0.0 || fixing == Null<Real>()) {
                        ALOG("Got invalid fixing for index " << index_->name() << " on " << d
                                                             << "Skipping this date, assuming no trigger");
                    } else {
                        // This is so we can use pastIndex and not fail on a missing fixing to be
                        // consistent with previous implemention, however maybe we should use fixing
                        // and be strict on needed fixings being present
                        auto fxInd = boost::dynamic_pointer_cast<QuantExt::FxIndex>(eqfxIndex);
                        trigger = checkBarrier(fixing, barrierType_, barrier_);
                        if (trigger)
                            exerciseDate_ = d;
                    }
                    d = calendar_.advance(d, 1, Days);
                }
            }
        }
    }

    // check todays spot, if triggered today set the exerciseDate, may have to pay a rebate
    if (!trigger) {
        trigger = checkBarrier(spot_->value(), barrierType_, barrier_);
        if (trigger)
            exerciseDate_ = today;
    }

    exercised_ = trigger;
    return trigger;
}

bool DoubleBarrierOptionWrapper::checkBarrier(Real spot, Real barrierLow, Real barrierHigh) const {
    return spot <= barrierLow || spot >= barrierHigh;
}

bool DoubleBarrierOptionWrapper::exercise() const {
    bool trigger = false;
    Date today = Settings::instance().evaluationDate();

    // check historical fixings - only check if the instrument is not calculated
    // really only needs to be checked if evaluation date changed
    if (!instrument_->isCalculated()) {
        if (startDate_ != Date() && startDate_ < today) {
            QL_REQUIRE(index_, "no index provided");
            QL_REQUIRE(calendar_ != Calendar(), "no calendar provided");

            
            boost::shared_ptr<QuantExt::EqFxIndexBase> eqfxIndex =
                boost::dynamic_pointer_cast<QuantExt::EqFxIndexBase>(index_);

            if (eqfxIndex) {
                Date d = calendar_.adjust(startDate_);
                while (d < today && !trigger) {
                    Real fixing = eqfxIndex->pastFixing(d);
                    if (fixing == 0.0 || fixing == Null<Real>()) {
                        ALOG("Got invalid fixing for index " << index_->name() << " on " << d
                                                             << "Skipping this date, assuming no trigger");
                    } else {
                        trigger = checkBarrier(fixing, barrierLow_, barrierHigh_);
                    }
                    d = calendar_.advance(d, 1, Days);
                }
            }
        }
    }

    // check todays spot, if triggered today set the exerciseDate, may have to pay a rebate
    if (!trigger) {
        trigger = checkBarrier(spot_->value(), barrierLow_, barrierHigh_);
        exerciseDate_ = today;
    }

    exercised_ = trigger;
    return trigger;
}

} // namespace data
} // namespace oreplus
