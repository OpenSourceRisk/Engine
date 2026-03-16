/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/analyticjyyoycapfloorengine.hpp
    \brief Analytic Jarrow Yildrim (JY) year on year cap floor engine
    \ingroup engines
*/

#ifndef quantext_analytic_jy_yoy_capfloor_engine_hpp
#define quantext_analytic_jy_yoy_capfloor_engine_hpp

#include <ql/instruments/inflationcapfloor.hpp>
#include <qle/models/crossassetmodel.hpp>

namespace QuantExt {

/*! Analytic Jarrow Yildrim (JY) year on year inflation cap floor engine
    \ingroup engines
 */
class AnalyticJyYoYCapFloorEngine : public YoYInflationCapFloor::engine {
public:
    /*! Constructor
        \param model the cross asset model to be used in the valuation.
        \param index the index of the inflation component to use within the cross asset model.
        \param indexIsInterpolated whether the underlying inflation index is interpolated or not
    */
    AnalyticJyYoYCapFloorEngine(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
        QuantLib::Size index, bool indexIsInterpolated);
    
    //! \name PricingEngine interface
    //@{
    void calculate() const override;
    //@}

private:
    const QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    QuantLib::Size index_;
    bool indexIsInterpolated_;

    /*! Return the variance of the log inflation index ratio \f$\ln(I(T)/I(S))\f$ under Jarrow Yildrim where
        \f$ 0 < S < T \f$. The value is given in Section 13 of <em>Modern Derivatives Pricing and Credit Exposure
        Analysis, 2015</em>.
    */
    QuantLib::Real varianceLogRatio(QuantLib::Time S, QuantLib::Time T) const;
};

}

#endif
