/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <orea/scenario/scenarioutilities.hpp>

#include <orea/scenario/scenario.hpp>

namespace ore {
namespace analytics {

Real getDifferenceScenario(const RiskFactorKey::KeyType keyType, const Real v1, const Real v2) { return 0.0; }

Real addDifferenceToScenario(const RiskFactorKey::KeyType keyType, const Real v, const Real d) { return 0.0; }

QuantLib::ext::shared_ptr<Scenario> getDifferenceScenario(const QuantLib::ext::shared_ptr<Scenario>& s1,
                                                          const QuantLib::ext::shared_ptr<Scenario>& s2,
                                                          const Date& targetScenarioAsOf,
                                                          const Real targetScenarioNumeraire) {

    QL_REQUIRE(s1->isAbsolute() && s2->isAbsolute(), "getDifferenceScenario(): both scenarios must be absolute ("
                                                         << std::boolalpha << s1->isAbsolute() << ", "
                                                         << s2->isAbsolute());

    QL_REQUIRE(s1->keysHash() == s2->keysHash(),
               "getDifferenceScenario(): both scenarios must have identical key sets");

    Date asof = targetScenarioAsOf;
    if (asof == Date() && s1->asof() == s2->asof())
        asof = s1->asof();

    QL_REQUIRE(asof != Date(), "getDifferenceScenario(): either both scenarios have to have the same asof date ("
                                   << s1->asof() << ", " << s2->asof()
                                   << ") or the target scenario asof date must be given.");

    auto result = s1->clone();
    result->setAsof(asof);
    result->label("differenceScenario(" + s1->label() + "," + s2->label());
    result->setNumeraire(targetScenarioNumeraire);
    result->setAbsolute(false);

    for (auto const& k : s1->keys()) {
        result->add(k, getDifferenceScenario(k.keytype, s2->get(k), s1->get(k)));
    }

    return result;
}

QuantLib::ext::shared_ptr<Scenario> addDifferenceToScenario(const QuantLib::ext::shared_ptr<Scenario>& s,
                                                            const QuantLib::ext::shared_ptr<Scenario>& d,
                                                            const Date& targetScenarioAsOf,
                                                            const Real targetScenarioNumeraire) {

    QL_REQUIRE(!d->isAbsolute(), "addDifferenceToScenario(): second argument must be difference scenario");
    QL_REQUIRE(s->keysHash() == d->keysHash(),
               "addDifferenceToScenario(): both scenarios must have identical key sets.");

    Date asof = targetScenarioAsOf;
    if (asof == Date() && s->asof() == d->asof())
        asof = s->asof();

    QL_REQUIRE(asof != Date(), "addDifferenceToScenario(): either both scenarios have to have the same asof date ("
                                   << s->asof() << ", " << d->asof()
                                   << ") or the target scenario asof date must be given.");

    auto result = s->clone();
    result->setAsof(asof);
    result->label("sumScenario(" + s->label() + "," + d->label());
    result->setNumeraire(targetScenarioNumeraire);
    result->setAbsolute(s->isAbsolute());

    for (auto const& k : s->keys()) {
        result->add(k, addDifferenceToScenario(k.keytype, s->get(k), d->get(k)));
    }

    return result;
}

QuantLib::ext::shared_ptr<Scenario> recastScenario(
    const QuantLib::ext::shared_ptr<Scenario>& scenario,
    const std::map<std::pair<RiskFactorKey::KeyType, std::string>, std::vector<std::vector<Real>>>& oldCoordinates,
    const std::map<std::pair<RiskFactorKey::KeyType, std::string>, std::vector<std::vector<Real>>>& newCoordinates) {
    return {};
}

} // namespace analytics
} // namespace ore
