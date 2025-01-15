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

class CamAmcCurrencySwapEngineBuilder : public CrossCurrencySwapEngineBuilderBase {
public:
    // for external cam, with additional simulation dates (AMC)
    CamAmcCurrencySwapEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                    const std::vector<Date>& simulationDates,
                                    const std::vector<Date>& stickyCloseOutDates)
        : CrossCurrencySwapEngineBuilderBase("CrossAssetModel", "AMC"), cam_(cam), simulationDates_(simulationDates),
          stickyCloseOutDates_(stickyCloseOutDates) {}

protected:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const std::vector<Currency>& ccys, const Currency& base,
                                                        bool useXccyYieldCurves,
                                                        const std::set<std::string>& eqNames) override;

private:
    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
    const std::vector<Date> stickyCloseOutDates_;
};

class AmcCgCurrencySwapEngineBuilder : public CrossCurrencySwapEngineBuilderBase {
public:
    // for external cam, with additional simulation dates (AMC)
    AmcCgCurrencySwapEngineBuilder(const QuantLib::ext::shared_ptr<ore::data::ModelCG>& modelCg,
                                   const std::set<Date>& valuationDates, const std::vector<Date>& closeOutDates,
                                   const bool useStickyCloseOutDates)
        : CrossCurrencySwapEngineBuilderBase("CrossAssetModel", "AMCCG"), modelCg_(modelCg),
          valuationDates_(valuationDates), closeOutDates_(closeOutDates),
          useStickyCloseOutDates_(useStickyCloseOutDates) {}

protected:
    QuantLib::ext::shared_ptr<PricingEngine> engineImpl(const std::vector<Currency>& ccys, const Currency& base,
                                                        bool useXccyYieldCurves,
                                                        const std::set<std::string>& eqNames) override;

private:
    QuantLib::ext::shared_ptr<ore::data::ModelCG> modelCg_;
    std::set<Date> valuationDates_;
    std::vector<Date> closeOutDates_;
    bool useStickyCloseOutDates_;
};

} // namespace data
} // namespace ore
