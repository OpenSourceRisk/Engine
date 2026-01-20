/*
 Copyright (C) 2020 Quaternion Risk Management Ltd

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

/*! \file ored/portfolio/convertiblebond.hpp
 \brief Convertible Bond trade data model and serialization
 \ingroup tradedata
 */

#pragma once

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/convertiblebonddata.hpp>
#include <ored/portfolio/trsunderlyingbuilder.hpp>

namespace ore {
namespace data {

//! Serializable Convertible Bond
class ConvertibleBond : public Trade {
public:
    //! Default constructor
    ConvertibleBond() : Trade("ConvertibleBond") {}

    //! Constructor for coupon bonds
    ConvertibleBond(const Envelope& env, const ConvertibleBondData& data)
        : Trade("ConvertibleBond", env), originalData_(data), data_(data) {}

    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    //! Add underlying Bond names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    const ConvertibleBondData& data() const { return data_; }
    const BondData& bondData() const { return data_.bondData(); }

private:
    ConvertibleBondData originalData_;
    mutable ConvertibleBondData data_;
};

struct ConvertibleBondTrsUnderlyingBuilder : public TrsUnderlyingBuilder {
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
    void updateUnderlying(const QuantLib::ext::shared_ptr<ReferenceDataManager>& refData, QuantLib::ext::shared_ptr<Trade>& underlying,
                          const std::string& parentId) const override;
};

struct ConvertibleBondBuilder : public BondBuilder {
    BondBuilder::Result build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                              const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                              const std::string& securityId) const override;
};

} // namespace data
} // namespace ore
