/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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

/*! \file orea/simm/simmconfiguration.hpp
    \brief SIMM configuration interface
*/

#pragma once

#include <set>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <orea/simm/crifconfiguration.hpp>
#include <orea/simm/crifrecord.hpp>
#include <ql/indexes/interestrateindex.hpp>
#include <ql/types.hpp>

namespace ore {
namespace analytics {

//! Abstract base class defining the interface for a SIMM configuration
class SimmConfiguration : public CrifConfiguration {
public:
    virtual ~SimmConfiguration() {}

    //! Enum indicating the relevant side of the SIMM calculation
    enum class SimmSide { Call, Post };

    /*! Risk class types in SIMM plus an All type for convenience

        \warning The ordering here matters. It is used in indexing in to
                 correlation matrices for the correlation between risk classes.

        \warning Internal methods rely on the last element being 'All'
    */
    enum class RiskClass { InterestRate, CreditQualifying, CreditNonQualifying, Equity, Commodity, FX, All };

    

    //! Margin types in SIMM plus an All type for convenience
    //! Internal methods rely on the last element being 'All'
    enum class MarginType { Delta, Vega, Curvature, BaseCorr, AdditionalIM, All }; 

    enum class IMModel {
        Schedule,
        SIMM,
        SIMM_R, // Equivalent to SIMM
        SIMM_P  // Equivalent to SIMM
    };

    //! SIMM regulators
    enum Regulation {
        APRA,
        CFTC,
        ESA,
        FINMA,
        KFSC,
        HKMA,
        JFSA,
        MAS,
        OSFI,
        RBI,
        SEC,
        SEC_unseg,
        USPR,
        NONREG,
        BACEN,
        SANT,
        SFC,
        UK,
        AMFQ,
        Included,
        Unspecified,
        Invalid
    };

    //! Give back a set containing the RiskClass values optionally excluding 'All'
    static std::set<RiskClass> riskClasses(bool includeAll = false);

    //! Give back a set containing the RiskType values optionally excluding 'All'
    static std::set<CrifRecord::RiskType> riskTypes(bool includeAll = false);

    //! Give back a set containing the MarginType values optionally excluding 'All'
    static std::set<MarginType> marginTypes(bool includeAll = false);

    //! Give back a set containing the ProductClass values optionally excluding 'All'
    static std::set<CrifRecord::ProductClass> productClasses(bool includeAll = false);

    //! For a given risk class, return the corresponding risk types
    static std::pair<CrifRecord::RiskType, CrifRecord::RiskType> riskClassToRiskType(const RiskClass& rc);

    //! For a given rirsk type, return the corresponding risk class
    static RiskClass riskTypeToRiskClass(const CrifRecord::RiskType& rt);

    //! Define ordering for ProductClass according to a waterfall:
    // Empty < RatesFX < Equity < Commodity < Credit
    // All is unhandled
    // Would use operator< but there is a bug in the visual studio compiler.
    static bool less_than(const CrifRecord::ProductClass& lhs, const CrifRecord::ProductClass& rhs);
    static bool greater_than(const CrifRecord::ProductClass& lhs, const CrifRecord::ProductClass& rhs);
    static bool less_than_or_equal_to(const CrifRecord::ProductClass& lhs, const CrifRecord::ProductClass& rhs);
    static bool greater_than_or_equal_to(const CrifRecord::ProductClass& lhs, const CrifRecord::ProductClass& rhs);

    //! Return the "worse" ProductClass using a waterfall logic:
    // RatesFX <
    static CrifRecord::ProductClass maxProductClass(CrifRecord::ProductClass pc1, CrifRecord::ProductClass pc2) {
        QL_REQUIRE(pc1 != CrifRecord::ProductClass::All && pc2 != CrifRecord::ProductClass::All,
                   "Cannot define worse product type if even one of the product classes is indeterminate.");
        return (less_than(pc1, pc2) ? pc2 : pc1);
    }

    //! Return the SIMM <em>bucket</em> names for the given risk type \p rt
    //! An empty vector is returned if the risk type has no buckets
    virtual std::vector<std::string> buckets(const CrifRecord::RiskType& rt) const = 0;

    //! Return true if the SIMM risk type \p rt has buckets
    virtual bool hasBuckets(const CrifRecord::RiskType& rt) const = 0;

    //! Return true if the SIMM risk type \p rt has buckets
    bool hasBucketMapping(const CrifRecord::RiskType& rt, const std::string& qualifier) const override {
        return bucketMapper()->has(rt, qualifier);
    }


    //! Return the list of SIMM <em>Label1</em> values for risk type \p rt
    //! An empty vector is returned if the risk type does not use <em>Label1</em>
    virtual std::vector<std::string> labels1(const CrifRecord::RiskType& rt) const = 0;

    //! Return the list of SIMM <em>Label2</em> values for risk type \p rt
    //! An empty vector is returned if the risk type does not use <em>Label2</em>
    virtual std::vector<std::string> labels2(const CrifRecord::RiskType& rt) const = 0;

    /*! Add SIMM <em>Label2</em> values under certain circumstances.

        For example, in v338 and later, CreditQ label2 should have the payment
        currency if sensitivty is not the result of a securitisation and
        "payment currency,Sec" if sensitivty is the result of a securitisation.
        Adding to label2 in the configuration means you do not have to have an
        exhaustive list up front.
    */
    virtual void addLabels2(const CrifRecord::RiskType& rt, const std::string& label_2) = 0;

    /*! Return the SIMM <em>risk weight</em> for the given risk type \p rt with
        the given \p qualifier and the given \p label_1. Three possibilities:
        -# there is a flat risk weight for the risk factor's RiskType so only need \p rt
        -# there is a qualifier-dependent risk weight for the risk factor's RiskType
           so need \p rt and \p qualifier
        -# there is a qualifier-dependent and label1-dependent risk weight for the risk
           factor's RiskType so need all three parameters
    */
    virtual QuantLib::Real weight(const CrifRecord::RiskType& rt, boost::optional<std::string> qualifier = boost::none,
                                  boost::optional<std::string> label_1 = boost::none,
                                  const std::string& calculationCurrency = "") const = 0;

    /*! Gives back the value of the scaling function used in the calculation of curvature risk
        for the risk type \p rt with SIMM <em>Label1</em> value \p label_1. The scaling function is:
        \f[
        SF(t) = 0.5 \times \min \left(1, \frac{14}{t} \right)
        \f]
        where \f$t\f$ is given in days.
    */
    virtual QuantLib::Real curvatureWeight(const CrifRecord::RiskType& rt, const std::string& label_1) const = 0;

    /*! Give back the SIMM <em>historical volatility ratio</em> for the risk type \p rt
     */
    virtual QuantLib::Real historicalVolatilityRatio(const CrifRecord::RiskType& rt) const = 0;

    /*! Give back the value of \f$\sigma_{kj}\f$ from the SIMM docs for risk type \p rt.
        In general, \p rt is a volatility risk type and the method returns:
        \f[
           \sigma_{kj} = \frac{RW_k \sqrt{\frac{365}{14}}}{\Phi^{-1}(0.99)}
        \f]
        where \f$RW_k\f$ is the corresponding delta risk weight and \f$\Phi(z)\f$ is
        the cumulative standard normal distribution.

        \remark For convenience, returns 1.0 if not applicable for risk type \p rt
    */
    virtual QuantLib::Real sigma(const CrifRecord::RiskType& rt, boost::optional<std::string> qualifier = boost::none,
                                 boost::optional<std::string> label_1 = boost::none,
                                 const std::string& calculationCurrency = "") const = 0;

    /*! Give back the scaling factor for the Interest Rate curvature margin
     */
    virtual QuantLib::Real curvatureMarginScaling() const = 0;

    /*! Give back the SIMM <em>concentration threshold</em> for the risk type \p rt and the
        SIMM \p qualifier
    */
    virtual QuantLib::Real concentrationThreshold(const CrifRecord::RiskType& rt, const std::string& qualifier) const = 0;

    /*! Return true if \p rt is a valid SIMM <em>RiskType</em> under the current configuration.
        Otherwise, return false.
    */
    virtual bool isValidRiskType(const CrifRecord::RiskType& rt) const = 0;

    //! Return the correlation between SIMM risk classes \p rc_1 and \p rc_2
    virtual QuantLib::Real correlationRiskClasses(const RiskClass& rc_1, const RiskClass& rc_2) const = 0;

    /*! Return the correlation between the \p firstQualifier with risk type \p firstRt,
        Label1 value of \p firstLabel_1 and Label2 value of \p firstLabel_2 *and* the
        \p secondQualifier with risk type \p secondRt, Label1 value of \p secondLabel_1
        and Label2 value of \p secondLabel_2

        \warning Returns 0 if no correlation found

        \todo test if the default return value of 0 makes sense
    */
    virtual QuantLib::Real correlation(const CrifRecord::RiskType& firstRt, const std::string& firstQualifier,
                                       const std::string& firstLabel_1, const std::string& firstLabel_2,
                                       const CrifRecord::RiskType& secondRt, const std::string& secondQualifier,
                                       const std::string& secondLabel_1, const std::string& secondLabel_2,
                                       const std::string& calculationCurrency = "") const = 0;

    virtual bool isSimmConfigCalibration() const { return false; }

protected:
    //! Number of risk classes including RiskClass::All
    static const QuantLib::Size numberOfRiskClasses;
    //! Number of margin types including MarginType::All
    static const QuantLib::Size numberOfMarginTypes;
    //! Number of regulations
    static const QuantLib::Size numberOfRegulations;
};

std::ostream& operator<<(std::ostream& out, const SimmConfiguration::SimmSide& side);

std::ostream& operator<<(std::ostream& out, const SimmConfiguration::RiskClass& rc);

std::ostream& operator<<(std::ostream& out, const SimmConfiguration::MarginType& mt);

std::ostream& operator<<(std::ostream& out, const SimmConfiguration::IMModel& model);

std::ostream& operator<<(std::ostream& out, const SimmConfiguration::Regulation& regulation);

SimmConfiguration::SimmSide parseSimmSide(const std::string& side);

SimmConfiguration::RiskClass parseSimmRiskClass(const std::string& rc);

SimmConfiguration::MarginType parseSimmMarginType(const std::string& mt);

SimmConfiguration::IMModel parseIMModel(const std::string& pc);

SimmConfiguration::Regulation parseRegulation(const std::string& regulation);

std::string combineRegulations(const std::string&, const std::string&);

//! Reads a string containing regulations applicable for a given CRIF record
std::set<std::string> parseRegulationString(const std::string& regsString,
                                            const std::set<std::string>& valueIfEmpty = {"Unspecified"});

//! Cleans a string defining regulations so that different permutations of the same set will
//! be seen as the same string, e.g. "APRA,SEC,ESA" and "SEC,ESA,APRA" should be equivalent.
std::string sortRegulationString(const std::string& regsString);

//! Removes a given vector of regulations from a string of regulations and returns a string with the regulations removed
std::string removeRegulations(const std::string& regsString, const std::vector<std::string>& regsToRemove);

//! Filters a string of regulations on a given vector of regulations and returns a string containing only those filtered
//! regulations
std::string filterRegulations(const std::string& regsString, const std::vector<std::string>& regsToFilter);

//! From a vector of regulations, determine the winning regulation based on order of priority
SimmConfiguration::Regulation getWinningRegulation(const std::vector<std::string>& winningRegulations);

} // namespace analytics
} // namespace ore
