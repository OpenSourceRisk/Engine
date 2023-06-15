/*
 Copyright (C) 2005, 2006 Theo Boafo
 Copyright (C) 2006, 2007 StatPro Italia srl
 Copyright (C) 2020 Quaternion Risk Managment Ltd

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

#include <qle/pricingengines/discretizedconvertible.hpp>

#include <ql/math/comparison.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantExt {

DiscretizedConvertible::DiscretizedConvertible(const ConvertibleBond::option::arguments& args,
                                               const ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                                               const Handle<Quote>& creditSpread, const TimeGrid& grid)
    : arguments_(args), process_(process), creditSpread_(creditSpread) {

    dividendValues_ = Array(arguments_.dividends.size(), 0.0);

    Date settlementDate = process_->riskFreeRate()->referenceDate();
    for (Size i = 0; i < arguments_.dividends.size(); i++) {
        if (arguments_.dividends[i]->date() >= settlementDate) {
            dividendValues_[i] =
                arguments_.dividends[i]->amount() * process_->riskFreeRate()->discount(arguments_.dividends[i]->date());
        }
    }

    DayCounter dayCounter = process_->riskFreeRate()->dayCounter();
    Date bondSettlement = arguments_.settlementDate;

    stoppingTimes_.resize(arguments_.exercise->dates().size());
    for (Size i = 0; i < stoppingTimes_.size(); ++i)
        stoppingTimes_[i] = dayCounter.yearFraction(bondSettlement, arguments_.exercise->date(i));

    callabilityTimes_.resize(arguments_.callabilityDates.size());
    for (Size i = 0; i < callabilityTimes_.size(); ++i)
        callabilityTimes_[i] = dayCounter.yearFraction(bondSettlement, arguments_.callabilityDates[i]);

    cashflowTimes_.resize(arguments_.cashflowDates.size());
    for (Size i = 0; i < cashflowTimes_.size(); ++i) {
        cashflowTimes_[i] = dayCounter.yearFraction(bondSettlement, arguments_.cashflowDates[i]);
    }

    dividendTimes_.resize(arguments_.dividendDates.size());
    for (Size i = 0; i < dividendTimes_.size(); ++i)
        dividendTimes_[i] = dayCounter.yearFraction(bondSettlement, arguments_.dividendDates[i]);

    if (!grid.empty()) {
        // adjust times to grid
        for (Size i = 0; i < stoppingTimes_.size(); i++)
            stoppingTimes_[i] = grid.closestTime(stoppingTimes_[i]);
        for (Size i = 0; i < cashflowTimes_.size(); i++)
            cashflowTimes_[i] = grid.closestTime(cashflowTimes_[i]);
        for (Size i = 0; i < callabilityTimes_.size(); i++)
            callabilityTimes_[i] = grid.closestTime(callabilityTimes_[i]);
        for (Size i = 0; i < dividendTimes_.size(); i++)
            dividendTimes_[i] = grid.closestTime(dividendTimes_[i]);
    }

    notionalTimes_.clear();
    for (auto const& d : arguments_.notionalDates) {
        notionalTimes_.push_back(grid.closestTime(dayCounter.yearFraction(bondSettlement, d)));
    }
}

std::vector<Time> DiscretizedConvertible::mandatoryTimes() const {
    std::vector<Time> result;
    // the stopping times might be negative, if an exercise date lies in the past
    for (auto const& t : stoppingTimes_) {
        if (t > 0.0)
            result.push_back(t);
    }
    std::copy(callabilityTimes_.begin(), callabilityTimes_.end(), std::back_inserter(result));
    std::copy(cashflowTimes_.begin(), cashflowTimes_.end(), std::back_inserter(result));
    return result;
}

void DiscretizedConvertible::reset(Size size) {

    // Set cashflow on last date
    values_ = Array(size, 0.0);
    for (Size i = 0; i < cashflowTimes_.size(); ++i) {
        if (close_enough(cashflowTimes_[i], cashflowTimes_.back())) {
            values_ += arguments_.cashflowAmounts[i];
        }
    }

    conversionProbability_ = Array(size, 0.0);
    spreadAdjustedRate_ = Array(size, 0.0);

    DayCounter rfdc = process_->riskFreeRate()->dayCounter();

    // this takes care of convertibility and conversion probabilities
    adjustValues();

    Real creditSpread = creditSpread_->value();
    Rate riskFreeRate = process_->riskFreeRate()->zeroRate(arguments_.maturityDate, rfdc, Continuous, NoFrequency);

    // Calculate blended discount rate to be used on roll back.
    for (Size j = 0; j < values_.size(); j++) {
        spreadAdjustedRate_[j] =
            conversionProbability_[j] * riskFreeRate + (1 - conversionProbability_[j]) * (riskFreeRate + creditSpread);
    }
}

void DiscretizedConvertible::postAdjustValuesImpl() {

    bool convertible = false;
    switch (arguments_.exercise->type()) {
    case Exercise::American:
        if (time() <= stoppingTimes_[1] && time() >= stoppingTimes_[0])
            convertible = true;
        break;
    case Exercise::European:
        if (isOnTime(stoppingTimes_[0]))
            convertible = true;
        break;
    case Exercise::Bermudan:
        for (Size i = 0; i < stoppingTimes_.size(); ++i) {
            if (isOnTime(stoppingTimes_[i]))
                convertible = true;
        }
        break;
    default:
        QL_FAIL("invalid option type");
    }

    for (Size i = 0; i < callabilityTimes_.size(); i++) {
        if (isOnTime(callabilityTimes_[i]))
            applyCallability(i, convertible);
    }

    for (Size i = 0; i < cashflowTimes_.size(); i++) {
        if (isOnTime(cashflowTimes_[i]) && !close_enough(cashflowTimes_[i], cashflowTimes_.back()))
            addCashflow(i);
    }

    if (convertible)
        applyConvertibility();
}

Real DiscretizedConvertible::getConversionRatio(const Real t) const {
    Size idx = std::distance(notionalTimes_.begin(), std::upper_bound(notionalTimes_.begin(), notionalTimes_.end(), t));
    if (idx > 0)
        --idx;
    return arguments_.notionals[std::min(idx, arguments_.notionals.size() - 1)] / arguments_.notionals.front() *
           arguments_.conversionRatio;
}

void DiscretizedConvertible::applyConvertibility() {
    Array grid = adjustedGrid();
    Real conversionRatio = getConversionRatio(time());
    for (Size j = 0; j < values_.size(); j++) {
        Real payoff = conversionRatio * grid[j];
        if (values_[j] <= payoff) {
            values_[j] = payoff;
            conversionProbability_[j] = 1.0;
        }
    }
}

void DiscretizedConvertible::applyCallability(Size i, bool convertible) {
    Size j;
    Array grid = adjustedGrid();
    Real conversionRatio = getConversionRatio(time());
    switch (arguments_.callabilityTypes[i]) {
    case Callability::Call:
        if (arguments_.callabilityTriggers[i] != Null<Real>() && arguments_.conversionValue != Null<Real>()) {
            Real trigger = arguments_.conversionValue * arguments_.callabilityTriggers[i];
            for (j = 0; j < values_.size(); j++) {
                // the callability is conditioned by the trigger...
                if (grid[j] >= trigger) {
                    // ...and might trigger conversion
                    values_[j] =
                        std::min(std::max(arguments_.callabilityPrices[i], conversionRatio * grid[j]), values_[j]);
                }
            }
        } else if (convertible) {
            for (j = 0; j < values_.size(); j++) {
                // exercising the callability might trigger conversion
                values_[j] = std::min(std::max(arguments_.callabilityPrices[i], conversionRatio * grid[j]), values_[j]);
            }
        } else {
            for (j = 0; j < values_.size(); j++) {
                values_[j] = std::min(arguments_.callabilityPrices[i], values_[j]);
            }
        }
        break;
    case Callability::Put:
        for (j = 0; j < values_.size(); j++) {
            values_[j] = std::max(values_[j], arguments_.callabilityPrices[i]);
        }
        break;
    default:
        QL_FAIL("unknown callability type");
    }
}

void DiscretizedConvertible::addCashflow(Size i) { values_ += arguments_.cashflowAmounts[i]; }

Array DiscretizedConvertible::adjustedGrid() const {
    Time t = time();
    Array grid = method()->grid(t);
    // add back all dividend amounts in the future
    for (Size i = 0; i < arguments_.dividends.size(); i++) {
        Time dividendTime = dividendTimes_[i];
        if (dividendTime >= t || close(dividendTime, t)) {
            const ext::shared_ptr<Dividend>& d = arguments_.dividends[i];
            DiscountFactor dividendDiscount =
                process_->riskFreeRate()->discount(dividendTime) / process_->riskFreeRate()->discount(t);
            for (Size j = 0; j < grid.size(); j++)
                grid[j] += d->amount(grid[j]) * dividendDiscount;
        }
    }
    return grid;
}

} // namespace QuantExt
