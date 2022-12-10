/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/pricingengines/commodityapoengine.hpp
    \brief commodity average price option engine
    \ingroup engines
*/

#ifndef quantext_commodity_apo_engine_hpp
#define quantext_commodity_apo_engine_hpp

#include <qle/instruments/commodityapo.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/blackscholesmodelwrapper.hpp>

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

namespace CommodityAveragePriceOptionMomementMatching {

// Return the the atm forward - accruals and the the volatility of
struct MomentMatchingResults {
    Time tn;
    Real forward;
    Real accruals;
    Real sigma;
    std::vector<QuantLib::Real> times;
    std::vector<Real> forwards;
    std::vector<Real> futureVols;
    std::vector<Real> spotVols;
    Real EA2;

    Real firstMoment();
    Real secondMoment();
    Real stdDev();
    Time timeToExpriy();
};

// Matches the first two moments of a lognormal distribution
// For options with accruals the strike of the options need to be adjusted by the accruals
// See Iain Clark - Commodity Option Pricing A Practitioner’s Guide - Section 2.74
MomentMatchingResults matchFirstTwoMomentsTurnbullWakeman(
    const ext::shared_ptr<CommodityIndexedAverageCashFlow>& flow,
    const ext::shared_ptr<QuantLib::BlackVolTermStructure>& vol,
    const std::function<double(const QuantLib::Date& expiry1, const QuantLib::Date& expiry2)>& rho,
    QuantLib::Real strike = QuantLib::Null<QuantLib::Real>());
}


/*! Commodity APO Engine base class
    Correlation is parametrized as \f$\rho(s, t) = \exp(-\beta * \abs(s - t))\f$
    where \f$s\f$ and \f$t\f$ are times to futures expiry.
 */
class CommodityAveragePriceOptionBaseEngine : public CommodityAveragePriceOption::engine {
public:
    CommodityAveragePriceOptionBaseEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                          const QuantLib::Handle<QuantExt::BlackScholesModelWrapper>& model,
                                          QuantLib::Real beta = 0.0);

    // if you want speed-optimized observability, use the other constructor
    CommodityAveragePriceOptionBaseEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                          const QuantLib::Handle<QuantLib::BlackVolTermStructure>& vol,
                                          QuantLib::Real beta = 0.0);

protected:
    //! Return the correlation between two future expiry dates \p ed_1 and \p ed_2
    QuantLib::Real rho(const QuantLib::Date& ed_1, const QuantLib::Date& ed_2) const;

    /*! In certain cases, the APO value is not model dependent. This method returns \c true if the APO value is model
        dependent. If the APO value is not model dependent, this method returns \c false and populates the results
        with the model independent value.
    */
    bool isModelDependent() const;

    /*! Check barriers on given (log-)price */
    bool barrierTriggered(const Real price, const bool logPrice) const;

    /*! Check whether option is alive depending on whether barrier was triggered */
    bool alive(const bool barrierTriggered) const;

    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> volStructure_;
    QuantLib::Real beta_;
    // used in checkBarrier() for efficiency, must be set by methods calling checkBarrier(p, true)
    mutable QuantLib::Real logBarrier_;
};

/*! Commodity APO Analytical Engine
    Analytical pricing based on the two-moment Turnbull-Wakeman approximation.
    Reference: Iain Clark, Commodity Option Pricing, Wiley, section 2.7.4
    See also the documentation in the ORE+ product catalogue.
*/
class CommodityAveragePriceOptionAnalyticalEngine : public CommodityAveragePriceOptionBaseEngine {
public:
    using CommodityAveragePriceOptionBaseEngine::CommodityAveragePriceOptionBaseEngine;
    void calculate() const override;
};

/*! Commodity APO Monte Carlo Engine
    Monte Carlo implementation of the APO payoff
    Reference: Iain Clark, Commodity Option Pricing, Wiley, section 2.7.4, equations (2.118) and (2.126)
*/
class CommodityAveragePriceOptionMonteCarloEngine : public CommodityAveragePriceOptionBaseEngine {
public:
    CommodityAveragePriceOptionMonteCarloEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                                const QuantLib::Handle<QuantExt::BlackScholesModelWrapper>& model,
                                                QuantLib::Size samples, QuantLib::Real beta = 0.0,
                                                const QuantLib::Size seed = 42)
        : CommodityAveragePriceOptionBaseEngine(discountCurve, model, beta), samples_(samples), seed_(seed) {}

    // if you want speed-optimized observability, use the other constructor
    CommodityAveragePriceOptionMonteCarloEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                                const QuantLib::Handle<QuantLib::BlackVolTermStructure>& vol,
                                                QuantLib::Size samples, QuantLib::Real beta = 0.0,
                                                const QuantLib::Size seed = 42)
        : CommodityAveragePriceOptionBaseEngine(discountCurve, vol, beta), samples_(samples), seed_(seed) {}

    void calculate() const override;

private:
    //! Calculations when underlying swap references a commodity spot price
    void calculateSpot() const;

    //! Calculations when underlying swap references a commodity spot price
    void calculateFuture() const;

    /*! Prepare data for APO calculation. The \p outVolatilities parameter will be populated with separate future
        contract volatilities taking into account the \p strike level. The number of elements of \p outVolatilities
        gives the number, N, of future contracts involved in the non-accrued portion of the APO. The matrix
        \p outSqrtCorr is populated with the square root of the correlation matrix between the future contracts.
        The \p outPrices vector will be populated with the current future price values. The \p futureIndex is
        populated with the index of the future to be used on each timestep in the simulation.
    */
    void setupFuture(std::vector<QuantLib::Real>& outVolatilities, QuantLib::Matrix& outSqrtCorr,
                     std::vector<QuantLib::Real>& outPrices, std::vector<QuantLib::Size>& futureIndex,
                     QuantLib::Real strike) const;

    /*! Return the \f$n\f$ timesteps from today, \f$t_0\f$, up to \f$t_n\f$ where \f$n > 0\f$. Note that each
        \f$t_i\f$ corresponds to a pricing date \f$d_i\f$ that is after today. The method returns the vector of time
        deltas \f$t_i - t_{i-1}\f$ for \f$i=1,\ldots,n\f$ and populates the vector \p outDates with the dates
        \f$d_0, d_1,...,d_n\f$. Note that the size of \p outDates is one larger than the size of the return vector.
    */
    std::vector<QuantLib::Real> timegrid(std::vector<QuantLib::Date>& outDates) const;

    QuantLib::Size samples_;
    QuantLib::Size seed_;
};

} // namespace QuantExt

#endif
