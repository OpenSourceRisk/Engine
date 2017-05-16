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

/*! \file portfolio/capfloor.hpp
    \brief Ibor cap, floor or collar trade data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/basketdata.hpp>

namespace ore {
namespace data {

//! Serializable CDS Index Tranche (Synthetic CDO)
/*! \ingroup tradedata
*/
class SyntheticCDO : public Trade {
public:
    SyntheticCDO() : Trade("SyntheticCDO") {}
    SyntheticCDO(const Envelope& env, const LegData& leg, const string& qualifier, const BasketData& basketData,
                 const string& protectionStart, const string& upfrontDate, double upfrontFee, double attachmentPoint,
                 double detachmentPoint)
        : Trade("SyntheticCDO", env), qualifier_(qualifier), legData_(leg), basketData_(basketData),
          protectionStart_(protectionStart), upfrontDate_(upfrontDate), upfrontFee_(upfrontFee),
          attachmentPoint_(attachmentPoint), detachmentPoint_(detachmentPoint) {}

    virtual void build(const boost::shared_ptr<EngineFactory>&);

    //! Inspectors
    //@{
    const string& qualifier() const { return qualifier_; }
    const LegData& leg() const { return legData_; }
    const BasketData& basketData() const { return basketData_; }
    const string& protectionStart() const { return protectionStart_; }
    const string& upfrontDate() const { return upfrontDate_; }
    const double& upfrontFee() const { return upfrontFee_; }
    const double& attachmentPoint() const { return attachmentPoint_; }
    const double& detachmentPoint() const { return detachmentPoint_; }
    //@}

    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);

private:
    string qualifier_;
    LegData legData_;
    BasketData basketData_;
    string protectionStart_;
    string upfrontDate_;
    double upfrontFee_;
    double attachmentPoint_;
    double detachmentPoint_;
};
}
}
