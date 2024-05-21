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

namespace ore {
namespace analytics {
using std::string;

/*! Simple Scenario class
    \ingroup scenario */
class SimpleScenario : public Scenario {
public:
    struct SharedData {
        std::vector<RiskFactorKey> keys;
        std::map<RiskFactorKey, std::size_t> keyIndex;
        std::map<std::pair<RiskFactorKey::KeyType, std::string>, std::vector<std::vector<Real>>> coordinates;
        std::size_t keysHash = 0;
    };

    SimpleScenario() {}
    //! if sharedData is not provided, the instance will create its own shared data block
    SimpleScenario(Date asof, const std::string& label = std::string(), Real numeraire = 0,
                   const boost::shared_ptr<SharedData>& sharedData = nullptr);

    const Date& asof() const override { return asof_; }
    void setAsof(const Date& d) override { asof_ = d; }

    const std::string& label() const override { return label_; }
    void label(const string& s) override { label_ = s; }

    Real getNumeraire() const override { return numeraire_; }
    void setNumeraire(Real n) override { numeraire_ = n; }

    bool isAbsolute() const override { return isAbsolute_; }
    const std::map<std::pair<RiskFactorKey::KeyType, std::string>, std::vector<std::vector<Real>>>&
    coordinates() const override {
        return sharedData_->coordinates;
    }

    std::size_t keysHash() const override { return sharedData_->keysHash; }

    bool has(const RiskFactorKey& key) const override;
    const std::vector<RiskFactorKey>& keys() const override { return sharedData_->keys; }
    void add(const RiskFactorKey& key, Real value) override;
    Real get(const RiskFactorKey& key) const override;

    //! This does _not_ close the shared data
    QuantLib::ext::shared_ptr<Scenario> clone() const override;

    void setAbsolute(const bool isAbsolute) override;
    void setCoordinates(const RiskFactorKey::KeyType type, const std::string& name,
                        const std::vector<std::vector<Real>>& coordinates);

    //! get shared data block (for construction of sister scenarios)
    const boost::shared_ptr<SharedData>& sharedData() const { return sharedData_; }

    //! get data, order is the same as in keys()
    const std::vector<Real>& data() const { return data_; }

private:
    QuantLib::ext::shared_ptr<SharedData> sharedData_;
    bool isAbsolute_ = true;
    Date asof_;
    std::string label_;
    Real numeraire_ = 0.0;
    std::vector<Real> data_;
};

} // namespace analytics
} // namespace ore
