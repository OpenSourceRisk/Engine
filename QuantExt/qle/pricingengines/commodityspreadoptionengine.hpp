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
                                    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volLong,
                                    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volShort, 
                                    Real rho = 1.0);

protected:
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> volLong_;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> volShort_; 
    QuantLib::Real rho_;
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
};

} // namespace QuantExt
