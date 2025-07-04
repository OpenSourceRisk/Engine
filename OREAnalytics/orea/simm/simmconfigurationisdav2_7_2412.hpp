/*
 Copyright (C) 2025 Quaternion Risk Management Ltd.
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

/*! \file orea/simm/simmconfigurationisdav2_7_2412.hpp
    \brief SIMM configuration for SIMM version 2.7+2412
*/

#pragma once

#include <orea/simm/simmconfigurationbase.hpp>

namespace ore {
namespace analytics {

/*! Class giving the SIMM configuration as outlined in the document
    <em>ISDA SIMM Methodology, version 2.7+2412.
        Effective Date: July 12, 2025.</em>
*/
class SimmConfiguration_ISDA_V2_7_2412 : public SimmConfigurationBase {
public:
    SimmConfiguration_ISDA_V2_7_2412(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                const QuantLib::Size& mporDays = 10,
                                const std::string& name = "SIMM ISDA 2.7+2412 (12 July 2025)",
                                const std::string version = "2.7+2412");

    //! Return the SIMM <em>Label2</em> value for the given interest rate index
    std::string label2(const QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>& irIndex) const override;

    //! Add SIMM <em>Label2</em> values under certain circumstances.
    void addLabels2(const CrifRecord::RiskType& rt, const std::string& label_2) override;

    QuantLib::Real curvatureMarginScaling() const override;

    QuantLib::Real weight(const CrifRecord::RiskType& rt, boost::optional<std::string> qualifier = boost::none,
                          boost::optional<std::string> label_1 = boost::none,
                          const std::string& calculationCurrency = "") const override;

    QuantLib::Real correlation(const CrifRecord::RiskType& firstRt, const std::string& firstQualifier,
                               const std::string& firstLabel_1, const std::string& firstLabel_2,
                               const CrifRecord::RiskType& secondRt, const std::string& secondQualifier,
                               const std::string& secondLabel_1, const std::string& secondLabel_2,
                               const std::string& calculationCurrency = "") const override;

private:
    //! Find the group of the \p qualifier
    QuantLib::Size group(const std::string& qualifier,
                         const std::map<QuantLib::Size, std::set<std::string>>& groups) const;

    /*! Map giving a currency's FX Volatility group (High or Regular). This concept
        was introduced in ISDA Simm 2.2
     */
    std::map<QuantLib::Size, std::set<std::string>> ccyGroups_;

    //! FX risk weight matrix
    QuantLib::Matrix rwFX_;

    //! FX Correlations when the calculation ccy is in the Regular Volatility group
    QuantLib::Matrix fxRegVolCorrelation_;

    //! FX Correlations when the calculation ccy is in the High Volatility group
    QuantLib::Matrix fxHighVolCorrelation_;

    //! IR Historical volatility ratio
    QuantLib::Real hvr_ir_;
};

} // namespace analytics
} // namespace ore
