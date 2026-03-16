/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file orea/engine/filteredsensitivitystream.hpp
    \brief Class that wraps a sensitivity stream and filters out negligible records
 */

#pragma once

#include <orea/engine/sensitivitystream.hpp>

#include <fstream>
#include <set>
#include <string>

namespace ore {
namespace analytics {

//! Class that wraps a sensitivity stream and filters out negligible records
class FilteredSensitivityStream : public SensitivityStream {
public:
    /*! Constructor providing the thresholds. If the absolute value of the delta is
        greater than the \p deltaThreshold or the absolute value of the gamma is greater
        than the \p gammaThreshold, then the SensitivityRecord is streamed
    */
    FilteredSensitivityStream(const QuantLib::ext::shared_ptr<SensitivityStream>& ss, QuantLib::Real deltaThreshold,
                              QuantLib::Real gammaThreshold);
    //! Constructor that uses the same \p threshold for delta and gamma
    FilteredSensitivityStream(const QuantLib::ext::shared_ptr<SensitivityStream>& ss, QuantLib::Real threshold);
    //! Returns the next SensitivityRecord in the stream after filtering
    SensitivityRecord next() override;
    //! Resets the stream so that SensitivityRecord objects can be streamed again
    void reset() override;

private:
    //! The underlying sensitivity stream that has been wrapped
    QuantLib::ext::shared_ptr<SensitivityStream> ss_;
    //! The delta threshold
    QuantLib::Real deltaThreshold_;
    //! The gamma threshold
    QuantLib::Real gammaThreshold_;
    //! Set to hold Delta Keys appearing in CrossGammas
    std::set<RiskFactorKey> deltaKeys_;
};

} // namespace analytics
} // namespace ore
