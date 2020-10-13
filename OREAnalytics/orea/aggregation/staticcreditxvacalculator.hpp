/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file orea/aggregation/staticcreditxvacalculator.hpp
    \brief XVA calculator with static credit
    \ingroup analytics
*/

#pragma once

#include <orea/aggregation/xvacalculator.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace QuantExt;
using namespace data;
using namespace std;

//! XVA Calculator base with static credit
/*!
  XVA is calculated using survival probability from market
*/
class StaticCreditXvaCalculator : public ValueAdjustmentCalculator {
public:
    StaticCreditXvaCalculator(
        //! Driving portfolio consistent with the cube below
        const boost::shared_ptr<Portfolio> portfolio,
        const boost::shared_ptr<Market> market,
        const string& configuration,
        const string& baseCurrency,
        //!
        const string& dvaName,
        //! FVA borrowing curve
        const string& fvaBorrowingCurve,
        //! FVA lending curve
        const string& fvaLendingCurve,
        const bool applyDynamicInitialMargin,
        const boost::shared_ptr<DynamicInitialMarginCalculator> dimCalculator,
        const boost::shared_ptr<NPVCube> tradeExposureCube,
        const boost::shared_ptr<NPVCube> nettingSetExposureCube,
        const Size tradeEpeIndex = 0, const Size tradeEneIndex = 1,
        const Size nettingSetEpeIndex = 0, const Size nettingSetEneIndex = 1);

    virtual const Real calculateCvaIncrement(const string& tid, const string& cid,
                                             const Date& d0, const Date& d1, const Real& rr);
    virtual const Real calculateDvaIncrement(const string& tid,
                                             const Date& d0, const Date& d1, const Real& rr);
    virtual const Real calculateNettingSetCvaIncrement(
        const string& nid, const string& cid, const Date& d0, const Date& d1, const Real& rr);
    virtual const Real calculateNettingSetDvaIncrement(
        const string& nid, const Date& d0, const Date& d1, const Real& rr);
    virtual const Real calculateFbaIncrement(const string& tid, const string& cid, const string& dvaName,
                                             const Date& d0, const Date& d1, const Real& dcf);
    virtual const Real calculateFcaIncrement(const string& tid, const string& cid, const string& dvaName,
                                             const Date& d0, const Date& d1, const Real& dcf);
    virtual const Real calculateNettingSetFbaIncrement(const string& nid, const string& cid, const string& dvaName,
                                                       const Date& d0, const Date& d1, const Real& dcf);
    virtual const Real calculateNettingSetFcaIncrement(const string& nid, const string& cid, const string& dvaName,
                                                       const Date& d0, const Date& d1, const Real& dcf);
    virtual const Real calculateNettingSetMvaIncrement(const string& nid, const string& cid,
                                                       const Date& d0, const Date& d1, const Real& dcf);

protected:
    map<Date, Size> dateIndexMap_; // cache for performance
};

} // namespace analytics
} // namespace ore
