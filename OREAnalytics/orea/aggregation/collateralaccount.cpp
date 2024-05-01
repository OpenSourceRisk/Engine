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

#include <orea/aggregation/collateralaccount.hpp>
#include <ql/errors.hpp>

using namespace std;
using namespace QuantLib;

namespace {
bool isMarginCallExpired(ore::analytics::CollateralAccount::MarginCall mc) { return !mc.openMarginRequest(); }

bool isMarginPayDateLessThan(ore::analytics::CollateralAccount::MarginCall mc1,
                             ore::analytics::CollateralAccount::MarginCall mc2) {
    return mc1.marginPayDate() < mc2.marginPayDate();
}
} // namespace

namespace ore {
using namespace data;
namespace analytics {

CollateralAccount::CollateralAccount(const QuantLib::ext::shared_ptr<NettingSetDefinition>& csaDef, const Date& date_t0)
    : csaDef_(csaDef), balance_t0_(0.0) {
    accountBalances_.push_back(balance_t0_);
    accountDates_.push_back(date_t0);
}

CollateralAccount::CollateralAccount(const QuantLib::ext::shared_ptr<NettingSetDefinition>& csaDef, const Real& balance_t0,
                                     const Date& date_t0)
    : csaDef_(csaDef), balance_t0_(balance_t0) {
    accountBalances_.push_back(balance_t0_);
    accountDates_.push_back(date_t0);
}

void CollateralAccount::updateAccountBalance(const Date& simulationDate, const Real& annualisedZeroRate) {
    for (unsigned i = 0; i < marginCalls_.size(); i++) {
        QL_REQUIRE(marginCalls_[i].openMarginRequest(), "CollateralAccount error, expired margin call found"
                                                            << " (should have been purged after expiry)");
        if (i != marginCalls_.size() - 1) {
            QL_REQUIRE(marginCalls_[i].marginPayDate() <= marginCalls_[i + 1].marginPayDate(),
                       "CollateralAccount error; vector of margin calls not sorted correctly");
        }

        if (marginCalls_[i].marginPayDate() <= simulationDate) {
            if (marginCalls_[i].marginPayDate() == accountDates_.back()) {
                accountBalances_.back() += marginCalls_[i].marginAmount();
            } else {
                QL_REQUIRE(marginCalls_[i].marginPayDate() > accountDates_.back(),
                           "CollateralAccount error; balance update failed due to invalid dates");
                int accrualDays = marginCalls_[i].marginPayDate() - accountDates_.back();
                // apply "effective" accrual rate (i.e. adjust for spread specified in netting set definition)
                Real accrualRate = (accountBalances_.back() >= 0.0) ? (annualisedZeroRate - csaDef_->csaDetails()->collatSpreadRcv())
                                                                    : (annualisedZeroRate - csaDef_->csaDetails()->collatSpreadPay());
                // bring collateral account up to margin payment date (note accrual rate is assumed to be compounded
                // daily)
                accountBalances_.push_back(accountBalances_.back() * std::pow(1.0 + accrualRate / 365.0, accrualDays) +
                                           marginCalls_[i].marginAmount());
                accountDates_.push_back(marginCalls_[i].marginPayDate());
            }
            marginCalls_[i] = MarginCall(0.0, Date(), Date(), false);
        }
    }
    marginCalls_.erase(std::remove_if(marginCalls_.begin(), marginCalls_.end(), isMarginCallExpired),
                       marginCalls_.end());
    if (simulationDate > accountDates_.back()) { // bring the collateral account up to simulation date
        int accrualDays = simulationDate - accountDates_.back();
        // apply "effective" accrual rate (i.e. adjust for spread specified in netting set definition)
        Real accrualRate = (accountBalances_.back() >= 0.0) ? (annualisedZeroRate - csaDef_->csaDetails()->collatSpreadRcv())
                                                            : (annualisedZeroRate - csaDef_->csaDetails()->collatSpreadPay());
        accountDates_.push_back(simulationDate);
        accountBalances_.push_back(accountBalances_.back() * std::pow(1.0 + accrualRate / 365.0, accrualDays));
    }
}

void CollateralAccount::updateMarginCall(const MarginCall& newMarginCall) {
    QL_REQUIRE(newMarginCall.openMarginRequest() == true, "CollateralAccount error, "
                                                              << "attempting to load expired margin call");
    if (marginCalls_.size() > 0) {
        QL_REQUIRE(marginCalls_.back().marginRequestDate() < newMarginCall.marginRequestDate(),
                   "CollateralAccount error, attempting to issue an old margin call");
    }
    QL_REQUIRE(newMarginCall.marginRequestDate() >= accountDates_.back(),
               "CollateralAccount error, old margin call being loaded");

    marginCalls_.push_back(newMarginCall);
    // sorts the vector of margin calls according to ascending pay-date
    std::sort(marginCalls_.begin(), marginCalls_.end(), isMarginPayDateLessThan);
}

void CollateralAccount::updateMarginCall(const Real& marginFlowAmount, const Date& marginPayDate,
                                         const Date& marginRequestDate) {
    MarginCall newMarginCall(marginFlowAmount, marginPayDate, marginRequestDate);
    QL_REQUIRE(newMarginCall.marginRequestDate() <= newMarginCall.marginPayDate(),
               "CollateralAccount error, attempting to issue an old margin call");
    updateMarginCall(newMarginCall);
}

Real CollateralAccount::accountBalance(const Date& date) const {
    QL_REQUIRE(accountDates_.front() <= date, "CollateralAccount error, invalid date for balance request");
    if (date >= accountDates_.back())
        return accountBalances_.back(); // flat extrapolation at far end
    for (unsigned i = 0; i < accountDates_.size(); i++) {
        if (date == accountDates_[i])
            return accountBalances_[i];
        else if (date > accountDates_[i] && date < accountDates_[i + 1])
            return accountBalances_[i];
        else
            continue;
    }
    QL_FAIL("CollateralAccount error, balance not found for date " << date);
}

Real CollateralAccount::outstandingMarginAmount(const Date& simulationDate) const {
    Real outstandingMarginCallAmounts = 0.0;
    for (unsigned i = 0; i < marginCalls_.size(); i++) {
        QL_REQUIRE(marginCalls_[i].openMarginRequest(), "CollateralAccount error, expired margin call found"
                                                            << " (should have been purged after expiry)");
        QL_REQUIRE(marginCalls_[i].marginPayDate() > simulationDate,
                   "CollateralAccount error, old margin call pay date,"
                       << " (should have been settled before now)");
        outstandingMarginCallAmounts += marginCalls_[i].marginAmount();
    }
    return outstandingMarginCallAmounts;
}

void CollateralAccount::closeAccount(const Date& closeDate) {
    QL_REQUIRE(closeDate > accountDates_.back(), "CollateralAccount error, invalid date "
                                                     << " for closure of Collateral Account");
    marginCalls_.clear();
    accountBalances_.push_back(0.0);
    accountDates_.push_back(closeDate);
}
} // namespace analytics
} // namespace ore
