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

/*! \file callablebond.hpp
 \brief callable bond trade data model and serialization
 \ingroup tradedata
 */

#pragma once

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/referencedatamanager.hpp>

namespace ore {
namespace data {

//! Callable bond data
class CallableBondData : public ore::data::XMLSerializable {

public:
    //! Bond Callability Data
    class CallabilityData : public ore::data::XMLSerializable {
    public:
        explicit CallabilityData(const std::string& nodeName) : initialised_(false), nodeName_(nodeName) {}

        bool initialised() const { return initialised_; }

        const ScheduleData& dates() const { return dates_; }

        const std::vector<std::string>& styles() const { return styles_; }
        const std::vector<std::string>& styleDates() const { return styleDates_; }
        const std::vector<double>& prices() const { return prices_; }
        const std::vector<std::string>& priceDates() const { return priceDates_; }
        const std::vector<std::string>& priceTypes() const { return priceTypes_; }
        const std::vector<std::string>& priceTypeDates() const { return priceTypeDates_; }
        const std::vector<bool>& includeAccrual() const { return includeAccrual_; }
        const std::vector<std::string>& includeAccrualDates() const { return includeAccrualDates_; }

        void fromXML(ore::data::XMLNode* node) override;
        ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) override;

    private:
        bool initialised_;
        std::string nodeName_;

        ScheduleData dates_;
        std::vector<std::string> styles_;
        std::vector<std::string> styleDates_;
        std::vector<double> prices_;
        std::vector<std::string> priceDates_;
        std::vector<std::string> priceTypes_;
        std::vector<std::string> priceTypeDates_;
        std::vector<bool> includeAccrual_;
        std::vector<std::string> includeAccrualDates_;
    };

    // Convertible Bond Data

    CallableBondData() {}
    explicit CallableBondData(const ore::data::BondData& bondData) : bondData_(bondData) {}

    const ore::data::BondData& bondData() const { return bondData_; }

    const CallabilityData& callData() const { return callData_; }
    const CallabilityData& putData() const { return putData_; }

    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) override;

    void populateFromBondReferenceData(const boost::shared_ptr<ore::data::ReferenceDataManager>& referenceData);

private:
    ore::data::BondData bondData_;
    CallabilityData callData_ = CallabilityData("CallData");
    CallabilityData putData_ = CallabilityData("PutData");
};

//! Serializable callable bond
class CallableBond : public Trade {
public:
    //! Default constructor
    CallableBond() : Trade("CallableBond") {}

    //! Constructor for coupon bonds
    CallableBond(const Envelope& env, const CallableBondData& data)
        : Trade("CallableBond", env), originalData_(data), data_(data) {}

    void build(const boost::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;

    //! Add underlying Bond names
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    const CallableBondData& data() const { return data_; }
    const BondData& bondData() const { return data_.bondData(); }

private:
    CallableBondData originalData_;
    mutable CallableBondData data_;
};

struct CallableBondBuilder : public BondBuilder {
    static BondBuilderRegister<CallableBondBuilder> reg_;
    virtual Result build(const boost::shared_ptr<EngineFactory>& engineFactory,
                         const boost::shared_ptr<ReferenceDataManager>& referenceData,
                         const std::string& securityId) const override;
};

} // namespace data
} // namespace ore
