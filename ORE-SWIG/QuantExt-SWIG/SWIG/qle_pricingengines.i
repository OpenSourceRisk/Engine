/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef qle_pricingengines_i
#define qle_pricingengines_i

%include common.i
%include date.i
%include termstructures.i
%include swaption.i
%include qle_termstructures.i

%shared_ptr(QuantExt::AnalyticLgmSwaptionEngine)
namespace QuantExt {
class AnalyticLgmSwaptionEngine : public PricingEngine {
  public:
    void enableCache(const bool lgm_H_constant = true, const bool lgm_alpha_constant = false);
    void clearCache();
    void setZetaShift(const Time t1, const Real shift);
    void resetZetaShift();
};
}

%shared_ptr(QuantExt::McMultiLegOptionEngine)
namespace QuantExt {
class McMultiLegOptionEngine : public PricingEngine {
};
}

%shared_ptr(QuantExt::NumericLgmMultiLegOptionEngine)
namespace QuantExt {
class NumericLgmMultiLegOptionEngine : public PricingEngine {
};
}

%shared_ptr(QuantExt::CommodityAveragePriceOptionAnalyticalEngine)
namespace QuantExt {
class CommodityAveragePriceOptionAnalyticalEngine : public PricingEngine {
  public:
    CommodityAveragePriceOptionAnalyticalEngine(
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
        const QuantLib::Handle<QuantLib::BlackVolTermStructure>& vol,
        QuantLib::Real beta = 0.0,
        QuantLib::DiffusionModelType modelType = QuantLib::DiffusionModelType::AsInputVolatilityType,
        QuantLib::Real displacement = 0.0);
};
}

%shared_ptr(QuantExt::CommoditySwaptionEngine)
namespace QuantExt {
class CommoditySwaptionEngine : public PricingEngine {
  public:
    CommoditySwaptionEngine(const Handle<YieldTermStructure>& discountCurve,
                            const Handle<QuantLib::BlackVolTermStructure>& vol,
                            Real beta = 0.0);
};
}

%shared_ptr(QuantExt::DiscountingBondTRSEngine)
namespace QuantExt {
class DiscountingBondTRSEngine : public PricingEngine {
  public:
    DiscountingBondTRSEngine(const Handle<YieldTermStructure>& discountCurve,
                             const bool treatSecuritySpreadAsCreditSpread,
                             const bool survivalWeightedFundingReturnCashflows);
};
}

%shared_ptr(QuantExt::BlackIndexCdsOptionEngine)
namespace QuantExt {
class BlackIndexCdsOptionEngine : public PricingEngine {
};
}

%shared_ptr(QuantExt::DiscountingSwapEngineDeltaGamma)
namespace QuantExt {
class DiscountingSwapEngineDeltaGamma : public PricingEngine {
  public:
    DiscountingSwapEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                                    const std::vector<Time>& bucketTimes = std::vector<Time>(),
                                    const bool computeDelta = false,
                                    const bool computeGamma = false,
                                    const bool computeBPS = false,
                                    const bool linearInZero = true);
};
}

#endif
