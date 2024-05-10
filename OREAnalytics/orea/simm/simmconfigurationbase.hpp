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

/*! \file orea/simm/simmconfigurationbase.hpp
    \brief Base SIMM configuration class
*/

#pragma once

#include <orea/simm/simmbucketmapper.hpp>
#include <orea/simm/simmcalibration.hpp>
#include <orea/simm/simmconcentration.hpp>
#include <orea/simm/simmconfiguration.hpp>

#include <ql/math/matrix.hpp>

#include <map>

namespace ore {
namespace analytics {

class SimmConfigurationBase : public SimmConfiguration {
public:
    typedef std::map<std::tuple<std::string, std::string, std::string>, QuantLib::Real> Amounts;

    //! Returns the SIMM configuration name
    const std::string& name() const override { return name_; }

    //! Returns the SIMM configuration version
    const std::string& version() const override { return version_; }

    //! Returns the SIMM bucket mapper used by the configuration
    const QuantLib::ext::shared_ptr<SimmBucketMapper>& bucketMapper() const override { return simmBucketMapper_; }

    //! Return true if the SIMM risk type \p rt has buckets
    bool hasBuckets(const CrifRecord::RiskType& rt) const override;

    /*! Return the SIMM <em>bucket</em> name for the given risk type \p rt
        and \p qualifier
    */
    std::string bucket(const CrifRecord::RiskType& rt, const std::string& qualifier) const override;

    const bool checkValue(const std::string&, const std::vector<std::string>&) const;

    //! Return the SIMM <em>bucket</em> names for the given risk type \p rt
    //! An empty vector is returned if the risk type has no buckets
    std::vector<std::string> buckets(const CrifRecord::RiskType& rt) const override;

    //! Return the list of SIMM <em>Label1</em> values for risk type \p rt
    //! An empty vector is returned if the risk type does not use <em>Label1</em>
    std::vector<std::string> labels1(const CrifRecord::RiskType& rt) const override;

    //! Return the list of SIMM <em>Label2</em> values for risk type \p rt
    //! An empty vector is returned if the risk type does not use <em>Label2</em>
    std::vector<std::string> labels2(const CrifRecord::RiskType& rt) const override;

    //! Add SIMM <em>Label2</em> values under certain circumstances.
    void addLabels2(const CrifRecord::RiskType& rt, const std::string& label_2) override {}

    /*! Return the SIMM <em>risk weight</em> for the given risk type \p rt with
        the given \p qualifier and the given \p label_1. Three possibilities:
        -# there is a flat risk weight for the risk factor's RiskType so only need \p rt
        -# there is a qualifier-dependent risk weight for the risk factor's RiskType
           so need \p rt and \p qualifier
        -# there is a qualifier-dependent and label1-dependent risk weight for the risk
           factor's RiskType so need all three parameters
    */
    QuantLib::Real weight(const CrifRecord::RiskType& rt, boost::optional<std::string> qualifier = boost::none,
                          boost::optional<std::string> label_1 = boost::none,
                          const std::string& calculationCurrency = "") const override;

    /*! Gives back the value of the scaling function used in the calculation of curvature risk
        for the risk type \p rt with SIMM <em>Label1</em> value \p label_1. The scaling function is:
        \f[
        SF(t) = 0.5 \times \min \left(1, \frac{14}{t} \right)
        \f]
        where \f$t\f$ is given in days.

        \warning An error is thrown if there is no curvature from the risk type \p rt
    */
    QuantLib::Real curvatureWeight(const CrifRecord::RiskType& rt, const std::string& label_1) const override;

    /*! Give back the SIMM <em>historical volatility ratio</em> for the risk type \p rt

        \remark 1.0 is returned if there is no historical volatility ratio for \p rt
    */
    QuantLib::Real historicalVolatilityRatio(const CrifRecord::RiskType& rt) const override;

    /*! Give back the value of \f$\sigma_{kj}\f$ from the SIMM docs for risk type \p rt.
        In general, \p rt is a volatility risk type and the method returns:
        \f[
           \sigma_{kj} = \frac{RW_k \sqrt{\frac{365}{14}}}{\Phi^{-1}(0.99)}
        \f]
        where \f$RW_k\f$ is the corresponding delta risk weight and \f$\Phi(z)\f$ is
        the cumulative standard normal distribution.

        \remark For convenience, returns 1.0 if not applicable for risk type \p rt
    */
    QuantLib::Real sigma(const CrifRecord::RiskType& rt, boost::optional<std::string> qualifier = boost::none,
                         boost::optional<std::string> label_1 = boost::none,
                         const std::string& calculationCurrency = "") const override;

    /*! Give back the scaling factor for the Interest Rate curvature margin
     */
    QuantLib::Real curvatureMarginScaling() const override { return 2.3; }

    /*! Give back the SIMM <em>concentration threshold</em> for the risk type \p rt and the
        SIMM \p qualifier
    */
    QuantLib::Real concentrationThreshold(const CrifRecord::RiskType& rt, const std::string& qualifier) const override {
        return simmConcentration_->threshold(rt, qualifier);
    }

    /*! Return true if \p rt is a valid SIMM <em>RiskType</em> under the current configuration.
        Otherwise, return false.
    */
    bool isValidRiskType(const CrifRecord::RiskType& rt) const override { return validRiskTypes_.count(rt) > 0; }

    //! Return the correlation between SIMM risk classes \p rc_1 and \p rc_2
    QuantLib::Real correlationRiskClasses(const RiskClass& rc_1, const RiskClass& rc_2) const override;

    /*! Return the correlation between the \p firstQualifier with risk type \p firstRt,
        Label1 value of \p firstLabel_1 and Label2 value of \p firstLabel_2 *and* the
        \p secondQualifier with risk type \p secondRt, Label1 value of \p secondLabel_1
        and Label2 value of \p secondLabel_2

        \remark if not using \p firstLabel_1 and \p secondLabel_1, just enter an empty
                string for both. Similarly for \p firstLabel_2 and \p secondLabel_2.

        \warning Returns 0 if no correlation found

        \todo test if the default return value of 0 makes sense
    */
    QuantLib::Real correlation(const CrifRecord::RiskType& firstRt, const std::string& firstQualifier,
                               const std::string& firstLabel_1, const std::string& firstLabel_2,
                               const CrifRecord::RiskType& secondRt, const std::string& secondQualifier,
                               const std::string& secondLabel_1, const std::string& secondLabel_2,
                               const std::string& calculationCurrency = "") const override;

    //! MPOR in days
    QuantLib::Size mporDays() const { return mporDays_; }

private:
    //! Name of the SIMM configuration
    std::string name_;
    //! Calculate variable for use in sigma method
    QuantLib::Real sigmaMultiplier() const;

protected:
    //! Constructor taking the SIMM configuration \p name and \p version
    SimmConfigurationBase(const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper, const std::string& name,
                          const std::string version, QuantLib::Size mporDays = 10);

    //! SIMM configuration version
    std::string version_;

    //! Used to map SIMM <em>Qualifier</em> names to SIMM <em>bucket</em> values
    QuantLib::ext::shared_ptr<SimmBucketMapper> simmBucketMapper_;

    //! Used to get the concentration thresholds for a given risk type and qualifier
    QuantLib::ext::shared_ptr<SimmConcentration> simmConcentration_;

    const std::tuple<std::string, std::string, std::string> makeKey(const std::string&, const std::string&,
                                                                    const std::string&) const;

    //! Helper method to find the index of the \p label in \p labels
    QuantLib::Size labelIndex(const std::string& label, const std::vector<std::string>& labels) const;

    //! A base implementation of addLabels2 that can be shared by derived classes
    void addLabels2Impl(const CrifRecord::RiskType& rt, const std::string& label_2);

    /*! Map giving the SIMM <em>bucket</em> names for each risk type. If risk type is
        not present in the map keys => there are no buckets for that risk type
    */
    std::map<CrifRecord::RiskType, std::vector<std::string>> mapBuckets_;

    /*! Map giving the possible SIMM <em>Label1</em> values for each risk type. If
        risk type is not present in the map keys then the <em>Label1</em> value is
        not used for that risk type
    */
    std::map<CrifRecord::RiskType, std::vector<std::string>> mapLabels_1_;

    /*! Map giving the possible SIMM <em>Label2</em> values for each risk type. If
        risk type is not present in the map keys then the <em>Label2</em> value is
        not used for that risk type
    */
    std::map<CrifRecord::RiskType, std::vector<std::string>> mapLabels_2_;

    /*! Risk weights, there are three types:
        -# risk type dependent only
        -# risk type and bucket dependent
        -# risk type, bucket and label1 dependent
    */
    std::map<CrifRecord::RiskType, QuantLib::Real> rwRiskType_;
    std::map<CrifRecord::RiskType, Amounts> rwBucket_;
    std::map<CrifRecord::RiskType, Amounts> rwLabel_1_;

    /*! Map from risk type to a vector of curvature weights. The size of the vector of
        weights for a given risk type must equal the size of the vector of Label1 values
        for that risk type in the map <code>mapLabels_1_</code>
    */
    std::map<CrifRecord::RiskType, std::vector<QuantLib::Real>> curvatureWeights_;

    //! Map from risk type to a historical volatility ratio.
    std::map<CrifRecord::RiskType, QuantLib::Real> historicalVolatilityRatios_;

    //! Set of valid risk types for the current configuration
    std::set<CrifRecord::RiskType> validRiskTypes_;

    //! Risk class correlation matrix
    Amounts riskClassCorrelation_;

    /*! Map from risk type to a matrix of inter-bucket correlations for that
        risk type i.e. correlation between qualifiers of the risk type that fall
        in different buckets
    */
    std::map<CrifRecord::RiskType, Amounts> interBucketCorrelation_;

    /*! Map from risk type to an intra-bucket correlation for that
        risk type i.e. correlation between qualifiers of the risk type that fall
        in the same bucket
    */
    std::map<CrifRecord::RiskType, Amounts> intraBucketCorrelation_;

    /*! @name Single Correlations
        Single correlation numbers that don't fit in to a structure. They can be
        populated in derived classes and are requested in the base implementation
        of the correlation method.
    */
    //!@{
    //! Correlation between xccy basis and any yield or inflation in same currency
    QuantLib::Real xccyCorr_;
    //! Correlation between any yield and inflation in same currency
    QuantLib::Real infCorr_;
    //! Correlation between any yield volatility and inflation volatility in same currency
    QuantLib::Real infVolCorr_;
    //! IR Label2 level i.e. sub-curve correlation
    QuantLib::Real irSubCurveCorr_;
    //! IR correlation across currencies
    QuantLib::Real irInterCurrencyCorr_;
    //! Credit-Q residual intra correlation
    QuantLib::Real crqResidualIntraCorr_;
    //! Credit-Q non-residual intra correlation when same qualifier but different vertex/source
    QuantLib::Real crqSameIntraCorr_;
    //! Credit-Q non-residual intra correlation when different qualifier
    QuantLib::Real crqDiffIntraCorr_;
    //! Credit-NonQ residual intra correlation
    QuantLib::Real crnqResidualIntraCorr_;
    //! Credit-NonQ non-residual intra correlation when same underlying names
    QuantLib::Real crnqSameIntraCorr_;
    //! Credit-NonQ non-residual intra correlation when different underlying names
    QuantLib::Real crnqDiffIntraCorr_;
    //! Credit-NonQ non-residual inter bucket correlation
    QuantLib::Real crnqInterCorr_;
    //! FX correlation
    QuantLib::Real fxCorr_;
    //! Base correlation risk factor correlation
    QuantLib::Real basecorrCorr_;
    //!@}
    //! Margin Period of risk in days
    QuantLib::Size mporDays_;
};

} // namespace analytics
} // namespace ore
