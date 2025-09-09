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
#include <ored/utilities/to_string.hpp>

#include <qle/indexes/eqfxindexbase.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/utilities/barrier.hpp>

#include <ql/instruments/barrieroption.hpp>
#include <ql/instruments/barriertype.hpp>
#include <ql/instruments/vanillaoption.hpp>
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
            npv = (today == exerciseDate_) ? multiplier2() * rebate_ * undMultiplier_ : 0.0;
        else
            npv = multiplier2() * getTimedNPV(activeUnderlyingInstrument_) * undMultiplier_;
        
        return npv + addNPV;
    } else {
        // if not exercised we just return the original option's NPV
        Real npv = multiplier2() * getTimedNPV(instrument_) * multiplier_;

        // Handling the edge case where barrier = strike, is KO, and underlying is only ITM when inside KO barrier.
        // NPV should then be zero, but the pricing engine might not necessarily be pricing it as zero at the boundary.
        auto vanillaOption = QuantLib::ext::dynamic_pointer_cast<VanillaOption>(activeUnderlyingInstrument_);
        if (vanillaOption) {
            auto payoff = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(vanillaOption->payoff());
            if (payoff && strikeAtBarrier(payoff->strike()) && 
                          ((barrierType_ == Barrier::DownOut && payoff->optionType() == Option::Put) ||
                           (barrierType_ == Barrier::UpOut && payoff->optionType() == Option::Call))) {
                npv = 0;
            }
        }
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
        auto vanillaOption = QuantLib::ext::dynamic_pointer_cast<VanillaOption>(activeUnderlyingInstrument_);
        if (vanillaOption) {
            auto payoff = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(vanillaOption->payoff());
            if (payoff && strikeAtBarrier(payoff->strike()) && 
              ((barrierType_ == Barrier::DownOut && payoff->optionType() == Option::Put) ||
                           (barrierType_ == Barrier::UpOut && payoff->optionType() == Option::Call))) {
                    return emptyMap;
            }
        }

        return instrument_->additionalResults();
    }
}

bool SingleBarrierOptionWrapper::strikeAtBarrier(Real strike) const {
    return close_enough(strike, barrier_);
}

bool SingleBarrierOptionWrapper::checkBarrier(Real spot) const {
        return ::QuantExt::checkBarrier(spot, barrierType_, barrier_, strictBarrier_);
}

bool SingleBarrierOptionWrapper::exercise() const {
    bool trigger = false;
    Date today = Settings::instance().evaluationDate();

    // check historical fixings - only check if the instrument is not calculated
    // really only needs to be checked if evaluation date changed
    if (!instrument_->isCalculated()) {
        if(overrideTriggered_) {
            trigger = *overrideTriggered_;
        } else if (startDate_ != Date() && startDate_ < today) {
            QL_REQUIRE(index_, "no index provided");
            QL_REQUIRE(calendar_ != Calendar(), "no calendar provided");
            QuantLib::ext::shared_ptr<QuantExt::EqFxIndexBase> eqfxIndex = QuantLib::ext::dynamic_pointer_cast<QuantExt::EqFxIndexBase>(index_);
            QuantLib::ext::shared_ptr<QuantExt::EqFxIndexBase> eqfxIndexLowHigh =
                barrierType_ == Barrier::DownOut || barrierType_ == Barrier::DownIn
                    ? QuantLib::ext::dynamic_pointer_cast<QuantExt::EqFxIndexBase>(indexLows_)
                    : QuantLib::ext::dynamic_pointer_cast<QuantExt::EqFxIndexBase>(indexHighs_);
            if (eqfxIndex) {
                Date d = calendar_.adjust(startDate_);
                while (d < today && !trigger) {
                    
                    Real fixing = Null<Real>();
                    if (eqfxIndexLowHigh == nullptr) {
                        fixing = eqfxIndex->pastFixing(d);
                    } else {
                        fixing = eqfxIndexLowHigh->pastFixing(d);
                        if (fixing == Null<Real>()) {
                            fixing = eqfxIndex->pastFixing(d);
                        }
                    }
                    if (fixing == Null<Real>()) {
                        StructuredMessage(
                            StructuredMessage::Category::Error, StructuredMessage::Group::Fixing,
                            "Missing fixing for index " + index_->name() + " on " + ore::data::to_string(d) +
                                ", Skipping this date, assuming no trigger",
                            std::map<std::string, std::string>({{"exceptionType", "Invalid or missing fixings"}}))
                            .log();
                    } else {
                        trigger = checkBarrier(fixing);
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
        if (overrideTriggered_) {
            trigger = *overrideTriggered_;
        } else {
            trigger = checkBarrier(spot_->value());
            if (trigger)
                exerciseDate_ = today;
        }
    }

    exercised_ = trigger;
    return trigger;
}

bool DoubleBarrierOptionWrapper::strikeAtBarrier(Real strike) const {
    return close_enough(strike, barrierLow_) || close_enough(strike, barrierHigh_);
}

bool DoubleBarrierOptionWrapper::checkBarrier(Real spotLow, Real spotHigh, int strictBarrier) const {
    if (strictBarrier == 1) {
        return spotLow < barrierLow_ || spotHigh > barrierHigh_;
    } else {
        return spotLow <= barrierLow_ || spotHigh >= barrierHigh_;
    }
}

bool DoubleBarrierOptionWrapper::exercise() const {
    bool trigger = false;
    Date today = Settings::instance().evaluationDate();

    // check historical fixings - only check if the instrument is not calculated
    // really only needs to be checked if evaluation date changed
    if (!instrument_->isCalculated()) {
        if(overrideTriggered_) {
            trigger = *overrideTriggered_;
        } else if (startDate_ != Date() && startDate_ < today) {
            QL_REQUIRE(index_, "no index provided");
            QL_REQUIRE(calendar_ != Calendar(), "no calendar provided");
            QuantLib::ext::shared_ptr<QuantExt::EqFxIndexBase> eqfxIndex =
                QuantLib::ext::dynamic_pointer_cast<QuantExt::EqFxIndexBase>(index_);
            auto indexL = indexLows_ != nullptr ? ext::dynamic_pointer_cast<QuantExt::EqFxIndexBase>(indexLows_) : nullptr;
            auto indexH = indexHighs_ != nullptr ? ext::dynamic_pointer_cast<QuantExt::EqFxIndexBase>(indexHighs_) : nullptr;;
            if (eqfxIndex) {
                Date d = calendar_.adjust(startDate_);
                while (d < today && !trigger) {
                    double dailyLow, dailyHigh = Null<Real>();

                    if (indexL == nullptr){
                        dailyLow = eqfxIndex->pastFixing(d);
                    } else {
                        dailyLow =  indexL->pastFixing(d);
                        dailyLow = dailyLow == Null<Real>() ? eqfxIndex->pastFixing(d) : dailyLow;
                    }
                    
                    if (indexH == nullptr){
                        dailyHigh = eqfxIndex->pastFixing(d);
                    } else {                      
                        dailyHigh = indexH->pastFixing(startDate_);
                        dailyHigh = dailyHigh == Null<Real>() ? eqfxIndex->pastFixing(d) : dailyHigh;
                    } 
                    
                    if (dailyLow == Null<Real>() || dailyHigh == Null<Real>()) {
                        StructuredMessage(
                            StructuredMessage::Category::Error, StructuredMessage::Group::Fixing,
                            "Missing fixing for index " + index_->name() + " on " + ore::data::to_string(d) +
                                ", Skipping this date, assuming no trigger",
                            std::map<std::string, std::string>({{"exceptionType", "Invalid or missing fixings"}}))
                            .log();
                    }else{
                        trigger = checkBarrier(dailyLow, dailyHigh, strictBarrier_);
                    }
                    d = calendar_.advance(d, 1, Days);
                }
            }
        }
    }

    // check todays spot, if triggered today set the exerciseDate, may have to pay a rebate
    if (!trigger) {
        if (overrideTriggered_) {
            trigger = *overrideTriggered_;
        } else {
            trigger = checkBarrier(spot_->value(), spot_->value(), strictBarrier_);
            if (trigger)
                exerciseDate_ = today;
        }
    }

    exercised_ = trigger;
    return trigger;
}

} // namespace data
} // namespace oreplus
