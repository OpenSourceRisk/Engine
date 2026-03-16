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

/*! \file orea/aggregation/xvacalculator.hpp
    \brief CVA calculator base class
    \ingroup analytics
*/

#pragma once

#include <orea/aggregation/dimcalculator.hpp>

#include <ored/portfolio/portfolio.hpp>
#include <ored/report/report.hpp>

#include <ql/time/date.hpp>

#include <ql/shared_ptr.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace QuantExt;
using namespace data;
using namespace std;

//! XVA Calculator base class
/*!
  Derived classes implement a constructor with the relevant additional input data
  and a build function that performs the XVA calculations for all netting sets and
  along all paths.
*/
class ValueAdjustmentCalculator {
public:

    ValueAdjustmentCalculator(
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

    virtual ~ValueAdjustmentCalculator() {}

    //! Compute cva along all paths and fill result structures
    virtual void build();

    virtual const vector<Date>& dates() { return tradeExposureCube_->dates(); };

    virtual const Date asof() { return market_->asofDate(); };

    virtual const Real calculateCvaIncrement(const string& tid, const string& cid,
                                             const Date& d0, const Date& d1, const Real& rr) = 0;
    virtual const Real calculateDvaIncrement(const string& tid,
                                             const Date& d0, const Date& d1, const Real& rr) = 0;
    virtual const Real calculateNettingSetCvaIncrement(
        const string& nid, const string& cid, const Date& d0, const Date& d1, const Real& rr) = 0;
    virtual const Real calculateNettingSetDvaIncrement(
        const string& nid, const Date& d0, const Date& d1, const Real& rr) = 0;
    virtual const Real calculateFbaIncrement(const string& tid, const string& cid, const string& dvaName,
                                             const Date& d0, const Date& d1, const Real& dcf) = 0;
    virtual const Real calculateFcaIncrement(const string& tid, const string& cid, const string& dvaName,
                                             const Date& d0, const Date& d1, const Real& dcf) = 0;
    virtual const Real calculateNettingSetFbaIncrement(const string& nid, const string& cid, const string& dvaName,
                                                       const Date& d0, const Date& d1, const Real& dcf) = 0;
    virtual const Real calculateNettingSetFcaIncrement(const string& nid, const string& cid, const string& dvaName,
                                                       const Date& d0, const Date& d1, const Real& dcf) = 0;
    virtual const Real calculateNettingSetMvaIncrement(const string& nid, const string& cid,
                                                       const Date& d0, const Date& d1, const Real& dcf) = 0;

    //! CVA map for all the trades
    const map<string, Real>& tradeCva();

    //! DVA map for all the trades
    const map<string, Real>& tradeDva();

    //! CVA map for all the netting sets
    const map<string, Real>& nettingSetCva();

    //! DVA map for all the netting sets
    const map<string, Real>& nettingSetDva();

    //! Sum CVA map for all the netting sets
    const map<string, Real>& nettingSetSumCva();

    //! Sum DVA map for all the netting sets
    const map<string, Real>& nettingSetSumDva();

    //! CVA for the specified trade
    const Real& tradeCva(const string& trade);

    //! DVA for the specified trade
    const Real& tradeDva(const string& trade);

    //! FBA for the specified trade
    const Real& tradeFba(const string& trade);

    //! FBA (excl own survival probability) for the specified trade
    const Real& tradeFba_exOwnSp(const string& trade);

    //! FBA (excl all survival probability) for the specified trade
    const Real& tradeFba_exAllSp(const string& trade);

    //! FCA for the specified trade
    const Real& tradeFca(const string& trade);

    //! FCA (excl own survival probability) for the specified trade
    const Real& tradeFca_exOwnSp(const string& trade);

    //! FCA (excl all survival probability) for the specified trade
    const Real& tradeFca_exAllSp(const string& trade);

    //! MVA for the specified trade
    const Real& tradeMva(const string& trade);

    //! Sum of trades' CVA for the specified netting set
    const Real& nettingSetSumCva(const string& nettingSet);

    //! Sum of trades' DVA for the specified netting set
    const Real& nettingSetSumDva(const string& nettingSet);

    //! CVA for the specified netting set
    const Real& nettingSetCva(const string& nettingSet);

    //! DVA for the specified netting set
    const Real& nettingSetDva(const string& nettingSet);

    //! FBA for the specified netting set
    const Real& nettingSetFba(const string& nettingSet);

    //! FBA (excl own survival probability) for the specified netting set
    const Real& nettingSetFba_exOwnSp(const string& nettingSet);

    //! FBA (excl all survival probability) for the specified netting set
    const Real& nettingSetFba_exAllSp(const string& nettingSet);

    //! FCA for the specified netting set
    const Real& nettingSetFca(const string& nettingSet);

    //! FCA (excl own survival probability) for the specified netting set
    const Real& nettingSetFca_exOwnSp(const string& nettingSet);

    //! FCA (excl all survival probability) for the specified netting set
    const Real& nettingSetFca_exAllSp(const string& nettingSet);

    //! MVA for the specified netting set
    const Real& nettingSetMva(const string& nettingSet);

protected:
    QuantLib::ext::shared_ptr<Portfolio> portfolio_;
    QuantLib::ext::shared_ptr<Market> market_;
    string configuration_;
    string baseCurrency_;
    string dvaName_;
    string fvaBorrowingCurve_;
    string fvaLendingCurve_;
    bool applyDynamicInitialMargin_;
    QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator> dimCalculator_;
    const QuantLib::ext::shared_ptr<NPVCube> tradeExposureCube_;
    const QuantLib::ext::shared_ptr<NPVCube> nettingSetExposureCube_;
    Size tradeEpeIndex_;
    Size tradeEneIndex_;
    Size nettingSetEpeIndex_;
    Size nettingSetEneIndex_;
    bool flipViewXVA_;
    string flipViewBorrowingCurvePostfix_;
    string flipViewLendingCurvePostfix_;

    map<string, string> nettingSetCpty_;
    // For each trade: values
    map<string, Real> tradeCva_;
    map<string, Real> tradeDva_;
    map<string, Real> tradeFba_;
    map<string, Real> tradeFba_exOwnSp_;
    map<string, Real> tradeFba_exAllSp_;
    map<string, Real> tradeFca_;
    map<string, Real> tradeFca_exOwnSp_;
    map<string, Real> tradeFca_exAllSp_;
    map<string, Real> tradeMva_; // FIXME: MVA is not computed at trade level yet, remains initialised at 0

    // For each netting: values
    map<string, Real> nettingSetSumCva_;
    map<string, Real> nettingSetSumDva_;
    map<string, Real> nettingSetCva_;
    map<string, Real> nettingSetDva_;
    map<string, Real> nettingSetFba_;
    map<string, Real> nettingSetFba_exOwnSp_;
    map<string, Real> nettingSetFba_exAllSp_;
    map<string, Real> nettingSetFca_;
    map<string, Real> nettingSetFca_exOwnSp_;
    map<string, Real> nettingSetFca_exAllSp_;
    map<string, Real> nettingSetMva_;
};

} // namespace analytics
} // namespace ore
