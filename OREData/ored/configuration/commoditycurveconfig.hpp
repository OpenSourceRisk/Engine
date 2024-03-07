/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/configuration/commoditycurveconfig.hpp
    \brief Commodity curve configuration class
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/bootstrapconfig.hpp>
#include <ored/configuration/curveconfig.hpp>
#include <boost/optional/optional.hpp>

namespace ore {
namespace data {

/*! Class for holding information on a set of instruments used in bootstrapping a piecewise price curve.
    \ingroup configuration
*/
class PriceSegment : public XMLSerializable {
public:
    //! Type of price segment being represented, i.e. type of instrument in the price segment.
    enum class Type { Future, AveragingFuture, AveragingSpot, AveragingOffPeakPower, OffPeakPowerDaily };

    //! Class to store quotes used in building daily off-peak power quotes.
    class OffPeakDaily : public XMLSerializable {
    public:
        //! Constructor.
        OffPeakDaily();

        //! Detailed constructor.
        OffPeakDaily(
            const std::vector<std::string>& offPeakQuotes,
            const std::vector<std::string>& peakQuotes);

        const std::vector<std::string>& offPeakQuotes() const { return offPeakQuotes_; }
        const std::vector<std::string>& peakQuotes() const { return peakQuotes_; }

        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;

    private:
        std::vector<std::string> offPeakQuotes_;
        std::vector<std::string> peakQuotes_;
    };

    //! \name Constructors
    //@{
    //! Default constructor
    PriceSegment();
    //! Detailed constructor
    PriceSegment(const std::string& type,
        const std::string& conventionsId,
        const std::vector<std::string>& quotes,
        const boost::optional<unsigned short>& priority = boost::none,
        const boost::optional<OffPeakDaily>& offPeakDaily = boost::none,
        const std::string& peakPriceCurveId = "",
        const std::string& peakPriceCalendar = "");
    //@}

    //! \name Inspectors
    //@{
    Type type() const;
    const std::string& conventionsId() const;
    const std::vector<std::string>& quotes() const;
    const boost::optional<unsigned short>& priority() const;
    const boost::optional<OffPeakDaily>& offPeakDaily() const;
    const std::string& peakPriceCurveId() const;
    const std::string& peakPriceCalendar() const;
    bool empty() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    //! Populate quotes
    void populateQuotes();

    std::string strType_;
    std::string conventionsId_;
    std::vector<std::string> quotes_;
    boost::optional<unsigned short> priority_;
    boost::optional<OffPeakDaily> offPeakDaily_;
    std::string peakPriceCurveId_;
    std::string peakPriceCalendar_;

    bool empty_;
    Type type_;
};

//! Commodity curve configuration
/*!
    \ingroup configuration
*/
class CommodityCurveConfig : public CurveConfig {
public:
    /*! The type of commodity curve that has been configured:
         - Direct: if the commodity price curve is built from commodity forward quotes
         - CrossCurrency: if the commodity price curve is implied from a price curve in a different currency
         - Basis: if the commodity price curve is built from basis quotes
         - Piecewise: if the commodity price curve is bootstrapped from sets of instruments
    */
    enum class Type { Direct, CrossCurrency, Basis, Piecewise };

    //! \name Constructors
    //@{
    //! Default constructor
    CommodityCurveConfig() : type_(Type::Direct), extrapolation_(true), addBasis_(true),
        monthOffset_(0), averageBase_(true) {}

    //! Detailed constructor for Direct commodity curve configuration
    CommodityCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
                         const std::vector<std::string>& quotes, const std::string& commoditySpotQuote = "",
                         const std::string& dayCountId = "A365", const std::string& interpolationMethod = "Linear",
                         bool extrapolation = true, const std::string& conventionsId = "");

    //! Detailed constructor for CrossCurrency commodity curve configuration
    CommodityCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
                         const std::string& basePriceCurveId, const std::string& baseYieldCurveId,
                         const std::string& yieldCurveId, bool extrapolation = true);

    //! Detailed constructor for Basis commodity curve configuration
    CommodityCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
                         const std::string& basePriceCurveId, const std::string& baseConventionsId,
                         const std::vector<std::string>& basisQuotes, const std::string& basisConventionsId,
                         const std::string& dayCountId = "A365", const std::string& interpolationMethod = "Linear",
                         bool extrapolation = true, bool addBasis = true, QuantLib::Natural monthOffset = 0,
                         bool averageBase = true);

    //! Detailed constructor for Piecewise commodity curve configuration
    CommodityCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
                         const std::vector<PriceSegment>& priceSegments, const std::string& dayCountId = "A365",
                         const std::string& interpolationMethod = "Linear", bool extrapolation = true,
                         const boost::optional<BootstrapConfig>& bootstrapConfig = boost::none);
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const Type& type() const { return type_; }
    const std::string& currency() const { return currency_; }
    const std::string& commoditySpotQuoteId() const { return commoditySpotQuoteId_; }
    const std::string& dayCountId() const { return dayCountId_; }
    const std::string& interpolationMethod() const { return interpolationMethod_; }
    const std::string& basePriceCurveId() const { return basePriceCurveId_; }
    const std::string& baseYieldCurveId() const { return baseYieldCurveId_; }
    const std::string& yieldCurveId() const { return yieldCurveId_; }
    bool extrapolation() const { return extrapolation_; }
    const vector<string>& fwdQuotes() const { return fwdQuotes_; }
    const std::string& conventionsId() const { return conventionsId_; }
    const std::string& baseConventionsId() const { return baseConventionsId_; }
    bool addBasis() const { return addBasis_; }
    QuantLib::Natural monthOffset() const { return monthOffset_; }
    bool averageBase() const { return averageBase_; }
    bool priceAsHistFixing() const { return priceAsHistFixing_; }
    const std::map<unsigned short, PriceSegment>& priceSegments() const { return priceSegments_; }
    const boost::optional<BootstrapConfig>& bootstrapConfig() const { return bootstrapConfig_; }
    //@}

    //! \name Setters
    //@{
    Type& type() { return type_; }
    std::string& currency() { return currency_; }
    std::string& commoditySpotQuoteId() { return commoditySpotQuoteId_; }
    std::string& dayCountId() { return dayCountId_; }
    std::string& interpolationMethod() { return interpolationMethod_; }
    std::string& basePriceCurveId() { return basePriceCurveId_; }
    std::string& baseYieldCurveId() { return baseYieldCurveId_; }
    std::string& yieldCurveId() { return yieldCurveId_; }
    bool& extrapolation() { return extrapolation_; }
    std::string& conventionsId() { return conventionsId_; }
    std::string& baseConventionsId() { return baseConventionsId_; }
    bool& addBasis() { return addBasis_; }
    QuantLib::Natural& monthOffset() { return monthOffset_; }
    bool& averageBase() { return averageBase_; }
    bool& priceAsHistFixing() { return priceAsHistFixing_; }
    void setPriceSegments(const std::map<unsigned short, PriceSegment>& priceSegments) {
        priceSegments_ = priceSegments;
    }
    void setBootstrapConfig(const BootstrapConfig& bootstrapConfig) { bootstrapConfig_ = bootstrapConfig; }
    //@}

private:
    //! Populate any dependent curve IDs.
    void populateRequiredCurveIds();

    //! Process price segments when configuring a Piecewise curve.
    void processSegments(std::vector<PriceSegment> priceSegments);

    Type type_;
    vector<string> fwdQuotes_;
    std::string currency_;
    std::string commoditySpotQuoteId_;
    std::string dayCountId_;
    std::string interpolationMethod_;
    std::string basePriceCurveId_;
    std::string baseYieldCurveId_;
    std::string yieldCurveId_;
    bool extrapolation_;
    std::string conventionsId_;
    std::string baseConventionsId_;
    bool addBasis_;
    QuantLib::Natural monthOffset_;
    bool averageBase_;
    bool priceAsHistFixing_;
    /*! The map key is the internal priority of the price segment and does not necessarily map the PriceSegment's 
        priority member value. We are allowing here for the priority to be unspecified in the PriceSegment during 
        configuration.
    */
    std::map<unsigned short, PriceSegment> priceSegments_;
    boost::optional<BootstrapConfig> bootstrapConfig_;
};

} // namespace data
} // namespace ore
