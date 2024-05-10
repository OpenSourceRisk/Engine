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

/*! \file ored/portfolio/nettingsetdefinition.hpp
    \brief Netting Set Definition - including CSA information where available
    \ingroup tradedata
*/

#pragma once

#include <boost/optional.hpp>
#include <ored/portfolio/nettingsetdetails.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/time/period.hpp>
#include <ql/utilities/null.hpp>

using ore::data::NettingSetDetails;

namespace ore {
namespace data {
using namespace QuantLib;

class CSA {
public:
    /*!
      Enumeration 'Type' specifies what type of
      collateral agreement is in place.
      - 'Bilateral' => both sides can request collateral margins
      - 'PostOnly' => only the counterparty is allowed to issue a call for additional collateral
      - 'CallOnly' => only WE are allowed to issue a margin call for additional collateral
    */
    enum Type { Bilateral, CallOnly, PostOnly };

    CSA(const Type& type,
        const string& csaCurrency, // three letter ISO code
        const string& index, const Real& thresholdPay, const Real& thresholdRcv, const Real& mtaPay, const Real& mtaRcv,
        const Real& iaHeld, const string& iaType, const Period& marginCallFreq, const Period& marginPostFreq,
        const Period& mpr, const Real& collatSpreadPay, const Real& collatSpreadRcv,
        const vector<string>& eligCollatCcys, // vector of three letter ISO codes
        bool applyInitialMargin, Type initialMarginType, const bool calculateIMAmount, const bool calculateVMAmount,
        const string& nonExemptIMRegulations)
        : type_(type), csaCurrency_(csaCurrency), index_(index), thresholdPay_(thresholdPay),
          thresholdRcv_(thresholdRcv), mtaPay_(mtaPay), mtaRcv_(mtaRcv), iaHeld_(iaHeld), iaType_(iaType),
          marginCallFreq_(marginCallFreq), marginPostFreq_(marginPostFreq), mpr_(mpr),
          collatSpreadPay_(collatSpreadPay), collatSpreadRcv_(collatSpreadRcv), eligCollatCcys_(eligCollatCcys),
          applyInitialMargin_(applyInitialMargin), initialMarginType_(initialMarginType),
          calculateIMAmount_(calculateIMAmount), calculateVMAmount_(calculateVMAmount),
          nonExemptIMRegulations_(nonExemptIMRegulations) {}

    //! Inspectors
    //@{
    /*! Nature of CSA margining agreement (e.g. Bilateral, PostOnly, CallOnly) */
    const Type& type() const { return type_; }
    /*! Master Currency of CSA */
    const string& csaCurrency() const { return csaCurrency_; }
    /*! Index that determines the compounding rate */
    const string& index() const { return index_; }
    /*! Threshold amount for margin calls issued by counterparty */
    Real thresholdPay() const { return thresholdPay_; }
    /*! Threshold amount when issuing calls to counterparty */
    Real thresholdRcv() const { return thresholdRcv_; }
    /*! Minimum Transfer Amount when posting collateral to counterparty */
    Real mtaPay() const { return mtaPay_; }
    /*! Minimum Transfer Amount when receiving collateral from counterparty */
    Real mtaRcv() const { return mtaRcv_; }
    /*! Sum of Independent Amounts covered by the CSA (positive => we hold the amount) */
    Real independentAmountHeld() const { return iaHeld_; }
    /*! 'Type' of Independent Amount as specified in the CSA */
    const string& independentAmountType() const { return iaType_; }
    /*! Margining Frequency when requesting collateral from counterparty (e.g. "1D", "1W") */
    const Period& marginCallFrequency() const { return marginCallFreq_; }
    /*! Margining Frequency when counterparty is requesting collateral (e.g. "1D", "1W") */
    const Period& marginPostFrequency() const { return marginPostFreq_; }
    /*! Margin Period of Risk (e.g. "1M") */
    const Period& marginPeriodOfRisk() const { return mpr_; }
    /*! Spread for interest accrual on held collateral */
    Real collatSpreadRcv() const { return collatSpreadRcv_; }
    /*! Spread for interest accrual on posted collateral */
    Real collatSpreadPay() const { return collatSpreadPay_; }
    /*! Eligible Collateral Currencies */
    vector<string> eligCollatCcys() const { return eligCollatCcys_; }
    /*! Apply (dynamic) initial margin in addition to variation margin */
    bool applyInitialMargin() { return applyInitialMargin_; }
    /*! Direction of (dynamic) initial margin */
    Type initialMarginType() { return initialMarginType_; }
    //@}
    /*! Calculate SIMM as IM (currently used only for SA-CCR) */
    bool calculateIMAmount() { return calculateIMAmount_; }
    /*! Calculate VM from NPV (currently used only for SA-CCR) */
    bool calculateVMAmount() { return calculateVMAmount_; }
    /*! IM regulations (whose trade sensitivities are usually exempt from margin/sensi calc) that we wish to include
        (currently used only for SA-CCR) */
    const string& nonExemptIMRegulations() { return nonExemptIMRegulations_; } 
    /*! invert all relevant aspects of the CSA */
    void invertCSA();

    void validate();

private:
    Type type_;
    string csaCurrency_;
    string index_;
    Real thresholdPay_;
    Real thresholdRcv_;
    Real mtaPay_;
    Real mtaRcv_;
    Real iaHeld_;
    string iaType_;
    Period marginCallFreq_;
    Period marginPostFreq_;
    Period mpr_;
    Real collatSpreadPay_;
    Real collatSpreadRcv_;
    vector<string> eligCollatCcys_;
    bool applyInitialMargin_;
    Type initialMarginType_;
    bool calculateIMAmount_, calculateVMAmount_;
    string nonExemptIMRegulations_;
};

CSA::Type parseCsaType(const string& s);

std::ostream& operator<<(std::ostream& out, CSA::Type t);

//! Netting Set Definition
/*!
  This class is a container for a definition of a netting agreement (including CSA information)

  \ingroup tradedata
*/
class NettingSetDefinition : public XMLSerializable {
public:
    /*!
      builds a NettingSetDefinition from an XML input
    */
    NettingSetDefinition(XMLNode* node);

    /*!
      Constructor for "uncollateralised" netting sets
    */
    NettingSetDefinition(const NettingSetDetails& nettingSetDetails);
    NettingSetDefinition(const string& nettingSetId)
        : NettingSetDefinition(NettingSetDetails(nettingSetId)) {}
    
    /*!
      Constructor for "collateralised" netting sets
    */
    NettingSetDefinition(const NettingSetDetails& nettingSetDetails, const string& bilateral,
                         const string& csaCurrency, // three letter ISO code
                         const string& index, const Real& thresholdPay, const Real& thresholdRcv, const Real& mtaPay,
                         const Real& mtaRcv, const Real& iaHeld, const string& iaType,
                         const string& marginCallFreq, // e.g. "1D", "2W", "3M", "4Y"
                         const string& marginPostFreq, // e.g. "1D", "2W", "3M", "4Y"
                         const string& mpr,            // e.g. "1D", "2W", "3M", "4Y"
                         const Real& collatSpreadPay, const Real& collatSpreadRcv,
                         const vector<string>& eligCollatCcys, // vector of three letter ISO codes
                         bool applyInitialMargin = false, const string& initialMarginType = "Bilateral",
                         const bool calculateIMAmount = false, const bool calculateVMAmount = false,
                         const string& nonExemptIMRegulations = "");

    NettingSetDefinition(const string& nettingSetId, const string& bilateral,
                         const string& csaCurrency, // three letter ISO code
                         const string& index, const Real& thresholdPay, const Real& thresholdRcv, const Real& mtaPay,
                         const Real& mtaRcv, const Real& iaHeld, const string& iaType,
                         const string& marginCallFreq, // e.g. "1D", "2W", "3M", "4Y"
                         const string& marginPostFreq, // e.g. "1D", "2W", "3M", "4Y"
                         const string& mpr,            // e.g. "1D", "2W", "3M", "4Y"
                         const Real& collatSpreadPay, const Real& collatSpreadRcv,
                         const vector<string>& eligCollatCcys, // vector of three letter ISO codes
                         bool applyInitialMargin = false, const string& initialMarginType = "Bilateral",
                         const bool calculateIMAmount = false, const bool calculateVMAmount = false,
                         const string& nonExemptIMRegulations = "")
        : NettingSetDefinition(NettingSetDetails(nettingSetId), bilateral, csaCurrency, index, thresholdPay,
                               thresholdRcv, mtaPay, mtaRcv, iaHeld, iaType, marginCallFreq, marginPostFreq, mpr,
                               collatSpreadPay, collatSpreadRcv, eligCollatCcys, applyInitialMargin, initialMarginType,
                               calculateIMAmount, calculateVMAmount, nonExemptIMRegulations) {}

    /*!
      loads NettingSetDefinition object from XML
    */
    void fromXML(XMLNode* node) override;
    /*!
      writes object to XML
    */
    XMLNode* toXML(XMLDocument& doc) const override;

    /*!
      validate the netting set definition including CSA details
    */
    void validate(); // contains "business logic" for more complete object definition

    //! Inspectors
    //@{
    /*! returns netting set id */
    const string& nettingSetId() const {
        return (nettingSetDetails_.empty() ? nettingSetId_ : nettingSetDetails_.nettingSetId());
    }
    /*! returns netting set details */
    const NettingSetDetails nettingSetDetails() const { return nettingSetDetails_; }
    /*! boolean specifying if ISDA agreement is covered by a Credit Support Annex */
    bool activeCsaFlag() const { return activeCsaFlag_; }
    /*! CSA details, if active */
    const QuantLib::ext::shared_ptr<CSA>& csaDetails() { return csa_; }

    // /*! Nature of CSA margining agreement (e.g. Bilateral, PostOnly, CallOnly) */
    // CSAType csaType() const {
    //     QL_REQUIRE(csaType_.is_initialized(), "csaType is not initialised");
    //     return csaType_.get();
    // }

    // /*! Master Currency of CSA */
    // const string& csaCurrency() const { return csaCurrency_; }
    // /*! Index that determines the compounding rate */
    // const string& index() const { return index_; }
    // /*! Threshold amount for margin calls issued by counterparty */
    // Real thresholdPay() const { return thresholdPay_; }
    // /*! Threshold amount when issuing calls to counterparty */
    // Real thresholdRcv() const { return thresholdRcv_; }
    // /*! Minimum Transfer Amount when posting collateral to counterparty */
    // Real mtaPay() const { return mtaPay_; }
    // /*! Minimum Transfer Amount when receiving collateral from counterparty */
    // Real mtaRcv() const { return mtaRcv_; }
    // /*! Sum of Independent Amounts covered by the CSA (positive => we hold the amount) */
    // Real independentAmountHeld() const { return iaHeld_; }
    // /*! 'Type' of Independent Amount as specified in the CSA */
    // const string& independentAmountType() const { return iaType_; }
    // /*! Margining Frequency when requesting collateral from counterparty (e.g. "1D", "1W") */
    // const Period& marginCallFrequency() const { return marginCallFreq_; }
    // /*! Margining Frequency when counterparty is requesting collateral (e.g. "1D", "1W") */
    // const Period& marginPostFrequency() const { return marginPostFreq_; }
    // /*! Margin Period of Risk (e.g. "1M") */
    // const Period& marginPeriodOfRisk() const { return mpr_; }
    // /*! Spread for interest accrual on held collateral */
    // Real collatSpreadRcv() const { return collatSpreadRcv_; }
    // /*! Spread for interest accrual on posted collateral */
    // Real collatSpreadPay() const { return collatSpreadPay_; }
    // /*! Eligible Collateral Currencies */
    // vector<string> eligCollatCcys() const { return eligCollatCcys_; }
    // /*! Apply (dynamic) initial margin in addition to variation margin */
    // bool applyInitialMargin() { return applyInitialMargin_; }
    // //@}

private:
    string nettingSetId_;
    NettingSetDetails nettingSetDetails_;
    bool activeCsaFlag_;
    QuantLib::ext::shared_ptr<CSA> csa_;

    // string csaTypeStr_;                // staging value for csaType_
    // boost::optional<CSAType> csaType_; // initialised during build()
    // string csaCurrency_;
    // string index_;
    // Real thresholdPay_;
    // Real thresholdRcv_;
    // Real mtaPay_;
    // Real mtaRcv_;
    // Real iaHeld_;
    // string iaType_;
    // string marginCallFreqStr_; // staging value for marginCallFreq_
    // string marginPostFreqStr_; // staging value for marginPostFreq_
    // Period marginCallFreq_;    // initialised during build()
    // Period marginPostFreq_;    // initialised during build()
    // string mprStr_;            // staging value for mpr_
    // Period mpr_;               // initialised during build()
    // Real collatSpreadPay_;
    // Real collatSpreadRcv_;
    // vector<string> eligCollatCcys_;
    // bool applyInitialMargin_;
    // bool calculateIMAmount_
    // bool calculateVMAmount_
};
} // namespace data
} // namespace ore
