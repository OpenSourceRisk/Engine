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

#include <orea/simm/simmconfigurationbase.hpp>
#include <orea/simm/utilities.hpp>

#include <boost/math/distributions/normal.hpp>
#include <boost/optional/optional_io.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/matrix.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <qle/indexes/ibor/termrateindex.hpp>

#include <boost/algorithm/string/predicate.hpp>

using namespace QuantLib;

using ore::data::checkCurrency;
using ore::data::parseInteger;
using QuantLib::Integer;
using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

// Ease syntax
using RiskType = SimmConfiguration::RiskType;
using RiskClass = SimmConfiguration::RiskClass;

namespace {

// Useful for the implementation of a number of methods below
vector<string> lookup(const RiskType& rt, const map<RiskType, vector<string>>& m) {
    if (m.count(rt) > 0) {
        return m.at(rt);
    } else {
        return {};
    }
}

string periodToLabels2(const QuantLib::Period& p) {
    if ((p.units() == Months && p.length() == 3) || (p.units() == Weeks && p.length() == 13)) {
        return "Libor3m";
    } else if ((p.units() == Months && p.length() == 6) || (p.units() == Weeks && p.length() == 26)) {
        return "Libor6m";
    } else if ((p.units() == Days && p.length() == 1) || p == 1 * Weeks) {
        // 7 days here is based on ISDA SIMM FAQ and Implementation Questions, Sep 4, 2019 Section E.9
        // Sub curve to be used for CNY seven-day repo rate (closest is OIS).
        return "OIS";
    } else if ((p.units() == Months && p.length() == 1) || (p.units() == Weeks && p.length() == 2) ||
               (p.units() == Weeks && p.length() == 4) || (p.units() == Days && p.length() >= 28 && p.length() <= 31)) {
        // 2 weeks here is based on ISDA SIMM Methodology paragraph 14:
        // "Any sub curve not given on the above list should be mapped to its closest equivalent."
        // A 2 week rate is more like sub-period than OIS.
        return "Libor1m";
    } else if ((p.units() == Months && p.length() == 12) || (p.units() == Years && p.length() == 1) ||
               (p.units() == Weeks && p.length() == 52)) {
        return "Libor12m";
    } else {
        return "";
    }
}

} // anonymous namespace

SimmConfigurationBase::SimmConfigurationBase(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                             const std::string& name, const std::string version, Size mporDays)
    : name_(name), version_(version), simmBucketMapper_(simmBucketMapper), mporDays_(mporDays) {}

bool SimmConfigurationBase::hasBuckets(const RiskType& rt) const { return simmBucketMapper_->hasBuckets(rt); }

string SimmConfigurationBase::bucket(const RiskType& rt, const string& qualifier) const {
    QL_REQUIRE(hasBuckets(rt), "The SIMM risk type " << rt << " does not have buckets");
    return simmBucketMapper_->bucket(rt, qualifier);
}

vector<string> SimmConfigurationBase::buckets(const RiskType& rt) const {
    QL_REQUIRE(isValidRiskType(rt),
               "The risk type " << rt << " is not valid for SIMM configuration with name" << name_);
    return lookup(rt, mapBuckets_);
}

vector<string> SimmConfigurationBase::labels1(const RiskType& rt) const {
    QL_REQUIRE(isValidRiskType(rt),
               "The risk type " << rt << " is not valid for SIMM configuration with name" << name_);
    return lookup(rt, mapLabels_1_);
}

vector<string> SimmConfigurationBase::labels2(const RiskType& rt) const {
    QL_REQUIRE(isValidRiskType(rt),
               "The risk type " << rt << " is not valid for SIMM configuration with name" << name_);
    return lookup(rt, mapLabels_2_);
}

string SimmConfigurationBase::labels2(const QuantLib::Period& p) const {
    string label2 = periodToLabels2(p);
    QL_REQUIRE(!label2.empty(), "Could not determine SIMM Label2 for period " << p);
    return label2;
}

string SimmConfigurationBase::labels2(const boost::shared_ptr<InterestRateIndex>& irIndex) const {

    string label2;
    if (boost::algorithm::starts_with(irIndex->name(), "BMA")) {
        // There was no municipal until later so override this in
        // derived configurations and use 'Prime' in base
        label2 = "Prime";
    } else if (irIndex->familyName() == "Prime") {
        label2 = "Prime";
    } else if(boost::dynamic_pointer_cast<QuantExt::TermRateIndex>(irIndex) != nullptr) {
	// see ISDA-SIMM-FAQ_Methodology-and-Implementation_20220323_clean.pdf: E.8 Term RFR rate risk should be treated as RFR rate risk
        label2 = "OIS";
    } else {
        label2 = periodToLabels2(irIndex->tenor());
        QL_REQUIRE(!label2.empty(), "Could not determine SIMM Label2 for index " << irIndex->name());
    }
    return label2;
}

QuantLib::Real SimmConfigurationBase::weight(const RiskType& rt, boost::optional<string> qualifier,
                                             boost::optional<std::string> label_1, const std::string&) const {

    QL_REQUIRE(isValidRiskType(rt),
               "The risk type " << rt << " is not valid for SIMM configuration with name" << name_);

    // If the risk type has flat risk weights, ignore last 2 parameters
    if (rwRiskType_.count(rt) > 0) {
        return rwRiskType_.at(rt);
    }

    // We now at least have bucket dependent risk weights so check qualifier and buckets
    QL_REQUIRE(qualifier, "Need a valid qualifier to return a risk weight because the risk type "
                              << rt << " has bucket dependent risk weights");
    QL_REQUIRE(!buckets(rt).empty(), "Could not find any buckets for risk type " << rt);
    string bucket = simmBucketMapper_->bucket(rt, *qualifier);

    // If risk weight for this risk type is bucket-dependent
    if (rwBucket_.count(rt) > 0) {
        auto idx = labelIndex(bucket, buckets(rt));

        return rwBucket_.at(rt)[idx];
    }

    // If we get to here, risk weight must depend on risk type, bucket and Label1
    if (rwLabel_1_.count({rt, bucket}) > 0) {
        QL_REQUIRE(label_1, "Need a valid Label1 value to return a risk weight because the risk type "
                                << rt << " has bucket and Label1 dependent risk weights");
        QL_REQUIRE(!labels1(rt).empty(), "Could not find any Label1 values for risk type " << rt);
        auto idx = labelIndex(*label_1, labels1(rt));

        return rwLabel_1_.at({rt, bucket})[idx];
    }

    // If we get to here, we have failed to get a risk weight
    QL_FAIL("Could not find a risk weight for (risk type, qualifier, Label1) = (" << rt << "," << qualifier << ","
                                                                                  << label_1 << ")");
}

QuantLib::Real SimmConfigurationBase::curvatureWeight(const RiskType& rt, const std::string& label_1) const {

    QL_REQUIRE(isValidRiskType(rt),
               "The risk type " << rt << " is not valid for SIMM configuration with name" << name_);

    QL_REQUIRE(curvatureWeights_.count(rt) > 0, "The risk type " << rt << " does not have a curvature weight.");

    QL_REQUIRE(!labels1(rt).empty(), "Could not find any Label1 values for risk type " << rt);
    auto idx = labelIndex(label_1, labels1(rt));

    return curvatureWeights_.at(rt)[idx];
}

Real SimmConfigurationBase::historicalVolatilityRatio(const RiskType& rt) const {
    QL_REQUIRE(isValidRiskType(rt),
               "The risk type " << rt << " is not valid for SIMM configuration with name" << name_);

    if (historicalVolatilityRatios_.count(rt) > 0) {
        return historicalVolatilityRatios_.at(rt);
    } else {
        return 1.0;
    }
}

Real SimmConfigurationBase::sigma(const RiskType& rt, boost::optional<std::string> qualifier,
                                  boost::optional<std::string> label_1, const std::string& calculationCurrency) const {

    Real sigmaMultiplier = SimmConfigurationBase::sigmaMultiplier();

    Real result = 1.0;
    Real deltaRiskWeight;
    std::string ccy1, ccy2;
    switch (rt) {
    case RiskType::CommodityVol:
        deltaRiskWeight = weight(RiskType::Commodity, qualifier, label_1);
        result = sigmaMultiplier * deltaRiskWeight;
        break;
    case RiskType::EquityVol:
        deltaRiskWeight = weight(RiskType::Equity, qualifier, label_1);
        result = sigmaMultiplier * deltaRiskWeight;
        break;
    case RiskType::FXVol:
        /*For FX vega (which depends on a pair of currencies), the risk weight to use here is
          the FX delta risk weight value. Before ISDA 2.2 this value is common for all currency pairs.
          For ISDA 2.2 the FX delta is from the sensitivity table, given explicitly
          in section I of the ISDA document, whose row is the FX volatility group of the first currency and
          whose column is the FX volatility group of the second currency.
         */
        ccy1 = (*qualifier).substr(0, 3);
        ccy2 = (*qualifier).substr(3, 3);
        // make sure they parse as currencies
        ore::data::parseCurrency(ccy1);
        ore::data::parseCurrency(ccy2);
        deltaRiskWeight = weight(RiskType::FX, ccy1, label_1, ccy2);
        result = sigmaMultiplier * deltaRiskWeight;
        break;
    default:
        break;
    }

    return result;
}

QuantLib::Real SimmConfigurationBase::correlation(const RiskType& firstRt, const string& firstQualifier,
                                                  const string& firstLabel_1, const string& firstLabel_2,
                                                  const RiskType& secondRt, const string& secondQualifier,
                                                  const string& secondLabel_1, const string& secondLabel_2,
                                                  const std::string&) const {

    // First check that we have valid risk types for the configuration in question
    QL_REQUIRE(isValidRiskType(firstRt),
               "The risk type " << firstRt << " is not valid for SIMM configuration with name" << name());
    QL_REQUIRE(isValidRiskType(secondRt),
               "The risk type " << secondRt << " is not valid for SIMM configuration with name" << name());

    // Deal with trivial case of everything equal
    if (firstRt == secondRt && firstQualifier == secondQualifier && firstLabel_1 == secondLabel_1 &&
        firstLabel_2 == secondLabel_2) {
        return 1.0;
    }

    // Deal with case of different risk types
    if ((firstRt != secondRt) && (firstQualifier == secondQualifier)) {
        if (((firstRt == RiskType::IRCurve || firstRt == RiskType::Inflation) && secondRt == RiskType::XCcyBasis) ||
            (firstRt == RiskType::XCcyBasis && (secondRt == RiskType::IRCurve || secondRt == RiskType::Inflation))) {
            // Between xccy basis and any yield or inflation in same currency
            return xccyCorr_;
        }
        if ((firstRt == RiskType::IRCurve && secondRt == RiskType::Inflation) ||
            (firstRt == RiskType::Inflation && secondRt == RiskType::IRCurve)) {
            // Between any yield and inflation in same currency
            return infCorr_;
        }
        if ((firstRt == RiskType::IRVol && secondRt == RiskType::InflationVol) ||
            (firstRt == RiskType::InflationVol && secondRt == RiskType::IRVol)) {
            // Between any yield volatility and inflation volatility in same currency
            return infVolCorr_;
        }
    }

    // Deal with IRCurve and IRVol correlations
    if ((firstRt == RiskType::IRCurve && secondRt == RiskType::IRCurve) ||
        (firstRt == RiskType::IRVol && secondRt == RiskType::IRVol)) {

        // If the qualifiers, i.e. currencies, are the same
        if (firstQualifier == secondQualifier) {
            // Label2 level, i.e. sub-curve, correlations
            if (firstLabel_2 != secondLabel_2) {
                QL_REQUIRE(
                    firstLabel_1 == "" && secondLabel_1 == "",
                    "When asking for Label2 level correlations, "
                        << "the Label1 level values should both contain the default parameter i.e. empty string");
                QL_REQUIRE(firstRt != RiskType::IRVol, "There is no correlation at the Label2 level for Risk_IRVol");
                return irSubCurveCorr_;
            }

            // Label1 level, i.e. tenor, correlations
            auto idx_1 = labelIndex(firstLabel_1, labels1(firstRt));
            auto idx_2 = labelIndex(secondLabel_1, labels1(secondRt));
            return irTenorCorrelation_[idx_1][idx_2];
        } else {
            // If the qualifiers, i.e. currencies, are not the same
            return irInterCurrencyCorr_;
        }
    }

    // Deal with inflation volatility correlations
    if (firstRt == RiskType::InflationVol && secondRt == RiskType::InflationVol) {
        return 1.0;
    }

    // Deal with CreditQ correlations
    if ((firstRt == RiskType::CreditQ && secondRt == RiskType::CreditQ) ||
        (firstRt == RiskType::CreditVol && secondRt == RiskType::CreditVol)) {

        // Get the bucket of each qualifier
        string bucket_1 = simmBucketMapper_->bucket(firstRt, firstQualifier);
        string bucket_2 = simmBucketMapper_->bucket(secondRt, secondQualifier);

        // Residual is special
        if (bucket_1 == "Residual" || bucket_2 == "Residual") {
            if (bucket_1 == bucket_2) {
                // Both Residual
                return crqResidualIntraCorr_;
            } else {
                // One is a residual bucket and the other is not
                return 0.0;
            }
        }

        // Non-residual
        if (bucket_1 == bucket_2) {
            // If same bucket
            if (firstQualifier != secondQualifier) {
                // If different qualifier (i.e. here issuer/seniority)
                return crqDiffIntraCorr_;
            } else {
                // If same qualifier (i.e. here issuer/seniority)
                return crqSameIntraCorr_;
            }
        } else {
            // Get the bucket indices of each qualifier for reading the matrix
            Integer b_1 = parseInteger(bucket_1) - 1;
            Integer b_2 = parseInteger(bucket_2) - 1;
            // If different buckets, return the non-residual inter-bucket correlation
            return interBucketCorrelation_.at(RiskType::CreditQ)[b_1][b_2];
        }
    }

    // Deal with CreditNonQ correlations
    if ((firstRt == RiskType::CreditNonQ && secondRt == RiskType::CreditNonQ) ||
        (firstRt == RiskType::CreditVolNonQ && secondRt == RiskType::CreditVolNonQ)) {

        // Get the bucket of each qualifier
        string bucket_1 = simmBucketMapper_->bucket(firstRt, firstQualifier);
        string bucket_2 = simmBucketMapper_->bucket(secondRt, secondQualifier);

        // Residual is special
        if (bucket_1 == "Residual" || bucket_2 == "Residual") {
            if (bucket_1 == bucket_2) {
                // Both Residual
                return crnqResidualIntraCorr_;
            } else {
                // One is a residual bucket and the other is not
                return 0.0;
            }
        }

        // Non-residual
        if (bucket_1 == bucket_2) {
            SimmVersion version = parseSimmVersion(version_);
            SimmVersion thresholdVersion = SimmVersion::V2_2;
            if (version >= thresholdVersion) {
                // In ISDA SIMM version 2.2 or greater, the CRNQ correlations differ depending on whether or not
                // the entities have the same group name i.e. CMBX.
                return firstLabel_2 == secondLabel_2 ? crnqSameIntraCorr_ : crnqDiffIntraCorr_;
            }
            // TODO:
            // If same bucket. For ISDA SIMM < 2.2 there is a section in the documentation where
            // you choose between a correlation if the underlying names are the same and another
            // correlation if the underlying names are different. The underlying names being the
            // same is defined in terms of an overlap of 80% in notional terms in underlying names.
            // Don't know how to pass this down yet so we just go on qualifiers here.
            return firstQualifier == secondQualifier ? crnqSameIntraCorr_ : crnqDiffIntraCorr_;
        } else {
            // If different buckets, return the inter-bucket correlation
            return crnqInterCorr_;
        }
    }

    // Deal with Equity correlations
    if ((firstRt == RiskType::Equity && secondRt == RiskType::Equity) ||
        (firstRt == RiskType::EquityVol && secondRt == RiskType::EquityVol)) {

        // Get the bucket of each qualifier
        string bucket_1 = simmBucketMapper_->bucket(firstRt, firstQualifier);
        string bucket_2 = simmBucketMapper_->bucket(secondRt, secondQualifier);

        // Residual is special, 0 correlation inter and intra except if same qualifier
        if (bucket_1 == "Residual" || bucket_2 == "Residual") {
            return firstQualifier == secondQualifier ? 1.0 : 0.0;
        }

        // Non-residual
        // Get the bucket index of each qualifier
        Integer b_1 = parseInteger(bucket_1) - 1;
        Integer b_2 = parseInteger(bucket_2) - 1;

        if (b_1 == b_2) {
            // If same bucket, return the intra-bucket correlation
            return firstQualifier == secondQualifier ? 1.0 : intraBucketCorrelation_.at(RiskType::Equity)[b_1];
        } else {
            // If different buckets, return the inter-bucket correlation
            return interBucketCorrelation_.at(RiskType::Equity)[b_1][b_2];
        }
    }

    // Deal with Commodity correlations
    if ((firstRt == RiskType::Commodity && secondRt == RiskType::Commodity) ||
        (firstRt == RiskType::CommodityVol && secondRt == RiskType::CommodityVol)) {

        // Get the bucket index of each qualifier
        Integer b_1 = parseInteger(simmBucketMapper_->bucket(firstRt, firstQualifier)) - 1;
        Integer b_2 = parseInteger(simmBucketMapper_->bucket(secondRt, secondQualifier)) - 1;

        if (b_1 == b_2) {
            // If same bucket, return the intra-bucket correlation
            return firstQualifier == secondQualifier ? 1.0 : intraBucketCorrelation_.at(RiskType::Commodity)[b_1];
        } else {
            // If different buckets, return the inter-bucket correlation
            return interBucketCorrelation_.at(RiskType::Commodity)[b_1][b_2];
        }
    }

    // Deal with FX correlations
    // TODO:
    // For FXVol, qualifier is a currency pair. Is it possible to get here
    // and to have something like secondQualifier = USDJPY and firstQualifier =
    // JPYUSD an to give back 0.5
    if ((firstRt == RiskType::FX && secondRt == RiskType::FX) ||
        (firstRt == RiskType::FXVol && secondRt == RiskType::FXVol)) {
        return firstQualifier == secondQualifier ? 1.0 : fxCorr_;
    }

    // Both risk types Base correlation
    if (firstRt == RiskType::BaseCorr && secondRt == RiskType::BaseCorr) {
        return basecorrCorr_;
    }

    // If we get to here
    return 0.0;
}

Real SimmConfigurationBase::sigmaMultiplier() const {
    // return 2.19486471232815;
    // Use boost inverse normal here as opposed to QL. Using QL inverse normal
    // will cause the ISDA SIMM unit tests to fail

    /* We write sqrt(365.0 / (1.4 * mporDays_)) so that this function is
       sqrt(365.0 / 14) for MPOR = 10 as expected and sqrt(365.0 / 1.4) for MPOR = 1.
       This is described in SIMM:Technical Paper (Version 10), Section I.2 */
    return sqrt(365.0 / (1.4 * mporDays_)) / boost::math::quantile(boost::math::normal(), 0.99);
}

QuantLib::Real SimmConfigurationBase::correlationRiskClasses(const RiskClass& rc_1, const RiskClass& rc_2) const {

    // Get the row and column index in the matrix of the risk classes
    Size index_1 = static_cast<Size>(rc_1);
    QL_REQUIRE(index_1 < riskClassCorrelation_.rows(),
               "The risk class " << rc_1 << "(" << index_1
                                 << ") is not consistent with the number of rows in the correlation matrix ("
                                 << riskClassCorrelation_.rows() << ")");
    Size index_2 = static_cast<Size>(rc_2);
    QL_REQUIRE(index_2 < riskClassCorrelation_.columns(),
               "The risk class " << rc_2 << "(" << index_2
                                 << ") is not consistent with the number of columns in the correlation matrix ("
                                 << riskClassCorrelation_.columns() << ")");

    return riskClassCorrelation_(index_1, index_2);
}

Size SimmConfigurationBase::labelIndex(const string& label, const vector<string>& labels) const {
    QL_REQUIRE(!labels.empty(), "Labels cannot be empty");
    auto it = std::find(labels.begin(), labels.end(), label);
    QL_REQUIRE(it != labels.end(), "The label '" << label << "' could not be found in the labels.");
    return std::distance(labels.begin(), it);
}

void SimmConfigurationBase::addLabels2Impl(const RiskType& rt, const string& label_2) {
    // Only currently need this for risk type CreditQ
    QL_REQUIRE(rt == RiskType::CreditQ, "addLabels2 only supported for RiskType_CreditQ");

    // Expect label of the form "CCY" or "CCY,Sec"
    if (label_2.size() == 3) {
        QL_REQUIRE(checkCurrency(label_2), "Expected a Label2 of size 3 to be a valid currency code");
    }

    if (label_2.size() == 7) {
        QL_REQUIRE(checkCurrency(label_2.substr(0, 3)), "Expected first 3 characters of Label2 ("
                                                            << label_2.substr(0, 3) << ") to be a valid currency code");
        QL_REQUIRE(label_2.substr(4) == "Sec", "Last 3 characters of Label2 should be 'Sec'");
        QL_REQUIRE(checkCurrency(label_2.substr(3, 1)), "Delimiter should be a comma");
    }

    QL_FAIL("Label2 passed to addLabels2 is unusable for RiskType " << rt);
}

} // namespace analytics
} // namespace ore
