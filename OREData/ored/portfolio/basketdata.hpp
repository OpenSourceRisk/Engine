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

/*! \file ored/portfolio/basketdata.hpp
    \brief credit basket data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/creditdefaultswapdata.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>
#include <vector>

namespace ore {
namespace data {

/*! Serializable credit basket data constituent
    \ingroup tradedata
*/
class BasketConstituent : public ore::data::XMLSerializable {
public:

    //! Default constructor
    BasketConstituent();

    //! Constructor taking an explicit \p creditCurveId.
    BasketConstituent(const std::string& issuerName, const std::string& creditCurveId, QuantLib::Real notional,
                      const std::string& currency, const std::string& qualifier,
                      QuantLib::Real priorNotional = QuantLib::Null<QuantLib::Real>(),
                      QuantLib::Real recovery = QuantLib::Null<QuantLib::Real>(),
                      const QuantLib::Date& auctionDate = QuantLib::Date(),
                      const QuantLib::Date& auctionSettlementDate = QuantLib::Date(),
                      const QuantLib::Date& defaultDate = QuantLib::Date(),
                      const QuantLib::Date& eventDeterminationDate = QuantLib::Date());

    //! Constructor taking an explicit \p creditCurveId and initialized by weight.
    BasketConstituent(const std::string& issuerName, const std::string& creditCurveId, QuantLib::Real weight,
                      const std::string& qualifier,
                      QuantLib::Real priorWeight = QuantLib::Null<QuantLib::Real>(),
                      QuantLib::Real recovery = QuantLib::Null<QuantLib::Real>(),
                      const QuantLib::Date& auctionDate = QuantLib::Date(),
                      const QuantLib::Date& auctionSettlementDate = QuantLib::Date(),
                      const QuantLib::Date& defaultDate = QuantLib::Date(),
                      const QuantLib::Date& eventDeterminationDate = QuantLib::Date());

    //! Constructor taking CDS reference information, \p cdsReferenceInfo.
    BasketConstituent(const std::string& issuerName, const ore::data::CdsReferenceInformation& cdsReferenceInfo,
                      QuantLib::Real notional, const std::string& currency, const std::string& qualifier,
                      QuantLib::Real priorNotional = QuantLib::Null<QuantLib::Real>(),
                      QuantLib::Real recovery = QuantLib::Null<QuantLib::Real>(),
                      const QuantLib::Date& auctionDate = QuantLib::Date(),
                      const QuantLib::Date& auctionSettlementDate = QuantLib::Date(),
                      const QuantLib::Date& defaultDate = QuantLib::Date(),
                      const QuantLib::Date& eventDeterminationDate = QuantLib::Date());

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const std::string& issuerName() const;
    const std::string& creditCurveId() const;
    const boost::optional<ore::data::CdsReferenceInformation>& cdsReferenceInfo() const;
    QuantLib::Real notional() const;
    const std::string& currency() const;
    QuantLib::Real priorNotional() const;
    QuantLib::Real recovery() const;
    QuantLib::Real weight() const;
    QuantLib::Real priorWeight() const;
    const QuantLib::Date& auctionDate() const;
    const QuantLib::Date& auctionSettlementDate() const;
    const QuantLib::Date& defaultDate() const;
    const QuantLib::Date& eventDeterminationDate() const;
    bool weightInsteadOfNotional() const;
    //@}

private:
    
    std::string issuerName_;
    boost::optional<ore::data::CdsReferenceInformation> cdsReferenceInfo_;
    std::string creditCurveId_;
    QuantLib::Real notional_;
    std::string currency_;
    std::string qualifier_;
    QuantLib::Real priorNotional_;
    QuantLib::Real weight_;
    QuantLib::Real priorWeight_;
    QuantLib::Real recovery_;
    QuantLib::Date auctionDate_;
    QuantLib::Date auctionSettlementDate_;
    QuantLib::Date defaultDate_;
    QuantLib::Date eventDeterminationDate_;
    bool weightInsteadOfNotional_;

    
};

/*! Compare BasketConstituent instances using their credit curve ID.

    If credit curve ID is not enough here, we should construct a private key member variable in BasketConstituent and 
    make this operator a friend that uses the key.
*/
bool operator<(const BasketConstituent& lhs, const BasketConstituent& rhs);

/*! Serializable credit basket data
    \ingroup tradedata
*/
class BasketData : public ore::data::XMLSerializable {
public:
    //! Default constructor
    BasketData();

    //! Constructor taking explicit set of \p basket constituents.
    BasketData(const std::vector<BasketConstituent>& constituents);

    //! \name Inspectors
    //@{
    const std::vector<BasketConstituent>& constituents() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::vector<BasketConstituent> constituents_;
};

}
}
