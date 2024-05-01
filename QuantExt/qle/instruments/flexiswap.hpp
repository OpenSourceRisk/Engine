/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/flexiswap.hpp
    \brief Flexi-Swap instrument with global notional bounds
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/position.hpp>
#include <ql/pricingengine.hpp>

namespace QuantExt {
using QuantLib::BusinessDayConvention;
using QuantLib::Date;
using QuantLib::DayCounter;
using QuantLib::IborIndex;
using QuantLib::Leg;
using QuantLib::Real;
using QuantLib::Schedule;
using QuantLib::Size;
using QuantLib::Time;
using QuantLib::VanillaSwap;

//! Flexi-Swap with global notional bounds
/*! The given non-standard swap defines the upper bound for the notionals, which must be non-increasing and consistent
  across the legs. Furthermore it is assumed that that floating leg's frequency divides the fixed leg's frequency. The
  notional in the Flexi-Swap can be adjusted on each fixing date corresponding to a whole fixed leg period to any
  value between the given lower bound and the original amount. The vector of lower bounds must therefore have
  the same size as the fixed leg vector in the non-standard swap. For periods with a fixing date on or before the
  evaluation date, is is assumed that the non-standard swap's notional is the relevant one, i.e. the lower bound is
  ignored for such periods.

  notionalCanBeDecreased marks fixed rate periods in which the notional can actually be decreased; defaults
                         to true,true,...,true if not given, i.e. the notional can be decreased in each
                         period.
*/

class FlexiSwap : public QuantLib::Swap {
public:
    class arguments;
    class results;
    class engine;
    FlexiSwap(const VanillaSwap::Type type, const std::vector<Real>& fixedNominal,
              const std::vector<Real>& floatingNominal, const Schedule& fixedSchedule,
              const std::vector<Real>& fixedRate, const DayCounter& fixedDayCount, const Schedule& floatingSchedule,
              const QuantLib::ext::shared_ptr<IborIndex>& iborIndex, const std::vector<Real>& gearing,
              const std::vector<Real>& spread, const std::vector<Real>& cappedRate,
              const std::vector<Real>& flooredRate, const DayCounter& floatingDayCount,
              const std::vector<Real>& lowerNotionalBound, const QuantLib::Position::Type optionPosition,
              const std::vector<bool>& notionalCanBeDecreased = std::vector<bool>(),
              boost::optional<BusinessDayConvention> paymentConvention = boost::none);

    //! \name Inspectors
    //@{
    VanillaSwap::Type type() const { return type_; }
    const std::vector<Real>& fixedNominal() const { return fixedNominal_; }
    const std::vector<Real>& floatingNominal() const { return floatingNominal_; }

    const Schedule& fixedSchedule() const { return fixedSchedule_; }
    const std::vector<Real>& fixedRate() const { return fixedRate_; }
    const DayCounter& fixedDayCount() const { return fixedDayCount_; }

    const Schedule& floatingSchedule() const { return floatingSchedule_; }
    const QuantLib::ext::shared_ptr<IborIndex>& iborIndex() const { return iborIndex_; }
    const std::vector<Real>& gearing() const { return gearing_; }
    const std::vector<Real>& spread() const { return spread_; }
    const std::vector<Real>& cappedRate() const { return cappedRate_; }
    const std::vector<Real>& flooredRate() const { return flooredRate_; }
    const DayCounter& floatingDayCount() const { return floatingDayCount_; }

    const std::vector<Real>& lowerNotionalBound() const { return lowerNotionalBound_; }
    const QuantLib::Position::Type optionPosition() const { return optionPosition_; }
    const std::vector<bool>& notionalCanBeDecreased() const { return notionalCanBeDecreased_; }

    BusinessDayConvention paymentConvention() const { return paymentConvention_; }

    const Leg& fixedLeg() const { return legs_[0]; }
    const Leg& floatingLeg() const { return legs_[1]; }
    //@}

    Real underlyingValue() const;

private:
    const VanillaSwap::Type type_;
    const std::vector<Real> fixedNominal_, floatingNominal_;
    const Schedule fixedSchedule_;
    const std::vector<Real> fixedRate_;
    const DayCounter fixedDayCount_;
    const Schedule floatingSchedule_;
    const QuantLib::ext::shared_ptr<IborIndex> iborIndex_;
    const std::vector<Real> gearing_;
    const std::vector<Real> spread_;
    const std::vector<Real> cappedRate_;
    const std::vector<Real> flooredRate_;
    const DayCounter floatingDayCount_;
    const std::vector<Real> lowerNotionalBound_;
    const QuantLib::Position::Type optionPosition_;
    const std::vector<bool> notionalCanBeDecreased_;
    BusinessDayConvention paymentConvention_;

    void setupArguments(QuantLib::PricingEngine::arguments*) const override;
    void setupExpired() const override;
    void fetchResults(const QuantLib::PricingEngine::results*) const override;

    mutable Real underlyingValue_; // result
};

//! %Arguments for Flexi-Swap
class FlexiSwap::arguments : public QuantLib::Swap::arguments {
public:
    arguments() : type(VanillaSwap::Receiver) {}
    VanillaSwap::Type type;
    std::vector<Real> fixedNominal, floatingNominal;

    std::vector<Date> fixedResetDates;
    std::vector<Date> fixedPayDates;
    std::vector<Time> floatingAccrualTimes;
    std::vector<Date> floatingResetDates;
    std::vector<Date> floatingFixingDates;
    std::vector<Date> floatingPayDates;

    std::vector<Real> fixedCoupons;
    std::vector<Real> fixedRate;
    std::vector<Real> floatingGearings;
    std::vector<Real> floatingSpreads;
    std::vector<Real> cappedRate;
    std::vector<Real> flooredRate;
    std::vector<Real> floatingCoupons;

    QuantLib::ext::shared_ptr<IborIndex> iborIndex;

    std::vector<Real> lowerNotionalBound;
    QuantLib::Position::Type optionPosition;
    std::vector<bool> notionalCanBeDecreased;

    void validate() const override;
};

//! %Results for Flexi-Swap
class FlexiSwap::results : public QuantLib::Swap::results {
public:
    Real underlyingValue;
    void reset() override;
};

//! Base class for Flexi-Swap engines
class FlexiSwap::engine : public QuantLib::GenericEngine<FlexiSwap::arguments, FlexiSwap::results> {};

} // namespace QuantExt
