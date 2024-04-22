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

/*! \file portfolio/balanceguaranteedswap.hpp
    \brief Balance Guaranteed Swap data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! Serializable Tranche for use in Balance Guaranteed Swaps
/*!
  \ingroup tradedata
*/
class BGSTrancheData : public ore::data::XMLSerializable {
public:
    BGSTrancheData() {}
    BGSTrancheData(const std::string& description, const std::string& securityId, const int seniority,
                   const std::vector<QuantLib::Real>& notionals, const std::vector<std::string>& notionalDates)
        : description_(description), securityId_(securityId), seniority_(seniority), notionals_(notionals),
          notionalDates_(notionalDates) {}

    //! \name Inspectors
    //@{
    const std::string& description() const { return description_; }
    const std::string& securityId() const { return securityId_; }
    int seniority() const { return seniority_; }
    const std::vector<QuantLib::Real>& notionals() const { return notionals_; }
    const std::vector<std::string>& notionalDates() const { return notionalDates_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::string description_;
    std::string securityId_;
    int seniority_;
    std::vector<QuantLib::Real> notionals_;
    std::vector<std::string> notionalDates_;
};

//! Serializable Balance Guaranteed Swap
/*!
  \ingroup tradedata
*/
class BalanceGuaranteedSwap : public ore::data::Trade {
public:
    BalanceGuaranteedSwap() : Trade("BalanceGuaranteedSwap") {}
    BalanceGuaranteedSwap(const ore::data::Envelope& env, const std::string& referenceSecurity,
                          const std::vector<BGSTrancheData>& tranches, const ore::data::Schedule schedule,
                          const std::vector<ore::data::LegData>& swap)
        : Trade("BalanceGuaranteedSwap", env), referenceSecurity_(referenceSecurity), tranches_(tranches), swap_(swap) {
    }

    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;

    //! \name Inspectors
    //@{
    const std::string& referenceSecurity() const { return referenceSecurity_; }
    const std::vector<BGSTrancheData>& tranches() const { return tranches_; }
    const ore::data::ScheduleData schedule() const { return schedule_; }
    const std::vector<ore::data::LegData>& swap() const { return swap_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    std::map<ore::data::AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>&) const override {
        return {{ore::data::AssetClass::BOND, {referenceSecurity_}}};
    }

private:
    std::string referenceSecurity_;
    std::vector<BGSTrancheData> tranches_;
    ore::data::ScheduleData schedule_;
    std::vector<ore::data::LegData> swap_;
};

} // namespace data
} // namespace ore
