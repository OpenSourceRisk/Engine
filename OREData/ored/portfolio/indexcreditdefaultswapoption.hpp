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

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/builders/indexcreditdefaultswap.hpp>
#include <ored/portfolio/indexcreditdefaultswapdata.hpp>

namespace ore {
namespace data {

class IndexCreditDefaultSwapOption : public Trade {
public:
    //! Default constructor
    IndexCreditDefaultSwapOption();

    //! Detailed constructor
    IndexCreditDefaultSwapOption(const ore::data::Envelope& env, const IndexCreditDefaultSwapData& swap,
                                 const ore::data::OptionData& option, QuantLib::Real strike,
                                 const std::string& indexTerm = "", const std::string& strikeType = "Spread",
                                 const QuantLib::Date& tradeDate = Date(), const QuantLib::Date& fepStartDate = Date());

    //! \name Trade
    //@{
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const IndexCreditDefaultSwapData& swap() const;
    const ore::data::OptionData& option() const;
    const std::string& indexTerm() const;
    QuantLib::Real strike() const;
    QuantLib::Option::Type callPut() const;
    const std::string& strikeType() const;
    const QuantLib::Date& tradeDate() const;
    const QuantLib::Date& fepStartDate() const;
    const CreditPortfolioSensitivityDecomposition sensitivityDecomposition() const;
    // only available after build()
    QuantLib::Real effectiveStrike() const;
    const std::string& effectiveStrikeType() const;
    const QuantLib::Period& effectiveIndexTerm() const;
    std::string creditCurveId() const;
    const std::string& volCurveId() const;
    const std::map<std::string, QuantLib::Real>& constituents() const;
    //@}

private:
    IndexCreditDefaultSwapData swap_;
    ore::data::OptionData option_;
    QuantLib::Real strike_;
    std::string indexTerm_;
    std::string strikeType_;
    QuantLib::Date tradeDate_;
    QuantLib::Date fepStartDate_;
    CreditPortfolioSensitivityDecomposition sensitivityDecomposition_;

    QuantLib::Real effectiveStrike_;
    std::string effectiveStrikeType_;
    QuantLib::Period effectiveIndexTerm_;
    std::string volCurveId_;

    //! Hold related notionals that are known on valuation date.
    struct Notionals {
        //! Notional assuming no defaults i.e. an index factor of 1. Equal to notional on \c swap_.
        QuantLib::Real full = 0.0;
        //! Outstanding index notional on the trade date of the index CDS option.
        QuantLib::Real tradeDate = 0.0;
        //! Outstanding index notional on the valuation date of the index CDS option.
        QuantLib::Real valuationDate = 0.0;
        //! The realised front end protection amount, as of the valuation date, that would be due on option exercise.
        QuantLib::Real realisedFep = 0.0;
    };

    //! Populated during trade building
    Notionals notionals_;

    //! map of all the constituents to notionals
    map<string, Real> constituents_;

    //! Populate constituent notionals and curve IDs from basket data
    void fromBasket(const QuantLib::Date& asof, std::map<std::string, QuantLib::Real>& constituents);

    //! Populate constituent notionals and curve IDs from reference data
    void fromReferenceData(const QuantLib::Date& asof, std::map<std::string, QuantLib::Real>& constituents,
                           const QuantLib::ext::shared_ptr<ReferenceDataManager>& refData);
};

} // namespace data
} // namespace ore
