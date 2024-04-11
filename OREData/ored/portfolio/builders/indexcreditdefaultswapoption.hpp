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

/*! \file portfolio/builders/indexcreditdefaultswap.hpp
\brief
\ingroup portfolio
*/

#pragma once

#include <qle/pricingengines/blackindexcdsoptionengine.hpp>

#include <ored/utilities/parsers.hpp>

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>

namespace ore {
namespace data {

//! Engine Builder base class for Index Credit Default Swap Options
/*! Pricing engines are cached by the index CDS option trade's currency, the index CDS constituent credit curve IDs
    and the index CDS credit curve ID. If the term of the underlying index CDS is provided, it is appended to the
    index CDS credit curve ID also for the purposes of caching an engine.

    \ingroup portfolio
*/

class IndexCreditDefaultSwapOptionEngineBuilder
    : public ore::data::CachingPricingEngineBuilder<std::vector<std::string>, const QuantLib::Currency&,
                                                    const std::string&, const std::string&,
                                                    const std::vector<std::string>&> {
public:
    CreditPortfolioSensitivityDecomposition sensitivityDecomposition();

protected:
    IndexCreditDefaultSwapOptionEngineBuilder(const std::string& model, const std::string& engine)
        : ore::data::CachingEngineBuilder<std::vector<std::string>, QuantLib::PricingEngine, const QuantLib::Currency&,
                                          const std::string&, const std::string&, const std::vector<std::string>&>(
              model, engine, {"IndexCreditDefaultSwapOption"}) {}

    std::vector<std::string> keyImpl(const QuantLib::Currency& ccy, const std::string& creditCurveId,
                                     const std::string& volCurveId,
                                     const std::vector<std::string>& creditCurveIds) override;
};

//! Black CDS option engine builder for index CDS options.
/*! This class creates a BlackIndexCdsOptionEngine
    \ingroup portfolio
*/

class BlackIndexCdsOptionEngineBuilder : public IndexCreditDefaultSwapOptionEngineBuilder {
public:
    BlackIndexCdsOptionEngineBuilder()
        : IndexCreditDefaultSwapOptionEngineBuilder("Black", "BlackIndexCdsOptionEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    engineImpl(const QuantLib::Currency& ccy, const std::string& creditCurveId, const std::string& volCurveId,
               const std::vector<std::string>& creditCurveIds) override;
};

//! Numerical Integration index CDS option engine.
class NumericalIntegrationIndexCdsOptionEngineBuilder : public IndexCreditDefaultSwapOptionEngineBuilder {
public:
    NumericalIntegrationIndexCdsOptionEngineBuilder()
        : IndexCreditDefaultSwapOptionEngineBuilder("LognormalAdjustedIndexSpread", "NumericalIntegrationEngine") {}

protected:
    virtual QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
    engineImpl(const QuantLib::Currency& ccy, const std::string& creditCurveId, const std::string& volCurveId,
               const std::vector<std::string>& creditCurveIds) override;
};

} // namespace data
} // namespace ore
