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

#include "indices.hpp"
#include <ored/utilities/indexparser.hpp>
#include <ored/configuration/conventions.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace ore;

boost::shared_ptr<data::Conventions> convs() {
    boost::shared_ptr<data::Conventions> conventions(new data::Conventions());
    boost::shared_ptr<ore::data::Convention> swapIndexEURConv(
        new ore::data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexEURLongConv(
        new ore::data::SwapIndexConvention("EUR-CMS-30Y", "EUR-6M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexUSDConv(
        new ore::data::SwapIndexConvention("USD-CMS-2Y", "USD-3M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexUSDLongConv(
        new ore::data::SwapIndexConvention("USD-CMS-30Y", "USD-3M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexGBPConv(
        new ore::data::SwapIndexConvention("GBP-CMS-2Y", "GBP-3M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexGBPLongConv(
        new ore::data::SwapIndexConvention("GBP-CMS-30Y", "GBP-6M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexCHFConv(
        new ore::data::SwapIndexConvention("CHF-CMS-2Y", "CHF-3M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexCHFLongConv(
        new ore::data::SwapIndexConvention("CHF-CMS-30Y", "CHF-6M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexJPYConv(
        new ore::data::SwapIndexConvention("JPY-CMS-2Y", "JPY-LIBOR-6M-SWAP-CONVENTIONS"));
    boost::shared_ptr<ore::data::Convention> swapIndexJPYLongConv(
        new ore::data::SwapIndexConvention("JPY-CMS-30Y", "JPY-LIBOR-6M-SWAP-CONVENTIONS"));

    conventions->add(swapIndexEURConv);
    conventions->add(swapIndexEURLongConv);
    conventions->add(swapIndexUSDConv);
    conventions->add(swapIndexUSDLongConv);
    conventions->add(swapIndexGBPConv);
    conventions->add(swapIndexGBPLongConv);
    conventions->add(swapIndexCHFConv);
    conventions->add(swapIndexCHFLongConv);
    conventions->add(swapIndexJPYConv);
    conventions->add(swapIndexJPYLongConv);

    boost::shared_ptr<ore::data::Convention> swapEURConv(new ore::data::IRSwapConvention(
        "EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    boost::shared_ptr<ore::data::Convention> swapUSDConv(
        new ore::data::IRSwapConvention("USD-3M-SWAP-CONVENTIONS", "US", "Semiannual", "MF", "30/360", "USD-LIBOR-3M"));
    boost::shared_ptr<ore::data::Convention> swapGBPConv(
        new ore::data::IRSwapConvention("GBP-3M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-3M"));
    boost::shared_ptr<ore::data::Convention> swapGBPLongConv(
        new ore::data::IRSwapConvention("GBP-6M-SWAP-CONVENTIONS", "UK", "Semiannual", "MF", "A365", "GBP-LIBOR-6M"));
    boost::shared_ptr<ore::data::Convention> swapCHFConv(
        new ore::data::IRSwapConvention("CHF-3M-SWAP-CONVENTIONS", "ZUB", "Annual", "MF", "30/360", "CHF-LIBOR-3M"));
    boost::shared_ptr<ore::data::Convention> swapCHFLongConv(
        new ore::data::IRSwapConvention("CHF-6M-SWAP-CONVENTIONS", "ZUB", "Annual", "MF", "30/360", "CHF-LIBOR-6M"));
    boost::shared_ptr<ore::data::Convention> swapJPYConv(new ore::data::IRSwapConvention(
        "JPY-LIBOR-6M-SWAP-CONVENTIONS", "JP", "Semiannual", "MF", "A365", "JPY-LIBOR-6M"));

    conventions->add(swapEURConv);
    conventions->add(swapUSDConv);
    conventions->add(swapGBPConv);
    conventions->add(swapGBPLongConv);
    conventions->add(swapCHFConv);
    conventions->add(swapCHFLongConv);
    conventions->add(swapJPYConv);

    return conventions;
}

namespace testsuite {

struct test_data {
    const char* str;
    const char* index_name;
    Period tenor;
};

static struct test_data index_data[] = {
    // parsing string,     index name,                     tenor
    {"EUR-EONIA-1D", "EoniaON Actual/360", 1 * Days},
    {"GBP-SONIA-1D", "SoniaON Actual/365 (Fixed)", 1 * Days},
    {"JPY-TONAR-1D", "TONARON Actual/365 (Fixed)", 1 * Days},
    {"CHF-TOIS", "CHF-TOISTN Actual/360", 1 * Days},
    {"USD-FedFunds", "FedFundsON Actual/360", 1 * Days},

    {"AUD-LIBOR-1W", "AUDLibor1W Actual/360", 1 * Weeks},
    {"AUD-LIBOR-1M", "AUDLibor1M Actual/360", 1 * Months},
    {"AUD-LIBOR-2M", "AUDLibor2M Actual/360", 2 * Months},
    {"AUD-LIBOR-3M", "AUDLibor3M Actual/360", 3 * Months},
    {"AUD-LIBOR-6M", "AUDLibor6M Actual/360", 6 * Months},
    {"AUD-LIBOR-12M", "AUDLibor1Y Actual/360", 1 * Years},
    {"AUD-LIBOR-1Y", "AUDLibor1Y Actual/360", 1 * Years},

    {"AUD-BBSW-1W", "AUD-BBSW1W Actual/365 (Fixed)", 1 * Weeks},
    {"AUD-BBSW-1M", "AUD-BBSW1M Actual/365 (Fixed)", 1 * Months},
    {"AUD-BBSW-2M", "AUD-BBSW2M Actual/365 (Fixed)", 2 * Months},
    {"AUD-BBSW-3M", "AUD-BBSW3M Actual/365 (Fixed)", 3 * Months},
    {"AUD-BBSW-6M", "AUD-BBSW6M Actual/365 (Fixed)", 6 * Months},
    {"AUD-BBSW-12M", "AUD-BBSW1Y Actual/365 (Fixed)", 1 * Years},
    {"AUD-BBSW-1Y", "AUD-BBSW1Y Actual/365 (Fixed)", 1 * Years},

    {"EUR-EURIBOR-1W", "Euribor1W Actual/360", 1 * Weeks},
    {"EUR-EURIBOR-2W", "Euribor2W Actual/360", 2 * Weeks},
    {"EUR-EURIBOR-3W", "Euribor3W Actual/360", 3 * Weeks},
    {"EUR-EURIBOR-1M", "Euribor1M Actual/360", 1 * Months},
    {"EUR-EURIBOR-2M", "Euribor2M Actual/360", 2 * Months},
    {"EUR-EURIBOR-3M", "Euribor3M Actual/360", 3 * Months},
    {"EUR-EURIBOR-4M", "Euribor4M Actual/360", 4 * Months},
    {"EUR-EURIBOR-5M", "Euribor5M Actual/360", 5 * Months},
    {"EUR-EURIBOR-6M", "Euribor6M Actual/360", 6 * Months},
    {"EUR-EURIBOR-7M", "Euribor7M Actual/360", 7 * Months},
    {"EUR-EURIBOR-8M", "Euribor8M Actual/360", 8 * Months},
    {"EUR-EURIBOR-9M", "Euribor9M Actual/360", 9 * Months},
    {"EUR-EURIBOR-10M", "Euribor10M Actual/360", 10 * Months},
    {"EUR-EURIBOR-11M", "Euribor11M Actual/360", 11 * Months},
    {"EUR-EURIBOR-12M", "Euribor1Y Actual/360", 1 * Years},
    {"EUR-EURIBOR-1Y", "Euribor1Y Actual/360", 1 * Years},

    {"EUR-LIBOR-1W", "EURLibor1W Actual/360", 1 * Weeks},
    {"EUR-LIBOR-1M", "EURLibor1M Actual/360", 1 * Months},
    {"EUR-LIBOR-2M", "EURLibor2M Actual/360", 2 * Months},
    {"EUR-LIBOR-3M", "EURLibor3M Actual/360", 3 * Months},
    {"EUR-LIBOR-6M", "EURLibor6M Actual/360", 6 * Months},
    {"EUR-LIBOR-12M", "EURLibor1Y Actual/360", 1 * Years},
    {"EUR-LIBOR-1Y", "EURLibor1Y Actual/360", 1 * Years},

    {"CAD-CDOR-1W", "CDOR1W Actual/360", 1 * Weeks},
    {"CAD-CDOR-1M", "CDOR1M Actual/360", 1 * Months},
    {"CAD-CDOR-2M", "CDOR2M Actual/360", 2 * Months},
    {"CAD-CDOR-3M", "CDOR3M Actual/360", 3 * Months},
    {"CAD-CDOR-6M", "CDOR6M Actual/360", 6 * Months},
    {"CAD-CDOR-12M", "CDOR1Y Actual/360", 1 * Years},
    {"CAD-CDOR-1Y", "CDOR1Y Actual/360", 1 * Years},

    {"CAD-BA-1W", "CDOR1W Actual/360", 1 * Weeks},
    {"CAD-BA-1M", "CDOR1M Actual/360", 1 * Months},
    {"CAD-BA-2M", "CDOR2M Actual/360", 2 * Months},
    {"CAD-BA-3M", "CDOR3M Actual/360", 3 * Months},
    {"CAD-BA-6M", "CDOR6M Actual/360", 6 * Months},
    {"CAD-BA-12M", "CDOR1Y Actual/360", 1 * Years},
    {"CAD-BA-1Y", "CDOR1Y Actual/360", 1 * Years},

    {"CZK-PRIBOR-6M", "CZK-PRIBOR6M Actual/360", 6 * Months},

    {"USD-LIBOR-1W", "USDLibor1W Actual/360", 1 * Weeks},
    {"USD-LIBOR-1M", "USDLibor1M Actual/360", 1 * Months},
    {"USD-LIBOR-2M", "USDLibor2M Actual/360", 2 * Months},
    {"USD-LIBOR-3M", "USDLibor3M Actual/360", 3 * Months},
    {"USD-LIBOR-6M", "USDLibor6M Actual/360", 6 * Months},
    {"USD-LIBOR-12M", "USDLibor1Y Actual/360", 1 * Years},
    {"USD-LIBOR-1Y", "USDLibor1Y Actual/360", 1 * Years},

    {"GBP-LIBOR-1W", "GBPLibor1W Actual/365 (Fixed)", 1 * Weeks},
    {"GBP-LIBOR-1M", "GBPLibor1M Actual/365 (Fixed)", 1 * Months},
    {"GBP-LIBOR-2M", "GBPLibor2M Actual/365 (Fixed)", 2 * Months},
    {"GBP-LIBOR-3M", "GBPLibor3M Actual/365 (Fixed)", 3 * Months},
    {"GBP-LIBOR-6M", "GBPLibor6M Actual/365 (Fixed)", 6 * Months},
    {"GBP-LIBOR-12M", "GBPLibor1Y Actual/365 (Fixed)", 1 * Years},
    {"GBP-LIBOR-1Y", "GBPLibor1Y Actual/365 (Fixed)", 1 * Years},

    {"JPY-LIBOR-1W", "JPYLibor1W Actual/360", 1 * Weeks},
    {"JPY-LIBOR-1M", "JPYLibor1M Actual/360", 1 * Months},
    {"JPY-LIBOR-2M", "JPYLibor2M Actual/360", 2 * Months},
    {"JPY-LIBOR-3M", "JPYLibor3M Actual/360", 3 * Months},
    {"JPY-LIBOR-6M", "JPYLibor6M Actual/360", 6 * Months},
    {"JPY-LIBOR-12M", "JPYLibor1Y Actual/360", 1 * Years},
    {"JPY-LIBOR-1Y", "JPYLibor1Y Actual/360", 1 * Years},

    {"JPY-TIBOR-1W", "Tibor1W Actual/365 (Fixed)", 1 * Weeks},
    {"JPY-TIBOR-1M", "Tibor1M Actual/365 (Fixed)", 1 * Months},
    {"JPY-TIBOR-2M", "Tibor2M Actual/365 (Fixed)", 2 * Months},
    {"JPY-TIBOR-3M", "Tibor3M Actual/365 (Fixed)", 3 * Months},
    {"JPY-TIBOR-6M", "Tibor6M Actual/365 (Fixed)", 6 * Months},
    {"JPY-TIBOR-12M", "Tibor1Y Actual/365 (Fixed)", 1 * Years},
    {"JPY-TIBOR-1Y", "Tibor1Y Actual/365 (Fixed)", 1 * Years},

    {"CAD-LIBOR-1W", "CADLibor1W Actual/360", 1 * Weeks},
    {"CAD-LIBOR-1M", "CADLibor1M Actual/360", 1 * Months},
    {"CAD-LIBOR-2M", "CADLibor2M Actual/360", 2 * Months},
    {"CAD-LIBOR-3M", "CADLibor3M Actual/360", 3 * Months},
    {"CAD-LIBOR-6M", "CADLibor6M Actual/360", 6 * Months},
    {"CAD-LIBOR-12M", "CADLibor1Y Actual/360", 1 * Years},
    {"CAD-LIBOR-1Y", "CADLibor1Y Actual/360", 1 * Years},

    {"CHF-LIBOR-1W", "CHFLibor1W Actual/360", 1 * Weeks},
    {"CHF-LIBOR-1M", "CHFLibor1M Actual/360", 1 * Months},
    {"CHF-LIBOR-2M", "CHFLibor2M Actual/360", 2 * Months},
    {"CHF-LIBOR-3M", "CHFLibor3M Actual/360", 3 * Months},
    {"CHF-LIBOR-6M", "CHFLibor6M Actual/360", 6 * Months},
    {"CHF-LIBOR-12M", "CHFLibor1Y Actual/360", 1 * Years},
    {"CHF-LIBOR-1Y", "CHFLibor1Y Actual/360", 1 * Years},

    {"SEK-STIBOR-1W", "SEK-STIBOR1W Actual/360", 1 * Weeks},
    {"SEK-STIBOR-1M", "SEK-STIBOR1M Actual/360", 1 * Months},
    {"SEK-STIBOR-2M", "SEK-STIBOR2M Actual/360", 2 * Months},
    {"SEK-STIBOR-3M", "SEK-STIBOR3M Actual/360", 3 * Months},
    {"SEK-STIBOR-6M", "SEK-STIBOR6M Actual/360", 6 * Months},

    {"SEK-LIBOR-1W", "SEKLibor1W Actual/360", 1 * Weeks},
    {"SEK-LIBOR-1M", "SEKLibor1M Actual/360", 1 * Months},
    {"SEK-LIBOR-2M", "SEKLibor2M Actual/360", 2 * Months},
    {"SEK-LIBOR-3M", "SEKLibor3M Actual/360", 3 * Months},
    {"SEK-LIBOR-6M", "SEKLibor6M Actual/360", 6 * Months},
    {"SEK-LIBOR-12M", "SEKLibor1Y Actual/360", 1 * Years},
    {"SEK-LIBOR-1Y", "SEKLibor1Y Actual/360", 1 * Years},

    {"NOK-NIBOR-1W", "NOK-NIBOR1W Actual/360", 1 * Weeks},
    {"NOK-NIBOR-1M", "NOK-NIBOR1M Actual/360", 1 * Months},
    {"NOK-NIBOR-2M", "NOK-NIBOR2M Actual/360", 2 * Months},
    {"NOK-NIBOR-3M", "NOK-NIBOR3M Actual/360", 3 * Months},
    {"NOK-NIBOR-6M", "NOK-NIBOR6M Actual/360", 6 * Months},
    {"NOK-NIBOR-9M", "NOK-NIBOR9M Actual/360", 9 * Months},
    {"NOK-NIBOR-12M", "NOK-NIBOR1Y Actual/360", 1 * Years},
    {"NOK-NIBOR-1Y", "NOK-NIBOR1Y Actual/360", 1 * Years},

    {"HKD-HIBOR-1W", "HKD-HIBOR1W Actual/365 (Fixed)", 1 * Weeks},
    {"HKD-HIBOR-2W", "HKD-HIBOR2W Actual/365 (Fixed)", 2 * Weeks},
    {"HKD-HIBOR-1M", "HKD-HIBOR1M Actual/365 (Fixed)", 1 * Months},
    {"HKD-HIBOR-2M", "HKD-HIBOR2M Actual/365 (Fixed)", 2 * Months},
    {"HKD-HIBOR-3M", "HKD-HIBOR3M Actual/365 (Fixed)", 3 * Months},
    {"HKD-HIBOR-6M", "HKD-HIBOR6M Actual/365 (Fixed)", 6 * Months},
    {"HKD-HIBOR-12M", "HKD-HIBOR1Y Actual/365 (Fixed)", 1 * Years},
    {"HKD-HIBOR-1Y", "HKD-HIBOR1Y Actual/365 (Fixed)", 1 * Years},

    {"SGD-SIBOR-1M", "SGD-SIBOR1M Actual/365 (Fixed)", 1 * Months},
    {"SGD-SIBOR-3M", "SGD-SIBOR3M Actual/365 (Fixed)", 3 * Months},
    {"SGD-SIBOR-6M", "SGD-SIBOR6M Actual/365 (Fixed)", 6 * Months},
    {"SGD-SIBOR-12M", "SGD-SIBOR1Y Actual/365 (Fixed)", 1 * Years},
    {"SGD-SIBOR-1Y", "SGD-SIBOR1Y Actual/365 (Fixed)", 1 * Years},

    {"SGD-SOR-1M", "SGD-SOR1M Actual/365 (Fixed)", 1 * Months},
    {"SGD-SOR-3M", "SGD-SOR3M Actual/365 (Fixed)", 3 * Months},
    {"SGD-SOR-6M", "SGD-SOR6M Actual/365 (Fixed)", 6 * Months},
    {"SGD-SOR-12M", "SGD-SOR1Y Actual/365 (Fixed)", 1 * Years},
    {"SGD-SOR-1Y", "SGD-SOR1Y Actual/365 (Fixed)", 1 * Years},

    {"DKK-LIBOR-1W", "DKKLibor1W Actual/360", 1 * Weeks},
    {"DKK-LIBOR-1M", "DKKLibor1M Actual/360", 1 * Months},
    {"DKK-LIBOR-2M", "DKKLibor2M Actual/360", 2 * Months},
    {"DKK-LIBOR-3M", "DKKLibor3M Actual/360", 3 * Months},
    {"DKK-LIBOR-6M", "DKKLibor6M Actual/360", 6 * Months},
    {"DKK-LIBOR-12M", "DKKLibor1Y Actual/360", 1 * Years},
    {"DKK-LIBOR-1Y", "DKKLibor1Y Actual/360", 1 * Years},

    {"DKK-CIBOR-1W", "DKK-CIBOR1W Actual/360", 1 * Weeks},
    {"DKK-CIBOR-1M", "DKK-CIBOR1M Actual/360", 1 * Months},
    {"DKK-CIBOR-2M", "DKK-CIBOR2M Actual/360", 2 * Months},
    {"DKK-CIBOR-3M", "DKK-CIBOR3M Actual/360", 3 * Months},
    {"DKK-CIBOR-6M", "DKK-CIBOR6M Actual/360", 6 * Months},
    {"DKK-CIBOR-12M", "DKK-CIBOR1Y Actual/360", 1 * Years},
    {"DKK-CIBOR-1Y", "DKK-CIBOR1Y Actual/360", 1 * Years},

    {"HUF-BUBOR-6M", "HUF-BUBOR6M Actual/360", 6 * Months},
    {"IDR-IDRFIX-6M", "IDR-IDRFIX6M Actual/360", 6 * Months},
    {"INR-MIFOR-6M", "INR-MIFOR6M Actual/365 (Fixed)", 6 * Months},
    {"MXN-TIIE-6M", "MXN-TIIE6M Actual/360", 6 * Months},
    {"PLN-WIBOR-6M", "PLN-WIBOR6M Actual/365 (Fixed)", 6 * Months},
    {"SKK-BRIBOR-6M", "SKK-BRIBOR6M Actual/360", 6 * Months},

    {"NZD-BKBM-1M", "NZD-BKBM1M Actual/Actual (ISDA)", 1 * Months},
    {"NZD-BKBM-2M", "NZD-BKBM2M Actual/Actual (ISDA)", 2 * Months},
    {"NZD-BKBM-3M", "NZD-BKBM3M Actual/Actual (ISDA)", 3 * Months},
    {"NZD-BKBM-4M", "NZD-BKBM4M Actual/Actual (ISDA)", 4 * Months},
    {"NZD-BKBM-5M", "NZD-BKBM5M Actual/Actual (ISDA)", 5 * Months},
    {"NZD-BKBM-6M", "NZD-BKBM6M Actual/Actual (ISDA)", 6 * Months},

    {"KRW-KORIBOR-1M", "KRW-KORIBOR Actual/365 (Fixed)", 1 * Months},
    {"KRW-KORIBOR-2M", "KRW-KORIBOR Actual/365 (Fixed)", 2 * Months },
    {"KRW-KORIBOR-3M", "KRW-KORIBOR Actual/365 (Fixed)", 3 * Months },
    {"KRW-KORIBOR-4M", "KRW-KORIBOR Actual/365 (Fixed)", 4 * Months },
    {"KRW-KORIBOR-5M", "KRW-KORIBOR Actual/365 (Fixed)", 5 * Months },
    {"KRW-KORIBOR-6M", "KRW-KORIBOR Actual/365 (Fixed)", 6 * Months },

    {"TWD-TAIBOR-1M", "TWD-TAIBOR Actual/365 (Fixed)", 1 * Months},
    {"TWD-TAIBOR-2M", "TWD-TAIBOR Actual/365 (Fixed)", 2 * Months },
    {"TWD-TAIBOR-3M", "TWD-TAIBOR Actual/365 (Fixed)", 3 * Months },
    {"TWD-TAIBOR-4M", "TWD-TAIBOR Actual/365 (Fixed)", 4 * Months },
    {"TWD-TAIBOR-5M", "TWD-TAIBOR Actual/365 (Fixed)", 5 * Months },
    {"TWD-TAIBOR-6M", "TWD-TAIBOR Actual/365 (Fixed)", 6 * Months },

    {"MYR-KLIBOR-1M", "MYR-KLIBOR Actual/365 (Fixed)", 1 * Months },
    {"MYR-KLIBOR-2M", "MYR-KLIBOR Actual/365 (Fixed)", 2 * Months },
    {"MYR-KLIBOR-3M", "MYR-KLIBOR Actual/365 (Fixed)", 3 * Months },
    {"MYR-KLIBOR-4M", "MYR-KLIBOR Actual/365 (Fixed)", 4 * Months },
    {"MYR-KLIBOR-5M", "MYR-KLIBOR Actual/365 (Fixed)", 5 * Months },
    {"MYR-KLIBOR-6M", "MYR-KLIBOR Actual/365 (Fixed)", 6 * Months } };

static struct test_data swap_index_data[] = {
    {"EUR-CMS-2Y", "EURLiborSwapIsdaFix2Y 30/360 (Bond Basis)", 2 * Years},
    {"EUR-CMS-30Y", "EURLiborSwapIsdaFix30Y 30/360 (Bond Basis)", 30 * Years},
    {"USD-CMS-2Y", "USDLiborSwapIsdaFix2Y 30/360 (Bond Basis)", 2 * Years},
    {"USD-CMS-30Y", "USDLiborSwapIsdaFix30Y 30/360 (Bond Basis)", 30 * Years},
    {"GBP-CMS-2Y", "GBPLiborSwapIsdaFix2Y Actual/365 (Fixed)", 2 * Years},
    {"GBP-CMS-30Y", "GBPLiborSwapIsdaFix30Y Actual/365 (Fixed)", 30 * Years},
    {"CHF-CMS-2Y", "CHFLiborSwapIsdaFix2Y 30/360 (Bond Basis)", 2 * Years},
    {"CHF-CMS-30Y", "CHFLiborSwapIsdaFix30Y 30/360 (Bond Basis)", 30 * Years},
    {"JPY-CMS-2Y", "JPYLiborSwapIsdaFix2Y Actual/365 (Fixed)", 2 * Years},
    {"JPY-CMS-30Y", "JPYLiborSwapIsdaFix30Y Actual/365 (Fixed)", 30 * Years},
};

void IndexTest::testIborIndexParsing() {
    BOOST_TEST_MESSAGE("Testing Ibor Index name parsing...");

    Size len = sizeof(index_data) / sizeof(index_data[0]);
    for (Size i = 0; i < len; ++i) {
        string str(index_data[i].str);
        string index_name(index_data[i].index_name);
        Period tenor(index_data[i].tenor);

        boost::shared_ptr<IborIndex> ibor;
        try {
            ibor = ore::data::parseIborIndex(str);
        } catch (std::exception& e) {
            BOOST_FAIL("Ibor Parser failed to parse \"" << str << "\" [exception:" << e.what() << "]");
        } catch (...) {
            BOOST_FAIL("Ibor Parser failed to parse \"" << str << "\" [unhandled]");
        }
        if (ibor) {
            BOOST_CHECK_EQUAL(ibor->name(), index_name);
            BOOST_CHECK_EQUAL(ibor->tenor(), tenor);

            BOOST_TEST_MESSAGE("Parsed \"" << str << "\" and got " << ibor->name());
        } else
            BOOST_FAIL("Ibor Parser(" << str << ") returned null pointer");
    }
}

void IndexTest::testIborIndexParsingFails() {
    BOOST_TEST_MESSAGE("Testing Ibor Index parsing fails...");

    // Test invalid strings
    BOOST_CHECK_THROW(ore::data::parseIborIndex("EUR-EONIA-1M"), std::exception);
    BOOST_CHECK_THROW(ore::data::parseIborIndex("EUR-EURIBOR-1D"), std::exception);
    BOOST_CHECK_THROW(ore::data::parseIborIndex("EUR-FALSE-6M"), std::exception);
    BOOST_CHECK_THROW(ore::data::parseIborIndex("It's a trap!"), std::exception);
}

void IndexTest::testSwapIndexParsing() {
    BOOST_TEST_MESSAGE("Testing Swap Index name parsing...");

    Handle<YieldTermStructure> h; // dummy

    Size len = sizeof(swap_index_data) / sizeof(swap_index_data[0]);
    data::Conventions conventions = *convs();

    for (Size i = 0; i < len; ++i) {
        string str(swap_index_data[i].str);
        string index_name(swap_index_data[i].index_name);
        Period tenor(swap_index_data[i].tenor);

        boost::shared_ptr<data::Convention> tmp = conventions.get(str);
        boost::shared_ptr<data::SwapIndexConvention> swapCon =
            boost::dynamic_pointer_cast<data::SwapIndexConvention>(tmp);
        tmp = conventions.get(swapCon->conventions());
        boost::shared_ptr<data::IRSwapConvention> con = boost::dynamic_pointer_cast<data::IRSwapConvention>(tmp);
        QL_REQUIRE(con, "no swap convention");
        boost::shared_ptr<SwapIndex> swap;
        try {
            swap = ore::data::parseSwapIndex(str, h, h, con);
        } catch (std::exception& e) {
            BOOST_FAIL("Swap Parser failed to parse \"" << str << "\" [exception:" << e.what() << "]");
        } catch (...) {
            BOOST_FAIL("Swap Parser failed to parse \"" << str << "\" [unhandled]");
        }
        if (swap) {
            BOOST_CHECK_EQUAL(swap->name(), index_name);
            BOOST_CHECK_EQUAL(swap->tenor(), tenor);

            BOOST_TEST_MESSAGE("Parsed \"" << str << "\" and got " << swap->name());
        } else
            BOOST_FAIL("Swap Parser(" << str << ") returned null pointer");
    }
}

test_suite* IndexTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("IndexTests");
    suite->add(BOOST_TEST_CASE(&IndexTest::testIborIndexParsing));
    suite->add(BOOST_TEST_CASE(&IndexTest::testIborIndexParsingFails));
    suite->add(BOOST_TEST_CASE(&IndexTest::testSwapIndexParsing));
    return suite;
}
}
