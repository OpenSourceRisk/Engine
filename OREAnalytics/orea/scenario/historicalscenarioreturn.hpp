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

/*! \file orea/scenario/historicalscenarioreturn.hpp
    \brief scenario returns from historical shifts
    \ingroup scenario
 */

#pragma once

#include <boost/make_shared.hpp>
#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenarioloader.hpp>
#include <orea/scenario/scenarioreader.hpp>
#include <orea/scenario/scenariosimmarket.hpp>


namespace ore {
namespace analytics {

//! Return type for historical scenario generation (absolute, relative, log)
class ReturnConfiguration : public XMLSerializable {
public:
    enum class ReturnType { Absolute, Relative, Log };

    struct Return {
        ReturnType type;
        double displacement;
    };

    using RiskFactorReturnConfig = std::map<std::pair<RiskFactorKey::KeyType, std::string>, Return>;

    //! default return types per risk factor
    ReturnConfiguration();

    //! customised return types per risk factor
    explicit ReturnConfiguration(const std::map<RiskFactorKey::KeyType, ReturnType>& returnType);

    explicit ReturnConfiguration(const RiskFactorReturnConfig& configs);

    /*! Compute return from v1, v2.
        The date parameters are are used to improve the log messages
    */
    QuantLib::Real returnValue(const RiskFactorKey& key, const QuantLib::Real v1, const QuantLib::Real v2,
                               const QuantLib::Date& d1, const QuantLib::Date& d2) const;

    //! apply return from v1, v2 to base value
    QuantLib::Real applyReturn(const RiskFactorKey& key, const QuantLib::Real baseValue,
                               const QuantLib::Real returnValue) const;

    //! get return types
    const Return& returnType(const RiskFactorKey& key) const;

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    RiskFactorReturnConfig returnType_;

    //! Perform checks on key
    void check(const RiskFactorKey& key) const;
};

std::ostream& operator<<(std::ostream& out, const ReturnConfiguration::ReturnType t);

} // namespace analytics
} // namespace ore
