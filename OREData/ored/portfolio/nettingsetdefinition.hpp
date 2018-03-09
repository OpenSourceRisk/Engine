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

#include <ored/utilities/xmlutils.hpp>
#include <ql/time/period.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

//! Netting Set Definition
/*!
  This class is a container for a definition of a netting agreement (including CSA information)

  \ingroup tradedata
*/
class NettingSetDefinition : public XMLSerializable {
public:
    /*!
      Enumeration 'CSAType' specifies what type of
      collateral agreement is in place.
      - 'Bilateral' => both sides can request collateral margins
      - 'PostOnly' => only the counterparty is allowed to issue a call for additional collateral
      - 'CallOnly' => only WE are allowed to issue a margin call for additional collateral
    */
    enum CSAType { Bilateral, CallOnly, PostOnly };

    /*!
      builds a NettingSetDefinition from an XML input
    */
    NettingSetDefinition(XMLNode* node);

    /*!
      Constructor for "uncollateralised" netting sets
    */
    NettingSetDefinition(const string& nettingSetId, const string& ctp);

    /*!
      Constructor for "collateralised" netting sets
    */
    NettingSetDefinition(const string& nettingSetId, const string& ctp, const string& bilateral,
                         const string& csaCurrency, // three letter ISO code
                         const string& index, const Real& thresholdPay, const Real& thresholdRcv, const Real& mtaPay,
                         const Real& mtaRcv, const Real& iaHeld, const string& iaType,
                         const string& marginCallFreq, // e.g. "1D", "2W", "3M", "4Y"
                         const string& marginPostFreq, // e.g. "1D", "2W", "3M", "4Y"
                         const string& mpr,            // e.g. "1D", "2W", "3M", "4Y"
                         const Real& collatSpreadPay, const Real& collatSpreadRcv,
                         const vector<string>& eligCollatCcys); //! vector of three letter ISO codes

    /*!
      loads NettingSetDefinition object from XML
    */
    void fromXML(XMLNode* node);
    /*!
      writes object to XML
    */
    XMLNode* toXML(XMLDocument& doc);

    /*!
      contains "business logic" for more complete object definition
    */
    void build(); // contains "business logic" for more complete object definition

    //! Inspectors
    //@{
    /*! returns netting set id */
    const string& nettingSetId() const { return nettingSetId_; }
    /*! returns counterparty on ISDA netting agreement */
    const string& counterparty() const { return ctp_; }
    /*! boolean specifying if ISDA agreement is covered by a Credit Support Annex */
    bool activeCsaFlag() const { return activeCsaFlag_; }
    /*! Nature of CSA margining agreement (e.g. Bilateral, PostOnly, CallOnly) */
    CSAType csaType() const { return csaType_; }
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
    //@}

private:
    string nettingSetId_;
    string ctp_;
    bool activeCsaFlag_;
    string csaTypeStr_; // staging value for csaType_
    CSAType csaType_;   // initialised during build()
    string csaCurrency_;
    string index_;
    Real thresholdPay_;
    Real thresholdRcv_;
    Real mtaPay_;
    Real mtaRcv_;
    Real iaHeld_;
    string iaType_;
    string marginCallFreqStr_; // staging value for marginCallFreq_
    string marginPostFreqStr_; // staging value for marginPostFreq_
    Period marginCallFreq_;    // initialised during build()
    Period marginPostFreq_;    // initialised during build()
    string mprStr_;            // staging value for mpr_
    Period mpr_;               // initialised during build()
    Real collatSpreadPay_;
    Real collatSpreadRcv_;
    vector<string> eligCollatCcys_;

    // object-status flags
    bool isLoaded_;
    bool isBuilt_;
};
} // namespace data
} // namespace ore
