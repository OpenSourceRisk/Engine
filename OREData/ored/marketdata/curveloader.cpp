/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <ored/marketdata/curveloader.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

using namespace boost;
using QuantLib::Size;

namespace openriskengine {
namespace data {

// Returns true if we can build this YieldCurveSpec with the given curve specs
static bool canBuild(boost::shared_ptr<YieldCurveSpec>& ycs, vector<boost::shared_ptr<YieldCurveSpec>>& specs,
                     const CurveConfigurations& curveConfigs) {
    string yieldCurveID = ycs->curveConfigID();
    boost::shared_ptr<YieldCurveConfig> curveConfig = curveConfigs.yieldCurveConfig(yieldCurveID);
    QL_REQUIRE(curveConfig, "No yield curve configuration found for config ID " << yieldCurveID);

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
            return false;
        }
    }
    // We can build everything required
    return true;
}

void order(vector<boost::shared_ptr<CurveSpec>>& curveSpecs, const CurveConfigurations& curveConfigs) {

    /* Order the curve specs and remove duplicates (i.e. those with same name). */
    sort(curveSpecs.begin(), curveSpecs.end());
    auto itSpec = unique(curveSpecs.begin(), curveSpecs.end());
    curveSpecs.resize(distance(curveSpecs.begin(), itSpec));

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

    /* Loop over yieldCurveSpec, remove all curvespecs that we can build by checking sortedYieldCurveSpecs
     * Repeat until yieldCurveSpec is empty
     */
    while (yieldCurveSpecs.size() > 0) {
        Size n = yieldCurveSpecs.size();

        auto it = yieldCurveSpecs.begin();
        while (it != yieldCurveSpecs.end()) {
            if (canBuild(*it, sortedYieldCurveSpecs, curveConfigs)) {
                DLOG("can build " << (*it)->curveConfigID());
                sortedYieldCurveSpecs.push_back(*it);
                it = yieldCurveSpecs.erase(it);
            } else {
                DLOG("can not (yet) build " << (*it)->curveConfigID());
                ++it;
            }
        }
        QL_REQUIRE(n > yieldCurveSpecs.size(), "missing curve or cycle in yield curve spec");
    }

    /* Now put them into the front of curveSpecs */
    curveSpecs.insert(curveSpecs.begin(), sortedYieldCurveSpecs.begin(), sortedYieldCurveSpecs.end());

    DLOG("Ordered Curves (" << curveSpecs.size() << ")")
    for (Size i = 0; i < curveSpecs.size(); ++i)
        DLOG(std::setw(2) << i << " " << curveSpecs[i]->name());
}
}
}
