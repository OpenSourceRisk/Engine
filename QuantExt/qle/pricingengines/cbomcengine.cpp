/*
 Copyright (C) 2020 Quaternion Risk Management Ltd.
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

#include <qle/pricingengines/cbomcengine.hpp>
#include <qle/math/bucketeddistribution.hpp>
#include <ql/experimental/credit/loss.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    void MonteCarloCBOEngine::interestWaterfall(Size i, // sample index
                                    Size j, // date index
                                    const vector<Date>& dates,
                                    map<Currency,vector<Cash> >& iFlows, 
                                    map<Currency,Cash>& tranche,
                                    map<Currency,vector<vector<Real> > >& balance,
                                    map<Currency,Real>& interest,
                                    map<Currency,Real>& interestAcc,
                                    Real cureAmount) const {
        Currency ccy = arguments_.ccy;

        Real tiny = 1e-9;
        if (balance[ccy][j][i] < tiny) {
            tranche[ccy].flow_ = 0.0;
            tranche[ccy].discountedFlow_ = 0.0;
            return;
        }

        Real ccyDis = (iFlows[ccy][j].flow_ > 0 ?
                       iFlows[ccy][j].discountedFlow_ / iFlows[ccy][j].flow_:
                       0.0);

        // Accrued Interest

        Real amount = std::min(iFlows[ccy][j].flow_, interestAcc[ccy]);

        tranche[ccy].flow_ += amount;
        tranche[ccy].discountedFlow_ += amount * ccyDis;

        iFlows[ccy][j].flow_  -= amount;
        iFlows[ccy][j].discountedFlow_ -= amount * ccyDis;

        interest[ccy] -= amount;

        // Truncate rounding errors
        balance[ccy][j][i] = std::max(balance[ccy][j][i], 0.0);
        iFlows[ccy][j].flow_ = std::max(iFlows[ccy][j].flow_, 0.0);
        iFlows[ccy][j].discountedFlow_ = std::max(iFlows[ccy][j].discountedFlow_, 0.0);
        tranche[ccy].discountedFlow_ = std::max(tranche[ccy].discountedFlow_, 0.0);
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    void MonteCarloCBOEngine::icocInterestWaterfall(Size i, // sample index
                                                    Size j, // date index
                                                    Size l, //tranche
                                                    const vector<Date>& dates,
                                                    map<Currency,vector<Cash> >& iFlows,
                                                    vector<map<Currency,Cash> >& tranches,
                                                    vector<map<Currency,vector<vector<Real> > > >& balances,
                                                    Real cureAmount) const {
        Currency ccy = arguments_.ccy;

        Real ccyDis = (iFlows[ccy][j].flow_ > 0 ?
                       iFlows[ccy][j].discountedFlow_ / iFlows[ccy][j].flow_:
                       0.0);

        //IC and OC

        Real cureAvailable = min(iFlows[ccy][j].flow_, cureAmount);

        for(Size k = 0; k <= l; k++){

            Real amount = std::min(balances[k][ccy][j][i], cureAvailable);

            tranches[k][ccy].flow_ += amount;
            tranches[k][ccy].discountedFlow_ += amount * ccyDis;

            iFlows[ccy][j].flow_           -= amount;
            iFlows[ccy][j].discountedFlow_ -= amount * ccyDis;

            balances[k][ccy][j][i] -= amount;

            cureAvailable -= amount;

            // truncate rounding errors
            balances[k][ccy][j][i] = std::max(balances[k][ccy][j][i], 0.0);
            iFlows[ccy][j].flow_ = std::max(iFlows[ccy][j].flow_, 0.0);
            iFlows[ccy][j].discountedFlow_ = std::max(iFlows[ccy][j].discountedFlow_, 0.0);
            tranches[k][ccy].discountedFlow_ = std::max(tranches[k][ccy].discountedFlow_, 0.0);
        }
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    void MonteCarloCBOEngine::principalWaterfall(Size i, // sample index
                                                Size j, // date index
                                                const vector<Date>& dates,
                                                map<Currency,vector<Cash> >& pFlows, 
                                                map<Currency,Cash>& tranche,
                                                map<Currency,vector<vector<Real> > >& balance,
                                                map<Currency,Real>& interest) const {
        Currency ccy = arguments_.ccy;

        Real ccyDis = (pFlows[ccy][j].flow_ > 0 ?
                       pFlows[ccy][j].discountedFlow_ / pFlows[ccy][j].flow_:
                       0.0);

        //Principal Waterfall

        Real amount = std::min(pFlows[ccy][j].flow_, balance[ccy][j][i]);

        tranche[ccy].flow_ += amount;
        tranche[ccy].discountedFlow_ += amount * ccyDis;

        pFlows[ccy][j].flow_           -= amount;
        pFlows[ccy][j].discountedFlow_ -= amount * ccyDis;

        balance[ccy][j][i] -= amount;

        // truncate rounding errors
        balance[ccy][j][i] = std::max(balance[ccy][j][i], 0.0);
        pFlows[ccy][j].flow_ = std::max(pFlows[ccy][j].flow_, 0.0);
        pFlows[ccy][j].discountedFlow_ = std::max(pFlows[ccy][j].discountedFlow_, 0.0);
        tranche[ccy].discountedFlow_ = std::max(tranche[ccy].discountedFlow_, 0.0);

        interest[ccy] -= std::min(interest[ccy], amount);
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    Real MonteCarloCBOEngine::icocCureAmount(Size i,
                        Size j,
                        Size k,
                        Real basketNotional,
                        Real basketInterest,
                        vector<map<Currency ,vector<vector<Real> > > >& trancheBalances,
                        vector<Real> trancheInterestRates,
                        Real icRatio,
                        Real ocRatio) const {
        Currency ccy = arguments_.ccy;

        Real cureAmount;

        if((icRatio < 0.) && (ocRatio < 0.)){
            cureAmount = 0.;
        }
        else{
            Real poC = 0.;
            Real piC = 0.;

            for(Size l = 0; l < k; l++){
                poC-= trancheBalances[l][ccy][j][i];
                piC-= trancheBalances[l][ccy][j][i]*trancheInterestRates[l];
            }

            poC+= basketNotional/ocRatio;
            if(trancheInterestRates[k] <= 0.){
                piC=poC;
            }
            else {
                piC+= basketInterest/icRatio;
                piC/= trancheInterestRates[k];
            }
            piC = std::max(piC, 0.);
            poC = std::max(poC, 0.);
            Real pTarget = std::min(poC, piC);

            cureAmount = max(trancheBalances[k][ccy][j][i] - pTarget, 0.) ;
        }
        return cureAmount;
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // copied from AmortizingCboLbEngine
    map<Date, string> MonteCarloCBOEngine::getLossDistributionDates(const Date& valuationDate) const {
        if (lossDistributionPeriods_.size() == 0)
            return map<Date, string>();

        // For each period, return first date in CBO schedule greater than or equal to it
        map<Date, string> lossDistributionDates;
        const vector<Date>& cboDates = arguments_.schedule.dates();
        Date date;
        vector<Date>::const_iterator it;
        ostringstream oss;
        for (Size i = 0; i < lossDistributionPeriods_.size(); i++) {
            date = valuationDate + lossDistributionPeriods_[i];
            oss << io::short_period(lossDistributionPeriods_[i]);
            it = lower_bound(cboDates.begin(), cboDates.end(), date);
            if (it == cboDates.begin()) {
                continue;
            } else if (it == cboDates.end()) {
                lossDistributionDates[cboDates.back()] = oss.str();
                break;
            } else {
                lossDistributionDates[*it] = oss.str();
            }
            oss.str("");
        }

        // Add the maturity date of the note also with a special string
        // This overwrites any entry for the last date above
        lossDistributionDates[cboDates.back()] = "Maturity";

        return lossDistributionDates;
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    void MonteCarloCBOEngine::calculate() const {

        Date today = Settings::instance().evaluationDate();
        initialize(); //set the underlying Basket 
        rdm_->reset();

        // Prepare additional results for loss distributions if they have been requested
        map<Date, string> lossDistributionDates = getLossDistributionDates(today);
        vector<Date> lossDistributionDatesVector;
        map<string, QuantLib::ext::shared_ptr<BucketedDistribution> > lossDistributionMap;
        if (!lossDistributionDates.empty()) {

            // calculate max loss from the pool
            Real maxLoss = 0.0;
            for (auto& bonds :  arguments_.basket->bonds()) {
                string name = bonds.first;
                maxLoss += bonds.second->notional(today) * arguments_.basket->multiplier(name) * (1.0 - arguments_.basket->recoveryRate(name));
            }

            // initialise the Loss BucketedDistribtions with the above maxLoss
            Size numBuckets = 100;
            for (map<Date, string>::iterator it = lossDistributionDates.begin(); it != lossDistributionDates.end(); it++) {
                QuantLib::ext::shared_ptr<BucketedDistribution> bucketedDistribution = 
                    QuantLib::ext::make_shared<BucketedDistribution> (0, maxLoss, numBuckets);
                // Set all probabilities = 0.0;
                for (Size i = 0; i < numBuckets; i++)
                    bucketedDistribution->probabilities()[i] = 0.0;
                lossDistributionMap[it->second] = bucketedDistribution;

                lossDistributionDatesVector.push_back(it->first);
            }
        }

        //get date grid, dependending on tranche (to be valued) 
        vector<Date> dates = arguments_.schedule.dates();

        //adjust the date grid, such that it starts with today
        std::vector<Date>::iterator it;
        for (it=dates.begin(); it<dates.end(); ++it)
            if(*it > today) break;

        dates.erase(dates.begin(), it);
        dates.insert(dates.begin(), today);

        Real tmax = 1.0 + ActualActual(ActualActual::ISDA).yearFraction(today, dates.back());

        arguments_.basket->fillFlowMaps();

        arguments_.basket->setGrid(dates);

        //other requirements 
        DayCounter feeDayCount = arguments_.feeDayCounter;
        Currency ccy = arguments_.ccy;

        //asset flows 
        map<Currency, vector<Cash> > cf;
        map<Currency, vector<Cash> > iFlows;
        map<Currency, vector<Cash> > pFlows;
        map<Currency, vector<Real> > basketNotional;

        //liability flows 
        vector<Real> basketValue(samples_, 0.0);
        vector<vector<Real> > trancheValue;
        vector<Tranche> tranches = arguments_.tranches;
        for (Size i = 0 ; i < tranches.size() ; i ++){
            vector<Real> tempTrancheValue(samples_, 0.0);
            trancheValue.push_back(tempTrancheValue);
        }

        vector <map<Currency, vector<vector<Real> > > > trancheBalance; 
        trancheBalance.resize(tranches.size());
        for (Size i = 0 ; i < tranches.size() ; i ++){
            trancheBalance[i][ccy].resize(
                                dates.size(), vector<Real>(samples_, 0.0));
        }

        vector<Real> feeValue(samples_, 0.0);
        vector<Real> subfeeValue(samples_, 0.0);

        /*
        //prepare for convergence analysis
        bool convergenceFlag = false;
        std::vector<double> runningMean(tranches.size(),0.0);
        std::vector<double> runningStdev(tranches.size(),0.0);
        if(ore::data::Log::instance().filter(ORE_DATA)){
            TLOG("convergence analysis - check first two moments");
            std::ostringstream labels;
            labels << "sample";
            for(Size k = 0 ; k < tranches.size(); k++)
                labels << "," << tranches[k].name << "-mean,"<< tranches[k].name << "-stdev";
            TLOG(labels.str());
            convergenceFlag = true;
        }
        */

        for (Size i = 0; i < samples_; i++) { 
            rdm_->nextSequence(tmax);

            for( Size k = 0 ; k < tranches.size() ; k++){
                trancheBalance[k][ccy][0][i] = arguments_.tranches[k].faceAmount;
            }

            vector< map<Currency, Real> >trancheInterest;
            trancheInterest.resize(tranches.size());
            for( Size k = 0 ; k < tranches.size() ; k++){
                trancheInterest[k][ccy] = 0.0;
            }

            //Get Collection from bondbasket and exchange into base currency...
            cf.clear();
            iFlows.clear();
            pFlows.clear();
            basketNotional.clear();

            map<Currency, vector<Cash> > cf_full = arguments_.basket->scenarioCashflow(dates);
            map<Currency, vector<Cash> > iFlows_full = arguments_.basket->scenarioInterestflow(dates);
            map<Currency, vector<Cash> > pFlows_full = arguments_.basket->scenarioPrincipalflow(dates);
            map<Currency, vector<Real> > basketNotional_full = arguments_.basket->scenarioRemainingNotional(dates);

            set<Currency> basketCurrency = arguments_.basket->unique_currencies();

            if(basketCurrency.size() > 1){

                vector<Cash> cf_single;
                vector<Cash> iFlows_single;
                vector<Cash> pFlows_single;
                vector<Real> basketNotional_single;

                for(size_t d = 0; d < dates.size(); d++){

                    double cf1 = 0.0;
                    double cf2 = 0.0;
                    double if1 = 0.0;
                    double if2 = 0.0;
                    double pf1 = 0.0;
                    double pf2 = 0.0;
                    //Cash pf(0.0, 0.0);
                    double bn = 0.0;

                    for(auto& basketCcy : basketCurrency){
                        cf1 += arguments_.basket->convert(cf_full[basketCcy][d].flow_, basketCcy, dates[d]);
                        cf2 += arguments_.basket->convert(cf_full[basketCcy][d].discountedFlow_, basketCcy, dates[d]);

                        if1 += arguments_.basket->convert(iFlows_full[basketCcy][d].flow_, basketCcy, dates[d]);
                        if2 += arguments_.basket->convert(iFlows_full[basketCcy][d].discountedFlow_, basketCcy, dates[d]);

                        pf1 += arguments_.basket->convert(pFlows_full[basketCcy][d].flow_, basketCcy, dates[d]);
                        pf2 += arguments_.basket->convert(pFlows_full[basketCcy][d].discountedFlow_, basketCcy, dates[d]);

                        bn += arguments_.basket->convert(basketNotional_full[basketCcy][d], basketCcy, dates[d]);
                    }
                    cf_single.push_back(Cash(cf1, cf2));
                    iFlows_single.push_back(Cash(if1,if2));
                    pFlows_single.push_back(Cash(pf1,pf2));
                    basketNotional_single.push_back(bn);
                }
                cf[ccy] = cf_single;
                iFlows[ccy] = iFlows_single;
                pFlows[ccy] = pFlows_single;
                basketNotional[ccy] = basketNotional_single;

            }
            else{
                cf = cf_full;
                iFlows = iFlows_full;
                pFlows = pFlows_full;
                basketNotional = basketNotional_full;
            }

            for (Size j = 1; j < dates.size(); j++) {

                //Back out discountfactors 

                Real intCcyDis = (iFlows[ccy][j].flow_ > 0 ?
                               iFlows[ccy][j].discountedFlow_ / iFlows[ccy][j].flow_ :
                               0.0);

                /**************************************************************
                 * check flows add up
                 */
                Real tiny = 1.0e-6;
                Real flowsCheck = fabs(cf[ccy][j].flow_
                                       -iFlows[ccy][j].flow_
                                       -pFlows[ccy][j].flow_);
                QL_REQUIRE( flowsCheck < tiny,
                           "Interest and Principal Flows don't sum to Total: "
                           << flowsCheck);

                Real dfFlowsCheck = fabs(cf[ccy][j].discountedFlow_
                                       -iFlows[ccy][j].discountedFlow_
                                       -pFlows[ccy][j].discountedFlow_);

                QL_REQUIRE( dfFlowsCheck < tiny,
                           "discounted Interest and Principal Flows don't sum to Total: "
                           << dfFlowsCheck);

                /**************************************************************
                 * tranche interest claim
                 */
                vector<map<Currency, Real> >trancheIntAcc;
                trancheIntAcc.resize(tranches.size());
                vector<Real> trancheInterestRates;
                // ccy
                for (Size k = 0 ; k< tranches.size(); k++){
                    Real trancheInterestRate =
                        tranches[k].leg[j-1]->amount()/
                        tranches[k].faceAmount;
                    trancheInterestRates.push_back(trancheInterestRate);
                    //ccy
                    trancheIntAcc[k][ccy] = trancheBalance[k][ccy][j-1][i]
                        * (trancheInterestRate);
                    trancheInterest[k][ccy] += trancheIntAcc[k][ccy];
                    trancheBalance[k][ccy][j][i]
                        = trancheBalance[k][ccy][j-1][i];
                }
                /**************************************************************
                 * Collections
                 */
                Real ccyDFlow = cf[ccy][j].discountedFlow_;
                Real basketInterest = iFlows[ccy][j].flow_; //for cure amount calc

                /**************************************************************
                 * Senior fees
                 */
                Real ccyFeeClaim = basketNotional[ccy][j] * arguments_.seniorFee
                    * feeDayCount.yearFraction(dates[j-1], dates[j]);

                Real ccyFeeFlow = std::min(ccyFeeClaim, iFlows[ccy][j].flow_);

                iFlows[ccy][j].flow_ -= ccyFeeFlow;
                iFlows[ccy][j].discountedFlow_ -= ccyFeeFlow * intCcyDis;

                cf[ccy][j].flow_ -= ccyFeeFlow;
                cf[ccy][j].discountedFlow_ -= ccyFeeFlow * intCcyDis;

                feeValue[i] += ccyFeeFlow * intCcyDis;

                QL_REQUIRE(cf[ccy][j].flow_ >= 0.0, "ccy flows < 0");


                /**************************************************************
                 * tranche waterfall
                 */
                vector<map<Currency, Real> > trancheAmortization;
                vector<map<Currency, Cash> > tranche;
                trancheAmortization.resize(tranches.size());
                tranche.resize(tranches.size());

                //Interest Waterfall incl. ICOC
                for (Size k = 0 ; k < tranches.size() ; k++){

                    tranche[k][ccy].flow_ = 0.;
                    tranche[k][ccy].discountedFlow_ = 0.;

                    Real icRatio = arguments_.tranches[k].icRatio;
                    Real ocRatio = arguments_.tranches[k].ocRatio;

                    //IC and OC Target Balances
                    Real cureAmount = icocCureAmount(i,j,k,
                                           basketNotional[ccy][j],
                                           basketInterest,
                                           trancheBalance,
                                           trancheInterestRates,
                                           icRatio,
                                           ocRatio);

                    interestWaterfall(i, j, dates, iFlows,
                          tranche[k], trancheBalance[k], 
                          trancheInterest[k],
                          trancheIntAcc[k], cureAmount); 

                    icocInterestWaterfall(i, j, k, dates, iFlows,
                                      tranche, trancheBalance, 
                                        cureAmount); 

                }

                //Principal Waterfall
                for (Size k = 0 ; k < tranches.size() ; k++){

                    principalWaterfall(i, j, dates, pFlows,
                                       tranche[k], trancheBalance[k],
                                       trancheInterest[k]);

                    cf[ccy][j].flow_ -= tranche[k][ccy].flow_;
                    cf[ccy][j].discountedFlow_ -= tranche[k][ccy].discountedFlow_;

                }

                /**************************************************************
                 * Subordinated Fee
                 */

                Real ccysubFeeClaim = basketNotional[ccy][j] * arguments_.subordinatedFee
                                    * feeDayCount.yearFraction(dates[j-1], dates[j]);

                Real ccysubFeeFlow = std::min(ccysubFeeClaim, iFlows[ccy][j].flow_);

                iFlows[ccy][j].flow_ -= ccysubFeeFlow;
                iFlows[ccy][j].discountedFlow_ -= ccysubFeeFlow * intCcyDis;

                cf[ccy][j].flow_ -= ccysubFeeFlow;
                cf[ccy][j].discountedFlow_ -= ccysubFeeFlow * intCcyDis;

                subfeeValue[i] += ccysubFeeFlow * intCcyDis;
                QL_REQUIRE(cf[ccy][j].flow_ >= -1.0E-5, "ccy flows < 0");

                /**************************************************************
                 * Kicker:
                 * Split excess flows between equity tranche (1-x) and senior fee (x)
                 */
                Real x = arguments_.equityKicker;

                Cash residual(0.0, 0.0);
                residual.discountedFlow_ = pFlows[ccy][j].discountedFlow_ + iFlows[ccy][j].discountedFlow_;
                residual.flow_ = pFlows[ccy][j].flow_ + iFlows[ccy][j].flow_;

                tranche.back()[ccy].flow_ += residual.flow_ * (1 - x);
                tranche.back()[ccy].discountedFlow_ +=  residual.discountedFlow_ * (1 - x);

                feeValue[i] += residual.discountedFlow_ * x;

                cf[ccy][j].flow_ -= residual.flow_;
                cf[ccy][j].discountedFlow_ -= residual.discountedFlow_;


                /**************************************************************
                 * Consistency checks
                 */
                QL_REQUIRE(cf[ccy][j].flow_ >= -1.e-5, "residual ccy flow < 0: "<<
                           cf[ccy][j].flow_);

                QL_REQUIRE(ccyFeeFlow  >= -1.e-5, "ccy fee flow < 0");

                for(Size k = 0; k < tranches.size() ; k++){
                    QL_REQUIRE(tranche[k][ccy].flow_ >= -1.e-5, "ccy "<<
                           tranches[k].name <<" flow < 0: "
                               <<tranche[k][ccy].flow_);
                }


                basketValue[i] += ccyDFlow;
                for(Size k = 0 ; k < tranches.size() ; k ++){
                    trancheValue[k][i] += tranche[k][ccy].discountedFlow_;
                }
                Real tranchenpvError(0.);
                for(Size k = 0 ; k < tranches.size() ; k ++){
                   tranchenpvError +=trancheValue[k][i];
                }
                Real  npvError(0.);
                if(basketValue[i] > 0.){
                   npvError = (feeValue[i] + subfeeValue[i] + tranchenpvError) / basketValue[i] - 1.0;
                   if(fabs(npvError) > errorTolerance_)
                        //ALOG("NPVs do not add up, rel. error " << npvError);
                        QL_FAIL("NPVs do not add up, rel. error " << npvError);
                }

                QL_REQUIRE(basketValue[i] >= 0.0,
                               "negative basket value " << basketValue[i]);

            } // end dates

            /**************************************************************
             * Loss Distribution
             */
            if (!lossDistributionDates.empty()) {
                // get the losses on this sample for each date
                map<Currency, vector<Cash> >
                    lossDist = arguments_.basket->scenarioLossflow(lossDistributionDatesVector);

                // foreach date, we see what bucket our loss falls into and we increase the probability for
                // that bucket by 1/samples
                for (Size k = 0; k < lossDistributionDatesVector.size(); k++) {
                    Real loss = lossDist[ccy][k].flow_;
                    Date d = lossDistributionDatesVector[k];
                    string dateString = lossDistributionDates[d];

                    QuantLib::ext::shared_ptr<BucketedDistribution> bd = lossDistributionMap[dateString];
                    Size index = bd->bucket(loss); // find the bucket we need to update
                    bd->probabilities()[index] += 1.0 / samples_;
                }
            }

            /*
            //convergence
            if(convergenceFlag){
                std::ostringstream meanStev;
                meanStev << i << fixed << setprecision(2);
                for(Size k = 0 ; k < tranches.size(); k++){
                    runningMean[k] += trancheValue[k][i];
                    double mean = runningMean[k] / (i + 1);
                    meanStev << "," << mean;
                    runningStdev[k] += trancheValue[k][i]*trancheValue[k][i];
                    double stdev = sqrt(runningStdev[k] / (i + 1)  - mean * mean);
                    meanStev << "," << stdev;
                }
                TLOG(meanStev.str());
            }
            */
        } // end samples

        //handle results...
        Stats basketStats(basketValue);
        vector<Stats> trancheStats;

        for(Size i = 0 ; i < tranches.size(); i++){
            Stats trancheStat(trancheValue[i]);
            trancheStats.push_back(trancheStat);
        }
        Stats feeStats(feeValue);
        Stats subfeeStats(subfeeValue);

        results_.basketValue = basketStats.mean();
        for(Size i = 0 ;i < tranches.size(); i ++){
            results_.trancheValue.push_back(trancheStats[i].mean());
        }
        results_.feeValue = feeStats.mean();
        results_.subfeeValue = subfeeStats.mean();

        results_.basketValueStd = basketStats.std();
        for(Size i = 0 ;i < tranches.size(); i ++){
            results_.trancheValueStd.push_back( trancheStats[i].std());
        }
        results_.feeValueStd = feeStats.std();
        results_.subfeeValueStd = subfeeStats.std();

        //distribution output
        Distribution basketDist = basketStats.histogram(bins_);
        results_.additionalResults["BasketDistribution"] = basketDist;
        Distribution feeDist = feeStats.histogram(bins_);
        results_.additionalResults["SeniorFeeDistribution"] = feeDist;
        Distribution subfeeDist = subfeeStats.histogram(bins_);
        results_.additionalResults["SubFeeDistribution"] = subfeeDist;
        std::vector<Distribution> trancheDistResults;
        for(Size i = 0 ;i < tranches.size(); i ++){
            Distribution trancheDist = trancheStats[i].histogram(bins_);
            trancheDistResults.push_back(trancheDist);
        }
        results_.additionalResults["TrancheValueDistribution"] = trancheDistResults;

        if (!lossDistributionDates.empty())
            results_.additionalResults["LossDistribution"] = lossDistributionMap;

        //define which tranche is in the npv-file and scale by investment amount
        for (Size i = 0; i < tranches.size(); i++) {
            if(tranches[i].name == arguments_.investedTrancheName){
                results_.value = trancheStats[i].mean();
                results_.errorEstimate = trancheStats[i].std();
            }
        }
        QL_REQUIRE(results_.value != Null<Real>(), "CBO Investment " << arguments_.investedTrancheName << " could not be assigned : no NPV");

        /*
        //results of all components in logFile
        LOG ("-----------------------------------");
        LOG ("CBO valuation results:");
        double liab = 0.0;
        for(Size i = 0 ; i < tranches.size(); i++){
            LOG( tranches[i].name << " - value " << fixed << setprecision(2) << trancheStats[i].mean() << " - stdev: " << trancheStats[i].std() << " - face amount " << tranches[i].faceAmount );
            liab += trancheStats[i].mean();
        }
        LOG ( "senior fee - value  " << fixed << setprecision(2) << feeStats.mean() << " - stdev " << feeStats.std());
        LOG ( "sub fee    - value  " << fixed << setprecision(2) << subfeeStats.mean() << " - stdev " << subfeeStats.std());
        LOG ( "portfolio  - value  " << fixed << setprecision(2) << basketStats.mean() << " - stdev " << basketStats.std());
        liab += feeStats.mean() + subfeeStats.mean();
        LOG ( "total: assets " << fixed << setprecision(2) << basketStats.mean() << " vs. liabilities " << liab );
        LOG ( "check total :" << fixed << setprecision(10) << basketStats.mean() - liab);
        LOG ("-----------------------------------");
        */

    }// calculate

}
