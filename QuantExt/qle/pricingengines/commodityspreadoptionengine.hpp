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
#include <qle/termstructures/correlationtermstructure.hpp>

namespace QuantExt {

/*! Commodity Spread Option Engine
    Uses the Kirk Approximation described in Iain J. Clark - Commodity Option Pricing - Section 2.9
    Rho is the correlation between two commodities and for asian future spreads the
    intra-asset correlation between two future contracts are parametrized as \f$\rho(s, t) = \exp(-\beta * \abs(s -
   t))\f$ where \f$s\f$ and \f$t\f$ are times to futures expiry.
 */
class CommoditySpreadOptionAnalyticalEngine : public CommoditySpreadOption::engine {
public:
    CommoditySpreadOptionAnalyticalEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                          const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volTSLongAsset,
                                          const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volTSShortAsset,
                                          const QuantLib::Handle<QuantExt::CorrelationTermStructure>& rho,
                                          Real beta = 0.0);
    void calculate() const override;

private:
    struct PricingParameter {
        Time tn;
        Real atm;
        Real sigma;
        Real accruals;
    };

    PricingParameter derivePricingParameterFromFlow(const ext::shared_ptr<CommodityCashFlow>& flow,
                                                    const ext::shared_ptr<BlackVolTermStructure>& vol,
                                                    const ext::shared_ptr<FxIndex>& fxIndex) const;

    //! Return the correlation between two future expiry dates \p ed_1 and \p ed_2
    double intraAssetCorrelation(const QuantLib::Date& e1, const QuantLib::Date& e2,
                                  const ext::shared_ptr<BlackVolTermStructure>& vol) const;

    double rho() const;

protected:
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> volTSLongAsset_;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> volTSShortAsset_;
    const QuantLib::Handle<QuantExt::CorrelationTermStructure> rho_;
    QuantLib::Real beta_;
};

} // namespace QuantExt
