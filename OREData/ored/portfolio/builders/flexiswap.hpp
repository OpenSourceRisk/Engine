/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/flexiswap.hpp
    \brief
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

#include <qle/models/lgm.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {
using ore::data::CachingPricingEngineBuilder;

//! Flexi Swap / BGS Engine Builder Base Class (id2 is used for BGS only)
class FlexiSwapBGSEngineBuilderBase
    : public CachingPricingEngineBuilder<std::string, const std::string&, const std::string&, const std::string&,
                                         const std::vector<QuantLib::Date>&, const QuantLib::Date&,
                                         const std::vector<QuantLib::Real>&> {
public:
    FlexiSwapBGSEngineBuilderBase(const std::string& tradeType, const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {tradeType}) {}

protected:
    virtual std::string keyImpl(const std::string& id, const std::string& id2, const std::string& key,
                                const std::vector<QuantLib::Date>& dates, const QuantLib::Date& maturity,
                                const std::vector<QuantLib::Real>& strikes) override {
        return id;
    }
};

//! Flexi Swap / BGS Discounting Engine Builder
class FlexiSwapBGSDiscountingEngineBuilderBase : public FlexiSwapBGSEngineBuilderBase {
public:
    FlexiSwapBGSDiscountingEngineBuilderBase(const std::string& tradeType)
        : FlexiSwapBGSEngineBuilderBase(tradeType, "DiscountedCashflows", "DiscountingSwapEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& id, const std::string& id2,
                                                                  const std::string& key,
                                                                  const std::vector<QuantLib::Date>& dates,
                                                                  const QuantLib::Date& maturity,
                                                                  const std::vector<QuantLib::Real>& strikes) override;
};

//! Flexi Swap / BGS Numeric LGM Grid Engine Builder Base Class
class FlexiSwapBGSLGMGridEngineBuilderBase : public FlexiSwapBGSEngineBuilderBase {
public:
    FlexiSwapBGSLGMGridEngineBuilderBase(const std::string& tradeType, const std::string& model)
        : FlexiSwapBGSEngineBuilderBase(tradeType, model, "Grid") {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::LGM> model(const std::string& id, const std::string& key,
                                           const std::vector<QuantLib::Date>& dates, const QuantLib::Date& maturity,
                                           const std::vector<QuantLib::Real>& strikes);
};

//! Flexi Swap Discounting Engine Builder
class FlexiSwapDiscountingEngineBuilder : public FlexiSwapBGSDiscountingEngineBuilderBase {
public:
    FlexiSwapDiscountingEngineBuilder() : FlexiSwapBGSDiscountingEngineBuilderBase("FlexiSwap") {}
};

//! Flexi Swap LGM Grid Engine Builder
class FlexiSwapLGMGridEngineBuilder : public FlexiSwapBGSLGMGridEngineBuilderBase {
public:
    FlexiSwapLGMGridEngineBuilder() : FlexiSwapBGSLGMGridEngineBuilderBase("FlexiSwap", "LGM") {}

protected:
    virtual QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& id, const std::string& id2,
                                                                  const std::string& key,
                                                                  const std::vector<QuantLib::Date>& dates,
                                                                  const QuantLib::Date& maturity,
                                                                  const std::vector<QuantLib::Real>& strikes) override;
};

} // namespace data
} // namespace ore
