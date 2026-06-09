/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/tradeutils.hpp
    \brief trade utility functions
    \ingroup portfolio
*/

#include <ored/portfolio/tradeutils.hpp>

#include <ored/portfolio/compositeinstrumentwrapper.hpp>

namespace ore {
namespace data {

std::set<QuantLib::ext::shared_ptr<InstrumentWrapper>>
unpackCompositeInstrumentWrappers(const std::set<QuantLib::ext::shared_ptr<InstrumentWrapper>>& inputWrappers) {
    std::set<QuantLib::ext::shared_ptr<InstrumentWrapper>> wrappers{inputWrappers};
    std::set<QuantLib::ext::shared_ptr<InstrumentWrapper>> wrappersTmp;
    bool compositeFound;
    do {
        compositeFound = false;
        for (auto const& w : wrappers) {
            if (auto comp = QuantLib::ext::dynamic_pointer_cast<CompositeInstrumentWrapper>(w)) {
                wrappersTmp.insert(comp->wrappers().begin(), comp->wrappers().end());
                compositeFound = true;
            } else {
                wrappersTmp.insert(w);
            }
        }
        wrappers.swap(wrappersTmp);
        wrappersTmp.clear();
    } while (compositeFound);
    return wrappers;
}

std::set<std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Real>> unpackCompositeInstruments(
    const std::set<std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Real>>& inputQlInstruments) {

    std::set<std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Real>> qlInstruments{inputQlInstruments};
    std::set<std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Real>> qlInstrumentsTmp;
    bool compositeFound;
    do {
        compositeFound = false;
        for (auto const& [qlInstrument, outerMult] : qlInstruments) {

            if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantExt::MultiCcyCompositeInstrument>(qlInstrument)) {
                for (auto const& [instr, innerMult, _] : c->components()) {
                    qlInstrumentsTmp.insert(std::make_pair(instr, outerMult * innerMult));
                }
                compositeFound = true;
            } else if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantLib::CompositeInstrument>(qlInstrument)) {
                for (auto const& [instr, innerMult] : c->components()) {
                    qlInstrumentsTmp.insert(std::make_pair(instr, outerMult * innerMult));
                }
                compositeFound = true;
            } else {
                qlInstrumentsTmp.insert(std::make_pair(qlInstrument, outerMult));
            }
        }
        qlInstruments.swap(qlInstrumentsTmp);
        qlInstrumentsTmp.clear();
    } while (compositeFound);

    return qlInstruments;
}

} // namespace data
} // namespace ore
