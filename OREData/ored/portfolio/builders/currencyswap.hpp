/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file multilegoption.hpp
    \brief multi leg option engine builder
*/

#pragma once

#include <qle/models/crossassetmodel.hpp>

#include <ored/portfolio/builders/swap.hpp>

namespace ore {
namespace data {

//! Multileg option engine builder for external cam, with additional simulation dates (AMC)
class CamAmcCurrencySwapEngineBuilder : public CrossCurrencySwapEngineBuilderBase {
public:
    // for external cam, with additional simulation dates (AMC)
    CamAmcCurrencySwapEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                    const std::vector<Date>& simulationDates)
        : CrossCurrencySwapEngineBuilderBase("CrossAssetModel", "AMC"), cam_(cam), simulationDates_(simulationDates) {}

protected:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const std::vector<Currency>& ccys, const Currency& base) override;

private:
    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
};

} // namespace data
} // namespace ore
