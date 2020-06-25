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

/*! \file orea/app/structuredanalyticserror.hpp
    \brief Structured analytics error
    \ingroup app
*/

#pragma once

#include <ored/utilities/log.hpp>

namespace ore {
namespace analytics {

class StructuredAnalyticsErrorMessage : public ore::data::StructuredErrorMessage {
public:
    StructuredAnalyticsErrorMessage(const std::string& type, const std::string& what);

    const std::string& type() const;
    const std::string& what() const;

protected:
    std::string json() const override;

private:
    std::string type_;
    std::string what_;
};

} // namespace analytics
} // namespace ore
