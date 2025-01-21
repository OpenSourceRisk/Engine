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

/*! \file portfolio/bondfuture.hpp
 \brief Bond trade data model and serialization
 \ingroup tradedata
 */
#pragma once

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/trade.hpp>

enum FutureType { UB, ZB, TWE, TN, ZN, ZF, Z3N, ZT };

//                      Deliverable Maturities	    CME Globex  Bloomberg
// 2-Year T-Note	        1 3/4 to 2 years	        ZT          TU
// 3-Year T-Note         9/12 to 3 years	            Z3N         3Y
// 5-Year T-Note	        4 1/6 to 5 1/4 years	    ZF          FV
// 10-Year T-Note	    6 1/2 to 8 years	        ZN          TY
// Ultra 10-Year T-Note	9 5/12 to 10 Years	        TN          UXY
// T-Bond	            15 years up to 25 years	    ZB          US
// 20-Year T-Bond	    19 2/12 to 19 11/12 years	TWE         TWE
// Ultra T-Bond	        25 years to 30 years	    UB          WN
// source: https://www.cmegroup.com/trading/interest-rates/basics-of-us-treasury-futures.html

namespace ore {
namespace data {

class BondFuture : public Trade {
public:
    //! Default constructor
    BondFuture() : Trade("BondFuture") {}

    //! Constructor taking an envelope
    BondFuture(Envelope env, const std::vector<std::string>& secList)
        : Trade("BondFuture", env), secList_(secList) {}

    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    //! Add underlying Bond names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    //! inspectors
    const std::vector<std::string>& secList() const { return secList_; }


protected:

    BondData identifyCtdBond(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory);

    std::vector<std::string> secList_;
    std::string contractName_;
    double notional_;
    //optional
    std::string expiry_;
    std::string underlyingType_; //futureType differentiating the underlying


};
} // namespace data
} // namespace ore
