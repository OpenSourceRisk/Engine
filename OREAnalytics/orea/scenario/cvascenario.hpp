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

#include <ql/errors.hpp>
#include <ql/types.hpp>
#include <ql/time/period.hpp>
#include <qle/utilities/serializationdate.hpp>
#include <map>
#include <set>

#pragma once

namespace ore {
namespace analytics {

//! Data types stored in the scenario class
/*! \ingroup scenario
    */
class CvaRiskFactorKey {
public:
    //! Risk Factor types
    enum class KeyType {
        None,
        InterestRate, 
        ForeignExchange,
        CreditCounterparty, // counterparty credit spreads
        CreditReference, // credit spreads that drive exposure
        Equity,
        Commodity
    };
    //! Margin Types
    enum class MarginType { 
        None, 
        Delta, 
        Vega 
    };

    typedef std::pair<CvaRiskFactorKey::KeyType, CvaRiskFactorKey::MarginType> CvaScenarioType;

    //! Constructor
    CvaRiskFactorKey() : keytype(KeyType::None), margintype(MarginType::None), name(""), period(QuantLib::Period()) {}
    //! Constructor
    CvaRiskFactorKey(const KeyType& iKeytype, const MarginType& iMargintype, const std::string& iName, const QuantLib::Period& iPeriod = QuantLib::Period())
        : keytype(iKeytype), margintype(iMargintype), name(iName), period(iPeriod) {}

    //! Key type
    KeyType keytype;
    //! Key name.
    /*! For ForeignExchange this is a pair ("EURUSD") for Discount or swaption it's just a currency ("EUR")
        *  and for an index it's the index name
        */
    MarginType margintype;
    std::string name;
    //! Index
    QuantLib::Period period;

private:
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int) {
        ar& keytype;
        ar& name;
        ar& index;
    }
};

inline bool operator<(const CvaRiskFactorKey& lhs, const CvaRiskFactorKey& rhs) {
    return std::tie(lhs.keytype, lhs.name, lhs.period) < std::tie(rhs.keytype, rhs.name, rhs.period);
}

inline bool operator==(const CvaRiskFactorKey& lhs, const CvaRiskFactorKey& rhs) {
    return lhs.keytype == rhs.keytype && lhs.name == rhs.name && lhs.period == rhs.period;
}

inline bool operator>(const CvaRiskFactorKey& lhs, const CvaRiskFactorKey& rhs) { return rhs < lhs; }
inline bool operator<=(const CvaRiskFactorKey& lhs, const CvaRiskFactorKey& rhs) { return !(lhs > rhs); }
inline bool operator>=(const CvaRiskFactorKey& lhs, const CvaRiskFactorKey& rhs) { return !(lhs < rhs); }
inline bool operator!=(const CvaRiskFactorKey& lhs, const CvaRiskFactorKey& rhs) { return !(lhs == rhs); }

std::ostream& operator<<(std::ostream& out, const CvaRiskFactorKey::KeyType& type);
std::ostream& operator<<(std::ostream& out, const CvaRiskFactorKey::MarginType& type);
std::ostream& operator<<(std::ostream& out, const CvaRiskFactorKey& key);

CvaRiskFactorKey::KeyType parseCvaRiskFactorKeyType(const std::string& str);
CvaRiskFactorKey::MarginType parseCvaRiskFactorMarginType(const std::string& str);
CvaRiskFactorKey parseCvaRiskFactorKey(const std::string& str);


class CvaScenario {
public:
    // Default constructor
    CvaScenario() {};

    virtual ~CvaScenario() {};

    void add(std::string id, QuantLib::Real value) {
        data_[id] = value;
        keys_.insert(id);
    }

    bool has(std::string id) const {
        auto it = data_.find(id);
        return it != data_.end();
    }

    virtual QuantLib::Real get(std::string id) const {
        auto it = data_.find(id);
        QL_REQUIRE(it != data_.end(), "Could not find id " << id << " in cva scenario.");
        return it->second;
    }
    const std::set<std::string>& keys() { return keys_; }

private:
    std::map<std::string, QuantLib::Real> data_;
    std::set<std::string> keys_;
};

class CvaShiftedScenario : public CvaScenario {
public:
    // Constructor
    CvaShiftedScenario(const QuantLib::ext::shared_ptr<CvaScenario>& baseScenario) : baseScenario_(baseScenario) {}

    QuantLib::Real get(std::string id) const override;
private:
    QuantLib::ext::shared_ptr<CvaScenario> baseScenario_;

};

} // namespace analytics
} // namespace ore
