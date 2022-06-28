/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#pragma once

#include <ql/experimental/fx/deltavolquote.hpp>
#include <ql/money.hpp>
#include <ql/option.hpp>
#include <ql/types.hpp>

#include <ored/utilities/xmlutils.hpp>
#include <ored/portfolio/trademonetary.hpp>

#include <boost/optional.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/version.hpp>
#include <boost/shared_ptr.hpp>

using ore::data::XMLNode;
using ore::data::XMLSerializable;

namespace ore {
namespace data {

class TradeBarrier : public TradeMonetary {

public:
    TradeBarrier() {};

    TradeBarrier(QuantLib::Real value, std::string currency) : TradeMonetary(value, currency) {};
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
};

} // namespace data
} // namespace ore
