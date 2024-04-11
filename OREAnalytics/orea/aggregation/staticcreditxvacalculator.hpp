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
        const QuantLib::ext::shared_ptr<Portfolio> portfolio,
	//! Today's market
        const QuantLib::ext::shared_ptr<Market> market,
	//! Market configuration to be used
        const string& configuration,
	//! Base currency amounts will be converted to
        const string& baseCurrency,
        //! Own party name for DVA calculations
        const string& dvaName,
        //! FVA borrowing curve
        const string& fvaBorrowingCurve,
        //! FVA lending curve
        const string& fvaLendingCurve,
	//! Deactivate initial margin calculation even if active at netting set level
        const bool applyDynamicInitialMargin,
	//! Dynamic Initial Margin calculator
        const QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator> dimCalculator,
	//! Storage ofdefault NPVs, close-out NPVs, cash flows at trade level
        const QuantLib::ext::shared_ptr<NPVCube> tradeExposureCube,
	//! Storage of sensitivity vectors at netting set level
        const QuantLib::ext::shared_ptr<NPVCube> nettingSetExposureCube,
	//! Index of the trade EPE storage in the internal exposure cube
	const Size tradeEpeIndex = 0,
	//! Index of the trade ENE storage in the internal exposure cube
	const Size tradeEneIndex = 1,
        //! Index of the netting set EPE storage in the internal exposure cube
	const Size nettingSetEpeIndex = 1,
	//! Index of the netting set ENE storage in the internal exposure cube
	const Size nettingSetEneIndex = 2,
	//! Flag to indicate flipped xva calculation
	const bool flipViewXVA = false,
	//! Postfix for flipView borrowing curve for fva
	const string& flipViewBorrowingCurvePostfix = "_BORROW",
	//! Postfix for flipView lending curve for fva
       	const string& flipViewLendingCurvePostfix = "_LEND");

    virtual const Real calculateCvaIncrement(const string& tid, const string& cid,
                                             const Date& d0, const Date& d1, const Real& rr) override;
    virtual const Real calculateDvaIncrement(const string& tid,
                                             const Date& d0, const Date& d1, const Real& rr) override;
    virtual const Real calculateNettingSetCvaIncrement(
        const string& nid, const string& cid, const Date& d0, const Date& d1, const Real& rr) override;
    virtual const Real calculateNettingSetDvaIncrement(
        const string& nid, const Date& d0, const Date& d1, const Real& rr) override;
    virtual const Real calculateFbaIncrement(const string& tid, const string& cid, const string& dvaName,
                                             const Date& d0, const Date& d1, const Real& dcf) override;
    virtual const Real calculateFcaIncrement(const string& tid, const string& cid, const string& dvaName,
                                             const Date& d0, const Date& d1, const Real& dcf) override;
    virtual const Real calculateNettingSetFbaIncrement(const string& nid, const string& cid, const string& dvaName,
                                                       const Date& d0, const Date& d1, const Real& dcf) override;
    virtual const Real calculateNettingSetFcaIncrement(const string& nid, const string& cid, const string& dvaName,
                                                       const Date& d0, const Date& d1, const Real& dcf) override;
    virtual const Real calculateNettingSetMvaIncrement(const string& nid, const string& cid,
                                                       const Date& d0, const Date& d1, const Real& dcf) override;

protected:
    map<Date, Size> dateIndexMap_; // cache for performance
};

} // namespace analytics
} // namespace ore
