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

#include <ored/marketdata/marketdatum.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/exercise.hpp>

namespace ore {
namespace data {

/*! Shared volatility configurations
    \ingroup configuration
*/
class VolatilityConfig : public ore::data::XMLSerializable {
public:
    VolatilityConfig(std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    void fromXMLNode(ore::data::XMLNode* node);
    void toXMLNode(XMLDocument& doc, XMLNode* node) const;

    QuantLib::Natural priority() const { return priority_; };
    Calendar calendar() const { return calendar_; };

private:
    Calendar calendar_;
    string calendarStr_;
    QuantLib::Natural priority_;
};

bool operator<(const VolatilityConfig& vc1, const VolatilityConfig& vc2);

class ProxyVolatilityConfig : public VolatilityConfig {
public:
    ProxyVolatilityConfig() {}
    ProxyVolatilityConfig(const std::string& proxyVolatilityCurve, const std::string& fxVolatilityCurve = "",
                                const std::string& correlationCurve = "", std::string calendarStr = std::string(),
                                QuantLib::Natural priority = 0)
        : VolatilityConfig(calendarStr, priority), 
        proxyVolatilityCurve_(proxyVolatilityCurve), fxVolatilityCurve_(fxVolatilityCurve),
        correlationCurve_(correlationCurve) {}

    //! \name Inspectors
    //@{
    const std::string& proxyVolatilityCurve() const;
    const std::string& fxVolatilityCurve() const;
    const std::string& correlationCurve() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::string proxyVolatilityCurve_;
    std::string fxVolatilityCurve_;
    std::string correlationCurve_;
};

class CDSProxyVolatilityConfig : public VolatilityConfig {
public:
    CDSProxyVolatilityConfig() {}
    explicit CDSProxyVolatilityConfig(const std::string& cdsVolatilityCurve)
        : cdsVolatilityCurve_(cdsVolatilityCurve) {}

    //! \name Inspectors
    //@{
    const std::string& cdsVolatilityCurve() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::string cdsVolatilityCurve_;
};

class QuoteBasedVolatilityConfig : public VolatilityConfig {
public:
    //! Default constructor
    QuoteBasedVolatilityConfig(MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
                               QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
                               std::string calendarStr = std::string(), QuantLib::Natural priority = 0)
        : VolatilityConfig(calendarStr, priority), quoteType_(quoteType), exerciseType_(exerciseType) {}

    //! \name Inspectors
    //@{
    const MarketDatum::QuoteType& quoteType() const { return quoteType_; }
    const QuantLib::Exercise::Type& exerciseType() const { return exerciseType_; }
    //@}

    //! \name Serialisation
    //@{
    void fromBaseNode(ore::data::XMLNode* node);
    void toBaseNode(ore::data::XMLDocument& doc, ore::data::XMLNode* node) const;
    //@}

private:
    MarketDatum::QuoteType quoteType_;
    QuantLib::Exercise::Type exerciseType_;
};

/*! Volatility configuration for a single constant volatility
    \ingroup configuration
 */
class ConstantVolatilityConfig : public QuoteBasedVolatilityConfig {
public:
    //! Default constructor
    ConstantVolatilityConfig(MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European, 
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! Explicit constructor
    ConstantVolatilityConfig(const std::string& quote,
        MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European, 
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! \name Inspectors
    //@{
    const std::string& quote() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::string quote_;
};

/*! Volatility configuration for a 1-D volatility curve
    \ingroup configuration
 */
class VolatilityCurveConfig : public QuoteBasedVolatilityConfig {
public:
    //! Default constructor
    VolatilityCurveConfig(MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        bool enforceMontoneVariance = true, std::string calendarStr = std::string(),
        QuantLib::Natural priority = 0);

    //! Explicit constructor
    VolatilityCurveConfig(const std::vector<std::string>& quotes, const std::string& interpolation,
        const std::string& extrapolation,
        MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        bool enforceMontoneVariance = true, std::string calendarStr = std::string(),
        QuantLib::Natural priority = 0);

    //! \name Inspectors
    //@{
    const std::vector<std::string>& quotes() const;
    const std::string& interpolation() const;
    const std::string& extrapolation() const;
    bool enforceMontoneVariance() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::vector<std::string> quotes_;
    std::string interpolation_;
    std::string extrapolation_;
    bool enforceMontoneVariance_;
};

/*! Base volatility configuration for a 2-D volatility surface
    \ingroup configuration
 */
class VolatilitySurfaceConfig : public QuoteBasedVolatilityConfig {
public:
    //! Default constructor
    VolatilitySurfaceConfig(MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! Explicit constructor
    VolatilitySurfaceConfig(const std::string& timeInterpolation, const std::string& strikeInterpolation,
        bool extrapolation, const std::string& timeExtrapolation,
        const std::string& strikeExtrapolation,
        MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! \name Inspectors
    //@{
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
    VolatilityStrikeSurfaceConfig(MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! Explicit constructor
    VolatilityStrikeSurfaceConfig(const std::vector<std::string>& strikes, const std::vector<std::string>& expiries,
        const std::string& timeInterpolation, const std::string& strikeInterpolation,
        bool extrapolation, const std::string& timeExtrapolation,
        const std::string& strikeExtrapolation,
        MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! \name Inspectors
    //@{
    const std::vector<std::string>& strikes() const;
    const std::vector<std::string>& expiries() const;
    //@}

    //! \name VolatilitySurfaceConfig
    //@{
    std::vector<std::pair<std::string, std::string>> quotes() const override;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::vector<std::string> strikes_;
    std::vector<std::string> expiries_;
};

/*! Volatility configuration for a 2-D delta volatility surface
    \ingroup configuration
 */
class VolatilityDeltaSurfaceConfig : public VolatilitySurfaceConfig {
public:
    //! Default constructor
    VolatilityDeltaSurfaceConfig(MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! Explicit constructor
    VolatilityDeltaSurfaceConfig(const std::string& deltaType, const std::string& atmType,
        const std::vector<std::string>& putDeltas, const std::vector<std::string>& callDeltas,
        const std::vector<std::string>& expiries, const std::string& timeInterpolation,
        const std::string& strikeInterpolation, bool extrapolation,
        const std::string& timeExtrapolation, const std::string& strikeExtrapolation,
        const std::string& atmDeltaType = "", bool futurePriceCorrection = true,
        MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! \name Inspectors
    //@{
    const std::string& deltaType() const;
    const std::string& atmType() const;
    const std::vector<std::string>& putDeltas() const;
    const std::vector<std::string>& callDeltas() const;
    const std::vector<std::string>& expiries() const;
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
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::string deltaType_;
    std::string atmType_;
    std::vector<std::string> putDeltas_;
    std::vector<std::string> callDeltas_;
    std::vector<std::string> expiries_;
    std::string atmDeltaType_;
    bool futurePriceCorrection_;
};

/*! Volatility configuration for a 2-D moneyness volatility surface
    \ingroup configuration
 */
class VolatilityMoneynessSurfaceConfig : public VolatilitySurfaceConfig {
public:
    //! Default constructor
    VolatilityMoneynessSurfaceConfig(MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! Explicit constructor
    VolatilityMoneynessSurfaceConfig(const std::string& moneynessType, const std::vector<std::string>& moneynessLevels,
        const std::vector<std::string>& expiries, const std::string& timeInterpolation,
        const std::string& strikeInterpolation, bool extrapolation,
        const std::string& timeExtrapolation, const std::string& strikeExtrapolation,
        bool futurePriceCorrection = true,
        MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! \name Inspectors
    //@{
    const std::string& moneynessType() const;
    const std::vector<std::string>& moneynessLevels() const;
    const std::vector<std::string>& expiries() const;
    bool futurePriceCorrection() const;
    //@}

    //! \name VolatilitySurfaceConfig
    //@{
    std::vector<std::pair<std::string, std::string>> quotes() const override;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::string moneynessType_;
    std::vector<std::string> moneynessLevels_;
    std::vector<std::string> expiries_;
    bool futurePriceCorrection_;
};

/*! Volatility configuration for an average future price option (APO) surface.
    \ingroup configuration
 */
class VolatilityApoFutureSurfaceConfig : public VolatilitySurfaceConfig {
public:
    //! Default constructor
    VolatilityApoFutureSurfaceConfig(MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! Explicit constructor
    VolatilityApoFutureSurfaceConfig(const std::vector<std::string>& moneynessLevels,
        const std::string& baseVolatilityId, const std::string& basePriceCurveId,
        const std::string& baseConventionsId, const std::string& timeInterpolation,
        const std::string& strikeInterpolation, bool extrapolation,
        const std::string& timeExtrapolation, const std::string& strikeExtrapolation,
        QuantLib::Real beta = 0.0, const std::string& maxTenor = "",
        MarketDatum::QuoteType quoteType = MarketDatum::QuoteType::RATE_LNVOL,
        QuantLib::Exercise::Type exerciseType = QuantLib::Exercise::Type::European,
        std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    //! \name Inspectors
    //@{
    const std::vector<std::string>& moneynessLevels() const;
    const std::string& baseVolatilityId() const;
    const std::string& basePriceCurveId() const;
    const std::string& baseConventionsId() const;
    QuantLib::Real beta() const;
    const std::string& maxTenor() const;
    //@}

    //! \name VolatilitySurfaceConfig
    //@{
    std::vector<std::pair<std::string, std::string>> quotes() const override;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    std::vector<std::string> moneynessLevels_;
    std::string baseVolatilityId_;
    std::string basePriceCurveId_;
    std::string baseConventionsId_;
    QuantLib::Real beta_;
    std::string maxTenor_;
};

class VolatilityConfigBuilder : public XMLSerializable {
public:
    explicit VolatilityConfigBuilder() {}
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    void loadVolatiltyConfigs(XMLNode* node);

    const std::vector<QuantLib::ext::shared_ptr<VolatilityConfig>>& volatilityConfig() { return volatilityConfig_; };

private:
    std::vector<QuantLib::ext::shared_ptr<VolatilityConfig>> volatilityConfig_;
};


} // namespace data
} // namespace ore
