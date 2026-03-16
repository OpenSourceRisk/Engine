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

/*! \file ored/portfolio/builders/bondrepo.hpp
    \ingroup builders
*/

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

using ore::data::CachingPricingEngineBuilder;

//! Bond Repo engine builder base class
class BondRepoEngineBuilderBase : public CachingPricingEngineBuilder<std::string, const std::string&> {
public:
    BondRepoEngineBuilderBase(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"BondRepo"}) {}

protected:
    std::string keyImpl(const std::string& repoCurveId) override { return repoCurveId; }
};

//! Discounting Bond Repo Engine Builder
class DiscountingBondRepoEngineBuilder : public BondRepoEngineBuilderBase {
public:
    DiscountingBondRepoEngineBuilder() : BondRepoEngineBuilderBase("DiscountedCashflows", "DiscountingRepoEngine") {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string& repoCurveId) override;
};

//! Accrual Bond Repo Engine Builder
class AccrualBondRepoEngineBuilder : public BondRepoEngineBuilderBase {
public:
    AccrualBondRepoEngineBuilder() : BondRepoEngineBuilderBase("Accrual", "AccrualRepoEngine") {}

protected:
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engineImpl(const std::string&) override;
};

} // namespace data
} // namespace ore
