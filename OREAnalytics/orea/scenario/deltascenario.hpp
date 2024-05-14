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

/*! \file scenario/deltascenario.hpp
    \brief Delta scenario class
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenario.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;

//-----------------------------------------------------------------------------------------------
//! Delta Scenario class
/*! This implementation stores a pointer to a "base scenario", as well as a smaller "delta" scenario,
  which stores values for any keys where the value is different to base.
  This is intended as an efficient storage mechanism for e.g. sensitivity scenarios,
  where many scenario instances are managed in memory but the actual differences are minor.

  \ingroup scenario
*/
class DeltaScenario : public virtual ore::analytics::Scenario {
public:
    //! Constructor
    DeltaScenario() {}
    //! Constructor
    DeltaScenario(const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& baseScenario,
                  const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& incrementalScenario);

    const Date& asof() const override { return delta_->asof(); }
    void setAsof(const Date& d) override { delta_->setAsof(d); }

    //! Return the scenario label
    const std::string& label() const override { return delta_->label(); }
    //! set the label
    void label(const string& s) override { delta_->label(s); }

    //! Get Numeraire ratio n = N(t) / N(0) so that Price(0) = N(0) * E [Price(t) / N(t) ]
    Real getNumeraire() const override;
    //! Set the Numeraire ratio n = N(t) / N(0) so that Price(0) = N(0) * E [Price(t) / N(t) ]
    void setNumeraire(Real n) override { delta_->setNumeraire(n); }

    //! Check, get, add a single market point
    bool has(const ore::analytics::RiskFactorKey& key) const override { return baseScenario_->has(key); }
    const std::vector<ore::analytics::RiskFactorKey>& keys() const override { return baseScenario_->keys(); }
    void add(const ore::analytics::RiskFactorKey& key, Real value) override;
    Real get(const ore::analytics::RiskFactorKey& key) const override;

    bool isAbsolute() const override { return baseScenario_->isAbsolute(); }
    void setAbsolute(const bool b) override { baseScenario_->setAbsolute(b); }

    const std::map<std::pair<RiskFactorKey::KeyType, std::string>, std::vector<std::vector<Real>>>&
    coordinates() const override {
        return baseScenario_->coordinates();
    }

    //! Get base
    QuantLib::ext::shared_ptr<Scenario> base() const { return baseScenario_; }

    //! Get delta
    QuantLib::ext::shared_ptr<Scenario> delta() const { return delta_; }

    QuantLib::ext::shared_ptr<Scenario> clone() const override;

    bool isCloseEnough(const QuantLib::ext::shared_ptr<Scenario>& s) const override;

protected:
    QuantLib::ext::shared_ptr<Scenario> baseScenario_;
    QuantLib::ext::shared_ptr<Scenario> delta_;
};

} // namespace sensitivity
} // namespace ore
