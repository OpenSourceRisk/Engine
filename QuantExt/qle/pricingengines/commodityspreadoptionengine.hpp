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

class CommoditySpreadOptionAnalyticalEngine : public CommoditySpreadOption::engine {
public:
    CommoditySpreadOptionAnalyticalEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volTSLongAsset,
                                    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volTSShortAsset,
                                    Real rho = 1.0, Real beta = 0.0);
    void calculate() const override;

private:

    struct PricingParameter {
        Time tn;
        Real atm;
        Real sigma;
        Real accruals;
    };

    PricingParameter
     derivePricingParameterFromFlow(const ext::shared_ptr<CommodityCashFlow>& flow,
                                    const ext::shared_ptr<BlackVolTermStructure>& vol,
                                    const ext::shared_ptr<FxIndex>& fxIndex) const;

    double rho(const QuantLib::Date& e1, const QuantLib::Date& e2,
              const ext::shared_ptr<BlackVolTermStructure>& vol) const;

protected:
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> volTSLongAsset_;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> volTSShortAsset_;
    QuantLib::Real rho_;
    QuantLib::Real beta_;
};

} // namespace QuantExt
