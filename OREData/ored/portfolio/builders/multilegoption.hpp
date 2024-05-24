/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ql/indexes/interestrateindex.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>

namespace ore {
namespace data {

//! MultiLeg option engine builder base class
class MultiLegOptionEngineBuilderBase
    : public CachingPricingEngineBuilder<string, const string&, const std::vector<Date>&, const Date&,
                                         const std::vector<Currency>&, const std::vector<Date>&,
                                         const std::vector<QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>&> {
public:
    MultiLegOptionEngineBuilderBase(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"MultiLegOption"}) {}

protected:
    string keyImpl(const string& id, const std::vector<Date>& exDates, const Date& maturityDate,
                   const std::vector<Currency>& currencies, const std::vector<Date>& fixingDates,
                   const std::vector<QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexes) override {
        return id;
    }
};

//! MultiLeg option engine builder for MC pricer
class CamMcMultiLegOptionEngineBuilder : public MultiLegOptionEngineBuilderBase {
public:
    CamMcMultiLegOptionEngineBuilder() : MultiLegOptionEngineBuilderBase("CrossAssetModel", "MC") {}

protected:
    QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const string& id, const std::vector<Date>& exDates, const Date& maturityDate,
               const std::vector<Currency>& currencies, const std::vector<Date>& fixingDates,
               const std::vector<QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexes) override;

private:
    std::string getCcyValue(const std::string& s, const std::string& c, const bool mandatory);
};

//! Multileg option engine builder for external cam, with additional simulation dates (AMC)
class CamAmcMultiLegOptionEngineBuilder : public MultiLegOptionEngineBuilderBase {
public:
    // for external cam, with additional simulation dates (AMC)
    CamAmcMultiLegOptionEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                      const std::vector<Date>& simulationDates)
        : MultiLegOptionEngineBuilderBase("CrossAssetModel", "AMC"), cam_(cam), simulationDates_(simulationDates) {}

protected:
    // the pricing engine depends on the ccys only
    string keyImpl(const string& id, const std::vector<Date>& exDates, const Date& maturityDate,
                   const std::vector<Currency>& currencies, const std::vector<Date>& fixingDates,
                   const std::vector<QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexes) override {
        std::string res;
        for (auto const& c : currencies) {
            res += c.code() + "_";
        }
        return res;
    }

    QuantLib::ext::shared_ptr<PricingEngine>
    engineImpl(const string& id, const std::vector<Date>& exDates, const Date& maturityDate,
               const std::vector<Currency>& currencies, const std::vector<Date>& fixingDates,
               const std::vector<QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexes) override;

private:
    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
};

} // namespace data
} // namespace ore
