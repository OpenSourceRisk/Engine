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

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/utilities/null.hpp>

namespace ore {
namespace data {

enum class CounterpartyCreditQuality {
                        IG, // Investment Grade
                        HY, // High Yield
                        NR // Not Rated 
                    };


//! Counterparty Information
/*!
This class is a container for information on a counterparty


\ingroup tradedata
*/
class CounterpartyInformation : public ore::data::XMLSerializable {
public:
    CounterpartyInformation(ore::data::XMLNode* node);

    CounterpartyInformation(std::string counterpartyId, bool isClearingCP = false, 
        CounterpartyCreditQuality creditQuality = CounterpartyCreditQuality::NR, 
        QuantLib::Real baCvaRiskWeight = QuantLib::Null<QuantLib::Real>(),
        QuantLib::Real saCcrRiskWeight = QuantLib::Null<QuantLib::Real>(), std::string saCvaRiskBucket = "" ) :
        counterpartyId_(counterpartyId), isClearingCP_(isClearingCP), creditQuality_(creditQuality), 
        baCvaRiskWeight_(baCvaRiskWeight), saCcrRiskWeight_(saCcrRiskWeight), saCvaRiskBucket_(saCvaRiskBucket) {}

    //! loads NettingSetDefinition object from XML
    void fromXML(ore::data::XMLNode* node) override;

    //! writes object to XML
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    //! Inspectors
    //@{
    /*! returns counterparty Id */
    const std::string& counterpartyId() const { return counterpartyId_; }
    /*! returns if clearing counterparty */
    bool isClearingCP() const { return isClearingCP_; }
    /*! returns counterparty credit quality */
    const CounterpartyCreditQuality& creditQuality() const { return creditQuality_; }
    /*! returns BA CVA risk weight*/
    QuantLib::Real baCvaRiskWeight() const { return baCvaRiskWeight_; }
    /*! returns SA CCR risk weight*/
    QuantLib::Real saCcrRiskWeight() const { return saCcrRiskWeight_; }
    /*! returns returns counterparty risk bucket*/
    const std::string& saCvaRiskBucket() const { return saCvaRiskBucket_; }
    //@}

private:
    std::string counterpartyId_;
    bool isClearingCP_;
    CounterpartyCreditQuality creditQuality_;
    QuantLib::Real baCvaRiskWeight_ = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real saCcrRiskWeight_ = QuantLib::Null<QuantLib::Real>();
    std::string saCvaRiskBucket_;
};

std::ostream& operator<<(std::ostream& out, const CounterpartyCreditQuality& cq);

CounterpartyCreditQuality parseCounterpartyCreditQuality(const std::string& cq);


} // data
} // ore
