/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file scenario/scenario.hpp
    \brief Scenario class
    \ingroup scenario
*/

#pragma once

#include <ored/utilities/serializationdate.hpp>

#include <ql/shared_ptr.hpp>
#include <ql/math/array.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>

#include <boost/functional/hash.hpp>
#include <map>
#include <unordered_map>
#include <vector>
namespace ore {
namespace analytics {
using QuantLib::Array;
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;
using std::string;

//! Data types stored in the scenario class
/*! \ingroup scenario
 */
class RiskFactorKey {
public:
    //! Risk Factor types
    enum class KeyType {
        None,
        DiscountCurve,
        YieldCurve,
        IndexCurve,
        SwaptionVolatility,
        YieldVolatility,
        OptionletVolatility,
        FXSpot,
        FXVolatility,
        EquitySpot,
        DividendYield,
        EquityVolatility,
        SurvivalProbability,
        SurvivalWeight,
        RecoveryRate,
        CreditState,
        CDSVolatility,
        BaseCorrelation,
        CPIIndex,
        ZeroInflationCurve,
        ZeroInflationCapFloorVolatility,
        YoYInflationCurve,
        YoYInflationCapFloorVolatility,
        CommodityCurve,
        CommodityVolatility,
        SecuritySpread,
        Correlation,
        CPR
    };

    //! Constructor
    RiskFactorKey() : keytype(KeyType::None), name(""), index(0) {}
    //! Constructor
    RiskFactorKey(const KeyType& iKeytype, const string& iName, const Size& iIndex = 0)
        : keytype(iKeytype), name(iName), index(iIndex) {}

    //! Key type
    KeyType keytype;
    //! Key name.
    /*! For FX this is a pair ("EURUSD") for Discount or swaption it's just a currency ("EUR")
     *  and for an index it's the index name
     */
    std::string name;
    //! Index
    Size index;

private:
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int) {
        ar& keytype;
        ar& name;
        ar& index;
    }
};

std::size_t hash_value(const RiskFactorKey& k);

inline bool operator<(const RiskFactorKey& lhs, const RiskFactorKey& rhs) {
    return std::tie(lhs.keytype, lhs.name, lhs.index) < std::tie(rhs.keytype, rhs.name, rhs.index);
}

inline bool operator==(const RiskFactorKey& lhs, const RiskFactorKey& rhs) {
    return lhs.keytype == rhs.keytype && lhs.name == rhs.name && lhs.index == rhs.index;
}

inline bool operator>(const RiskFactorKey& lhs, const RiskFactorKey& rhs) { return rhs < lhs; }
inline bool operator<=(const RiskFactorKey& lhs, const RiskFactorKey& rhs) { return !(lhs > rhs); }
inline bool operator>=(const RiskFactorKey& lhs, const RiskFactorKey& rhs) { return !(lhs < rhs); }
inline bool operator!=(const RiskFactorKey& lhs, const RiskFactorKey& rhs) { return !(lhs == rhs); }

std::ostream& operator<<(std::ostream& out, const RiskFactorKey::KeyType& type);
std::ostream& operator<<(std::ostream& out, const RiskFactorKey& key);

RiskFactorKey::KeyType parseRiskFactorKeyType(const string& str);
RiskFactorKey parseRiskFactorKey(const string& str);

//-----------------------------------------------------------------------------------------------
//! Scenario Base Class
/*! A scenario contains a single cross asset model sample in terms of
  yield curves by currency, FX rates, etc.

  This base class provides the interface to add and retrieve data to and from a scenario.
  Concrete simple and memory optimized "compact" scenario classes are derived from this.

  \ingroup scenario
*/
class Scenario {
public:
    //! Destructor
    virtual ~Scenario() {}

    //! Return the scenario asof date
    virtual const Date& asof() const = 0;
    //! Set the asof date
    virtual void setAsof(const Date& d) = 0;

    //! Get the scenario label
    virtual const string& label() const = 0;
    //! Set the scenario label
    virtual void label(const string&) = 0;

    //! Get Numeraire ratio n = N(t) / N(0) so that Price(0) = N(0) * E [Price(t) / N(t) ]
    virtual Real getNumeraire() const = 0;
    //! Set the Numeraire ratio n = N(t) / N(0) so that Price(0) = N(0) * E [Price(t) / N(t) ]
    virtual void setNumeraire(Real n) = 0;

    //! Check whether this scenario provides the data for the given key
    virtual bool has(const RiskFactorKey& key) const = 0;
    //! Risk factor keys for which this scenario provides data
    virtual const std::vector<RiskFactorKey>& keys() const = 0;
    //! Add an element to the scenario
    virtual void add(const RiskFactorKey& key, Real value) = 0;
    //! Get an element from the scenario
    virtual Real get(const RiskFactorKey& key) const = 0;

    //! Is this an absolute or difference scenario?
    virtual bool isAbsolute() const = 0;
    //! Set if this is an absolute scenario
    virtual void setAbsolute(const bool b) = 0;
    //! Get coordinates
    virtual const std::map<std::pair<RiskFactorKey::KeyType, std::string>, std::vector<std::vector<Real>>>&
    coordinates() const = 0;

    //! clones a scenario and returns a pointer to the new object
    virtual QuantLib::ext::shared_ptr<Scenario> clone() const = 0;

    //! checks for equality up to numerical differences
    virtual bool isCloseEnough(const QuantLib::ext::shared_ptr<Scenario>& s) const;

    //! return fingerprint identifying the set of rf keys of the scenarios, or 0 if not provided by the implementation
    virtual std::size_t keysHash() const { return 0; }

private:
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive&, const unsigned int) {}
};

enum class ShiftScheme { Forward, Backward, Central };
enum class ShiftType { Absolute, Relative };

ShiftScheme parseShiftScheme(const std::string& s);
std::ostream& operator<<(std::ostream& out, const ShiftScheme& shiftScheme);

ShiftType parseShiftType(const std::string& s);
std::ostream& operator<<(std::ostream& out, const ShiftType& shiftType);

} // namespace analytics
} // namespace ore

template <> 
struct std::hash<ore::analytics::RiskFactorKey> {
    std::size_t operator()(const ore::analytics::RiskFactorKey& k) const { return hash_value(k); }
};
