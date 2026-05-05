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

/*! \file ored/configuration/cdsvolcurveconfig.hpp
    \brief CDS and index CDS volatility configuration
    \ingroup configuration
*/

#pragma once

#include <ql/shared_ptr.hpp>
#include <qle/termstructures/indexcdsvolstripper.hpp>
#include <ored/configuration/curveconfig.hpp>
#include <ored/configuration/onedimsolverconfig.hpp>
#include <ored/configuration/volatilityconfig.hpp>

namespace ore {
namespace data {

using QuoteDimension = QuantExt::IndexCdsVolStripper::QuoteDimension;

/*! CDS and index CDS volatility configuration

    \ingroup configuration
*/
class CDSVolatilityCurveConfig : public CurveConfig {
public:
    //! Information needed in a `CDSVolatilityCurveConfig` when the quotes provided are prices.
    class PriceInfo : public XMLSerializable {
    public:
        //! Struct giving the index factors and realised front end protection.
        struct IndexFactors {
            // Current index factor i.e. index factor as of the market data date.
            QuantLib::Real indexFactor = 1.0;
            // Index factor that the strike is applied against.
            // For the on-the-run version of a given series, this is the same as the `indexFactor`.
            // For the off-the-run version(s) of a given series, this will be greater than the `indexFactor`.
            QuantLib::Real indexFactorStrike = 1.0;
            // The realised front end protection factor already accrued for the version of the index series for which 
            // we are stripping volatilities from premia.
            QuantLib::Real realisedFep = 0.0;
        };

        PriceInfo() = default;

        PriceInfo(std::string cdsConventionsId,
            QuantLib::Real runningCoupon,
            QuantLib::ext::optional<IndexFactors> indexFactors = QuantLib::ext::nullopt,
            std::string engineOverride = "",
            QuantLib::ext::optional<OneDimSolverConfig> solverConfig = QuantLib::ext::nullopt,
            QuantLib::ext::optional<QuoteDimension> quoteDimension = QuantLib::ext::nullopt);

        //! \name Inspectors
        //@{
        const std::string& cdsConventionsId() const;
        const QuantLib::Real runningCoupon() const;
        const QuantLib::ext::optional<IndexFactors>& indexFactors() const;
        const std::string& engineOverride() const;
        const QuantLib::ext::optional<OneDimSolverConfig>& solverConfig() const;
        QuoteDimension quoteDimension() const;
        //@}

        //! \name Serialisation
        //@{
        void fromXML(XMLNode* node) override;
        ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
        //@}

    private:
        std::string cdsConventionsId_;
        QuantLib::Real runningCoupon_ = QuantLib::Null<QuantLib::Real>();
        QuantLib::ext::optional<IndexFactors> indexFactors_;
        std::string engineOverride_;
        QuantLib::ext::optional<OneDimSolverConfig> solverConfig_;
        QuantLib::ext::optional<QuoteDimension> quoteDimension_;
    };

    //! Default constructor
    CDSVolatilityCurveConfig();

    //! Detailed constructor
    CDSVolatilityCurveConfig(const std::string& curveId, const std::string& curveDescription,
                             const QuantLib::ext::shared_ptr<VolatilityConfig>& volatilityConfig,
                             const std::string& dayCounter = "A365", const std::string& calendar = "NullCalendar",
                             const std::string& strikeType = "", const std::string& quoteName = "",
                             QuantLib::Real strikeFactor = 1.0, const std::vector<QuantLib::Period>& terms = {},
                             const std::vector<std::string>& termCurves = {},
                             const std::vector<QuantLib::Date>& termMaturities = {},
                             QuantLib::ext::optional<PriceInfo> priceInfo = QuantLib::ext::nullopt);

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<VolatilityConfig>& volatilityConfig() const;
    const std::string& dayCounter() const;
    const std::string& calendar() const;
    const std::string& strikeType() const;
    const std::string& quoteName() const;
    QuantLib::Real strikeFactor() const;
    const std::vector<QuantLib::Period>& terms() const;
    const std::vector<std::string>& termCurves() const;
    const std::vector<QuantLib::Date>& termMaturities() const;
    const QuantLib::ext::optional<PriceInfo>& priceInfo() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    QuantLib::ext::shared_ptr<VolatilityConfig> volatilityConfig_;
    std::string dayCounter_;
    std::string calendar_;
    std::string strikeType_;
    std::string quoteName_;
    QuantLib::Real strikeFactor_;
    std::vector<QuantLib::Period> terms_;
    std::vector<std::string> termCurves_;
    std::vector<QuantLib::Date> termMaturities_;
    QuantLib::ext::optional<PriceInfo> priceInfo_;

    //! Populate CurveConfig::quotes_ with the required quotes.
    void populateQuotes();

    //! Populate required curve ids
    void populateRequiredIds() const override;

    //! Give back the quote stem used in construction of the quote strings
    std::string quoteStem(MarketDatum::QuoteType quoteType) const;

    //! Checks on consistency of terms, term curves and term maturities.
    void validate(const std::string& checkSite) const;
};

} // namespace data
} // namespace ore
