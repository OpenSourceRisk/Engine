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

#include <ored/portfolio/optionexercisedata.hpp>

#include <ored/utilities/parsers.hpp>

using namespace QuantLib;
using std::ostream;
using std::string;
using std::vector;

namespace ore {
namespace data {

OptionExerciseData::OptionExerciseData() : price_(Null<Real>()) {}

OptionExerciseData::OptionExerciseData(const string& date, const string& price) : strDate_(date), strPrice_(price) {
    init();
}

void OptionExerciseData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ExerciseData");
    strDate_ = XMLUtils::getChildValue(node, "Date", true);
    strPrice_ = XMLUtils::getChildValue(node, "Price", false);
    init();
}

XMLNode* OptionExerciseData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ExerciseData");
    XMLUtils::addChild(doc, node, "Date", strDate_);
    if (!strPrice_.empty())
        XMLUtils::addChild(doc, node, "Price", strPrice_);
    return node;
}

void OptionExerciseData::init() {
    date_ = parseDate(strDate_);
    if (!strPrice_.empty())
        price_ = parseReal(strPrice_);
}

} // namespace data
} // namespace ore
