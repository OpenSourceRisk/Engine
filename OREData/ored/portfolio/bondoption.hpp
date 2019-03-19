/*
Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file portfolio/bondoption.hpp
\brief bond option data model and serialization
\ingroup tradedata
*/

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/optiondata.hpp> 

namespace ore {
    namespace data {

        //! Serializable Bond Option
        /*!
        \ingroup tradedata
        */
        class BondOption : public Trade {
        public:
            //! Default constructor
            BondOption() : Trade("BondOption"), zeroBond_(false) {}

            //! Constructor for option of coupon bonds
            BondOption(Envelope env, OptionData option, double strike, double quantity,
                string issuerId, string creditCurveId, string securityId, string referenceCurveId,
                string settlementDays, string calendar, string issueDate, LegData& coupons)
                : Trade("BondOption", env), option_(option), strike_(strike), quantity_(quantity),
                issuerId_(issuerId), creditCurveId_(creditCurveId), securityId_(securityId),
                referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
                issueDate_(issueDate), coupons_(std::vector<LegData>{coupons}), faceAmount_(0), maturityDate_(), currency_(),
                zeroBond_(false) {}

            //! Constructor for option of coupon bonds with multiple phases (represented as legs)
            BondOption(Envelope env, OptionData option, double strike, double quantity,
                string issuerId, string creditCurveId, string securityId, string referenceCurveId,
                string settlementDays, string calendar, string issueDate, const std::vector<LegData>& coupons)
                : Trade("BondOption", env), option_(option), strike_(strike), quantity_(quantity),
                issuerId_(issuerId), creditCurveId_(creditCurveId), securityId_(securityId),
                referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
                issueDate_(issueDate), coupons_(coupons), faceAmount_(0), maturityDate_(), currency_(), zeroBond_(false) {}

            //! Constructor for option of zero bonds
            BondOption(Envelope env, OptionData option, double strike, double quantity,
                string issuerId, string creditCurveId, string securityId, string referenceCurveId,
                string settlementDays, string calendar, Real faceAmount, string maturityDate, string currency,
                string issueDate)
                : Trade("BondOption", env), option_(option), strike_(strike), quantity_(quantity),
                issuerId_(issuerId), creditCurveId_(creditCurveId), securityId_(securityId),
                referenceCurveId_(referenceCurveId), settlementDays_(settlementDays), calendar_(calendar),
                issueDate_(issueDate), coupons_(), faceAmount_(faceAmount), maturityDate_(maturityDate), currency_(currency),
                zeroBond_(true) {}

            // Build QuantLib/QuantExt instrument, link pricing engine
            virtual void build(const boost::shared_ptr<EngineFactory>&);

            //! Return the fixings that will be requested to price the bond option given the \p settlementDate.
            std::map<std::string, std::set<QuantLib::Date>> fixings(
                const QuantLib::Date& settlementDate = QuantLib::Date()) const override;

            virtual void fromXML(XMLNode* node);
            virtual XMLNode* toXML(XMLDocument& doc);

            const OptionData& option() const { return option_; } 
            double strike() const { return strike_; } 
            double quantity() const { return quantity_; } 
            double redemption() const { return redemption_; }
            string priceType() const { return priceType_; }
            const string& issuerId() const { return issuerId_; }
            const string& creditCurveId() const { return creditCurveId_; }
            const string& securityId() const { return securityId_; }
            const string& referenceCurveId() const { return referenceCurveId_; }
            const string& settlementDays() const { return settlementDays_; }
            const string& calendar() const { return calendar_; }
            const string& issueDate() const { return issueDate_; }
            const std::vector<LegData>& coupons() const { return coupons_; }
            const Real& faceAmount() const { return faceAmount_; }
            const string& maturityDate() const { return maturityDate_; }
            const string& currency() const { return currency_; }

        protected:
            virtual boost::shared_ptr<LegData> createLegData() const;

        private:
            OptionData option_; 
            double strike_; 
            double quantity_; 
            double redemption_; 
            string priceType_;
            string issuerId_;
            string creditCurveId_;
            string securityId_;
            string referenceCurveId_;
            string settlementDays_;
            string calendar_;
            string issueDate_;
            std::vector<LegData> coupons_;
            Real faceAmount_;
            string maturityDate_;
            string currency_;
            bool zeroBond_;
            //! Store the name of the underlying floating leg index
            std::string underlyingIndex_;
        };
    } // namespace data
} // namespace ore


