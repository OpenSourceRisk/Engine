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

/*! \file qle/pricingengines/analyticjycpicapfloorengine.hpp
    \brief Analytic Jarrow Yildrim (JY) CPI cap floor engine
    \ingroup engines
*/

#ifndef quantext_analytic_jy_cpi_capfloor_engine_hpp
#define quantext_analytic_jy_cpi_capfloor_engine_hpp

#include <ql/instruments/cpicapfloor.hpp>
#include <qle/models/crossassetmodel.hpp>

namespace QuantExt {

/*! Analytic Jarrow Yildrim (JY) CPI cap floor engine
    \ingroup engines
 */
class AnalyticJyCpiCapFloorEngine : public CPICapFloor::engine {
public:
    /*! Constructor
        \param model the cross asset model to be used in the valuation.
        \param index the index of the inflation component to use within the cross asset model.
    */
    AnalyticJyCpiCapFloorEngine(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
        QuantLib::Size index);
    
    //! \name PricingEngine interface
    //@{
    void calculate() const override;
    //@}

private:
    const QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    QuantLib::Size index_;
};

}

#endif
