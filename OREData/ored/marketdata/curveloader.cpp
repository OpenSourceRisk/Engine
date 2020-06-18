/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <algorithm>
#include <ored/marketdata/curveloader.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/marketdata/structuredcurveerror.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

using namespace boost;
using QuantLib::Size;
using std::find_if;
using std::map;
using std::string;

namespace ore {
namespace data {

// Returns true if we can build this commodity curve spec with the given yield curve specs and commodity curve specs
bool canBuild(boost::shared_ptr<CurveSpec>& spec, vector<boost::shared_ptr<YieldCurveSpec>>& yieldSpecs,
              vector<boost::shared_ptr<CurveSpec>>& commoditySpecs, const CurveConfigurations& curveConfigs,
              map<string, string>& missingDependents, map<string, string>& errors, bool continueOnError) {

    // Check if we have the commodity curve configuration for the current commodity curve spec
    string curveId = spec->curveConfigID();
    if (!curveConfigs.hasCommodityCurveConfig(curveId)) {
        string errMsg = "Can't get commodity curve configuration for curve " + curveId;
        if (continueOnError) {
            errors[spec->name()] = errMsg;
            TLOG(errMsg);
            return false;
        } else {
            QL_FAIL(errMsg);
        }
    }

    // Get the commodity curve configuration
    boost::shared_ptr<CommodityCurveConfig> curveConfig = curveConfigs.commodityCurveConfig(curveId);

    // If it is direct, we can build it so just return true
    if (curveConfig->type() == CommodityCurveConfig::Type::Direct) {
        missingDependents[curveId] = "";
        return true;
    }

    // If it is cross currency type, need to check the two yield curves.
    if (curveConfig->type() == CommodityCurveConfig::Type::CrossCurrency) {

        // 1. check if we have the base yield curve
        auto ycIt =
            find_if(yieldSpecs.begin(), yieldSpecs.end(), [&curveConfig](const boost::shared_ptr<YieldCurveSpec>& ycs) {
                return ycs->curveConfigID() == curveConfig->baseYieldCurveId();
            });
        if (ycIt == yieldSpecs.end()) {
            DLOG("Required yield curve " << curveConfig->baseYieldCurveId() << " for " << curveId << " not available");
            missingDependents[curveId] = curveConfig->baseYieldCurveId();
            return false;
        }

        // 2. check if we have the commodity curve currency yield curve
        ycIt =
            find_if(yieldSpecs.begin(), yieldSpecs.end(), [&curveConfig](const boost::shared_ptr<YieldCurveSpec>& ycs) {
                return ycs->curveConfigID() == curveConfig->yieldCurveId();
            });
        if (ycIt == yieldSpecs.end()) {
            DLOG("Required yield curve " << curveConfig->yieldCurveId() << " for " << curveId << " not available");
            missingDependents[curveId] = curveConfig->yieldCurveId();
            return false;
        }
    }

    // If it is cross currency or basis type, need to check the base price curve.

    // 3. check if we already have the base commodity curve
    auto it =
        find_if(commoditySpecs.begin(), commoditySpecs.end(), [&curveConfig](const boost::shared_ptr<CurveSpec>& cs) {
            return cs->curveConfigID() == curveConfig->basePriceCurveId();
        });
    if (it == commoditySpecs.end()) {
        DLOG("Required commodity curve " << curveConfig->basePriceCurveId() << " for " << curveId << " not available");
        missingDependents[curveId] = curveConfig->basePriceCurveId();
        return false;
    }

    // If we get here, we have a non-direct curve and we can build everything required
    missingDependents[curveId] = "";
    return true;
}

// Returns true if we can build this commodity volatility curve spec with the given commodity volatility curve specs
// This is solely to account for commodity volatility structures that depend on other commodity volatility structures.
bool canBuild(boost::shared_ptr<CurveSpec>& spec, vector<boost::shared_ptr<CurveSpec>>& cvSpecs,
              const CurveConfigurations& curveConfigs, map<string, string>& missingDependents,
              map<string, string>& errors, bool continueOnError) {

    // Check if we have the commodity volatility curve configuration for the current commodity curve spec
    string curveId = spec->curveConfigID();
    if (!curveConfigs.hasCommodityVolatilityConfig(curveId)) {
        string errMsg = "Can't get commodity volatility curve configuration for curve " + curveId;
        if (continueOnError) {
            errors[spec->name()] = errMsg;
            TLOG(errMsg);
            return false;
        } else {
            QL_FAIL(errMsg);
        }
    }

    // Get the commodity curve configuration
    auto cvConfig = curveConfigs.commodityVolatilityConfig(curveId);

    // Currently, the only surface we have to check is the ApoFutureSurface as it is the only one that depends on
    // another commodity volatility surface.
    if (auto vapo = boost::dynamic_pointer_cast<VolatilityApoFutureSurfaceConfig>(cvConfig->volatilityConfig())) {

        // The base volatility surface ID is in the form of a spec i.e. CommodityVolatility/<CCY>/<COMM_NAME>
        auto vSpec = parseCurveSpec(vapo->baseVolatilityId());

        // Check if we already have the base commodity volatility surface.
        auto it = find_if(cvSpecs.begin(), cvSpecs.end(), [&vSpec](const boost::shared_ptr<CurveSpec>& cs) {
            return cs->curveConfigID() == vSpec->curveConfigID();
        });

        if (it == cvSpecs.end()) {
            DLOG("Required commodity volatility curve " << vSpec->curveConfigID() << " for " << curveId
                                                        << " not available");
            missingDependents[curveId] = vSpec->curveConfigID();
            return false;
        }
    }

    missingDependents[curveId] = "";
    return true;
}

// Order the commodity curve specs
// We assume that curveSpecs have already been ordered here via the top level order function below
void orderCommodity(vector<boost::shared_ptr<CurveSpec>>& curveSpecs,
                    vector<boost::shared_ptr<YieldCurveSpec>>& ycSpecs, const CurveConfigurations& curveConfigs,
                    map<string, string>& errors, bool continueOnError) {

    // Get iterator to the first commodity curve spec in curveSpecs, if there is one
    auto firstCommIt = find_if(curveSpecs.begin(), curveSpecs.end(), [](const boost::shared_ptr<CurveSpec>& cs) {
        return cs->baseType() == CurveSpec::CurveType::Commodity;
    });

    // If no commodity curve specs, there is nothing to do
    if (firstCommIt == curveSpecs.end())
        return;

    // Get iterator to the last commodity curve spec in curveSpecs
    auto lastCommRit = find_if(curveSpecs.rbegin(), curveSpecs.rend(), [](const boost::shared_ptr<CurveSpec>& cs) {
        return cs->baseType() == CurveSpec::CurveType::Commodity;
    });
    auto lastCommIt = lastCommRit.base();

    // Remove the commodity curve specs from the curveSpecs for ordering
    vector<boost::shared_ptr<CurveSpec>> commCurveSpecs(firstCommIt, lastCommIt);
    firstCommIt = curveSpecs.erase(firstCommIt, lastCommIt);

    // Order the commodity curve specs in cvCurveSpecs
    vector<boost::shared_ptr<CurveSpec>> orderedCommCurveSpecs;

    // Map to sort the missing dependencies
    map<string, string> missingDependents;

    // Use same logic to populate orderedCvCurveSpecs as is used to populate sorted yield curve specs below
    while (commCurveSpecs.size() > 0) {

        // Record size of commodity specs before checking if any of them can build
        Size n = commCurveSpecs.size();

        // Check each curve remaining in cvCurveSpecs
        auto it = commCurveSpecs.begin();
        while (it != commCurveSpecs.end()) {
            if (canBuild(*it, ycSpecs, orderedCommCurveSpecs, curveConfigs, missingDependents, errors,
                         continueOnError)) {
                DLOG("Can build " << (*it)->curveConfigID());
                orderedCommCurveSpecs.push_back(*it);
                it = commCurveSpecs.erase(it);
            } else {
                DLOG("Cannot (yet) build " << (*it)->curveConfigID());
                ++it;
            }
        }

        // If cvCurveSpecs has not been reduced in size after checking if any can build, we stop with error message
        if (n == commCurveSpecs.size()) {
            for (auto cs : commCurveSpecs) {
                if (errors.count(cs->name()) > 0) {
                    WLOG("Cannot build curve " << cs->curveConfigID() << " due to error: " << errors.at(cs->name()));
                } else {
                    WLOG("Cannot build curve " << cs->curveConfigID() << ", dependent curves missing");
                    errors[cs->name()] = "dependent curves missing - " + missingDependents[cs->curveConfigID()];
                }
                ALOG(StructuredCurveErrorMessage(cs->curveConfigID(), "Cannot build curve", errors.at(cs->name())));
            }
            break;
        }
    }

    // Insert the sorted commodity specs back at the correct location in the original curveSpecs
    curveSpecs.insert(firstCommIt, orderedCommCurveSpecs.begin(), orderedCommCurveSpecs.end());
}

// TODO: align all of this logic instead of having all of these copy/paste functions. There was a ticket where this
//       was done via Boost Graph in a more succint way.

// Order the commodity volatility curve specs
// We assume that curveSpecs have already been ordered here via the top level order function below
void orderCommodityVolatilities(vector<boost::shared_ptr<CurveSpec>>& curveSpecs,
                                const CurveConfigurations& curveConfigs, map<string, string>& errors,
                                bool continueOnError) {

    // Get iterator to the first commodity volatility curve spec in curveSpecs, if there is one.
    auto firstCommIt = find_if(curveSpecs.begin(), curveSpecs.end(), [](const boost::shared_ptr<CurveSpec>& cs) {
        return cs->baseType() == CurveSpec::CurveType::CommodityVolatility;
    });

    // If no commodity volatility curve specs, there is nothing to do
    if (firstCommIt == curveSpecs.end())
        return;

    // Get iterator to the last commodity volatility curve spec in curveSpecs
    auto lastCommRit = find_if(curveSpecs.rbegin(), curveSpecs.rend(), [](const boost::shared_ptr<CurveSpec>& cs) {
        return cs->baseType() == CurveSpec::CurveType::CommodityVolatility;
    });
    auto lastCommIt = lastCommRit.base();

    // Remove the commodity volatility curve specs from the curveSpecs for ordering
    vector<boost::shared_ptr<CurveSpec>> cvCurveSpecs(firstCommIt, lastCommIt);
    firstCommIt = curveSpecs.erase(firstCommIt, lastCommIt);

    // Order the commodity volatility curve specs in cvCurveSpecs
    vector<boost::shared_ptr<CurveSpec>> orderedCvCurveSpecs;

    // Map to sort the missing dependencies
    map<string, string> missingDependents;

    // Use same logic to populate orderedCvCurveSpecs as is used to populate sorted yield curve specs below
    while (cvCurveSpecs.size() > 0) {

        // Record size of commodity volatility curve specs before checking if any of them can build
        Size n = cvCurveSpecs.size();

        // Check each curve remaining in cvCurveSpecs
        auto it = cvCurveSpecs.begin();
        while (it != cvCurveSpecs.end()) {
            if (canBuild(*it, orderedCvCurveSpecs, curveConfigs, missingDependents, errors, continueOnError)) {
                DLOG("Can build " << (*it)->curveConfigID());
                orderedCvCurveSpecs.push_back(*it);
                it = cvCurveSpecs.erase(it);
            } else {
                DLOG("Cannot (yet) build " << (*it)->curveConfigID());
                ++it;
            }
        }

        // If cvCurveSpecs has not been reduced in size after checking if any can build, we stop with error message
        if (n == cvCurveSpecs.size()) {
            for (auto cs : cvCurveSpecs) {
                if (errors.count(cs->name()) > 0) {
                    WLOG("Cannot build curve " << cs->curveConfigID() << " due to error: " << errors.at(cs->name()));
                } else {
                    WLOG("Cannot build curve " << cs->curveConfigID() << ", dependent curves missing");
                    errors[cs->name()] = "dependent curves missing - " + missingDependents[cs->curveConfigID()];
                }
                ALOG(StructuredCurveErrorMessage(cs->curveConfigID(), "Cannot build curve", errors.at(cs->name())));
            }
            break;
        }
    }

    // Insert the sorted commodity volatility specs back at the correct location in the original curveSpecs
    curveSpecs.insert(firstCommIt, orderedCvCurveSpecs.begin(), orderedCvCurveSpecs.end());
}

// Returns true if we can build this YieldCurveSpec with the given curve specs
static bool canBuild(boost::shared_ptr<CurveSpec>& evcs, vector<boost::shared_ptr<EquityVolatilityCurveSpec>>& specs,
                     const CurveConfigurations& curveConfigs, map<string, string>& missingDependents,
                     map<string, string>& errors, bool continueOnError) {

    string curveId = evcs->curveConfigID();
    if (!curveConfigs.hasEquityVolCurveConfig(curveId)) {
        string errMsg = "Can't get equity vol curve configuration for " + curveId;
        if (continueOnError) {
            errors[evcs->name()] = errMsg;
            TLOG(errMsg);
            return false;
        } else {
            QL_FAIL(errMsg);
        }
    }

    boost::shared_ptr<EquityVolatilityCurveConfig> curveConfig = curveConfigs.equityVolCurveConfig(curveId);
    if (curveConfig->isProxySurface()) {
        // The base volatility surface ID is in the form of a spec i.e. CommodityVolatility/<CCY>/<COMM_NAME>
        auto proxy = curveConfig->proxySurface();

        // Check if we already have the base commodity volatility surface.
        auto it = find_if(specs.begin(), specs.end(), [&proxy](const boost::shared_ptr<EquityVolatilityCurveSpec>& cs) {
            return cs->curveConfigID() == proxy;
        });

        if (it == specs.end()) {
            DLOG("Required equity volatility curve " << proxy << " for " << curveId << " not available");
            missingDependents[curveId] = proxy;
            return false;
        }
    }

    // We can build everything required
    missingDependents[curveId] = "";
    return true;
}

// Order the equity volatility curve specs
// We assume that curveSpecs have already been ordered here via the top level order function below
void orderEquityVolatilities(vector<boost::shared_ptr<CurveSpec>>& curveSpecs, const CurveConfigurations& curveConfigs,
                             map<string, string>& errors, bool continueOnError) {

    // Get iterator to the first equity volatility curve spec in curveSpecs, if there is one.
    auto firstEqIt = find_if(curveSpecs.begin(), curveSpecs.end(), [](const boost::shared_ptr<CurveSpec>& cs) {
        return cs->baseType() == CurveSpec::CurveType::EquityVolatility;
    });

    // If no equity volatility curve specs, there is nothing to do
    if (firstEqIt == curveSpecs.end())
        return;

    // Get iterator to the last equity volatility curve spec in curveSpecs
    auto lastEqRit = find_if(curveSpecs.rbegin(), curveSpecs.rend(), [](const boost::shared_ptr<CurveSpec>& cs) {
        return cs->baseType() == CurveSpec::CurveType::EquityVolatility;
    });
    auto lastEqIt = lastEqRit.base();

    // Remove the equity volatility curve specs from the curveSpecs for ordering
    vector<boost::shared_ptr<CurveSpec>> eqvCurveSpecs(firstEqIt, lastEqIt);
    firstEqIt = curveSpecs.erase(firstEqIt, lastEqIt);

    // Order the equity volatility curve specs in cvCurveSpecs
    vector<boost::shared_ptr<EquityVolatilityCurveSpec>> orderedEqCurveSpecs;

    // Map to sort the missing dependencies
    map<string, string> missingDependents;

    // Use same logic to populate orderedCvCurveSpecs as is used to populate sorted yield curve specs below
    while (eqvCurveSpecs.size() > 0) {

        // Record size of equity volatility curve specs before checking if any of them can build
        Size n = eqvCurveSpecs.size();

        // Check each curve remaining in cvCurveSpecs
        auto it = eqvCurveSpecs.begin();
        while (it != eqvCurveSpecs.end()) {
            if (canBuild(*it, orderedEqCurveSpecs, curveConfigs, missingDependents, errors, continueOnError)) {
                DLOG("Can build " << (*it)->curveConfigID());
                auto eqSpec = boost::dynamic_pointer_cast<EquityVolatilityCurveSpec>(*it);
                orderedEqCurveSpecs.push_back(eqSpec);
                it = eqvCurveSpecs.erase(it);
            } else {
                DLOG("Cannot (yet) build " << (*it)->curveConfigID());
                ++it;
            }
        }

        // If eqvCurveSpecs has not been reduced in size after checking if any can build, we stop with error message
        if (n == eqvCurveSpecs.size()) {
            for (auto cs : eqvCurveSpecs) {
                if (errors.count(cs->name()) > 0) {
                    WLOG("Cannot build curve " << cs->curveConfigID() << " due to error: " << errors.at(cs->name()));
                } else {
                    WLOG("Cannot build curve " << cs->curveConfigID() << ", dependent curves missing");
                    errors[cs->name()] = "dependent curves missing - " + missingDependents[cs->curveConfigID()];
                }
                ALOG(StructuredCurveErrorMessage(cs->curveConfigID(), "Cannot build curve", errors.at(cs->name())));
            }
            break;
        }
    }

    // Insert the sorted commodity volatility specs back at the correct location in the original curveSpecs
    curveSpecs.insert(firstEqIt, orderedEqCurveSpecs.begin(), orderedEqCurveSpecs.end());
}

// Returns true if we can build this YieldCurveSpec with the given curve specs
static bool canBuild(boost::shared_ptr<YieldCurveSpec>& ycs, vector<boost::shared_ptr<YieldCurveSpec>>& specs,
                     const CurveConfigurations& curveConfigs, map<string, string>& missingDependents,
                     map<string, string>& errors, bool continueOnError) {

    string yieldCurveID = ycs->curveConfigID();
    if (!curveConfigs.hasYieldCurveConfig(yieldCurveID)) {
        string errMsg = "Can't get yield curve configuration for " + yieldCurveID;
        if (continueOnError) {
            errors[ycs->name()] = errMsg;
            TLOG(errMsg);
            return false;
        } else {
            QL_FAIL(errMsg);
        }
    }

    boost::shared_ptr<YieldCurveConfig> curveConfig = curveConfigs.yieldCurveConfig(yieldCurveID);
    set<string> requiredYieldCurveIDs = curveConfig->requiredYieldCurveIDs();
    for (auto it : requiredYieldCurveIDs) {
        // search for this name in the vector specs, return false if not found, otherwise move to next required id
        bool ok = false;
        for (Size i = 0; i < specs.size(); i++) {
            if (specs[i]->curveConfigID() == it)
                ok = true;
        }
        if (!ok) {
            DLOG("required yield curve " << it << " for " << yieldCurveID << " not (yet) available");
            missingDependents[yieldCurveID] = it;
            return false;
        }
    }

    // We can build everything required
    missingDependents[yieldCurveID] = "";
    return true;
}

void order(vector<boost::shared_ptr<CurveSpec>>& curveSpecs, const CurveConfigurations& curveConfigs,
           std::map<std::string, std::string>& errors, bool continueOnError) {

    /* Order the curve specs and remove duplicates (i.e. those with same name).
     * The sort() call relies on CurveSpec::operator< which ensures a few properties:
     * - FX loaded before FXVol
     * - Eq loaded before EqVol
     * - Inf loaded before InfVol
     * - rate curves, swap indices, swaption vol surfaces before correlation curves
     */
    sort(curveSpecs.begin(), curveSpecs.end());
    auto itSpec = unique(curveSpecs.begin(), curveSpecs.end());
    curveSpecs.resize(std::distance(curveSpecs.begin(), itSpec));

    /* remove the YieldCurveSpecs from curveSpecs
     */
    vector<boost::shared_ptr<YieldCurveSpec>> yieldCurveSpecs;
    itSpec = curveSpecs.begin();
    while (itSpec != curveSpecs.end()) {
        if ((*itSpec)->baseType() == CurveSpec::CurveType::Yield) {
            boost::shared_ptr<YieldCurveSpec> spec = boost::dynamic_pointer_cast<YieldCurveSpec>(*itSpec);
            yieldCurveSpecs.push_back(spec);
            itSpec = curveSpecs.erase(itSpec);
        } else
            ++itSpec;
    }

    /* Now sort the yieldCurveSpecs, store them in sortedYieldCurveSpecs  */
    vector<boost::shared_ptr<YieldCurveSpec>> sortedYieldCurveSpecs;

    /* Map to sort the missing dependencies */
    map<string, string> missingDependents;

    /* Loop over yieldCurveSpec, remove all curvespecs that we can build by checking sortedYieldCurveSpecs
     * Repeat until yieldCurveSpec is empty
     */
    while (yieldCurveSpecs.size() > 0) {
        Size n = yieldCurveSpecs.size();

        auto it = yieldCurveSpecs.begin();
        while (it != yieldCurveSpecs.end()) {
            if (canBuild(*it, sortedYieldCurveSpecs, curveConfigs, missingDependents, errors, continueOnError)) {
                DLOG("can build " << (*it)->curveConfigID());
                sortedYieldCurveSpecs.push_back(*it);
                it = yieldCurveSpecs.erase(it);
            } else {
                DLOG("can not (yet) build " << (*it)->curveConfigID());
                ++it;
            }
        }

        if (n == yieldCurveSpecs.size()) {
            for (auto ycs : yieldCurveSpecs) {
                if (errors.count(ycs->name()) > 0) {
                    WLOG("Cannot build curve " << ycs->curveConfigID() << " due to error: " << errors.at(ycs->name()));
                } else {
                    WLOG("Cannot build curve " << ycs->curveConfigID() << ", dependent curves missing");
                    errors[ycs->name()] = "dependent curves missing - " + missingDependents[ycs->curveConfigID()];
                }
                ALOG(StructuredCurveErrorMessage(ycs->curveConfigID(), "Cannot build curve", errors.at(ycs->name())));
            }
            break;
        }
    }

    /* Now put them into the front of curveSpecs */
    curveSpecs.insert(curveSpecs.begin(), sortedYieldCurveSpecs.begin(), sortedYieldCurveSpecs.end());

    // Order the commodity specs within the curveSpecs
    orderCommodity(curveSpecs, sortedYieldCurveSpecs, curveConfigs, errors, continueOnError);

    // Order the commodity volatility specs within the curveSpecs
    orderCommodityVolatilities(curveSpecs, curveConfigs, errors, continueOnError);

    // Order the equity volatility specs within the curveSpecs
    orderEquityVolatilities(curveSpecs, curveConfigs, errors, continueOnError);

    DLOG("Ordered Curves (" << curveSpecs.size() << ")")
    for (Size i = 0; i < curveSpecs.size(); ++i)
        DLOG(std::setw(2) << i << " " << curveSpecs[i]->name());
}
} // namespace data
} // namespace ore
