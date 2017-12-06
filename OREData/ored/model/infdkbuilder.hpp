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

/*! \file ored/model/infdkbuilder.hpp
    \brief Builder for a Lognormal INF model component
    \ingroup models
*/

#pragma once

#include <map>
#include <ostream>
#include <vector>

#include <ored/marketdata/market.hpp>
#include <ored/model/infdkdata.hpp>
#include <qle/models/crossassetmodel.hpp>

using namespace QuantLib;

namespace ore {
    namespace data {

        //! Builder for a Lognormal INF model component
        /*!
        This class is a utility to turn an INF model component's description
        into an INF model parametrization which can be used to ultimately
        instanciate a CrossAssetModel.

        \ingroup models
        */
        class InfDkBuilder {
        public:
            //! Constructor
            InfDkBuilder( //! Market object
                const boost::shared_ptr<ore::data::Market>& market,
                //! INF model parameters/dscription
                const boost::shared_ptr<InfDkData>& data,
                //! Market configuration to use
                const std::string& configuration = Market::defaultConfiguration);

            //! \name Inspectors
            //@{
            std::string infIndex() { return data_->infIndex(); }
            boost::shared_ptr<QuantExt::InfDkParametrization>& parametrization() { return parametrization_; }
            std::vector<boost::shared_ptr<CalibrationHelper>> optionBasket() { return optionBasket_; }
            //@}
        private:
            void buildCapFloorBasket();

            boost::shared_ptr<ore::data::Market> market_;
            const std::string configuration_;
            boost::shared_ptr<InfDkData> data_;
            boost::shared_ptr<ZeroInflationIndex> inflationIndex_;
            boost::shared_ptr<QuantExt::InfDkParametrization> parametrization_;
            std::vector<boost::shared_ptr<CalibrationHelper>> optionBasket_;
            Array optionExpiries_;
        };
    } // namespace data
} // namespace ore
