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

/*! \file scenario/simplescenario.hpp
    \brief Simple scenario class
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenario.hpp>

#include <boost/serialization/export.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>

namespace ore {
namespace analytics {
using std::string;

//-----------------------------------------------------------------------------------------------
//! Simple Scenario class
/*! This implementation stores the data in several maps within in each scenario class instance.
  This contains redundant information (keys) across scenario instances which can cause issues
  when many scenario instances are managed in memory.

  \ingroup scenario
*/
class SimpleScenario : public Scenario {
public:
    //! Constructor
    SimpleScenario() {}
    //! Constructor
    SimpleScenario(Date asof, const std::string& label = "", Real numeraire = 0)
        : asof_(asof), numeraire_(numeraire), label_(label) {}

    //! Return the scenario asof date
    const Date& asof() const override { return asof_; }

    //! Return the scenario label
    const std::string& label() const override { return label_; }
    //! set the label
    void label(const string& s) override { label_ = s; }

    //! Get Numeraire ratio n = N(t) / N(0) so that Price(0) = N(0) * E [Price(t) / N(t) ]
    Real getNumeraire() const override { return numeraire_; }
    //! Set the Numeraire ratio n = N(t) / N(0) so that Price(0) = N(0) * E [Price(t) / N(t) ]
    void setNumeraire(Real n) override { numeraire_ = n; }

    //! Check, get, add a single market point
    bool has(const RiskFactorKey& key) const override;
    const std::vector<RiskFactorKey>& keys() const override { return keys_; }
    void add(const RiskFactorKey& key, Real value) override;
    Real get(const RiskFactorKey& key) const override;

    QuantLib::ext::shared_ptr<Scenario> clone() const override;

    //! get data map
    const std::map<RiskFactorKey, Real> data() const { return data_; }

private:
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Scenario>(*this);
        ar& asof_;
        ar& numeraire_;
        ar& data_;
        ar& keys_;
        ar& label_;
    }
    Date asof_;
    Real numeraire_;
    std::map<RiskFactorKey, Real> data_;
    std::vector<RiskFactorKey> keys_;
    std::string label_;
};
} // namespace analytics
} // namespace ore
