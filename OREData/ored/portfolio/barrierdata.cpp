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

#include <ored/portfolio/barrierdata.hpp>
#include <ored/utilities/parsers.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void BarrierData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "BarrierData");
    type_ = XMLUtils::getChildValue(node, "Type", true);
    style_ = XMLUtils::getChildValue(node, "Style", false);
    XMLNode* levelData = XMLUtils::getChildNode(node, "LevelData");
    if (levelData) {
        vector<XMLNode*> lvls = XMLUtils::getChildrenNodes(levelData, "Level");
        for (auto& n : lvls) {
            TradeBarrier barrier;
            barrier.fromXML(n);
            tradeBarriers_.push_back(barrier);
            levels_.push_back(XMLUtils::getChildValueAsDouble(n, "Value", true));
        }
    } else {
        levels_ = XMLUtils::getChildrenValuesAsDoubles(node, "Levels", "Level");
        for (Size i = 0; i < levels_.size(); i++)
            tradeBarriers_.push_back(TradeBarrier(levels_[i], ""));
    }
    rebate_ = XMLUtils::getChildValueAsDouble(node, "Rebate", false);
    rebateCurrency_ = XMLUtils::getChildValue(node, "RebateCurrency", false);
    rebatePayTime_ = XMLUtils::getChildValue(node, "RebatePayTime", false);
    initialized_ = true;
}

XMLNode* BarrierData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("BarrierData");
    XMLUtils::addChild(doc, node, "Type", type_);
    if (!style_.empty())
        XMLUtils::addChild(doc, node, "Style", style_);
    XMLUtils::addChild(doc, node, "Rebate", rebate_);
    XMLUtils::addChildren(doc, node, "Levels", "Level", levels_);
    if (!rebateCurrency_.empty())
        XMLUtils::addChild(doc, node, "RebateCurrency", rebateCurrency_);
    if (!rebatePayTime_.empty())
        XMLUtils::addChild(doc, node, "RebatePayTime", rebatePayTime_);
    return node;
}
} // namespace data
} // namespace oreplus
