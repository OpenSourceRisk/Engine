/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/commodityblackvoladaptertermstructure.hpp
    \brief Wrapper Class for wrapping regular BlackVolTermStructures as CommodityFutureVolTermStructures
    \ingroup termstructures
*/

#pragma once

#include <qle/termstructures/commodityblackvoltermstructure.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace QuantExt {
class CommodityFutureBlackVolatilityAdapterTermStructure : public QuantLib::BlackVolTermStructure {
public:
    CommodityFutureBlackVolatilityAdapterTermStructure(const boost::shared_ptr<QuantLib::BlackVolTermStructure>& volTs)
        : QuantLib::BlackVolTermStructure(volTs->settlementDays(), volTs->calendar(), volTs->businessDayConvention(),
                                          volTs->dayCounter()),
          volTs_(volTs){};
    // TODO override all the base method and delegate to wrapped ts
protected:    
    Real blackVarianceImpl(Time t, Real strike) const override;
    Real blackVolImpl(Time t, Real strike) const override;
private:
    ext::shared_ptr<QuantLib::BlackVolTermStructure> volTs_;
};

inline Real CommodityFutureBlackVolatilityAdapterTermStructure::blackVolImpl(Time contractExpiry, Real strike) const {
    return volTs_->blackVol(contractExpiry, strike);
}

inline Real CommodityFutureBlackVolatilityAdapterTermStructure::blackVarianceImpl(Time t, Real strike) const {
    QL_FAIL("Compute Variance in pricing engine and not commodity future black Vol Surface");
}
    
    


}


