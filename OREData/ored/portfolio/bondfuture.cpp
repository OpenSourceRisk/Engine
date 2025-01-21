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


#include <ored/portfolio/bondfuture.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/forwardbond.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void BondFuture::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("BondFuture::build() called for trade " << id());

    //choose between explicit data, deduction & reference data

    //identify CTD bond

    //build using forward bond


}

BondData BondFuture::identifyCtdBond(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    auto market = engineFactory->market();

    // get settlement price for future
    double sp;
    try {
        // TODO implement futureprice method
    } catch (const std::exception& e) {
        // MESSAGE: failed to retrieve future price quote
    }

    // loop secList retrieve bondprice (bp) and conversion factor (cf)
    // find lowest value for bp_i - sp x cf_i

    double lowestValue = 1e6; // arbitray high number
    string ctdSec = string();

    for (const auto& sec : secList_) {

        try {
            // TODO implement bondPrice method
            double bp = 1.0; // market->bondPrice(sec)->value();
            double cf = market->conversionFactor(sec)->value();
            //if conversion factor not given, deduce from method... 
            double value = bp - sp * cf;

            // store result
            if (value < lowestValue) {
                lowestValue = value;
                ctdSec = sec;
            }

        } catch (const std::exception& e) {
            // MESSAGE: failed to retrieve bond price quote or conversion factor for SECID
        }
    }
    // Check if results
    QL_REQUIRE(!ctdSec.empty(), "No CTD bond found.");

    // retrieve ctd bonddata ...
    BondData ctdBondData(ctdSec, notional_);
    ctdBondData.populateFromBondReferenceData(engineFactory->referenceData());

    return ctdBondData;

} // BondFuture::ctdBond

void BondFuture::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* bondFutureNode = XMLUtils::getChildNode(node, "BondFutureData");
    QL_REQUIRE(bondFutureNode, "No BondFutureData Node");

    contractName_ = XMLUtils::getChildValue(bondFutureNode, "ContractName", true);
    notional_ = XMLUtils::getChildValueAsDouble(bondFutureNode, "Notional", true);
    //optional
    expiry_ = XMLUtils::getChildValue(bondFutureNode, "Expiry", false, "");
    underlyingType_ = XMLUtils::getChildValue(bondFutureNode, "UnderlyingType", false, "");

    secList_.clear();
    XMLUtils::checkNode(bondFutureNode, "DeliveryBasket");

    //doubel check sthis 
    for (XMLNode* child = XMLUtils::getChildNode(node, "SecurityId"); child; child = XMLUtils::getNextSibling(child)) {
        secList_.push_back(XMLUtils::getChildValue(child, "SecurityId", true));
    }

}

} // namespace data
} // namespace ore
