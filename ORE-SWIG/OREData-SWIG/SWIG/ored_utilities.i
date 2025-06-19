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

#ifndef ored_utilitles_i
#define ored_utilitles_i

%include types.i

%{
using std::string;
using std::map;

using ore::data::addMarketObjectDependencies;
using ore::data::marketObjectToCurveSpec;
using ore::data::currencyToDiscountCurve;
using ore::data::swapIndexDiscountCurve;
using ore::data::buildCollateralCurveConfig;
using ore::data::getCollateralisedDiscountCcy;
using ore::data::isCollateralCurve;
using ore::data::CurveConfigurations;
using ore::data::MarketObject;

%}
void addMarketObjectDependencies(
    std::map<std::string, std::map<MarketObject, std::set<std::string>>>* objects,
    const ext::shared_ptr<CurveConfigurations>& curveConfigs, const std::string& baseCcy,
    const std::string& baseCcyDiscountCurve);

std::string marketObjectToCurveSpec(const MarketObject& mo, const std::string& name, const std::string& baseCcy,
                                    const ext::shared_ptr<CurveConfigurations>& curveConfigs);

std::string currencyToDiscountCurve(const std::string& ccy, const std::string& baseCcy,
                                    const std::string& baseCcyDiscountCurve,
                                    const ext::shared_ptr<CurveConfigurations>& curveConfigs);

std::string swapIndexDiscountCurve(const std::string& ccy, const std::string& baseCcy = std::string(), 
    const std::string& swapIndexConvId = std::string());

void buildCollateralCurveConfig(const string& curveId, const std::string& baseCcy,
                               const std::string& baseCcyDiscountCurve,
                                const ext::shared_ptr<CurveConfigurations>& curveConfigs);

std::set<std::string> getCollateralisedDiscountCcy(const std::string& ccy,
    const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs);

const bool isCollateralCurve(const std::string& id, std::vector<std::string>& tokens);

#endif