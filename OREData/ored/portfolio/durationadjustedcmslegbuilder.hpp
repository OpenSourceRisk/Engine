/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file durationadjustedcmslegbuilder.hpp
    \brief leg builder for duration adjusted cms coupon legs
    \ingroup portfolio
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ql/cashflow.hpp>

namespace oreplus {
namespace data {

class DurationAdjustedCmsLegBuilder : public ore::data::LegBuilder {
public:
    DurationAdjustedCmsLegBuilder() : LegBuilder("DurationAdjustedCMS") {}

    QuantLib::Leg buildLeg(const ore::data::LegData& data,
                           const boost::shared_ptr<ore::data::EngineFactory>& engineFactory,
                           ore::data::RequiredFixings& requiredFixings,
                           const std::string& configuration) const override;
};

} // namespace data
} // namespace oreplus
