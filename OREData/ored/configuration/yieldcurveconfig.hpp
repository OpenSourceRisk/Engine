/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2023 Oleg Kulkov
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

/*! \file ored/configuration/yieldcurveconfig.hpp
    \brief Yield curve configuration classes
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/bootstrapconfig.hpp>
#include <ored/configuration/curveconfig.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <ql/patterns/visitor.hpp>
#include <ql/types.hpp>
#include <ql/termstructures/bootstraphelper.hpp>

#include <boost/none.hpp>
#include <boost/optional.hpp>
#include <ql/shared_ptr.hpp>

#include <set>
#include <map>

namespace ore {
namespace data {
using boost::optional;
using ore::data::XMLNode;
using QuantLib::AcyclicVisitor;
using QuantLib::Real;
using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

//! Base class for yield curve segments.
/*!
  \ingroup configuration
*/
class YieldCurveSegment : public XMLSerializable {
public:
    //! supported segment types
    enum class Type {
        Zero,
        ZeroSpread,
        Discount,
        Deposit,
        FRA,
        Future,
        OIS,
        Swap,
        AverageOIS,
        TenorBasis,
        TenorBasisTwo,
        BMABasis,
        FXForward,
        CrossCcyBasis,
        CrossCcyFixFloat,
        DiscountRatio,
        FittedBond,
        WeightedAverage,
        YieldPlusDefault,
        IborFallback,
        BondYieldShifted
    };
    //! Default destructor
    virtual ~YieldCurveSegment() {}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    Type type() const { return type_; }
    // TODO: why typeID?
    const string& typeID() const { return typeID_; }
    const string& conventionsID() const { return conventionsID_; }
    const QuantLib::Pillar::Choice pillarChoice() const { return pillarChoice_; }
    Size priority() const { return priority_; }
    Size minDistance() const { return minDistance_; }
    const vector<pair<string, bool>>& quotes() const { return quotes_; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&);
    //@}

protected:
    //! \name Constructors
    //@{
    //! Default constructor
    YieldCurveSegment() {}
    //! Detailed constructor - assumes all quotes are mandatory
    YieldCurveSegment(const string& typeID, const string& conventionsID, const vector<string>& quotes);
    //@}

    //! Quote and optional flag pair
    vector<pair<string, bool>> quotes_;

    //! Utility to build a quote, optional flag defaults to false
    pair<string, bool> quote(const string& name, bool opt = false) { return make_pair(name, opt); }

private:
    // TODO: why type and typeID?
    Type type_;
    string typeID_;
    string conventionsID_;
    QuantLib::Pillar::Choice pillarChoice_ = QuantLib::Pillar::LastRelevantDate;
    Size priority_ = 0;
    Size minDistance_ = 1;
};

//! Direct yield curve segment
/*!
  A direct yield curve segment is used when the segments is entirely defined by
  a set of quotes that are passed to the constructor.

  \ingroup configuration
*/
class DirectYieldCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    DirectYieldCurveSegment() {}
    //! Detailed constructor
    DirectYieldCurveSegment(const string& typeID, const string& conventionsID, const vector<string>& quotes);
    //! Default destructor
    virtual ~DirectYieldCurveSegment() {}
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}
};

//! Simple yield curve segment
/*!
  A simple yield curve segment is used when the curve segment is determined by
  a set of quotes and a projection curve.

  \ingroup configuration
*/
class SimpleYieldCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    SimpleYieldCurveSegment() {}
    //! Detailed constructor
    SimpleYieldCurveSegment(const string& typeID, const string& conventionsID, const vector<string>& quotes,
                            const string& projectionCurveID = string());
    //! Default destructor
    virtual ~SimpleYieldCurveSegment() {}
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& projectionCurveID() const { return projectionCurveID_; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

private:
    string projectionCurveID_;
};

//! Average OIS yield curve segment
/*!
  The average OIS yield curve segment is used e.g. for USD OIS curve building where
  the curve segment is determined by  a set of composite quotes and a projection curve.
  The composite quote is  represented here as a pair of quote strings, a tenor basis spread
  and an interest rate swap quote.

  \ingroup configuration
*/
class AverageOISYieldCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    AverageOISYieldCurveSegment() {}
    //! Detailed constructor
    AverageOISYieldCurveSegment(const string& typeID, const string& conventionsID, const vector<string>& quotes,
                                const string& projectionCurveID);
    //! Default destructor
    virtual ~AverageOISYieldCurveSegment() {}
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& projectionCurveID() const { return projectionCurveID_; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

private:
    string projectionCurveID_;
};

//! Tenor Basis yield curve segment
/*!
  Yield curve building from tenor basis swap quotes requires a set of tenor
  basis spread quotes and the projection curve for either the shorter or the longer tenor
  which acts as the reference curve.

  \ingroup configuration
*/
class TenorBasisYieldCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    TenorBasisYieldCurveSegment() {}
    //! Detailed constructor
    TenorBasisYieldCurveSegment(const string& typeID, const string& conventionsID, const vector<string>& quotes,
                                const string& receiveProjectionCurveID, const string& payProjectionCurveID);
    //! Default destructor
    virtual ~TenorBasisYieldCurveSegment() {}
    //@}

    //!\name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& receiveProjectionCurveID() const { return receiveProjectionCurveID_; }
    const string& payProjectionCurveID() const { return payProjectionCurveID_; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

private:
    string receiveProjectionCurveID_;
    string payProjectionCurveID_;
};

//! Cross Currency yield curve segment
/*!
  Cross currency basis spread adjusted discount curves for 'domestic' currency cash flows
  are built using this segment type which requires cross currency basis spreads quotes,
  the spot FX quote ID and at least the 'foreign' discount curve ID.
  Projection curves for both currencies can be provided as well for consistency with
  tenor basis in each currency.

  \ingroup configuration
*/
class CrossCcyYieldCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    CrossCcyYieldCurveSegment() {}
    //! Detailed constructor
    CrossCcyYieldCurveSegment(const string& typeID, const string& conventionsID, const vector<string>& quotes,
                              const string& spotRateID, const string& foreignDiscountCurveID,
                              const string& domesticProjectionCurveID = string(),
                              const string& foreignProjectionCurveID = string());
    //! Default destructor
    virtual ~CrossCcyYieldCurveSegment() {}
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& spotRateID() const { return spotRateID_; }
    const string& foreignDiscountCurveID() const { return foreignDiscountCurveID_; }
    const string& domesticProjectionCurveID() const { return domesticProjectionCurveID_; }
    const string& foreignProjectionCurveID() const { return foreignProjectionCurveID_; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

private:
    string spotRateID_;
    string foreignDiscountCurveID_;
    string domesticProjectionCurveID_;
    string foreignProjectionCurveID_;
};

//! Zero Spreaded yield curve segment
/*!
  A zero spreaded segment is used to build a yield curve from zero spread quotes and
  a reference yield curve.

  \ingroup configuration
*/
class ZeroSpreadedYieldCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    ZeroSpreadedYieldCurveSegment() {}
    //! Detailed constructor
    ZeroSpreadedYieldCurveSegment(const string& typeID, const string& conventionsID, const vector<string>& quotes,
                                  const string& referenceCurveID);
    //! Default destructor
    virtual ~ZeroSpreadedYieldCurveSegment() {}
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& referenceCurveID() const { return referenceCurveID_; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

private:
    string referenceCurveID_;
};

//! Weighted average yield curve segment
/*!
  A weighted average segment is used to build a yield curve from two source curves and weights. The
  resulting discount factor is the weighted sum of the source curves' discount factors.

  \ingroup configuration
*/
class WeightedAverageYieldCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    WeightedAverageYieldCurveSegment() {}
    //! Detailed constructor
    WeightedAverageYieldCurveSegment(const string& typeID, const string& referenceCurveID1,
                                     const string& referenceCurveID2, const Real weight1, const Real weight2);
    //! Default destructor
    virtual ~WeightedAverageYieldCurveSegment() {}
    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& referenceCurveID1() const { return referenceCurveID1_; }
    const string& referenceCurveID2() const { return referenceCurveID2_; }
    Real weight1() const { return weight1_; }
    Real weight2() const { return weight2_; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

private:
    string referenceCurveID1_, referenceCurveID2_;
    double weight1_, weight2_;
};

//! Yield plus default curves segment
/*!
  A yield plus default curves segment is used to build a yield curve from a source yield curve and
  a weighted sum of default curves interpreted as zero curves (zero recovery, hazard rate =
  instantaneous forward rate)

  \ingroup configuration
*/
class YieldPlusDefaultYieldCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    YieldPlusDefaultYieldCurveSegment() {}
    //! Detailed constructor
    YieldPlusDefaultYieldCurveSegment(const string& typeID, const string& referenceCurveID,
                                      const std::vector<std::string>& defaultCurveIDs,
                                      const std::vector<Real>& weights);
    //! Default destructor
    virtual ~YieldPlusDefaultYieldCurveSegment() {}
    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& referenceCurveID() const { return referenceCurveID_; }
    const std::vector<std::string>& defaultCurveIDs() { return defaultCurveIDs_; }
    const std::vector<Real>& weights() { return weights_; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

private:
    string referenceCurveID_;
    std::vector<std::string> defaultCurveIDs_;
    std::vector<Real> weights_;
};

//! Discount ratio yield curve segment
/*! Used to configure a QuantExt::DiscountRatioModifiedCurve.

    \ingroup configuration
*/
class DiscountRatioYieldCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    DiscountRatioYieldCurveSegment() {}
    //! Detailed constructor
    DiscountRatioYieldCurveSegment(const std::string& typeId, const std::string& baseCurveId,
                                   const std::string& baseCurveCurrency, const std::string& numeratorCurveId,
                                   const std::string& numeratorCurveCurrency, const std::string& denominatorCurveId,
                                   const std::string& denominatorCurveCurrency);
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& baseCurveId() const { return baseCurveId_; }
    const string& baseCurveCurrency() const { return baseCurveCurrency_; }
    const string& numeratorCurveId() const { return numeratorCurveId_; }
    const string& numeratorCurveCurrency() const { return numeratorCurveCurrency_; }
    const string& denominatorCurveId() const { return denominatorCurveId_; }
    const string& denominatorCurveCurrency() const { return denominatorCurveCurrency_; }
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor& v) override;
    //@}

private:
    std::string baseCurveId_;
    std::string baseCurveCurrency_;
    std::string numeratorCurveId_;
    std::string numeratorCurveCurrency_;
    std::string denominatorCurveId_;
    std::string denominatorCurveCurrency_;
};

//! FittedBond yield curve segment
/*!
  A bond segment is used to build a yield curve from liquid bond quotes.

  \ingroup configuration
*/
class FittedBondYieldCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    FittedBondYieldCurveSegment() {}
    //! Detailed constructor
    FittedBondYieldCurveSegment(const string& typeID, const vector<string>& quotes,
                                const map<string, string>& iborIndexCurves, const bool extrapolateFlat);

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const map<string, string>& iborIndexCurves() const { return iborIndexCurves_; }
    const bool extrapolateFlat() const { return extrapolateFlat_; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

private:
    map<string, string> iborIndexCurves_;
    bool extrapolateFlat_;
};

//! Ibor Fallback yield curve segment
/*!
  A curve segment to build a Ibor forwarding curve from an OIS RFR index and a fallback spread

  \ingroup configuration
*/
class IborFallbackCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    IborFallbackCurveSegment() {}
    //! Detailed constructor
    IborFallbackCurveSegment(const string& typeID, const string& iborIndex, const string& rfrCurve,
                             const boost::optional<string>& rfrIndex, const boost::optional<Real>& spread);

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& iborIndex() const { return iborIndex_; }
    const string& rfrCurve() const { return rfrCurve_; }
    const boost::optional<string>& rfrIndex() const { return rfrIndex_; }
    const boost::optional<Real>& spread() const { return spread_; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

private:
    string iborIndex_;
    string rfrCurve_;
    boost::optional<string> rfrIndex_;
    boost::optional<Real> spread_;
};

//! Bond yield shifted yield curve segment
/*!
  An average spread between curve and bond's yield is used to shift an existing yield curve.

  \ingroup configuration
*/
class BondYieldShiftedYieldCurveSegment : public YieldCurveSegment {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    BondYieldShiftedYieldCurveSegment() {}
    //! Detailed constructor
    BondYieldShiftedYieldCurveSegment(const string& typeID, const string& referenceCurveID, const vector<string>& quotes,
                                      const map<string, string>& iborIndexCurves, const bool extrapolateFlat);

    //! Default destructor
    virtual ~BondYieldShiftedYieldCurveSegment() {}
    //@}
    
    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& referenceCurveID() const { return referenceCurveID_; }
    const map<string, string>& iborIndexCurves() const { return iborIndexCurves_; }
    const bool extrapolateFlat() const { return extrapolateFlat_; }
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

private:
    string referenceCurveID_;
    map<string, string> iborIndexCurves_;
    bool extrapolateFlat_;
    boost::optional<Real> spread_;
    boost::optional<Real> bondYield_;
};

//! Yield Curve configuration
/*!
  Wrapper class containing all yield curve segments needed to build a yield curve.

  \ingroup configuration
 */
class YieldCurveConfig : public CurveConfig {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    YieldCurveConfig() {}
    //! Detailed constructor
    YieldCurveConfig(const string& curveID, const string& curveDescription, const string& currency,
                     const string& discountCurveID, const vector<QuantLib::ext::shared_ptr<YieldCurveSegment>>& curveSegments,
                     const string& interpolationVariable = "Discount", const string& interpolationMethod = "LogLinear",
                     const string& zeroDayCounter = "A365", bool extrapolation = true,
                     const BootstrapConfig& bootstrapConfig = BootstrapConfig(),
                     const Size mixedInterpolationCutoff = 1);
    //! Default destructor
    virtual ~YieldCurveConfig() {}
    //@}

    //! \name Serialization
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& currency() const { return currency_; }
    const string& discountCurveID() const { return discountCurveID_; }
    const vector<QuantLib::ext::shared_ptr<YieldCurveSegment>>& curveSegments() const { return curveSegments_; }
    const string& interpolationVariable() const { return interpolationVariable_; }
    const string& interpolationMethod() const { return interpolationMethod_; }
    Size mixedInterpolationCutoff() const { return mixedInterpolationCutoff_; }
    const string& zeroDayCounter() const { return zeroDayCounter_; }
    bool extrapolation() const { return extrapolation_; }
    const BootstrapConfig& bootstrapConfig() const { return bootstrapConfig_; }
    //@}

    //! \name Setters
    //@{
    string& interpolationVariable() { return interpolationVariable_; }
    string& interpolationMethod() { return interpolationMethod_; }
    Size& mixedInterpolationCutoff() { return mixedInterpolationCutoff_; }
    string& zeroDayCounter() { return zeroDayCounter_; }
    bool& extrapolation() { return extrapolation_; }
    void setBootstrapConfig(const BootstrapConfig& bootstrapConfig) { bootstrapConfig_ = bootstrapConfig; }
    //@}

    const vector<string>& quotes() override;

private:
    void populateRequiredCurveIds();

    // Mandatory members
    string currency_;
    string discountCurveID_;
    vector<QuantLib::ext::shared_ptr<YieldCurveSegment>> curveSegments_;

    // Optional members
    string interpolationVariable_;
    string interpolationMethod_;
    string zeroDayCounter_;
    bool extrapolation_;
    BootstrapConfig bootstrapConfig_;
    Size mixedInterpolationCutoff_;
};

// Map form curveID to YieldCurveConfig
using YieldCurveConfigMap = std::map<string, QuantLib::ext::shared_ptr<YieldCurveConfig>>;

} // namespace data
} // namespace ore
