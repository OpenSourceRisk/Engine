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

/*! \file balanceguaranteedswap.hpp
    \brief Balance Guaranteed Swap instrument
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

//! Balance Guaranteed Swap
/*! Notice the comment in the NumericLgmBgsFlexiSwapEngine concerning the start of the prepayments, this means that the
  tranche notionals for periods with a start date in the past or on the evaluation date should include actual (known)
  prepayments. For future periods the notionals should correspond to a zero CPR assumption on the other hand.
*/

class BalanceGuaranteedSwap : public QuantLib::Swap {
public:
    class arguments;
    class results;
    class engine;
    BalanceGuaranteedSwap(const VanillaSwap::Type type, const std::vector<std::vector<Real>>& trancheNominals,
                          const Schedule& nominalSchedule, const Size referencedTranche, const Schedule& fixedSchedule,
                          const std::vector<Real>& fixedRate, const DayCounter& fixedDayCount,
                          const Schedule& floatingSchedule, const QuantLib::ext::shared_ptr<IborIndex>& iborIndex,
                          const std::vector<Real>& gearing, const std::vector<Real>& spread,
                          const std::vector<Real>& cappedRate, const std::vector<Real>& flooredRate,
                          const DayCounter& floatingDayCount,
                          boost::optional<BusinessDayConvention> paymentConvention = boost::none);

    //! \name Inspectors
    //@{
    VanillaSwap::Type type() const { return type_; }
    const std::vector<std::vector<Real>>& trancheNominal() const { return trancheNominals_; }
    const Schedule& nominalSchedule() const { return nominalSchedule_; }
    const Size referencedTranche() const { return referencedTranche_; }

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

    BusinessDayConvention paymentConvention() const { return paymentConvention_; }

    const Leg& fixedLeg() const { return legs_[0]; }
    const Leg& floatingLeg() const { return legs_[1]; }
    //@}

    Real trancheNominal(const Size trancheIndex, const Date& d);

private:
    const VanillaSwap::Type type_;
    const std::vector<std::vector<Real>> trancheNominals_;
    const Schedule nominalSchedule_;
    const Size referencedTranche_;
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
    BusinessDayConvention paymentConvention_;

    void setupArguments(QuantLib::PricingEngine::arguments*) const override;
    void setupExpired() const override;
    void fetchResults(const QuantLib::PricingEngine::results*) const override;
};

//! %Arguments for Balance Guaranteed Swap
class BalanceGuaranteedSwap::arguments : public QuantLib::Swap::arguments {
public:
    arguments() : type(VanillaSwap::Receiver) {}
    VanillaSwap::Type type;
    std::vector<std::vector<Real>> trancheNominals;
    std::vector<Date> trancheNominalDates;
    QuantLib::Frequency trancheNominalFrequency;
    Size referencedTranche;

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

    Leg fixedLeg, floatingLeg;

    void validate() const override;
};

//! %Results for Balance Guaranteed Swap
class BalanceGuaranteedSwap::results : public QuantLib::Swap::results {
public:
    void reset() override;
};

//! Base class for Balance Guaranteed Swap engines
class BalanceGuaranteedSwap::engine
    : public QuantLib::GenericEngine<BalanceGuaranteedSwap::arguments, BalanceGuaranteedSwap::results> {};

} // namespace QuantExt
