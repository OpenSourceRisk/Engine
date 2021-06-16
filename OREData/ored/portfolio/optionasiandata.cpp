/*
  Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

 #include <ored/portfolio/optionasiandata.hpp>
 #include <ored/utilities/parsers.hpp>
 #include <ored/utilities/to_string.hpp>
 #include <algorithm>

 using QuantLib::Average;

 namespace ore {
 namespace data {

 OptionAsianData::OptionAsianData() : asianType_(AsianType::Price), averageType_(Average::Type::Arithmetic) {}

 OptionAsianData::OptionAsianData(const AsianType& asianType, const Average::Type& averageType)
     : asianType_(asianType), averageType_(averageType) {}

 void OptionAsianData::fromXML(XMLNode* node) {
     XMLUtils::checkNode(node, "AsianData");
     std::string strAsianType = XMLUtils::getChildValue(node, "AsianType", true);
     populateAsianType(strAsianType);
     averageType_ = parseAverageType(XMLUtils::getChildValue(node, "AverageType", true));
 }

 XMLNode* OptionAsianData::toXML(XMLDocument& doc) {
     XMLNode* node = doc.allocNode("AsianData");
     XMLUtils::addChild(doc, node, "AsianType", to_string(asianType_));
     XMLUtils::addChild(doc, node, "AverageType", to_string(averageType_));

     return node;
 }

 void OptionAsianData::populateAsianType(const std::string& s) {
     if (s == "Price") {
         asianType_ = AsianType::Price;
     } else if (s == "Strike") {
         asianType_ = AsianType::Strike;
     } else {
         QL_FAIL("expected AsianType Price or Strike.");
     }
 }

 std::ostream& operator<<(std::ostream& out, const OptionAsianData::AsianType& asianType) {
     switch (asianType) {
     case OptionAsianData::AsianType::Price:
         return out << "Price";
     case OptionAsianData::AsianType::Strike:
         return out << "Strike";
     default:
         QL_FAIL("Could not convert the asianType enum value to string.");
     }
 }

 } // namespace data
 } // namespace ore
