/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/pricingengines/commodityspreadoptionengine.hpp
    \brief commodity spread option engine
    \ingroup engines
*/

#pragma once

#include <qle/instruments/commodityspreadoption.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/blackscholesmodelwrapper.hpp>

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

/*! Commodity spread option base class
 */
class CommoditySpreadOptionBaseEngine : public CommoditySpreadOption::engine {
public:
    // if you want speed-optimized observability, use the other constructor
    CommoditySpreadOptionBaseEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volTSLongAsset,
                                    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volTSShortAsset, 
                                    Real rho = 1.0,
                                    Real beta = 0.0);

protected:
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> volTSLongAsset_;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> volTSShortAsset_; 
    QuantLib::Real rho_;
    QuantLib::Real beta_;
};

/*! Commodity Spread Option Analytical Engine
    Analytical pricing based on the Kirk approximation.
    Reference: Iain Clark, Commodity Option Pricing, Wiley, section 2.9
    See also the documentation in the ORE+ product catalogue.
*/
class CommoditySpreadOptionAnalyticalEngine : public CommoditySpreadOptionBaseEngine {
public:
    using CommoditySpreadOptionBaseEngine::CommoditySpreadOptionBaseEngine;
    void calculate() const override;

private:
    std::tuple<double, double, double> timeAndAtmAndSigmaFromCashFlow(const ext::shared_ptr<CommodityCashFlow>& flow,
                                                                      const ext::shared_ptr<BlackVolTermStructure>& vol,
                                                                      const ext::shared_ptr<FxIndex>& fxIndex) const;
    
    double kirkFormula(double F1, double sigma1, double t1, double w1, double F2, double sigma2, double t2, double w2,
                       double K, double tte, double ttp) const;

    double rho(const QuantLib::Date& e1, const QuantLib::Date& e2,
               const ext::shared_ptr<BlackVolTermStructure>& vol) const;
};

} // namespace QuantExt
