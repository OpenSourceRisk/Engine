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

/*! \file ored/configuration/commodityvolcurveconfig.hpp
    \brief Commodity volatility curve configuration
    \ingroup configuration
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {

/*! Shared volatility configurations
    \ingroup configuration
*/
class VolatilityConfig : public ore::data::XMLSerializable {
};

/*! Volatility configuration for a single constant volatility
    \ingroup configuration
 */
class ConstantVolatilityConfig : public VolatilityConfig {
public:
    //! Default constructor
    ConstantVolatilityConfig();

    //! Explicit constructor
    ConstantVolatilityConfig(const std::string& quote);

    //! \name Inspectors
    //@{
    const std::string& quote() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) override;
    //@}

private:
    std::string quote_;
};

/*! Volatility configuration for a 1-D volatility curve
    \ingroup configuration
 */
class VolatilityCurveConfig : public VolatilityConfig {
public:
    //! Default constructor
    VolatilityCurveConfig();

    //! Explicit constructor
    VolatilityCurveConfig(
        const std::vector<std::string>& quotes,
        const std::string& interpolation,
        const std::string& extrapolation);

    //! \name Inspectors
    //@{
    const std::vector<std::string>& quotes() const;
    const std::string& interpolation() const;
    const std::string& extrapolation() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) override;
    //@}

private:
    std::vector<std::string> quotes_;
    std::string interpolation_;
    std::string extrapolation_;
};

/*! Base volatility configuration for a 2-D volatility surface
    \ingroup configuration
 */
class VolatilitySurfaceConfig : public VolatilityConfig {
public:
    //! Default constructor
    VolatilitySurfaceConfig();

    //! Explicit constructor
    VolatilitySurfaceConfig(
        const std::vector<std::string>& expiries,
        const std::string& timeInterpolation,
        const std::string& strikeInterpolation,
        bool extrapolation,
        const std::string& timeExtrapolation,
        const std::string& strikeExtrapolation);

    //! \name Inspectors
    //@{
    const std::vector<std::string>& expiries() const;
    const std::string& timeInterpolation() const;
    const std::string& strikeInterpolation() const;
    bool extrapolation() const;
    const std::string& timeExtrapolation() const;
    const std::string& strikeExtrapolation() const;
    //@}

    /*! Return a vector of pairs of expiry and strike. The first element in the pair is the expiry and the second 
        element in the pair is the string representation of the strike. This will be useful for building the vector 
        of quote strings in classes that have a VolatilitySurfaceConfig.
    */
    virtual std::vector<std::pair<std::string, std::string>> quotes() const = 0;

protected:
    /*! Populate members from the provided \p node. Can be called by fromXML in derived classes.
    */
    void fromNode(ore::data::XMLNode* node);

    /*! Add members to the provided \p node. Can be called by toXML in derived classes.
    */
    void addNodes(ore::data::XMLDocument& doc, ore::data::XMLNode* node) const;

private:
    std::vector<std::string> expiries_;
    std::string timeInterpolation_;
    std::string strikeInterpolation_;
    bool extrapolation_;
    std::string timeExtrapolation_;
    std::string strikeExtrapolation_;
};

/*! Volatility configuration for a 2-D absolute strike volatility surface
    \ingroup configuration
 */
class VolatilityStrikeSurfaceConfig : public VolatilitySurfaceConfig {
public:
    //! Default constructor
    VolatilityStrikeSurfaceConfig();

    //! Explicit constructor
    VolatilityStrikeSurfaceConfig(
        const std::vector<std::string>& strikes,
        const std::vector<std::string>& expiries,
        const std::string& timeInterpolation,
        const std::string& strikeInterpolation,
        bool extrapolation,
        const std::string& timeExtrapolation,
        const std::string& strikeExtrapolation);

    //! \name Inspectors
    //@{
    const std::vector<std::string>& strikes() const;
    //@}

    //! \name VolatilitySurfaceConfig
    //@{
    std::vector<std::pair<std::string, std::string>> quotes() const override;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) override;
    //@}

private:
    std::vector<std::string> strikes_;
};

/*! Volatility configuration for a 2-D delta volatility surface
    \ingroup configuration
 */
class VolatilityDeltaSurfaceConfig : public VolatilitySurfaceConfig {
public:
    //! Default constructor
    VolatilityDeltaSurfaceConfig();

    //! Explicit constructor
    VolatilityDeltaSurfaceConfig(
        const std::string& deltaType,
        const std::string& atmType,
        const std::vector<std::string>& putDeltas,
        const std::vector<std::string>& callDeltas,
        const std::vector<std::string>& expiries,
        const std::string& timeInterpolation,
        const std::string& strikeInterpolation,
        bool extrapolation,
        const std::string& timeExtrapolation,
        const std::string& strikeExtrapolation,
        const std::string& atmDeltaType = "",
        bool futurePriceCorrection = true);

    //! \name Inspectors
    //@{
    const std::string& deltaType() const;
    const std::string& atmType() const;
    const std::vector<std::string>& putDeltas() const;
    const std::vector<std::string>& callDeltas() const;
    const std::string& atmDeltaType() const;
    bool futurePriceCorrection() const;
    //@}

    //! \name VolatilitySurfaceConfig
    //@{
    std::vector<std::pair<std::string, std::string>> quotes() const override;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) override;
    //@}

private:
    std::string deltaType_;
    std::string atmType_;
    std::vector<std::string> putDeltas_;
    std::vector<std::string> callDeltas_;
    std::string atmDeltaType_;
    bool futurePriceCorrection_;
};

/*! Volatility configuration for a 2-D moneyness volatility surface
    \ingroup configuration
 */
class VolatilityMoneynessSurfaceConfig : public VolatilitySurfaceConfig {
public:
    //! Default constructor
    VolatilityMoneynessSurfaceConfig();

    //! Explicit constructor
    VolatilityMoneynessSurfaceConfig(
        const std::string& moneynessType,
        const std::vector<std::string>& moneynessLevels,
        const std::vector<std::string>& expiries,
        const std::string& timeInterpolation,
        const std::string& strikeInterpolation,
        bool extrapolation,
        const std::string& timeExtrapolation,
        const std::string& strikeExtrapolation);

    //! \name Inspectors
    //@{
    const std::string& moneynessType() const;
    const std::vector<std::string>& moneynessLevels() const;
    //@}

    //! \name VolatilitySurfaceConfig
    //@{
    std::vector<std::pair<std::string, std::string>> quotes() const override;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) override;
    //@}

private:
    std::string moneynessType_;
    std::vector<std::string> moneynessLevels_;
};

}
}
