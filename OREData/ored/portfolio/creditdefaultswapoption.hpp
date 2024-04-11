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

/*! \file ored/portfolio/creditdefaultswapoption.hpp
    \brief credit default swap option trade data model and serialization
    \ingroup tradedata
 */

#pragma once

#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/portfolio/optiondata.hpp>
#include <boost/optional/optional.hpp>

/*! Serializable Credit Default Swap Option
    \ingroup tradedata
*/
namespace ore {
namespace data {

class CreditDefaultSwapOption : public Trade {
public:
    /*! Hold information on a default that has occurred and for which an auction has been held.

        If the CDS option has knockout set to false, a default payment will be made on expiry of the option in the 
        event of a default. Also, if knockout is set to true, we would still need to know this amount between the 
        auction date and the auction settlement date, typically 3 business days, to assign a value to the option 
        trade. Between the default date and the auction date, the recovery rate still trades so there should be enough 
        information in the market data to price the trade using the CDS option engine.
    */
    class AuctionSettlementInformation : public XMLSerializable {
    public:
        //! Default constructor
        AuctionSettlementInformation();

        //! Detailed constructor
        AuctionSettlementInformation(const QuantLib::Date& auctionSettlementDate,
            QuantLib::Real& auctionFinalPrice);

        //! \name Inspectors
        //@{
        const QuantLib::Date& auctionSettlementDate() const;
        QuantLib::Real auctionFinalPrice() const;
        //@}

        //! \name Serialisation
        //@{
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
        //@}

    private:
        QuantLib::Date auctionSettlementDate_;
        QuantLib::Real auctionFinalPrice_;
    };

    //! Default constructor
    CreditDefaultSwapOption();

    //! Constructor
    CreditDefaultSwapOption(const Envelope& env,
        const OptionData& option,
        const CreditDefaultSwapData& swap,
        QuantLib::Real strike = QuantLib::Null<QuantLib::Real>(),
        const std::string& strikeType = "Spread",
        bool knockOut = true,
        const std::string& term = "",
        const boost::optional<AuctionSettlementInformation>& asi = boost::none);

    //! \name Trade interface
    //@{
    void build(const QuantLib::ext::shared_ptr<EngineFactory>& ef) override;
    //@}

    //! \name Inspectors
    //@{
    const OptionData& option() const;
    const CreditDefaultSwapData& swap() const;
    QuantLib::Real strike() const;
    const std::string& strikeType() const;
    bool knockOut() const;
    const std::string& term() const;
    const boost::optional<AuctionSettlementInformation>& auctionSettlementInformation() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    OptionData option_;
    CreditDefaultSwapData swap_;
    QuantLib::Real strike_;
    std::string strikeType_;
    bool knockOut_;
    std::string term_;
    boost::optional<AuctionSettlementInformation> asi_;

    //! Build CDS option given that no default
    void buildNoDefault(const QuantLib::ext::shared_ptr<EngineFactory>& ef);

    //! Build instrument given that default has occurred
    void buildDefaulted(const QuantLib::ext::shared_ptr<EngineFactory>& ef);

    //! Add the premium payment
    Date addPremium(const QuantLib::ext::shared_ptr<EngineFactory>& ef, const QuantLib::Currency& tradeCurrency,
                    const std::string& marketConfig,
                    std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>& additionalInstruments,
                    std::vector<QuantLib::Real>& additionalMultipliers);
};

}
}
