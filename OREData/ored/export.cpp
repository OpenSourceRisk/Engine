/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <boost/serialization/export.hpp>
#include <ored/marketdata/strike.hpp>
#include <ored/marketdata/marketdatum.hpp>

BOOST_CLASS_EXPORT_GUID(ore::data::MoneyMarketQuote, "MoneyMarketQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::FRAQuote, "FRAQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::ImmFraQuote, "ImmFraQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::SwapQuote, "SwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::ZeroQuote, "ZeroQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::DiscountQuote, "DiscountQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::MMFutureQuote, "MMFutureQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::OIFutureQuote, "OIFutureQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BasisSwapQuote, "BasisSwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BMASwapQuote, "BMASwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CrossCcyBasisSwapQuote, "CrossCcyBasisSwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CrossCcyFixFloatSwapQuote, "CrossCcyFixFloatSwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CdsQuote, "CdsQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::HazardRateQuote, "HazardRateQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::RecoveryRateQuote, "RecoveryRateQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::SwaptionQuote, "SwaptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::SwaptionShiftQuote, "SwaptionShiftQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BondOptionQuote, "BondOptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BondOptionShiftQuote, "BondOptionShiftQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CapFloorQuote, "CapFloorQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CapFloorShiftQuote, "CapFloorShiftQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::FXSpotQuote, "FXSpotQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::FXForwardQuote, "FXForwardQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::FXOptionQuote, "FXOptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::ZcInflationSwapQuote, "ZcInflationSwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::InflationCapFloorQuote, "InflationCapFloorQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::ZcInflationCapFloorQuote, "ZcInflationCapFloorQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::YoYInflationSwapQuote, "YoYInflationSwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::YyInflationCapFloorQuote, "YyInflationCapFloorQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::SeasonalityQuote, "SeasonalityQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::EquitySpotQuote, "EquitySpotQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::EquityForwardQuote, "EquityForwardQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::EquityDividendYieldQuote, "EquityDividendYieldQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::EquityOptionQuote, "EquityOptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::SecuritySpreadQuote, "SecuritySpreadQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BaseCorrelationQuote, "BaseCorrelationQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::IndexCDSOptionQuote, "IndexCDSOptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CommoditySpotQuote, "CommoditySpotQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CommodityForwardQuote, "CommodityForwardQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CommodityOptionQuote, "CommodityOptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CorrelationQuote, "CorrelationQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CPRQuote, "CPRQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BondPriceQuote, "BondPriceQuote");


BOOST_CLASS_EXPORT_GUID(ore::data::AbsoluteStrike, "AbsoluteStrike");
BOOST_CLASS_EXPORT_GUID(ore::data::DeltaStrike, "DeltaStrike");
BOOST_CLASS_EXPORT_GUID(ore::data::AtmStrike, "AtmStrike");
BOOST_CLASS_EXPORT_GUID(ore::data::MoneynessStrike, "MoneynessStrike");
