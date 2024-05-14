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

#include <qle/instruments/bondbasket.hpp>
#include <qle/cashflows/scaledcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

    const double BondBasket::getScalar(const std::string&  name, const QuantLib::Date& currentDate) const{
        double result = -1.0;
        for(auto& bond : qlBonds_){
            if(bond.first == name){
                const QuantLib::Leg& leg = bond.second->cashflows();
                vector<Real> scalar = reinvestmentScalar(name);
                for(size_t d = 1; d < leg.size(); d++){
                    if(leg[d-1]->date() < currentDate && leg[d]->date() >= currentDate)
                        result = scalar[d];
                }
            }
        }
        return result;
    }

    void BondBasket::fillFlowMaps(){

        cashflows_.clear();
        interestFlows_.clear();
        notionalFlows_.clear();
        feeFlows_.clear();

        for(auto bond : qlBonds_){

            std::string name = bond.first;
            double multi = multiplier(name);
            vector<double> scalar = reinvestmentScalar(name);
            Leg leg = bond.second->cashflows();

            std::vector<ext::shared_ptr<QuantLib::CashFlow> > cashflows;
            std::vector<ext::shared_ptr<QuantLib::CashFlow> > interestFlows;
            std::vector<ext::shared_ptr<QuantLib::CashFlow> > notionalFlows;
            std::vector<ext::shared_ptr<QuantLib::CashFlow> > feeFlows;

            for (size_t j = 0; j < leg.size(); j++) {

                QuantLib::ext::shared_ptr<QuantLib::CashFlow> ptrFlow = leg[j];

                if(flowType(name,j) == "int"){

                    QuantLib::ext::shared_ptr<QuantLib::Coupon> ptrCoupon =
                            QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(ptrFlow);
                    QL_REQUIRE(ptrCoupon, "expected couopn type");

                    QuantLib::ext::shared_ptr<QuantExt::ScaledCoupon> newPtr( new QuantExt::ScaledCoupon(multi * scalar[j], ptrCoupon ));
                    interestFlows.push_back(newPtr);
                    cashflows.push_back(newPtr);

                } else if(flowType(name,j) == "xnl"){

                    double amort = 1.0;
                    if(reinvestmentEndDate_ > ptrFlow->date())
                        amort = 0.0;

                    ext::shared_ptr<CashFlow>
                        flow(new SimpleCashFlow(ptrFlow->amount() * multi * scalar[j] * amort, ptrFlow->date()));
                    notionalFlows.push_back(flow);
                    cashflows.push_back(flow);

                }  else if (flowType(name,j) == "fee"){

                    ext::shared_ptr<CashFlow>
                        flow(new SimpleCashFlow(ptrFlow->amount() * multi * scalar[j], ptrFlow->date()));
                    feeFlows.push_back(flow);
                    cashflows.push_back(flow);

                } else {
                    //ALOG("bond " << name << " leg " << j << " paymentDate " << leg[j]->date() << " could not be assigned");
                }

            }//leg - j
            cashflows_[name] = cashflows;
            notionalFlows_[name] = notionalFlows;
            feeFlows_[name] = feeFlows;
            interestFlows_[name] = interestFlows;
        }//bonds - b
    }//void BondBasket::fillFlowMaps()


    Real sum(const Cash& c, const Cash& d) {
        return c.flow_ + d.flow_;
    }

    Real sumDiscounted(const Cash& c, const Cash& d) {
        return c.discountedFlow_ + d.discountedFlow_;
    }

    Real BondBasket::convert(Real amount, Currency ccy, Date date) {

        if(date == Date())
            date = Settings::instance().evaluationDate();

        if (ccy == baseCcy_)
            return amount;
        else {
            std::string code = ccy.code();
            double fxRate = fxIndex(code)->fixing(date, false);
            double inBase = fxRate * amount;
            return inBase;
        }
    }

    BondBasket::BondBasket(const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::Bond>>& qlBonds,
                            const std::map<std::string, double>& recoveries,
                            const std::map<std::string, double>& multipliers,
                            const std::map<std::string, QuantLib::Handle<QuantLib::YieldTermStructure>>& yieldTermStructures,
                            const std::map<std::string, Currency>& currencies,
                            const QuantLib::ext::shared_ptr<Pool> pool,
                            Currency baseCcy,
                            const std::map <string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndexMap,
                            const QuantLib::Date & reinvestmentEndDate,
                            const std::map<std::string, std::vector<double>>& reinvestmentScalar,
                            const std::map<std::string, std::vector<std::string>>& flowType)
        :   qlBonds_(qlBonds),
            recoveries_(recoveries),
            multipliers_(multipliers),
            yieldTermStructures_(yieldTermStructures),
            currencies_(currencies),
            pool_(pool),
            baseCcy_(baseCcy),
            fxIndexMap_(fxIndexMap),
            reinvestmentEndDate_(reinvestmentEndDate),
            reinvestmentScalar_(reinvestmentScalar),
            flowType_(flowType)
            {

                QL_REQUIRE(!qlBonds_.empty(), "no bonds given");
                QL_REQUIRE(qlBonds_.size() == recoveries_.size(), "mismatch number bonds vs recoveries");
                QL_REQUIRE(qlBonds_.size() == multipliers_.size(), "mismatch number bonds vs multipliers");
                QL_REQUIRE(qlBonds_.size() == yieldTermStructures_.size(), "mismatch number bonds vs yieldTermStructures");
                QL_REQUIRE(qlBonds_.size() == currencies_.size(), "mismatch number bonds vs currencies");

                for (const auto& ccy : currencies_)
                    unique_currencies_.insert(ccy.second);

                grid_ = vector<Date>();

    }

    const QuantLib::ext::shared_ptr<Pool>& BondBasket::pool() const {
        return pool_;
    }

    const double BondBasket::recoveryRate (const std::string& name) const {
        auto it = recoveries_.find(name);
        if(it != recoveries_.end())
            return it->second;
        QL_FAIL("no recovery value for bond " << name);
    };

    const double BondBasket::multiplier (const std::string& name) const {
        auto it = multipliers_.find(name);
        if(it != multipliers_.end())
            return it->second;
        QL_FAIL("no multiplier for bond " << name);
    };

    const QuantLib::Handle<QuantLib::YieldTermStructure> BondBasket::yts(const std::string& name) const{
        auto it = yieldTermStructures_.find(name);
        if(it != yieldTermStructures_.end())
            return it->second;
        QL_FAIL("no yieldTermStructure for bond " << name);
    }

    const Currency BondBasket::currency(const std::string& name) const{
        auto it = currencies_.find(name);
        if(it != currencies_.end())
            return it->second;
        QL_FAIL("no currency for bond " << name);
    }

    const vector<double> BondBasket::reinvestmentScalar(const std::string& name) const{
        auto it = reinvestmentScalar_.find(name);
        if(it != reinvestmentScalar_.end())
            return it->second;
        QL_FAIL("no reinvestmentScalar for bond " << name);
    }

    const string BondBasket::flowType(const std::string& name, int idx) const{
        auto it = flowType_.find(name);
        if(it != flowType_.end())
            return it->second[idx];
        QL_FAIL("no flowType for bond " << name);
    }

    const QuantLib::ext::shared_ptr<QuantExt::FxIndex> BondBasket::fxIndex(const std::string& name) const{
        auto it = fxIndexMap_.find(name);
        if(it != fxIndexMap_.end())
            return it->second;
        QL_FAIL("no fxIndex for bond " << name);
    }

    void BondBasket::setGrid(vector<Date> dates) {
        grid_ = dates;
        for (auto bond : qlBonds_) {
            string name = bond.first;
            cashflow2grid_[name].resize(cashflows_[name].size(), -1);
            for (Size j = 0; j < cashflows_[name].size(); j++) {
                Date d = cashflows_[name][j]->date();
                for (Size k = 1; k < dates.size(); k++) {
                    if (d > dates[k-1] && d <= dates[k]) {
                        cashflow2grid_[name][j] = k;
                        continue;
                    }
                }
            }
            interestflow2grid_[name].resize(interestFlows_[name].size(),-1);
            for (Size j = 0; j < interestFlows_[name].size(); j++) {
                Date d = interestFlows_[name][j]->date();
                for (Size k = 1; k < dates.size(); k++) {
                    if (d > dates[k-1] && d <= dates[k]) {
                        interestflow2grid_[name][j] = k;
                        continue;
                    }
                }
            }
            notionalflow2grid_[name].resize(notionalFlows_[name].size(), -1);
            for (Size j = 0; j < notionalFlows_[name].size(); j++) {
                Date d = notionalFlows_[name][j]->date();
                for (Size k = 1; k < dates.size(); k++) {
                    if (d > dates[k-1] && d <= dates[k]) {
                        notionalflow2grid_[name][j] = k;
                        continue;
                    }
                }
            }
            feeflow2grid_[name].resize(feeFlows_[name].size(), -1);
            for (Size j = 0; j < feeFlows_[name].size(); j++) {
                Date d = feeFlows_[name][j]->date();
                for (Size k = 1; k < dates.size(); k++) {
                    if (d > dates[k-1] && d <= dates[k]) {
                        feeflow2grid_[name][j] = k;
                        continue;
                    }
                }
            }
        }
    }

    std::map<Currency, vector<Cash> >
    BondBasket::scenarioCashflow(vector<Date> dates) {
        QL_REQUIRE(grid_.size() > 0, "grid not set");
        Date today = Settings::instance().evaluationDate();
        map<Currency,vector<Cash> > cf;
        for (const auto & ccy : unique_currencies_)
            cf[ccy].resize(dates.size(), Cash());
        for (const auto & bond : qlBonds_) {
            string name = bond.first;
            DayCounter dc = yts(name)->dayCounter();
            Currency ccy = currency(name);
            Real defaultTime = pool_->getTime(name);
            Real maturityTime = dc.yearFraction(today, bond.second->maturityDate());
            for (Size j = 0; j < cashflows_[name].size(); j++) {
                Date d = cashflows_[name][j]->date();
                Real t = dc.yearFraction(today, d);

                if (t < defaultTime) {
                    Size k = cashflow2grid_[name][j];
                    //QL_REQUIRE(k >= 0 && k < grid_.size(), "k out of range");
                    if (k < grid_.size()) {
                        cf[ccy][k].flow_ += cashflows_[name][j]->amount();
                        cf[ccy][k].discountedFlow_
                            += cashflows_[name][j]->amount()
                            * yts(name)->discount(t);
                    }
                }
                else break;
            }
            if (defaultTime < maturityTime) {
                for (Size k = 1; k < dates.size(); k++) {
                    Real t1 = dc.yearFraction(today, dates[k-1]);
                    Real t2 = dc.yearFraction(today, dates[k]);
                    if (defaultTime >= t1 && defaultTime < t2) {
                        cf[ccy][k].flow_
                            += recoveryRate(name) * bond.second->notional(dates[k-1]) * multiplier(name);
                        cf[ccy][k].discountedFlow_
                            += recoveryRate(name) * bond.second->notional(dates[k-1]) * multiplier(name)
                            * yts(name)->discount(defaultTime);
                    }
                }
            }
        }
        return cf;
    }

    std::map<Currency, vector<Cash> >
    BondBasket::scenarioInterestflow(vector<Date> dates) {
        QL_REQUIRE(grid_.size() > 0, "grid not set");
        Date today = Settings::instance().evaluationDate();
        map<Currency,vector<Cash> > cf;
        for (const auto & ccy : unique_currencies_)
            cf[ccy].resize(dates.size(), Cash());
        for (const auto & bond : qlBonds_) {
            string name = bond.first;
            DayCounter dc = yts(name)->dayCounter();
            Currency ccy = currency(name);
            Real defaultTime = pool_->getTime(name);
            for (Size j = 0; j < interestFlows_[name].size(); j++) {
                Date d = interestFlows_[name][j]->date();
                Real t = dc.yearFraction(today, d);
                if (t < defaultTime) {
                    Size k = interestflow2grid_[name][j];
                    //QL_REQUIRE(k >= 0 && k < grid_.size(), "k out of range");
                    if (k < grid_.size()) {
                        cf[ccy][k].flow_ += interestFlows_[name][j]->amount();
                        cf[ccy][k].discountedFlow_
                            += interestFlows_[name][j]->amount()
                            * yts(name)->discount(t);
                    }
                }
                else break;
            }
        }
        return cf;
    }

    std::map<Currency, vector<Cash> >
    BondBasket::scenarioPrincipalflow(vector<Date> dates) {
        QL_REQUIRE(grid_.size() > 0, "grid not set");
        Date today = Settings::instance().evaluationDate();
        map<Currency,vector<Cash> > cf;
        for (const auto & ccy : unique_currencies_)
            cf[ccy].resize(dates.size(), Cash());
        for (const auto & bond : qlBonds_) {
            string name = bond.first;
            DayCounter dc = yts(name)->dayCounter();
            Currency ccy = currency(name);
            Real defaultTime = pool_->getTime(name);
            Real maturityTime = dc.yearFraction(today, bond.second->maturityDate());
            for (Size j = 0; j < notionalFlows_[name].size(); j++) {
                Date d = notionalFlows_[name][j]->date();
                Real t = dc.yearFraction(today, d);
                if (t < defaultTime) {
                    Size k = notionalflow2grid_[name][j];
                    //QL_REQUIRE(k >= 0 && k < grid_.size(), "k out of range");
                    if (k < grid_.size()) {
                        cf[ccy][k].flow_ += notionalFlows_[name][j]->amount();
                        cf[ccy][k].discountedFlow_
                            += notionalFlows_[name][j]->amount()
                            * yts(name)->discount(t);
                    }
                }
                else break;
            }
            if (defaultTime < maturityTime) {
                for (Size k = 1; k < dates.size(); k++) {
                    Real t1 = dc.yearFraction(today, dates[k-1]);
                    Real t2 = dc.yearFraction(today, dates[k]);
                    if (defaultTime >= t1 &&
                        defaultTime < t2) {
                        cf[ccy][k].flow_
                            += recoveryRate(name) * bond.second->notional(dates[k-1]) * multiplier(name);
                        cf[ccy][k].discountedFlow_
                            += recoveryRate(name) * bond.second->notional(dates[k-1]) * multiplier(name)
                            * yts(name)->discount(defaultTime);
                    }
                }
            }
        }
        return cf;
    }

    std::map<Currency, vector<Cash>> BondBasket::scenarioFeeflow(const std::vector<QuantLib::Date>& dates) {
        QL_REQUIRE(grid_.size() > 0, "grid not set");
        Date today = Settings::instance().evaluationDate();
        map<Currency,vector<Cash> > cf;
        for (const auto & ccy : unique_currencies_)
            cf[ccy].resize(dates.size(), Cash());
        for (const auto & bond : qlBonds_) {
            string name = bond.first;
            DayCounter dc = yts(name)->dayCounter();
            Currency ccy = currency(name);
            Real defaultTime = pool_->getTime(name);
            for (Size j = 0; j < feeFlows_[name].size(); j++) {
                Date d = feeFlows_[name][j]->date();
                Real t = dc.yearFraction(today, d);
                if (t < defaultTime) {
                    Size k = feeflow2grid_[name][j];
                    if (k < dates.size()) {
                        cf[ccy][k].flow_ += feeFlows_[name][j]->amount();
                        cf[ccy][k].discountedFlow_
                        += feeFlows_[name][j]->amount() * yts(name)->discount(t);

                    }
                }
                else break;
            }
        }
        return cf;
    }

    std::map<Currency, vector<Cash> >
    BondBasket::scenarioLossflow(vector<Date> dates) {
        QL_REQUIRE(grid_.size() > 0, "grid not set");
        Date today = Settings::instance().evaluationDate();
        map<Currency,vector<Cash> > cf;
        for (const auto & ccy : unique_currencies_)
            cf[ccy].resize(dates.size(), Cash());
        for (const auto & bond : qlBonds_) {
            string name = bond.first;
            DayCounter dc = yts(name)->dayCounter();
            Currency ccy = currency(name);
            Real defaultTime = pool_->getTime(name);
            Real maturityTime = dc.yearFraction(today, bond.second->maturityDate());
            if (defaultTime < maturityTime) {
                for (Size k = 1; k < dates.size(); k++) {
                    Real t1 = dc.yearFraction(today, dates[k-1]);
                    Real t2 = dc.yearFraction(today, dates[k]);
                    if (defaultTime >= t1 &&
                        defaultTime < t2) {
                        cf[ccy][k].flow_
                        += (1.0 - recoveryRate(name)) * bond.second->notional(dates[k-1]) * multiplier(name);
                        cf[ccy][k].discountedFlow_
                        += (1.0 - recoveryRate(name)) * bond.second->notional(dates[k-1]) * multiplier(name)
                        * yts(name)->discount(defaultTime);
                    }
                }
            }
        }
        return cf;
    }

    std::map<Currency, vector<Real> >
    BondBasket::scenarioRemainingNotional(vector<Date> dates) {
        QL_REQUIRE(grid_.size() > 0, "grid not set");
        Date today = Settings::instance().evaluationDate();
        map<Currency,vector<Real> > cf;
        for (const auto & ccy : unique_currencies_)
            cf[ccy].resize(dates.size(), 0.0);
        for (const auto & bond : qlBonds_) {
            string name = bond.first;
            DayCounter dc = yts(name)->dayCounter();
            Currency ccy = currency(name);
            Real defaultTime = pool_->getTime(name);
            for (Size k = 1; k < dates.size(); k++) {
                Real t1 = dc.yearFraction(today, dates[k]);
                if (defaultTime >= t1) {
                    cf[ccy][k] +=  bond.second->notional(dates[k]) * multiplier(name) * getScalar(name, dates[k]);
                }
            }
        }
        return cf;
    }

}

