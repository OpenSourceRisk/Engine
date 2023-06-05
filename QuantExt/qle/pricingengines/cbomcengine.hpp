/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
 All rights reserved.
 */

/*! \file qle/pricingengines/cashflowcboengines.hpp
 \brief Monte Carlo pricing engine for the cashflow CDO instrument
 */

#pragma once

#include <qle/pricingengines/cboengine.hpp>

#include <ql/experimental/credit/distribution.hpp>
#include <ql/experimental/credit/randomdefaultmodel.hpp>
#include <ql/math/distributions/bivariatenormaldistribution.hpp>
#include <ql/math/distributions/normaldistribution.hpp>

namespace QuantExt {
using namespace QuantLib;
using namespace std;

//--------------------------------------------------------------------------
//! CBO engine, Monte Carlo for the sample payoff
/*!
     This class implements the waterfall structures and Monte Carlo pricing
     of the cash flow CBO.

     For more information refer to the detailed QuantExt documentation.

     \ingroup engines

     \todo Return distributions as additional engine results
 */
class MonteCarloCBOEngine : public CBO::engine {
public:
    MonteCarloCBOEngine(
        /*!
        Random default model for generating samples of default
        times for the portfolio of names
        */
        boost::shared_ptr<RandomDefaultModel> rdm,
        //! Number of Monte Carlo samples
        Size samples = 1000,
        //! Discretization for resulting distributions
        Size bins = 20,
        //! npvError tolerance
        double errorTolerance = 1.0e-6,
        //! Periods from valuation date for which to return loss distributions
        std::vector<QuantLib::Period> lossDistributionPeriods = std::vector<QuantLib::Period>())
        : rdm_(rdm), samples_(samples), bins_(bins), errorTolerance_(errorTolerance),
        lossDistributionPeriods_(lossDistributionPeriods) {}
    void calculate() const override;

private:
    //! interest waterfall
    void interestWaterfall(Size sampleIndex, Size dateIndex, const vector<Date>& dates,
                           map<Currency, vector<Cash>>& basketFlow, map<Currency, Cash>& trancheFlow,
                           map<Currency, vector<vector<Real>>>& balance, map<Currency, Real>& interest,
                           map<Currency, Real>& interestAcc, Real cureAmount) const;
    //! icoc interest waterfall
    void icocInterestWaterfall(Size i, // sample index
                               Size j, // date index
                               Size k, // tranche index
                               const vector<Date>& dates, map<Currency, vector<Cash>>& iFlows,
                               vector<map<Currency, Cash>>& tranches,
                               vector<map<Currency, vector<vector<Real>>>>& balances, Real cureAmount) const;

    //! pricipal waterfall
    void principalWaterfall(Size sampleIndex, Size dateIndex, const vector<Date>& dates,
                            map<Currency, vector<Cash>>& basketFlow, map<Currency, Cash>& trancheFlow,
                            map<Currency, vector<vector<Real>>>& balance, map<Currency, Real>& interest) const;
    //! icoc cure amount
    Real icocCureAmount(Size sampleIndex, Size dateIndex, Size trancheNo, Real basketNotional, Real basketInterest,
                        vector<map<Currency, vector<vector<Real>>>>& trancheBalances, vector<Real> trancheInterestRates,
                        Real icRatios, Real ocRatios) const;

    //! Return dates on the CBO schedule that are closest to the requested \p lossDistributionPeriods
    std::map<QuantLib::Date, std::string> getLossDistributionDates(const QuantLib::Date& valuationDate) const;

    boost::shared_ptr<RandomDefaultModel> rdm_;
    Size samples_;
    Size bins_;
    double errorTolerance_;

    //! Periods from valuation date for which to return loss distributions
    std::vector<QuantLib::Period> lossDistributionPeriods_;
};

} // namespace QuantExt
