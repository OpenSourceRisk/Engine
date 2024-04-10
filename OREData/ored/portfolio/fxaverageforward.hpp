/*
  Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

 /*! \file portfolio/fxaverageforward.hpp
     \brief Fx Average Forward data model and serialization
     \ingroup tradedata
 */

 #pragma once

 #include <ored/portfolio/asianoption.hpp>

 namespace ore {
 namespace data {

 //! Serializable Fx Average Forward
 /*!
   Payoff: (fixedPayer ? 1 : -1) * (referenceNotional * averageFX - settlementNotional)
   \ingroup tradedata
 */
 class FxAverageForward : public Trade {
 public:
     //! Default constructor
     FxAverageForward() : Trade("FxAverageForward") {}
     //! Constructor
     FxAverageForward(const Envelope& env, const ScheduleData& observationDates, const string& paymentDate,
		      bool fixedPayer,
		      const std::string& referenceCurrency, double referenceNotional,
		      const std::string settlementCurrency, double settlementNotional,
		      const std::string& fxIndex, const string& settlement = "Cash")
       : Trade("FxAverageForward", env),
	 observationDates_(observationDates), paymentDate_(paymentDate), fixedPayer_(fixedPayer),
	 referenceCurrency_(referenceCurrency), referenceNotional_(referenceNotional),
	 settlementCurrency_(settlementCurrency), settlementNotional_(settlementNotional),
	 fxIndex_(fxIndex), settlement_(settlement) {}

     //! Build QuantLib/QuantExt instrument, link pricing engine
     void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

     //! \name Inspectors
     //@{
     const ScheduleData& observationDates() const { return observationDates_; }
     const string& paymentDate() const { return paymentDate_; }
     bool fixedPayer() const { return fixedPayer_; }
     const string& referenceCurrency() const { return referenceCurrency_; }
     double referenceNotional() const { return referenceNotional_; }
     const string& settlementCurrency() const { return settlementCurrency_; }
     double settlementNotional() const { return settlementNotional_; }
     const std::string& fxIndex() const { return fxIndex_; }
     const string& settlement() const { return settlement_; }
     //@}

     //! \name Serialisation
     //@{
     virtual void fromXML(XMLNode* node) override;
     virtual XMLNode* toXML(XMLDocument& doc) const override;
     //@}

     const std::map<std::string,boost::any>& additionalData() const override;

 private:
     ScheduleData observationDates_;
     string paymentDate_;
     bool fixedPayer_;
     string referenceCurrency_;
     double referenceNotional_;
     string settlementCurrency_;
     double settlementNotional_;
     //! Needed for past fixings
     std::string fxIndex_;
     std::string settlement_;
     bool inverted_ = false; // set during build()
 };

 } // namespace data
 } // namespace ore
