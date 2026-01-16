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

#ifndef ored_yieldcurveconfig_i
#define ored_yieldcurveconfig_i

%include std_set.i
%include ored_xmlutils.i
%include ored_curveconfig.i
%include ored_curvespec.i
%include ored_xmlutils.i

%{
using ore::data::YieldCurveConfig;
using ore::data::YieldCurveSegment;
using ore::data::DirectYieldCurveSegment;
using ore::data::SimpleYieldCurveSegment;
using ore::data::AverageOISYieldCurveSegment;
using ore::data::TenorBasisYieldCurveSegment;
using ore::data::CrossCcyYieldCurveSegment;
using ore::data::ZeroSpreadedYieldCurveSegment;
using ore::data::WeightedAverageYieldCurveSegment;
using ore::data::YieldPlusDefaultYieldCurveSegment;
using ore::data::DiscountRatioYieldCurveSegment;
using ore::data::FittedBondYieldCurveSegment;
using ore::data::IborFallbackCurveSegment;
using ore::data::BondYieldShiftedYieldCurveSegment;
using ore::data::XMLSerializable;

%}

%shared_ptr(YieldCurveSegment)
class YieldCurveSegment : public XMLSerializable {
public:
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
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    Type type() const;
    const std::string& conventionsID() const;
    const QuantLib::Pillar::Choice pillarChoice() const;
    Size priority() const;
    Size minDistance() const;
    const std::vector<pair<std::string, bool>>& quotes() const;
protected:
    YieldCurveSegment();
};


%shared_ptr(DirectYieldCurveSegment)
class DirectYieldCurveSegment : public YieldCurveSegment {
public:
    DirectYieldCurveSegment();
    DirectYieldCurveSegment(const std::string& typeID, const std::string& conventionsID, const std::vector<std::string>& quotes);
    ~DirectYieldCurveSegment();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;   
};

%shared_ptr(SimpleYieldCurveSegment)
class SimpleYieldCurveSegment : public YieldCurveSegment {
public:
    SimpleYieldCurveSegment();
    SimpleYieldCurveSegment(const std::string& typeID, const std::string& conventionsID, const std::vector<std::string>& quotes,
                            const std::string& projectionCurveID = std::string());
    ~SimpleYieldCurveSegment() {}
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& projectionCurveID() const;
};

%shared_ptr(AverageOISYieldCurveSegment)
class AverageOISYieldCurveSegment : public YieldCurveSegment {
public:
    AverageOISYieldCurveSegment();
    AverageOISYieldCurveSegment(const std::string& typeID, const std::string& conventionsID, const std::vector<std::string>& quotes,
                                const std::string& projectionCurveID);
    ~AverageOISYieldCurveSegment();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& projectionCurveID() const;
};

%shared_ptr(TenorBasisYieldCurveSegment)
class TenorBasisYieldCurveSegment : public YieldCurveSegment {
public:
    TenorBasisYieldCurveSegment();
    TenorBasisYieldCurveSegment(const std::string& typeID, const std::string& conventionsID, const std::vector<std::string>& quotes,
                                const std::string& receiveProjectionCurveID, const std::string& payProjectionCurveID);
    ~TenorBasisYieldCurveSegment();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& receiveProjectionCurveID() const;
    const std::string& payProjectionCurveID() const;
};

%shared_ptr(CrossCcyYieldCurveSegment)
class CrossCcyYieldCurveSegment : public YieldCurveSegment {
public:
    CrossCcyYieldCurveSegment();
    CrossCcyYieldCurveSegment(const std::string& typeID, const std::string& conventionsID, const std::vector<std::string>& quotes,
                              const std::string& spotRateID, const std::string& foreignDiscountCurveID,
                              const std::string& domesticProjectionCurveID = std::string(),
                              const std::string& foreignProjectionCurveID = std::string());
    ~CrossCcyYieldCurveSegment();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& spotRateID() const;
    const std::string& foreignDiscountCurveID() const;
    const std::string& domesticProjectionCurveID() const;
    const std::string& foreignProjectionCurveID() const;
};

%shared_ptr(ZeroSpreadedYieldCurveSegment)
class ZeroSpreadedYieldCurveSegment : public YieldCurveSegment {
public:
    ZeroSpreadedYieldCurveSegment();
    ZeroSpreadedYieldCurveSegment(const std::string& typeID, const std::string& conventionsID, const std::vector<std::string>& quotes,
                                  const std::string& referenceCurveID);
    ~ZeroSpreadedYieldCurveSegment();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& referenceCurveID() const;
};

%shared_ptr(WeightedAverageYieldCurveSegment)
class WeightedAverageYieldCurveSegment : public YieldCurveSegment {
public:
    WeightedAverageYieldCurveSegment();
    WeightedAverageYieldCurveSegment(const std::string& typeID, const std::string& referenceCurveID1,
                                     const std::string& referenceCurveID2, const Real weight1, const Real weight2);
    ~WeightedAverageYieldCurveSegment();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& referenceCurveID1() const;
    const std::string& referenceCurveID2() const;
    Real weight1() const;
    Real weight2() const;
};

%shared_ptr(YieldPlusDefaultYieldCurveSegment)
class YieldPlusDefaultYieldCurveSegment : public YieldCurveSegment {
public:
    YieldPlusDefaultYieldCurveSegment();
    YieldPlusDefaultYieldCurveSegment(const std::string& typeID, const std::string& referenceCurveID,
                                      const std::vector<std::string>& defaultCurveIDs,
                                      const std::vector<Real>& weights);
    ~YieldPlusDefaultYieldCurveSegment();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& referenceCurveID() const { return referenceCurveID_; }
    const std::vector<std::string>& defaultCurveIDs() { return defaultCurveIDs_; }
    const std::vector<Real>& weights() { return weights_; }
};

%shared_ptr(DiscountRatioYieldCurveSegment)
class DiscountRatioYieldCurveSegment : public YieldCurveSegment {
public:
    DiscountRatioYieldCurveSegment();
    DiscountRatioYieldCurveSegment(const std::string& typeId, const std::string& baseCurveId,
                                   const std::string& baseCurveCurrency, const std::string& numeratorCurveId,
                                   const std::string& numeratorCurveCurrency, const std::string& denominatorCurveId,
                                   const std::string& denominatorCurveCurrency);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& baseCurveId() const;
    const std::string& baseCurveCurrency() const;
    const std::string& numeratorCurveId() const;
    const std::string& numeratorCurveCurrency() const;
    const std::string& denominatorCurveId() const;
    const std::string& denominatorCurveCurrency() const;
};

%shared_ptr(FittedBondYieldCurveSegment)
class FittedBondYieldCurveSegment : public YieldCurveSegment {
public:
    FittedBondYieldCurveSegment();
    FittedBondYieldCurveSegment(const std::string& typeID, const std::vector<std::string>& quotes,
                                const std::map<std::string, std::string>& iborIndexCurves, const bool extrapolateFlat);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::map<std::string, std::string>& iborIndexCurves() const;
    const bool extrapolateFlat() const;
};

%shared_ptr(IborFallbackCurveSegment)
class IborFallbackCurveSegment : public YieldCurveSegment {
public:
    IborFallbackCurveSegment();
    IborFallbackCurveSegment(const std::string& typeID, const std::string& iborIndex, const std::string& rfrCurve,
                             const QuantLib::ext::optional<std::string>& rfrIndex, const QuantLib::ext::optional<Real>& spread);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& iborIndex() const;
    const std::string& rfrCurve() const;
    const QuantLib::ext::optional<std::string>& rfrIndex() const;
    const QuantLib::ext::optional<Real>& spread() const;
};

%shared_ptr(BondYieldShiftedYieldCurveSegment)
class BondYieldShiftedYieldCurveSegment : public YieldCurveSegment {
public:
    BondYieldShiftedYieldCurveSegment();
    BondYieldShiftedYieldCurveSegment(const std::string& typeID, const std::string& referenceCurveID, const std::vector<std::string>& quotes,
                                      const std::map<std::string, std::string>& iborIndexCurves, const bool extrapolateFlat);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& referenceCurveID() const;
    const std::map<std::string, std::string>& iborIndexCurves() const;
    const bool extrapolateFlat() const;
};


%template(YieldCurveSegmentVector) std::vector<ext::shared_ptr<YieldCurveSegment>>;

%shared_ptr(YieldCurveConfig)
class YieldCurveConfig : public CurveConfig {
public:
    YieldCurveConfig();
    YieldCurveConfig(const std::string& curveID, const std::string& curveDescription, const std::string& currency,
                     const std::string& discountCurveID, const std::vector<ext::shared_ptr<YieldCurveSegment>>& curveSegments,
                     const std::string& interpolationVariable = "Discount", const std::string& interpolationMethod = "LogLinear",
                     const std::string& zeroDayCounter = "A365", bool extrapolation = true,
                     const std::string& extrapolationMethod = "ContinuousForward",
                     const BootstrapConfig& bootstrapConfig = BootstrapConfig(),
                     const QuantLib::Size mixedInterpolationCutoff = 1);

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    const std::string& currency() const;
    const std::string& discountCurveID() const;
    const std::vector<ext::shared_ptr<YieldCurveSegment>>& curveSegments() const;
    const std::string& interpolationVariable() const;
    const std::string& interpolationMethod() const;
    QuantLib::Size mixedInterpolationCutoff() const;
    const std::string& zeroDayCounter() const;
    bool extrapolation() const;
    const BootstrapConfig& bootstrapConfig() const;
    void setBootstrapConfig(const BootstrapConfig& bootstrapConfig);

    const std::vector<std::string>& quotes();

}; 

#endif
