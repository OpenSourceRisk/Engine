/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file qle/pricingengines/commodityspreadoptionengine.hpp
    \brief commodity bachelier spread option engine
    \ingroup engines
*/

#pragma once

#include <qle/instruments/commodityspreadoption.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/assetmodelwrapper.hpp>

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/termstructures/correlationtermstructure.hpp>
#include <ql/pricingengines/diffusioncalculator.hpp>

namespace QuantExt {

/*! Commodity Bachelier Spread Option Engine
    Uses quoted normal spread volatilities and the Bachelier formula to price the spread option.
 */
class CommodityBachelierSpreadOptionAnalyticalEngine : public CommoditySpreadOption::engine {
public:
    CommodityBachelierSpreadOptionAnalyticalEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                                   const QuantLib::Handle<QuantLib::BlackVolTermStructure>& normalSpreadVolTS);
    void calculate() const override;    


protected:
    struct PricingParameter {
        Real atm;
        Real accruals;
        std::vector<QuantLib::Date> pricingDates;
        std::vector<std::string> indexNames;
        std::vector<Real> fixings;
        std::vector<QuantLib::Date> expiries;
    };

    PricingParameter derivePricingParameterFromFlow(const ext::shared_ptr<CommodityCashFlow>& flow,
                                                    const Date& expiryDate,
                                                    const ext::shared_ptr<FxIndex>& fxIndex) const;

    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    QuantLib::Handle<QuantLib::BlackVolTermStructure> normalSpreadVolTS_;
};

} // namespace QuantExt
