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

/*! \file orea/scenario/spreadscenario.hpp
    \brief Spread scenario class
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenario.hpp>

#include <boost/make_shared.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>

namespace ore {
namespace analytics {

/*! A spread scenario holds absolute values for all keys in the scenario and in addition
    spread values for a subset of the keys. It returns the spread value for a key if this
    is available, otherwise the absolute value. The absolute value can be retrieved via
    an the inspector getAbsoluteValue().

    When adding a value with add() this will add an absolute value. A spread can be added
    via addSpreadValue().

    Semantically, the absolute value should always be the base value, so that the base value
    combined with the spread given the absolute scenario value.
*/

class SpreadScenario : public virtual Scenario {
public:
    SpreadScenario() {}
    SpreadScenario(const boost::shared_ptr<Scenario>& absoluteValues, const boost::shared_ptr<Scenario>& spreadValues);

    //! Scenario interface
    const Date& asof() const override;
    const std::string& label() const override;
    void label(const string& s) override;
    Real getNumeraire() const override;
    void setNumeraire(Real n) override;
    bool has(const RiskFactorKey& key) const override;
    const std::vector<RiskFactorKey>& keys() const override;
    // adds absolute value
    void add(const RiskFactorKey& key, Real value) override;
    // gets spread value, if existent, otherwise absolute value
    Real get(const RiskFactorKey& key) const override;
    boost::shared_ptr<Scenario> clone() const override;

    // check if spread value exists for a key
    virtual bool hasSpreadValue(const RiskFactorKey& key) const;
    // add spread value
    virtual void addSpreadValue(const RiskFactorKey& key, Real value);
    // get absolute value
    virtual Real getAbsoluteValue(const RiskFactorKey& key) const;
    // get spread value (no fall back to absolute value as in get())
    virtual Real getSpreadValue(const RiskFactorKey& key) const;

private:
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Scenario>(*this);
        ar& absoluteValues_;
        ar& spreadValues_;
    }

    boost::shared_ptr<Scenario> absoluteValues_;
    boost::shared_ptr<Scenario> spreadValues_;
};

} // namespace analytics
} // namespace ore
