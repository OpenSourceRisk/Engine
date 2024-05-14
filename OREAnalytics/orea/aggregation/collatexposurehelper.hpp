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

/*! \file orea/aggregation/collatexposurehelper.hpp
    \brief Collateral Exposure Helper Functions (stored in base currency)
    \ingroup analytics
*/

#pragma once

#include <orea/aggregation/collateralaccount.hpp>
#include <ored/portfolio/collateralbalance.hpp>
#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/date.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace data;

//! Collateral Exposure Helper
/*!
  This class contains helper functions to aid in the calculation
  of collateralised exposures.

  It can be used to calculate margin requirements in the presence of
  e.g. thresholds and minimum transfer amounts, update collateral
  account details with e.g. new margin call info, and return
  collateralised exposures to the user/invoker.

  For further information refer to the detailed ORE documentation.

  \ingroup analytics
*/
class CollateralExposureHelper {
public:
    /*!
      Enumeration 'CalculationType' specifies how the collateralised
      exposures should be calculated (please refer to Sungard white-paper
      titled "Closing In On the CloseOut"):
      - 'Symmetric' => margin calls only settled after margin period of risk
      - 'AsymmetricCVA' => margin requested from ctp only settles after margin period of risk
      -- (our margin postings settle instantaneously)
      - 'AsymmetricDVA' => margin postings to ctp only settle after margin period of risk
      -- (margin calls to receive collateral from counterparty settle instantaneously)
      - 'NoLag' => margin calls/postings settled without margin period of risk delay
    */
    enum CalculationType { Symmetric, AsymmetricCVA, AsymmetricDVA, NoLag };

    /*!
      Calculates CSA margin requirement, taking the following into account
      - uncollateralised value
      - collateral value
      - threshold
      - minimum transfer amount
      - independent amount
    */
    static Real marginRequirementCalc(const QuantLib::ext::shared_ptr<CollateralAccount>& collat, const Real& uncollatValue,
                                      const Date& simulationDate);

    /*!
      Performs linear interpolation between dates to
      estimate the value as of simulationDate.
      Flat extrapolation at far end.
    */
    template <class T>
    static Real estimateUncollatValue(const Date& simulationDate, const Real& npv_t0, const Date& date_t0,
                                      const vector<vector<T>>& scenPvProfiles, const unsigned& scenIndex,
                                      const vector<Date>& dateGrid);

    /*!
      Checks if margin call is in need of update, and updates if necessary
    */
    static void updateMarginCall(const QuantLib::ext::shared_ptr<CollateralAccount>& collat, const Real& uncollatValue,
                                 const Date& simulationDate, const Real& accrualFactor,
                                 const CalculationType& calcType = Symmetric, const bool& eligMarginReqDateUs = true,
                                 const bool& eligMarginReqDateCtp = true);

    /*!
        Computes the Credit Support Amount for the portfolio, given an unsecured exposure as input
        All calculations done in CSA currency
    */
    static Real creditSupportAmount(
        const QuantLib::ext::shared_ptr<ore::data::NettingSetDefinition>& nettingSet,
        const Real& uncollatValueCsaCur);

    /*!
      Takes a netting set (and scenario exposures) as input
      and returns collateral balance paths per scenario
    */
    static QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<CollateralAccount>>> collateralBalancePaths(
        const QuantLib::ext::shared_ptr<NettingSetDefinition>& csaDef, const Real& nettingSetPv, const Date& date_t0,
        const vector<vector<Real>>& nettingSetValues, const Date& nettingSet_maturity, const vector<Date>& dateGrid,
        const Real& csaFxTodayRate, const vector<vector<Real>>& csaFxScenarioRates, const Real& csaTodayCollatCurve,
        const vector<vector<Real>>& csaScenCollatCurves, const CalculationType& calcType = Symmetric,
        const QuantLib::ext::shared_ptr<CollateralBalance>& balance = QuantLib::ext::shared_ptr<CollateralBalance>());
};

//! Convert text representation to CollateralExposureHelper::CalculationType
CollateralExposureHelper::CalculationType parseCollateralCalculationType(const string& s);
} // namespace analytics
} // namespace ore
