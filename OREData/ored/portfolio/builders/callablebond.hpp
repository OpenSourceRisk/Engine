/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/callablebond.hpp */

#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

#include <ql/time/date.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

class CallableBondEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<std::string, const std::string&, const std::string&,
                                                    const std::string&, const std::string&, const std::string&,
                                                    const std::string&, const QuantLib::Date&> {
protected:
    CallableBondEngineBuilder(const std::string& model, const std::string& engine)
        : CachingEngineBuilder(model, engine, {"CallableBond"}) {}

    std::string keyImpl(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
                        const std::string& securityId, const std::string& referenceCurveId,
                        const std::string& incomeCurveId, const QuantLib::Date& maturityDate) override {
        return id;
    }
};

class CallableBondLgmEngineBuilder : public CallableBondEngineBuilder {
public:
    explicit CallableBondLgmEngineBuilder(const std::string& engine) : CallableBondEngineBuilder("LGM", engine) {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::IrModel> model(const std::string& id, const std::string& ccy,
                                                       const QuantLib::Date& maturityDate,
                                                       const bool generateAdditionalResults);

    // Args cover a list of FD resp. Grid engine parameters
    template <class... Args>
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    makeEngine(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
               const std::string& securityId, const std::string& referenceCurveId, const std::string& incomeCurveId,
               const QuantLib::Date& maturityDate, Args... args);
};

class CallableBondCamEngineBuilder : public CallableBondEngineBuilder {
public:
    explicit CallableBondCamEngineBuilder(const std::string& engine) : CallableBondEngineBuilder("CrossAssetModel", engine) {}

protected:
    bool dynamicCreditModelEnabled() const {
        return parseBool(modelParameter("EnableCredit", {}, false, "false"));
    }

    Handle<QuantExt::CrossAssetModel> model(const std::string& id, const std::string& ccy,
                                            const std::string& creditCurveId, const QuantLib::Date& maturityDate,
                                            const bool generateAdditionalResults);
};

class CallableBondLgmFdEngineBuilder : public CallableBondLgmEngineBuilder {
public:
    CallableBondLgmFdEngineBuilder() : CallableBondLgmEngineBuilder("FD") {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::PricingEngine>
    engineImpl(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
               const std::string& securityId, const std::string& referenceCurveId, const std::string& incomeCurveId,
               const QuantLib::Date& maturityDate) override;
};

class CallableBondLgmGridEngineBuilder : public CallableBondLgmEngineBuilder {
public:
    CallableBondLgmGridEngineBuilder() : CallableBondLgmEngineBuilder("Grid") {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::PricingEngine>
    engineImpl(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
               const std::string& securityId, const std::string& referenceCurveId, const std::string& incomeCurveId,
               const QuantLib::Date& maturityDate) override;
};

class CallableBondCamMcBaseEngineBuilder : public CallableBondCamEngineBuilder {
public:
    CallableBondCamMcBaseEngineBuilder(const std::string& engine) : CallableBondCamEngineBuilder(engine) {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::PricingEngine> engine(const Handle<QuantExt::CrossAssetModel>& cam,
                                                              const std::vector<Date>& simulationDates,
                                                              const std::vector<Date>& stickyCloseOutDates);
};

class CallableBondCamMcEngineBuilder : public CallableBondCamMcBaseEngineBuilder {
public:
    CallableBondCamMcEngineBuilder() : CallableBondCamMcBaseEngineBuilder("MC") {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::PricingEngine>
    engineImpl(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
               const std::string& securityId, const std::string& referenceCurveId, const std::string& incomeCurveId,
               const QuantLib::Date& maturityDate) override;
};

class CallableBondCamAmcEngineBuilder : public CallableBondCamMcBaseEngineBuilder {
public:
    CallableBondCamAmcEngineBuilder(const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,
                                    const std::vector<Date>& simulationDates,
                                    const std::vector<Date>& stickyCloseOutDates)
        : CallableBondCamMcBaseEngineBuilder("AMC"), cam_(cam), simulationDates_(simulationDates),
          stickyCloseOutDates_(stickyCloseOutDates) {}

protected:
    QuantLib::ext::shared_ptr<QuantExt::PricingEngine>
    engineImpl(const std::string& id, const std::string& ccy, const std::string& creditCurveId,
               const std::string& securityId, const std::string& referenceCurveId, const std::string& incomeCurveId,
               const QuantLib::Date& maturityDate) override;

private:
    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> cam_;
    const std::vector<Date> simulationDates_;
    const std::vector<Date> stickyCloseOutDates_;
};

} // namespace data
} // namespace ore
