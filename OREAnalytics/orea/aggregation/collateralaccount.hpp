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

/*! \file orea/aggregation/collateralaccount.hpp
    \brief Collateral Account Balance (stored in base currency)
    \ingroup analytics
*/

#pragma once

#include <ored/portfolio/nettingsetdefinition.hpp>
#include <ored/utilities/log.hpp>

#include <ql/time/date.hpp>

#include <ql/shared_ptr.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace data;

//! Collateral Account
/*!
  This class holds information corresponding to collateral cash accounts.
  It stores a balance as well as an asof date for the balance. The class
  also includes "margin" information relating to the most recent margin
  call (e.g. call amount, status, expected pay date.

  The idea is that this class can be updated on-the-run with new margin
  requirements and collateral balances, and the timestamps updated
  accordingly.

  For further information refer to the detailed ORE documentation.

  \ingroup analytics
*/
class CollateralAccount {
public:
    //! Default constructor
    CollateralAccount() {}
    //! Constructor assuming initial collateral account balance is zero
    CollateralAccount( //! CSA details including threshold, minimum transfer amount, margining frequency etc
        const QuantLib::ext::shared_ptr<NettingSetDefinition>& csaDef,
        //! Today's date
        const Date& date_t0);
    //! Constructor taking an initial collateral account balance
    CollateralAccount( //! CSA details including threshold, minimum transfer amount, margining frequency etc
        const QuantLib::ext::shared_ptr<NettingSetDefinition>& csaDef,
        //! Initial collateral account balance
        const Real& balance_t0,
        //! Today's date
        const Date& date_t0);

    //! Margin Call
    /*!
      This class is essentially a container for open margin call details

      \ingroup analytics
    */
    class MarginCall {
    public:
        MarginCall(const Real& marginFlowAmount, const Date& marginPayDate, const Date& marginRequestDate,
                   const bool& openMarginRequest = true)
            : openMarginRequest_(openMarginRequest), marginFlowAmount_(marginFlowAmount), marginPayDate_(marginPayDate),
              marginRequestDate_(marginRequestDate) {}

        //! Inspectors
        //@{
        /*! check if there is an outstanding margin call awaiting agreement/settlement */
        bool openMarginRequest() const { return openMarginRequest_; }
        /*! open margin request amount (+ -> call, - -> post) */
        Real marginAmount() const { return marginFlowAmount_; }
        /*! expected payment date of outstanding collateral margin */
        Date marginPayDate() const { return marginPayDate_; }
        /*!  The date at which the outstanding margin was requested */
        Date marginRequestDate() const { return marginRequestDate_; }
        //@}

    private:
        bool openMarginRequest_;
        Real marginFlowAmount_;
        Date marginPayDate_;
        Date marginRequestDate_;
    };

    //! Inspectors
    //@{
    /*! csa (netting set) definition */
    QuantLib::ext::shared_ptr<NettingSetDefinition> csaDef() const { return csaDef_; }
    /*! account balance at start date */
    Real balance_t0() const { return balance_t0_; }
    /*! most up-to-date account balance */
    Real accountBalance() const { return accountBalances_.back(); }
    /*! account balance as of requested date */
    Real accountBalance(const Date& date) const;
    /*! most recent account balance reset date */
    Date balanceDate() const { return accountDates_.back(); }
    //@}

    /*!
      Returns the sum of all outstanding margin call amounts
    */
    Real outstandingMarginAmount(const Date& simulationDate) const;

    /*!
      Updates the account balance, does this by checking if
      any outstanding margin calls are due for settlement.

      \note The accrual rate is assumed to be compounded daily
    */
    void updateAccountBalance(const Date& simulationDate, const Real& annualisedZeroRate = 0.0);
    /*!
      Updates the margin call details for this account
    */
    void updateMarginCall(const MarginCall& newMarginCall);
    void updateMarginCall(const Real& marginFlowAmount, const Date& marginPayDate, const Date& marginRequestDate);
    /*!
      Closes the account as of a given date (i.e. sets the balance to zero)
    */
    void closeAccount(const Date& closeDate);

private:
    QuantLib::ext::shared_ptr<NettingSetDefinition> csaDef_;
    Real balance_t0_;
    vector<Real> accountBalances_;
    vector<Date> accountDates_;
    vector<MarginCall> marginCalls_;
};
} // namespace analytics
} // namespace ore
