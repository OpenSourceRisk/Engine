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

#include <ored/portfolio/builders/cliquetoption.hpp>
#include <ored/scripting/engines/cliquetoptionmcscriptengine.hpp>

using namespace ore::data;
using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<PricingEngine> EquityCliquetOptionMcScriptEngineBuilder::engineImpl(const string& assetName,
                                                                                      const Currency& ccy,
                                                                                      const AssetClass& assetClass) {

    Size nPaths = parseInteger(engineParameter("Samples"));
    Size regressionOrder = parseInteger(engineParameter("RegressionOrder"));
    bool interactive = parseBool(engineParameter("Interactive"));
    bool scriptedLibraryOverride = parseBool(engineParameter("ScriptedLibraryOverride", {}, false));

    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = getBlackScholesProcess(assetName, ccy, assetClass);
    Handle<YieldTermStructure> discountCurve =
        market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
    return QuantLib::ext::make_shared<CliquetOptionMcScriptEngine>("EQ-" + assetName, ccy.code(), ccy.code(), gbsp, tradeTypes_,
                                                           nPaths, regressionOrder, interactive,
                                                           scriptedLibraryOverride);
}

} // namespace data
} // namespace ore
