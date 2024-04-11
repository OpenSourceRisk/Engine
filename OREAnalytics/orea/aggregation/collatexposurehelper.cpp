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

#include <orea/aggregation/collatexposurehelper.hpp>
#include <ql/errors.hpp>

using namespace std;
using namespace QuantLib;

#define FLAT_INTERPOLATION 1

namespace ore {
using namespace data;
namespace analytics {

CollateralExposureHelper::CalculationType parseCollateralCalculationType(const string& s) {
    static map<string, CollateralExposureHelper::CalculationType> m = {
        {"Symmetric", CollateralExposureHelper::Symmetric},
        {"AsymmetricCVA", CollateralExposureHelper::AsymmetricCVA},
        {"AsymmetricDVA", CollateralExposureHelper::AsymmetricDVA},
        {"NoLag", CollateralExposureHelper::NoLag}
    };
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Collateral Calculation Type \"" << s << "\" not recognized");
    }
}

std::ostream& operator<<(std::ostream& out, CollateralExposureHelper::CalculationType t) {
    if (t == CollateralExposureHelper::Symmetric)
        out << "Symmetric";
    else if (t == CollateralExposureHelper::AsymmetricCVA)
        out << "AsymmetricCVA";
    else if (t == CollateralExposureHelper::AsymmetricDVA)
        out << "AsymmetricDVA";
    else if (t == CollateralExposureHelper::NoLag)
        out << "NoLag";
    else
        QL_FAIL("Collateral calculation type not covered");
    return out;
}

Real CollateralExposureHelper::marginRequirementCalc(const QuantLib::ext::shared_ptr<CollateralAccount>& collat,
                                                     const Real& uncollatValue, const Date& simulationDate) {
    // first step, make sure collateral balance is up to date.
    //        collat->updateAccountBalance(simulationDate);

    Real collatBalance = collat->accountBalance();
    Real csa = creditSupportAmount(collat->csaDef(), uncollatValue);

    Real openMargins = collat->outstandingMarginAmount(simulationDate);
    Real collatShortfall = csa - collatBalance - openMargins;

    Real mta;
    if (collatShortfall >= 0.0)
      mta = collat->csaDef()->csaDetails()->mtaRcv();
    else
      mta = collat->csaDef()->csaDetails()->mtaPay();

    Real deliveryAmount = fabs(collatShortfall) >= mta ? (collatShortfall) : 0.0;

    return deliveryAmount;
}

 Real CollateralExposureHelper::creditSupportAmount(
    const QuantLib::ext::shared_ptr<ore::data::NettingSetDefinition>& nettingSet, 
    const Real& uncollatValueCsaCur) {

    Real ia = nettingSet->csaDetails()->independentAmountHeld();
    Real threshold, csa;
    if (uncollatValueCsaCur + ia >= 0) {
        threshold = nettingSet->csaDetails()->thresholdRcv();
        csa = max(uncollatValueCsaCur + ia - threshold, 0.0);
    }
    else {
        threshold = nettingSet->csaDetails()->thresholdPay();
        // N.B. the min and change of sign on threshold.
        csa = min(uncollatValueCsaCur + ia + threshold, 0.0);
    }
    return csa;
}
  
template <class T>
Real CollateralExposureHelper::estimateUncollatValue(const Date& simulationDate, const Real& npv_t0,
                                                     const Date& date_t0, const vector<vector<T>>& scenPvProfiles,
                                                     const unsigned& scenIndex, const vector<Date>& dateGrid) {

    QL_REQUIRE(simulationDate >= date_t0, "CollatExposureHelper error: simulation date < start date");
    QL_REQUIRE(dateGrid[0] >= date_t0, "CollatExposureHelper error: cube dateGrid starts before t0");

    if (simulationDate >= dateGrid.back())
        return scenPvProfiles.back()[scenIndex]; // flat extrapolation
    if (simulationDate == date_t0)
        return npv_t0;
    for (unsigned i = 0; i < dateGrid.size(); i++) {
        if (dateGrid[i] == simulationDate)
            // return if collat sim-date is the same as an exposure grid date
            return scenPvProfiles[i][scenIndex];
#ifdef FLAT_INTERPOLATION
        else if (simulationDate < dateGrid.front())
            return scenPvProfiles.front()[scenIndex];
        else if (i < dateGrid.size() - 1 && simulationDate > dateGrid[i] && simulationDate < dateGrid[i + 1])
            return scenPvProfiles[i + 1][scenIndex];
#endif
    }

    // if none of above criteria are met, perform interpolation
    Date t1, t2;
    Real npv1, npv2;
    if (simulationDate <= dateGrid[0]) {
        t1 = date_t0;
        t2 = dateGrid[0];
        npv1 = npv_t0;
        npv2 = scenPvProfiles[0][scenIndex];
    } else {
        vector<Date>::const_iterator it = lower_bound(dateGrid.begin(), dateGrid.end(), simulationDate);
        QL_REQUIRE(it != dateGrid.end(), "CollatExposureHelper error; "
                                             << "date interpolation points not found (it.end())");
        QL_REQUIRE(it != dateGrid.begin(), "CollatExposureHelper error; "
                                               << "date interpolation points not found (it.begin())");
        Size pos1 = (it - 1) - dateGrid.begin();
        Size pos2 = it - dateGrid.begin();
        t1 = dateGrid[pos1];
        t2 = dateGrid[pos2];
        npv1 = scenPvProfiles[pos1][scenIndex];
        npv2 = scenPvProfiles[pos2][scenIndex];
    }

    Real newPv = npv1 + ((npv2 - npv1) * (double(simulationDate - t1) / double(t2 - t1)));
    QL_REQUIRE((npv1 <= newPv && newPv <= npv2) || (npv1 >= newPv && newPv >= npv2),
               "CollatExposureHelper error; "
                   << "interpolated Pv value " << newPv << " out of range (" << npv1 << " " << npv2 << ") "
                   << "for simulation date " << simulationDate << " between " << t1 << " and " << t2);

    return newPv;
}

void CollateralExposureHelper::updateMarginCall(const QuantLib::ext::shared_ptr<CollateralAccount>& collat,
                                                const Real& uncollatValue, const Date& simulationDate,
                                                const Real& annualisedZeroRate, const CalculationType& calcType,
                                                const bool& eligMarginReqDateUs, const bool& eligMarginReqDateCtp) {
    collat->updateAccountBalance(simulationDate, annualisedZeroRate);

    Real margin = marginRequirementCalc(collat, uncollatValue, simulationDate);

    if (margin != 0.0) {
        // settle margin call on appropriate date
        // (dependent upon MPR and collateralised calculation methodology)
        Date marginPayDate;
	// RL 2020-07-17
	// 1) If the calculation type is set to NoLag:
	//    Collateral balances are NOT delayed by the MPoR, but we use the close-out NPV in exposure calculations,
	//    see similar comment and the equivalent change in the post processor.
	// 2) Otherwise:
	//    Collateral balances are delayed by the MPoR (if possible, i.e. the valuation grid has MPoR spacing),
	//    and we use the default date NPV.
	//    This is the treatment in the ORE releases up to June 2020).
	Period lag = (calcType == NoLag ? 0*Days : collat->csaDef()->csaDetails()->marginPeriodOfRisk());
        if (margin > 0.0 && eligMarginReqDateUs) {
	    marginPayDate =
                (calcType == AsymmetricDVA ? simulationDate : simulationDate + lag);
            collat->updateMarginCall(margin, marginPayDate, simulationDate);
        } else if (margin < 0.0 && eligMarginReqDateCtp) {
            marginPayDate =
                (calcType == AsymmetricCVA ? simulationDate : simulationDate + lag);
            collat->updateMarginCall(margin, marginPayDate, simulationDate);
        } else {
        }
    }
}

QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<CollateralAccount>>> CollateralExposureHelper::collateralBalancePaths(
    const QuantLib::ext::shared_ptr<NettingSetDefinition>& csaDef, const Real& nettingSetPv, const Date& date_t0,
    const vector<vector<Real>>& nettingSetValues, const Date& nettingSet_maturity, const vector<Date>& dateGrid,
    const Real& csaFxTodayRate, const vector<vector<Real>>& csaFxScenarioRates, const Real& csaTodayCollatCurve,
    const vector<vector<Real>>& csaScenCollatCurves, const CalculationType& calcType,
    const QuantLib::ext::shared_ptr<CollateralBalance>& balance) {

    try {
        // step 1; build a collateral account object, assuming t0 VM balance from the balance object (zero balance if missing),
        //         and calculate t0 margin requirement

        Real initialBalance = 0.0;
        if (balance && balance->variationMargin() != Null<Real>()) {            
            initialBalance = balance->variationMargin();
            DLOG("initial collateral balance: " << initialBalance);
        }
        else {
            DLOG("initial collateral balance not found");
        }
        
        QuantLib::ext::shared_ptr<CollateralAccount> tmpAcc(new CollateralAccount(csaDef, initialBalance, date_t0));
        DLOG("tmp initial collateral balance: " << tmpAcc->balance_t0());
        DLOG("tmp current collateral balance: " << tmpAcc->accountBalance());

        Real bal_t0 = marginRequirementCalc(tmpAcc, nettingSetPv, date_t0);

        // step 2; build a new collateral account object with t0 balance = bal_t0
        // a copy of this new object will be used as base for each scenario collateral path
        CollateralAccount baseAcc(csaDef, bal_t0, date_t0);
        DLOG("base current collateral balance: " << bal_t0 << ", " << baseAcc.accountBalance());

        // step 3; build an empty container for the return value(s)
        QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<CollateralAccount>>> scenarioCollatPaths(
            new vector<QuantLib::ext::shared_ptr<CollateralAccount>>());

        // step 4; start loop over scenarios
        Size numScenarios = nettingSetValues.front().size();
        QL_REQUIRE(numScenarios == csaFxScenarioRates.front().size(), "netting values -v- scenario FX rate mismatch");
        Date simEndDate = std::min(nettingSet_maturity, dateGrid.back()) + csaDef->csaDetails()->marginPeriodOfRisk();
        for (unsigned i = 0; i < numScenarios; i++) {
            QuantLib::ext::shared_ptr<CollateralAccount> collat(new CollateralAccount(baseAcc));
            Date tmpDate = date_t0; // the date which gets evolved
            Date nextMarginReqDateUs = date_t0;
            Date nextMarginReqDateCtp = date_t0;
            while (tmpDate <= simEndDate) {
                QL_REQUIRE(tmpDate <= nextMarginReqDateUs && tmpDate <= nextMarginReqDateCtp &&
                               (tmpDate == nextMarginReqDateUs || tmpDate == nextMarginReqDateCtp),
                           "collateral balance path generation error; invalid time stepping");
                bool eligMarginReqDateUs = tmpDate == nextMarginReqDateUs ? true : false;
                bool eligMarginReqDateCtp = tmpDate == nextMarginReqDateCtp ? true : false;
                Real uncollatVal = estimateUncollatValue(tmpDate, nettingSetPv, date_t0, nettingSetValues, i, dateGrid);
                Real fxValue = estimateUncollatValue(tmpDate, csaFxTodayRate, date_t0, csaFxScenarioRates, i, dateGrid);
                Real annualisedZeroRate =
                    estimateUncollatValue(tmpDate, csaTodayCollatCurve, date_t0, csaScenCollatCurves, i, dateGrid);
                uncollatVal /= fxValue;
                updateMarginCall(collat, uncollatVal, tmpDate, annualisedZeroRate, calcType, eligMarginReqDateUs,
                                 eligMarginReqDateCtp);

                if (nextMarginReqDateUs == tmpDate)
                    nextMarginReqDateUs = tmpDate + collat->csaDef()->csaDetails()->marginCallFrequency();
                if (nextMarginReqDateCtp == tmpDate)
                    nextMarginReqDateCtp = tmpDate + collat->csaDef()->csaDetails()->marginPostFrequency();
                tmpDate = std::min(nextMarginReqDateUs, nextMarginReqDateCtp);
            }
            QL_REQUIRE(tmpDate > simEndDate,
                       "collateral balance path generation error; while loop terminated too early. ("
                           << tmpDate << ", " << simEndDate << ")");
            // set account balance to zero after maturity of portfolio
            collat->closeAccount(simEndDate + Period(1, Days));
            scenarioCollatPaths->push_back(collat);
        }
        return scenarioCollatPaths;
    } catch (const std::exception& e) {
        QL_FAIL(e.what());
    } catch (...) {
        QL_FAIL("CollateralExposureHelper - unknown error when generating collateralBalancePaths");
    }
}
} // namespace analytics
} // namespace ore
