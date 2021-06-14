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

 #include <ored/portfolio/fxasianoption.hpp>
 #include <ored/utilities/xmlutils.hpp>
 #include <ql/errors.hpp>

 namespace ore {
 namespace data {

 void FxAsianOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
     // Checks
     QL_REQUIRE(quantity_ > 0, "Fx Asian option requires a positive quatity");
     QL_REQUIRE(strike_ > 0, "Fx Asian option requires a positive strike");

     const boost::shared_ptr<Market>& market = engineFactory->market();

     // Check that we can populate index_. Reused logic from AutomaticExercise FX vanilla options
     QL_REQUIRE(!fxIndex_.empty(), "FX Asian option trade "
                                         << id()
                                         << " has automatic exercise so the FXIndex node needs to be populated.");

     // The strike is the number of units of sold currency (currency_) per unit of bought currency (assetName_).
     // So, the convention here is that the sold currency is domestic and the bought currency is foreign.
     // Note: intentionally use null calendar and 0 day fixing lag here because we will ask the FX index for its
     //       value on the expiry date without adjustment.
     index_ = buildFxIndex(fxIndex_, currency_, assetName_, market,
                             engineFactory->configuration(MarketContext::pricing), "NullCalendar", 0);

     // Populate the external index name so that fixings work.
     indexName_ = fxIndex_;

     // Build the trade using the shared functionality in the base class.
     AsianOptionTrade::build(engineFactory);
 }

 void FxAsianOption::fromXML(XMLNode* node) {
     AsianOptionTrade::fromXML(node);

     XMLNode* fxNode = XMLUtils::getChildNode(node, "FxAsianOptionData");
     QL_REQUIRE(fxNode, "No FxAsianOptionData Node");

     option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
     QL_REQUIRE(option_.payoffType() == "Asian", "Expected PayoffType Asian for FxAsianOption.");
     XMLNode* asianNode = XMLUtils::getChildNode(fxNode, "AsianData");
     if (asianNode)
         asianData_.fromXML(asianNode);

     XMLNode* scheduleDataNode = XMLUtils::getChildNode(fxNode, "ObservationDates");
     observationDates_.fromXML(scheduleDataNode);

     XMLNode* tmpBought = XMLUtils::getChildNode(fxNode, "BoughtAmount");
     XMLNode* tmpSold = XMLUtils::getChildNode(fxNode, "SoldAmount");
     bool vanillaFlavour = tmpBought && tmpSold;
     if (vanillaFlavour) {
         LOG("Vanilla Flavour");
         assetName_ = XMLUtils::getChildValue(fxNode, "BoughtCurrency", true);
	 currency_ = XMLUtils::getChildValue(fxNode, "SoldCurrency", true);
	 double boughtAmount = XMLUtils::getChildValueAsDouble(fxNode, "BoughtAmount", true);
	 double soldAmount = XMLUtils::getChildValueAsDouble(fxNode, "SoldAmount", true);
	 strike_ = soldAmount / boughtAmount;
	 quantity_ = boughtAmount;	 
	 fxIndex_ = XMLUtils::getChildValue(fxNode, "FXIndex", true);
     } else {
         LOG("Scripted Flavour");
         currency_ = XMLUtils::getChildValue(fxNode, "Currency", true);
	 quantity_ = XMLUtils::getChildValueAsDouble(fxNode, "Quantity", true);
	 strike_ = XMLUtils::getChildValueAsDouble(fxNode, "Strike", false);
	 // Get fx index and asset name from the underlying node
	 XMLNode* tmp = XMLUtils::getChildNode(fxNode, "Underlying");
	 FXUnderlying underlying;
	 underlying.fromXML(tmp);
	 fxIndex_ = underlying.name();
	 boost::shared_ptr<QuantExt::FxIndex> fxi = parseFxIndex(fxIndex_);
	 assetName_ = fxi->sourceCurrency().code() == currency_ ?
	     fxi->targetCurrency().code() : fxi->sourceCurrency().code();
     }
 }

 XMLNode* FxAsianOption::toXML(XMLDocument& doc) {
     // TODO: Should call parent class to xml?
     XMLNode* node = Trade::toXML(doc);

     XMLNode* fxNode = doc.allocNode("FxAsianOptionData");
     XMLUtils::appendNode(node, fxNode);

     XMLUtils::appendNode(fxNode, option_.toXML(doc));
     XMLUtils::appendNode(fxNode, asianData_.toXML(doc));
     auto tmp = observationDates_.toXML(doc);
     XMLUtils::setNodeName(doc, tmp, "ObservationDates");
     XMLUtils::appendNode(fxNode, tmp);

     XMLUtils::addChild(doc, fxNode, "BoughtCurrency", boughtCurrency());
     XMLUtils::addChild(doc, fxNode, "BoughtAmount", boughtAmount());
     XMLUtils::addChild(doc, fxNode, "SoldCurrency", soldCurrency());
     XMLUtils::addChild(doc, fxNode, "SoldAmount", soldAmount());

     if (!fxIndex_.empty())
         XMLUtils::addChild(doc, fxNode, "FXIndex", fxIndex_);

     return node;
 }

 } // namespace data
 } // namespace ore
