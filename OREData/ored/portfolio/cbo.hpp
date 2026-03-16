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

/*! \file portfolio/cbo.hpp
    \brief collateralized bond obligation data model
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/trade.hpp>
#include <qle/instruments/cbo.hpp>
#include <ored/portfolio/bondbasket.hpp>
#include <ored/portfolio/trsunderlyingbuilder.hpp>
#include <ored/portfolio/tranche.hpp>
#include <ored/portfolio/referencedata.hpp>

namespace ore {
namespace data {

class CboReferenceDatum : public ReferenceDatum {
public:
    static constexpr const char* TYPE = "CBO";
    struct CboStructure : XMLSerializable {

        BondBasket bondbasketdata;
        std::string feeDayCounter;
        std::string seniorFee;
        std::string subordinatedFee;
        std::string equityKicker;
        std::string ccy;
        std::string reinvestmentEndDate;
        std::vector<QuantLib::ext::shared_ptr<TrancheData>> trancheData;
        ScheduleData scheduleData;
        std::string daycounter;
        std::string paymentConvention;

        void fromXML(XMLNode* node) override;
        XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    };

    CboReferenceDatum() { setType(TYPE); }
    CboReferenceDatum(const string& id) : ReferenceDatum(TYPE, id) {}
    CboReferenceDatum(const string& id, const CboStructure& cboStructure) : ReferenceDatum(TYPE, id), cboStructure_(cboStructure) {}

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    const CboStructure& cbostructure() const { return cboStructure_; }
    void setCboStructure(const CboStructure& cboStructure) { cboStructure_ = cboStructure; }

private:
    CboStructure cboStructure_;
};

class CBO : public Trade {
public:
    CBO() : Trade("CBO") {}

    //! Add underlying Bond names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    // Trade interface
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    QuantLib::Real notional() const override { return notional_; }
    std::string notionalCurrency() const override { return ccy_; }

    std::string investedTrancheName() { return investedTrancheName_; };
    const BondBasket& bondBasketData() { return bondbasketdata_; }
    double underlyingMultiplier() { return multiplier_; }

protected:
    std::map<std::string, std::set<Date>> fixings_;

private:

    void populateFromCboReferenceData(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager);
    void populateFromCboReferenceData(const QuantLib::ext::shared_ptr<CboReferenceDatum>& cboReferenceDatum);
    void validateCbo();

    QuantLib::ext::shared_ptr<QuantExt::BondBasket> bondbasket_;
    BondBasket bondbasketdata_;
    std::string feeDayCounter_;
    std::string seniorFee_;
    std::string subordinatedFee_;
    std::string equityKicker_;
    std::string ccy_;
    std::string reinvestmentEndDate_;
    std::string investedTrancheName_;
    std::vector<QuantLib::ext::shared_ptr<TrancheData>> trancheData_;
    ScheduleData scheduleData_;
    std::string daycounter_;
    std::string paymentConvention_;
    double investedNotional_;
    std::string structureId_;
    double multiplier_;

};

struct CBOTrsUnderlyingBuilder : public TrsUnderlyingBuilder {
    void
    build(const std::string& parentId, const QuantLib::ext::shared_ptr<Trade>& underlying,
          const std::vector<Date>& valuationDates, const std::vector<Date>& paymentDates,
          const std::string& fundingCurrency, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
          QuantLib::ext::shared_ptr<QuantLib::Index>& underlyingIndex, Real& underlyingMultiplier,
          std::map<std::string, double>& indexQuantities, std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndices,
          Real& initialPrice, std::string& assetCurrency, std::string& creditRiskCurrency,
          std::map<std::string, SimmCreditQualifierMapping>& creditQualifierMapping,
          const std::function<QuantLib::ext::shared_ptr<QuantExt::FxIndex>(
              const QuantLib::ext::shared_ptr<Market> market, const std::string& configuration, const std::string& domestic,
              const std::string& foreign, std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndices)>&
              getFxIndex,
          const std::string& underlyingDerivativeId, RequiredFixings& fixings, std::vector<Leg>& returnLegs) const override;
};

} // namespace data
} // namespace ore
