/*
  Copyright (C) 2023 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file orepsimm/ored/portfolio/equityindexdecomposition.hpp
    \brief Helper function used for the index decompositon
 */
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/currencyhedgedequityindexdecomposition.hpp>

namespace ore {
namespace data {

static const std::map<std::string, std::string> defaultFxIndexMap {
    {"AEDAUD", "TR20H-AUD-AED"}, {"ARSAUD", "TR20H-AUD-ARS"}, {"AUDBBD", "TR20H-AUD-BBD"}, {"AUDBDT", "TR20H-AUD-BDT"},
    {"AUDBGN", "TR20H-AUD-BGN"}, {"AUDBHD", "TR20H-AUD-BHD"}, {"AUDBND", "TR20H-AUD-BND"}, {"AUDBRL", "TR20H-AUD-BRL"},
    {"AUDCAD", "TR20H-AUD-CAD"}, {"AUDCHF", "TR20H-AUD-CHF"}, {"AUDCLP", "TR20H-AUD-CLP"}, {"AUDCNH", "TR20H-AUD-CNH"},
    {"AUDCNY", "TR20H-AUD-CNY"}, {"AUDCOP", "TR20H-AUD-COP"}, {"AUDCRC", "TR20H-AUD-CRC"}, {"AUDCYP", "TR20H-AUD-CYP"},
    {"AUDCZK", "TR20H-AUD-CZK"}, {"AUDDKK", "TR20H-AUD-DKK"}, {"AUDDZD", "TR20H-AUD-DZD"}, {"AUDEGP", "TR20H-AUD-EGP"},
    {"AUDEUR", "TR20H-AUD-EUR"}, {"AUDFJD", "TR20H-AUD-FJD"}, {"AUDGBP", "TR20H-AUD-GBP"}, {"AUDGHS", "TR20H-AUD-GHS"},
    {"AUDGNF", "TR20H-AUD-GNF"}, {"AUDGYD", "TR20H-AUD-GYD"}, {"AUDHKD", "TR20H-AUD-HKD"}, {"AUDHNL", "TR20H-AUD-HNL"},
    {"AUDHRK", "TR20H-AUD-HRK"}, {"AUDHTG", "TR20H-AUD-HTG"}, {"AUDHUF", "TR20H-AUD-HUF"}, {"AUDIDR", "TR20H-AUD-IDR"},
    {"AUDILS", "TR20H-AUD-ILS"}, {"AUDINR", "TR20H-AUD-INR"}, {"AUDIRR", "TR20H-AUD-IRR"}, {"AUDISK", "TR20H-AUD-ISK"},
    {"AUDJMD", "TR20H-AUD-JMD"}, {"AUDJOD", "TR20H-AUD-JOD"}, {"AUDJPY", "TR20H-AUD-JPY"}, {"AUDKES", "TR20H-AUD-KES"},
    {"AUDKRW", "TR20H-AUD-KRW"}, {"AUDKWD", "TR20H-AUD-KWD"}, {"AUDLBP", "TR20H-AUD-LBP"}, {"AUDLYD", "TR20H-AUD-LYD"},
    {"AUDMAD", "TR20H-AUD-MAD"}, {"AUDMUR", "TR20H-AUD-MUR"}, {"AUDMXN", "TR20H-AUD-MXN"}, {"AUDMYR", "TR20H-AUD-MYR"},
    {"AUDNGN", "TR20H-AUD-NGN"}, {"AUDNOK", "TR20H-AUD-NOK"}, {"AUDNPR", "TR20H-AUD-NPR"}, {"AUDNZD", "TR20H-AUD-NZD"},
    {"AUDOMR", "TR20H-AUD-OMR"}, {"AUDPAB", "TR20H-AUD-PAB"}, {"AUDPEN", "TR20H-AUD-PEN"}, {"AUDPGK", "TR20H-AUD-PGK"},
    {"AUDPHP", "TR20H-AUD-PHP"}, {"AUDPKR", "TR20H-AUD-PKR"}, {"AUDPLN", "TR20H-AUD-PLN"}, {"AUDRON", "TR20H-AUD-RON"},
    {"AUDRUB", "TR20H-AUD-RUB"}, {"AUDSAR", "TR20H-AUD-SAR"}, {"AUDSBD", "TR20H-AUD-SBD"}, {"AUDSEK", "TR20H-AUD-SEK"},
    {"AUDSGD", "TR20H-AUD-SGD"}, {"AUDSKK", "TR20H-AUD-SKK"}, {"AUDSRD", "TR20H-AUD-SRD"}, {"AUDSYP", "TR20H-AUD-SYP"},
    {"AUDTHB", "TR20H-AUD-THB"}, {"AUDTND", "TR20H-AUD-TND"}, {"AUDTRY", "TR20H-AUD-TRY"}, {"AUDTTD", "TR20H-AUD-TTD"},
    {"AUDTWD", "TR20H-AUD-TWD"}, {"AUDUSD", "TR20H-AUD-USD"}, {"AUDUYU", "TR20H-AUD-UYU"}, {"AUDVES", "TR20H-AUD-VES"},
    {"AUDVND", "TR20H-AUD-VND"}, {"AUDVUV", "TR20H-AUD-VUV"}, {"AUDXAF", "TR20H-AUD-XAF"}, {"AUDXPF", "TR20H-AUD-XPF"},
    {"AUDZAR", "TR20H-AUD-ZAR"}, {"CADCHF", "TR20H-CAD-CHF"}, {"CADJPY", "TR20H-CAD-JPY"}, {"CHFDKK", "TR20H-CHF-DKK"},
    {"CHFJPY", "TR20H-CHF-JPY"}, {"CHFPLN", "TR20H-CHF-PLN"}, {"CHFRON", "TR20H-CHF-RON"}, {"CHFRUB", "TR20H-CHF-RUB"},
    {"CHFTHB", "TR20H-CHF-THB"}, {"CHFZAR", "TR20H-CHF-ZAR"}, {"AEDEUR", "TR20H-EUR-AED"}, {"AFNEUR", "TR20H-EUR-AFN"},
    {"ALLEUR", "TR20H-EUR-ALL"}, {"ANGEUR", "TR20H-EUR-ANG"}, {"AOAEUR", "TR20H-EUR-AOA"}, {"ARSEUR", "TR20H-EUR-ARS"},
    {"AUDEUR", "TR20H-EUR-AUD"}, {"AZNEUR", "TR20H-EUR-AZN"}, {"BAMEUR", "TR20H-EUR-BAM"}, {"BBDEUR", "TR20H-EUR-BBD"},
    {"BDTEUR", "TR20H-EUR-BDT"}, {"BGNEUR", "TR20H-EUR-BGN"}, {"BHDEUR", "TR20H-EUR-BHD"}, {"BNDEUR", "TR20H-EUR-BND"},
    {"BOBEUR", "TR20H-EUR-BOB"}, {"BRLEUR", "TR20H-EUR-BRL"}, {"BWPEUR", "TR20H-EUR-BWP"}, {"BYNEUR", "TR20H-EUR-BYN"},
    {"CADEUR", "TR20H-EUR-CAD"}, {"CDFEUR", "TR20H-EUR-CDF"}, {"CHFEUR", "TR20H-EUR-CHF"}, {"CLPEUR", "TR20H-EUR-CLP"},
    {"CNHEUR", "TR20H-EUR-CNH"}, {"CNYEUR", "TR20H-EUR-CNY"}, {"COPEUR", "TR20H-EUR-COP"}, {"CRCEUR", "TR20H-EUR-CRC"},
    {"CZKEUR", "TR20H-EUR-CZK"}, {"DKKEUR", "TR20H-EUR-DKK"}, {"DOPEUR", "TR20H-EUR-DOP"}, {"DZDEUR", "TR20H-EUR-DZD"},
    {"EGPEUR", "TR20H-EUR-EGP"}, {"ETBEUR", "TR20H-EUR-ETB"}, {"EURFJD", "TR20H-EUR-FJD"}, {"EURGBP", "TR20H-EUR-GBP"},
    {"EURGHS", "TR20H-EUR-GHS"}, {"EURGNF", "TR20H-EUR-GNF"}, {"EURGTQ", "TR20H-EUR-GTQ"}, {"EURGYD", "TR20H-EUR-GYD"},
    {"EURHKD", "TR20H-EUR-HKD"}, {"EURHNL", "TR20H-EUR-HNL"}, {"EURHRK", "TR20H-EUR-HRK"}, {"EURHTG", "TR20H-EUR-HTG"},
    {"EURHUF", "TR20H-EUR-HUF"}, {"EURIDR", "TR20H-EUR-IDR"}, {"EURILS", "TR20H-EUR-ILS"}, {"EURINR", "TR20H-EUR-INR"},
    {"EURIQD", "TR20H-EUR-IQD"}, {"EURIRR", "TR20H-EUR-IRR"}, {"EURISK", "TR20H-EUR-ISK"}, {"EURJMD", "TR20H-EUR-JMD"},
    {"EURJOD", "TR20H-EUR-JOD"}, {"EURJPY", "TR20H-EUR-JPY"}, {"EURKES", "TR20H-EUR-KES"}, {"EURKGS", "TR20H-EUR-KGS"},
    {"EURKHR", "TR20H-EUR-KHR"}, {"EURKRW", "TR20H-EUR-KRW"}, {"EURKWD", "TR20H-EUR-KWD"}, {"EURKYD", "TR20H-EUR-KYD"},
    {"EURKZT", "TR20H-EUR-KZT"}, {"EURLAK", "TR20H-EUR-LAK"}, {"EURLBP", "TR20H-EUR-LBP"}, {"EURLKR", "TR20H-EUR-LKR"},
    {"EURLYD", "TR20H-EUR-LYD"}, {"EURMAD", "TR20H-EUR-MAD"}, {"EURMGA", "TR20H-EUR-MGA"}, {"EURMKD", "TR20H-EUR-MKD"},
    {"EURMMK", "TR20H-EUR-MMK"}, {"EURMOP", "TR20H-EUR-MOP"}, {"EURMUR", "TR20H-EUR-MUR"}, {"EURMWK", "TR20H-EUR-MWK"},
    {"EURMXN", "TR20H-EUR-MXN"}, {"EURMYR", "TR20H-EUR-MYR"}, {"EURMZN", "TR20H-EUR-MZN"}, {"EURNAD", "TR20H-EUR-NAD"},
    {"EURNGN", "TR20H-EUR-NGN"}, {"EURNOK", "TR20H-EUR-NOK"}, {"EURNPR", "TR20H-EUR-NPR"}, {"EURNZD", "TR20H-EUR-NZD"},
    {"EUROMR", "TR20H-EUR-OMR"}, {"EURPAB", "TR20H-EUR-PAB"}, {"EURPEN", "TR20H-EUR-PEN"}, {"EURPHP", "TR20H-EUR-PHP"},
    {"EURPKR", "TR20H-EUR-PKR"}, {"EURPLN", "TR20H-EUR-PLN"}, {"EURPYG", "TR20H-EUR-PYG"}, {"EURQAR", "TR20H-EUR-QAR"},
    {"EURRON", "TR20H-EUR-RON"}, {"EURRSD", "TR20H-EUR-RSD"}, {"EURRUB", "TR20H-EUR-RUB"}, {"EURSAR", "TR20H-EUR-SAR"},
    {"EURSDG", "TR20H-EUR-SDG"}, {"EURSEK", "TR20H-EUR-SEK"}, {"EURSGD", "TR20H-EUR-SGD"}, {"EURSRD", "TR20H-EUR-SRD"},
    {"EURSVC", "TR20H-EUR-SVC"}, {"EURSYP", "TR20H-EUR-SYP"}, {"EURTHB", "TR20H-EUR-THB"}, {"EURTJS", "TR20H-EUR-TJS"},
    {"EURTMT", "TR20H-EUR-TMT"}, {"EURTND", "TR20H-EUR-TND"}, {"EURTRY", "TR20H-EUR-TRY"}, {"EURTTD", "TR20H-EUR-TTD"},
    {"EURTWD", "TR20H-EUR-TWD"}, {"EURTZS", "TR20H-EUR-TZS"}, {"EURUAH", "TR20H-EUR-UAH"}, {"EURUGX", "TR20H-EUR-UGX"},
    {"EURUSD", "TR20H-EUR-USD"}, {"EURUYU", "TR20H-EUR-UYU"}, {"EURVES", "TR20H-EUR-VES"}, {"EURVND", "TR20H-EUR-VND"},
    {"EURXAF", "TR20H-EUR-XAF"}, {"EURXCD", "TR20H-EUR-XCD"}, {"EURXOF", "TR20H-EUR-XOF"}, {"EURYER", "TR20H-EUR-YER"},
    {"EURZAR", "TR20H-EUR-ZAR"}, {"EURZMW", "TR20H-EUR-ZMW"}, {"AEDGBP", "TR20H-GBP-AED"}, {"AFNGBP", "TR20H-GBP-AFN"},
    {"ALLGBP", "TR20H-GBP-ALL"}, {"ANGGBP", "TR20H-GBP-ANG"}, {"AOAGBP", "TR20H-GBP-AOA"}, {"ARSGBP", "TR20H-GBP-ARS"},
    {"AUDGBP", "TR20H-GBP-AUD"}, {"AZNGBP", "TR20H-GBP-AZN"}, {"BAMGBP", "TR20H-GBP-BAM"}, {"BBDGBP", "TR20H-GBP-BBD"},
    {"BDTGBP", "TR20H-GBP-BDT"}, {"BGNGBP", "TR20H-GBP-BGN"}, {"BHDGBP", "TR20H-GBP-BHD"}, {"BNDGBP", "TR20H-GBP-BND"},
    {"BOBGBP", "TR20H-GBP-BOB"}, {"BRLGBP", "TR20H-GBP-BRL"}, {"BWPGBP", "TR20H-GBP-BWP"}, {"BYNGBP", "TR20H-GBP-BYN"},
    {"CADGBP", "TR20H-GBP-CAD"}, {"CDFGBP", "TR20H-GBP-CDF"}, {"CHFGBP", "TR20H-GBP-CHF"}, {"CLPGBP", "TR20H-GBP-CLP"},
    {"CNYGBP", "TR20H-GBP-CNY"}, {"COPGBP", "TR20H-GBP-COP"}, {"CRCGBP", "TR20H-GBP-CRC"}, {"CZKGBP", "TR20H-GBP-CZK"},
    {"DJFGBP", "TR20H-GBP-DJF"}, {"DKKGBP", "TR20H-GBP-DKK"}, {"DOPGBP", "TR20H-GBP-DOP"}, {"DZDGBP", "TR20H-GBP-DZD"},
    {"EGPGBP", "TR20H-GBP-EGP"}, {"ETBGBP", "TR20H-GBP-ETB"}, {"EURGBP", "TR20H-GBP-EUR"}, {"FJDGBP", "TR20H-GBP-FJD"},
    {"GBPGEL", "TR20H-GBP-GEL"}, {"GBPGHS", "TR20H-GBP-GHS"}, {"GBPGNF", "TR20H-GBP-GNF"}, {"GBPGTQ", "TR20H-GBP-GTQ"},
    {"GBPGYD", "TR20H-GBP-GYD"}, {"GBPHKD", "TR20H-GBP-HKD"}, {"GBPHNL", "TR20H-GBP-HNL"}, {"GBPHRK", "TR20H-GBP-HRK"},
    {"GBPHTG", "TR20H-GBP-HTG"}, {"GBPHUF", "TR20H-GBP-HUF"}, {"GBPIDR", "TR20H-GBP-IDR"}, {"GBPILS", "TR20H-GBP-ILS"},
    {"GBPINR", "TR20H-GBP-INR"}, {"GBPIQD", "TR20H-GBP-IQD"}, {"GBPIRR", "TR20H-GBP-IRR"}, {"GBPISK", "TR20H-GBP-ISK"},
    {"GBPJMD", "TR20H-GBP-JMD"}, {"GBPJOD", "TR20H-GBP-JOD"}, {"GBPJPY", "TR20H-GBP-JPY"}, {"GBPKES", "TR20H-GBP-KES"},
    {"GBPKGS", "TR20H-GBP-KGS"}, {"GBPKHR", "TR20H-GBP-KHR"}, {"GBPKRW", "TR20H-GBP-KRW"}, {"GBPKWD", "TR20H-GBP-KWD"},
    {"GBPKYD", "TR20H-GBP-KYD"}, {"GBPKZT", "TR20H-GBP-KZT"}, {"GBPLAK", "TR20H-GBP-LAK"}, {"GBPLBP", "TR20H-GBP-LBP"},
    {"GBPLKR", "TR20H-GBP-LKR"}, {"GBPLYD", "TR20H-GBP-LYD"}, {"GBPMAD", "TR20H-GBP-MAD"}, {"GBPMDL", "TR20H-GBP-MDL"},
    {"GBPMGA", "TR20H-GBP-MGA"}, {"GBPMKD", "TR20H-GBP-MKD"}, {"GBPMMK", "TR20H-GBP-MMK"}, {"GBPMOP", "TR20H-GBP-MOP"},
    {"GBPMUR", "TR20H-GBP-MUR"}, {"GBPMWK", "TR20H-GBP-MWK"}, {"GBPMXN", "TR20H-GBP-MXN"}, {"GBPMYR", "TR20H-GBP-MYR"},
    {"GBPMZN", "TR20H-GBP-MZN"}, {"GBPNAD", "TR20H-GBP-NAD"}, {"GBPNGN", "TR20H-GBP-NGN"}, {"GBPNIO", "TR20H-GBP-NIO"},
    {"GBPNOK", "TR20H-GBP-NOK"}, {"GBPNPR", "TR20H-GBP-NPR"}, {"GBPNZD", "TR20H-GBP-NZD"}, {"GBPOMR", "TR20H-GBP-OMR"},
    {"GBPPAB", "TR20H-GBP-PAB"}, {"GBPPEN", "TR20H-GBP-PEN"}, {"GBPPHP", "TR20H-GBP-PHP"}, {"GBPPKR", "TR20H-GBP-PKR"},
    {"GBPPLN", "TR20H-GBP-PLN"}, {"GBPPYG", "TR20H-GBP-PYG"}, {"GBPQAR", "TR20H-GBP-QAR"}, {"GBPRON", "TR20H-GBP-RON"},
    {"GBPRSD", "TR20H-GBP-RSD"}, {"GBPRUB", "TR20H-GBP-RUB"}, {"GBPRWF", "TR20H-GBP-RWF"}, {"GBPSAR", "TR20H-GBP-SAR"},
    {"GBPSDG", "TR20H-GBP-SDG"}, {"GBPSEK", "TR20H-GBP-SEK"}, {"GBPSGD", "TR20H-GBP-SGD"}, {"GBPSRD", "TR20H-GBP-SRD"},
    {"GBPSVC", "TR20H-GBP-SVC"}, {"GBPSYP", "TR20H-GBP-SYP"}, {"GBPTHB", "TR20H-GBP-THB"}, {"GBPTJS", "TR20H-GBP-TJS"},
    {"GBPTMT", "TR20H-GBP-TMT"}, {"GBPTND", "TR20H-GBP-TND"}, {"GBPTRY", "TR20H-GBP-TRY"}, {"GBPTTD", "TR20H-GBP-TTD"},
    {"GBPTWD", "TR20H-GBP-TWD"}, {"GBPTZS", "TR20H-GBP-TZS"}, {"GBPUAH", "TR20H-GBP-UAH"}, {"GBPUGX", "TR20H-GBP-UGX"},
    {"GBPUSD", "TR20H-GBP-USD"}, {"GBPUYU", "TR20H-GBP-UYU"}, {"GBPVES", "TR20H-GBP-VES"}, {"GBPVND", "TR20H-GBP-VND"},
    {"GBPXAF", "TR20H-GBP-XAF"}, {"GBPXCD", "TR20H-GBP-XCD"}, {"GBPXOF", "TR20H-GBP-XOF"}, {"GBPYER", "TR20H-GBP-YER"},
    {"GBPZAR", "TR20H-GBP-ZAR"}, {"GBPZMW", "TR20H-GBP-ZMW"}, {"JPYSGD", "TR20H-JPY-SGD"}, {"JPYMXN", "TR20H-MXN-JPY"},
    {"JPYNOK", "TR20H-NOK-JPY"}, {"NOKSEK", "TR20H-NOK-SEK"}, {"AEDNZD", "TR20H-NZD-AED"}, {"ARSNZD", "TR20H-NZD-ARS"},
    {"AUDNZD", "TR20H-NZD-AUD"}, {"BBDNZD", "TR20H-NZD-BBD"}, {"BDTNZD", "TR20H-NZD-BDT"}, {"BGNNZD", "TR20H-NZD-BGN"},
    {"BHDNZD", "TR20H-NZD-BHD"}, {"BNDNZD", "TR20H-NZD-BND"}, {"BRLNZD", "TR20H-NZD-BRL"}, {"CADNZD", "TR20H-NZD-CAD"},
    {"CHFNZD", "TR20H-NZD-CHF"}, {"CLPNZD", "TR20H-NZD-CLP"}, {"CNYNZD", "TR20H-NZD-CNY"}, {"COPNZD", "TR20H-NZD-COP"},
    {"CRCNZD", "TR20H-NZD-CRC"}, {"CYPNZD", "TR20H-NZD-CYP"}, {"CZKNZD", "TR20H-NZD-CZK"}, {"DKKNZD", "TR20H-NZD-DKK"},
    {"DZDNZD", "TR20H-NZD-DZD"}, {"EGPNZD", "TR20H-NZD-EGP"}, {"EURNZD", "TR20H-NZD-EUR"}, {"FJDNZD", "TR20H-NZD-FJD"},
    {"GBPNZD", "TR20H-NZD-GBP"}, {"GHSNZD", "TR20H-NZD-GHS"}, {"GNFNZD", "TR20H-NZD-GNF"}, {"GYDNZD", "TR20H-NZD-GYD"},
    {"HKDNZD", "TR20H-NZD-HKD"}, {"HNLNZD", "TR20H-NZD-HNL"}, {"HRKNZD", "TR20H-NZD-HRK"}, {"HTGNZD", "TR20H-NZD-HTG"},
    {"HUFNZD", "TR20H-NZD-HUF"}, {"IDRNZD", "TR20H-NZD-IDR"}, {"ILSNZD", "TR20H-NZD-ILS"}, {"INRNZD", "TR20H-NZD-INR"},
    {"IRRNZD", "TR20H-NZD-IRR"}, {"ISKNZD", "TR20H-NZD-ISK"}, {"JMDNZD", "TR20H-NZD-JMD"}, {"JODNZD", "TR20H-NZD-JOD"},
    {"JPYNZD", "TR20H-NZD-JPY"}, {"KESNZD", "TR20H-NZD-KES"}, {"KRWNZD", "TR20H-NZD-KRW"}, {"KWDNZD", "TR20H-NZD-KWD"},
    {"LBPNZD", "TR20H-NZD-LBP"}, {"LYDNZD", "TR20H-NZD-LYD"}, {"MADNZD", "TR20H-NZD-MAD"}, {"MURNZD", "TR20H-NZD-MUR"},
    {"MXNNZD", "TR20H-NZD-MXN"}, {"MYRNZD", "TR20H-NZD-MYR"}, {"NGNNZD", "TR20H-NZD-NGN"}, {"NOKNZD", "TR20H-NZD-NOK"},
    {"NPRNZD", "TR20H-NZD-NPR"}, {"NZDOMR", "TR20H-NZD-OMR"}, {"NZDPAB", "TR20H-NZD-PAB"}, {"NZDPEN", "TR20H-NZD-PEN"},
    {"NZDPGK", "TR20H-NZD-PGK"}, {"NZDPHP", "TR20H-NZD-PHP"}, {"NZDPKR", "TR20H-NZD-PKR"}, {"NZDPLN", "TR20H-NZD-PLN"},
    {"NZDRON", "TR20H-NZD-RON"}, {"NZDRUB", "TR20H-NZD-RUB"}, {"NZDSAR", "TR20H-NZD-SAR"}, {"NZDSBD", "TR20H-NZD-SBD"},
    {"NZDSEK", "TR20H-NZD-SEK"}, {"NZDSGD", "TR20H-NZD-SGD"}, {"NZDSKK", "TR20H-NZD-SKK"}, {"NZDSRD", "TR20H-NZD-SRD"},
    {"NZDSYP", "TR20H-NZD-SYP"}, {"NZDTHB", "TR20H-NZD-THB"}, {"NZDTND", "TR20H-NZD-TND"}, {"NZDTRY", "TR20H-NZD-TRY"},
    {"NZDTTD", "TR20H-NZD-TTD"}, {"NZDTWD", "TR20H-NZD-TWD"}, {"NZDUSD", "TR20H-NZD-USD"}, {"NZDUYU", "TR20H-NZD-UYU"},
    {"NZDVES", "TR20H-NZD-VES"}, {"NZDVND", "TR20H-NZD-VND"}, {"NZDVUV", "TR20H-NZD-VUV"}, {"NZDXAF", "TR20H-NZD-XAF"},
    {"NZDXPF", "TR20H-NZD-XPF"}, {"NZDZAR", "TR20H-NZD-ZAR"}, {"PLNRUB", "TR20H-PLN-RUB"}, {"JPYSEK", "TR20H-SEK-JPY"},
    {"AEDUSD", "TR20H-USD-AED"}, {"ARSUSD", "TR20H-USD-ARS"}, {"BRLUSD", "TR20H-USD-BRL"}, {"CADUSD", "TR20H-USD-CAD"},
    {"CHFUSD", "TR20H-USD-CHF"}, {"CLPUSD", "TR20H-USD-CLP"}, {"CNHUSD", "TR20H-USD-CNH"}, {"CNYUSD", "TR20H-USD-CNY"},
    {"COPUSD", "TR20H-USD-COP"}, {"DKKUSD", "TR20H-USD-DKK"}, {"HKDUSD", "TR20H-USD-HKD"}, {"IDRUSD", "TR20H-USD-IDR"},
    {"INRUSD", "TR20H-USD-INR"}, {"JPYUSD", "TR20H-USD-JPY"}, {"KRWUSD", "TR20H-USD-KRW"}, {"KZTUSD", "TR20H-USD-KZT"},
    {"MXNUSD", "TR20H-USD-MXN"}, {"MYRUSD", "TR20H-USD-MYR"}, {"NOKUSD", "TR20H-USD-NOK"}, {"PENUSD", "TR20H-USD-PEN"},
    {"PHPUSD", "TR20H-USD-PHP"}, {"RONUSD", "TR20H-USD-RON"}, {"SARUSD", "TR20H-USD-SAR"}, {"SEKUSD", "TR20H-USD-SEK"},
    {"SGDUSD", "TR20H-USD-SGD"}, {"THBUSD", "TR20H-USD-THB"}, {"TRYUSD", "TR20H-USD-TRY"}, {"TWDUSD", "TR20H-USD-TWD"},
    {"USDZAR", "TR20H-USD-ZAR"}, {"JPYZAR", "TR20H-ZAR-JPY"}};

boost::shared_ptr<CurrencyHedgedEquityIndexDecomposition>
loadCurrencyHedgedIndexDecomposition(const std::string& name, const boost::shared_ptr<ReferenceDataManager>& refDataMgr,
                                     const boost::shared_ptr<CurveConfigurations>& curveConfigs) {
    boost::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum> indexRefData;
    boost::shared_ptr<EquityIndexReferenceDatum> underlyingRefData;
    std::map<std::string, std::pair<double, std::string>> currencyWeightsAndFxIndexNames;
    
    if (refDataMgr) {
        try {
            indexRefData = boost::dynamic_pointer_cast<CurrencyHedgedEquityIndexReferenceDatum>(
                refDataMgr->getData("CurrencyHedgedEquityIndex", name));
        } catch (...) {
            // Index not a CurrencyHedgedEquityIndex or referencedata is missing, don't throw here
        }

        if (indexRefData != nullptr) {
            try {
                underlyingRefData = boost::dynamic_pointer_cast<EquityIndexReferenceDatum>(
                    refDataMgr->getData("EquityIndex", indexRefData->underlyingIndexName()));
            } catch (...) {
                // referencedata is missing, but don't throw here, return null ptr
            }
        }
    }

    if (indexRefData == nullptr || underlyingRefData == nullptr) {
        return nullptr;
    }

    // Load currency Weights
    std::map<std::string, double> currencyWeights;
    std::string indexCurrency;
    std::string underlyingIndexCurrency;
    std::string fxIndexName;
    
    // Get currency hedged index currency
    if (curveConfigs && curveConfigs->hasEquityCurveConfig(indexRefData->id())) {
        indexCurrency = curveConfigs->equityCurveConfig(indexRefData->id())->currency();
    } else {
        WLOG("Can not find curveConfig for " << indexRefData->id() << " and can not determine the index currecy");
        return nullptr;
    }

    
    if (curveConfigs && curveConfigs->hasEquityCurveConfig(indexRefData->underlyingIndexName())) {
        
        // Get Fx Index to convert UnderlyingIndexCCY into HedgedIndexCurrency
        std::string underlyingIndexName = indexRefData->underlyingIndexName();
        underlyingIndexCurrency =
            curveConfigs->equityCurveConfig(indexRefData->underlyingIndexName())->currency();
        
        std::string sortedPair = underlyingIndexCurrency >= indexCurrency
                                     ? indexCurrency + underlyingIndexCurrency
                                     : underlyingIndexCurrency + indexCurrency;

        auto fxIndexIt = defaultFxIndexMap.find(sortedPair);
        if (fxIndexIt != defaultFxIndexMap.end()) {
            fxIndexName = fxIndexIt->second;
        }

        // Load the currency weights at referenceDate for hedge notionals 
        QuantLib::Date refDate =
            CurrencyHedgedEquityIndexDecomposition::referenceDate(indexRefData, Settings::instance().evaluationDate());
        
        std::vector<std::pair<std::string, double>> underlyingIndexWeightsAtRebalancing;

        if (indexRefData->currencyWeights().empty() && refDataMgr->hasData("EquityIndex", underlyingIndexName, refDate)) {
            auto undIndexRefDataAtRefDate = refDataMgr->getData("EquityIndex", underlyingIndexName, refDate);
            underlyingIndexWeightsAtRebalancing =
                boost::dynamic_pointer_cast<EquityIndexReferenceDatum>(undIndexRefDataAtRefDate)->underlyings();
        } else {
            underlyingIndexWeightsAtRebalancing = indexRefData->currencyWeights();
        }
        
        if (underlyingIndexWeightsAtRebalancing.empty()) {
            currencyWeights[underlyingIndexCurrency] = 1.0;
        } else {
            for (const auto& [name, weight] : underlyingIndexWeightsAtRebalancing) {
                // try look up currency in reference data and add if FX delta risk if necessary
                if (curveConfigs->hasEquityCurveConfig(name)) {
                    auto ecc = curveConfigs->equityCurveConfig(name);
                    auto eqCcy = ecc->currency();
                    currencyWeights[eqCcy] += weight;
                } else {
                    // Assume Index currency
                    currencyWeights[underlyingIndexCurrency] += weight;
                }
            }
        }
    }

    for (const auto& [currency, weight] : currencyWeights) {
        if (currency != indexCurrency) {
            std::string sortedPair = currency >= indexCurrency ? indexCurrency + currency : currency + indexCurrency;
            auto defaultIndexIt = defaultFxIndexMap.find(sortedPair);
            if (defaultIndexIt != defaultFxIndexMap.end()) {
                currencyWeightsAndFxIndexNames[currency] = std::make_pair(weight, defaultIndexIt->second);
            } else {
                currencyWeightsAndFxIndexNames[currency] =
                    std::make_pair(weight, "GENERIC-" + indexCurrency + "-" + currency);
            }
        }
    }
    return boost::make_shared<CurrencyHedgedEquityIndexDecomposition>(name, indexRefData, underlyingRefData,
                                                                      indexCurrency, underlyingIndexCurrency,
                                                                      fxIndexName, currencyWeightsAndFxIndexNames);
}

QuantLib::Date CurrencyHedgedEquityIndexDecomposition::referenceDate(
    const boost::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum>& refData,
                                                      const QuantLib::Date& asof) {
    QuantLib::Date hedgingDate = CurrencyHedgedEquityIndexDecomposition::rebalancingDate(refData, asof);
    if (hedgingDate == QuantLib::Date()) {
        return QuantLib::Date();
    } else {
        return refData->hedgeCalendar().advance(hedgingDate, -refData->referenceDateOffset() * QuantLib::Days,
                                                QuantLib::Preceding);
    }
}

QuantLib::Date CurrencyHedgedEquityIndexDecomposition::rebalancingDate(
    const boost::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum>& refData,
                                                        const QuantLib::Date& asof) {
    if (refData->rebalancingStrategy() ==
        ore::data::CurrencyHedgedEquityIndexReferenceDatum::RebalancingDate::EndOfMonth) {
        QuantLib::Date lastBusinessDayOfCurrentMonth = QuantLib::Date::endOfMonth(asof);
        lastBusinessDayOfCurrentMonth =
            refData->hedgeCalendar().adjust(lastBusinessDayOfCurrentMonth, QuantLib::Preceding);
        if (asof == lastBusinessDayOfCurrentMonth) {
            return asof;
        } else {
            return refData->hedgeCalendar().advance(QuantLib::Date(1, asof.month(), asof.year()), -1 * QuantLib::Days,
                                                    QuantLib::Preceding);
        }
    }
    return QuantLib::Date();
}

QuantLib::Date CurrencyHedgedEquityIndexDecomposition::referenceDate(const QuantLib::Date& asof) const {
    return CurrencyHedgedEquityIndexDecomposition::referenceDate(this->indexRefData(), asof);
}

QuantLib::Date CurrencyHedgedEquityIndexDecomposition::rebalancingDate(const QuantLib::Date& asof) const {
    return CurrencyHedgedEquityIndexDecomposition::rebalancingDate(this->indexRefData(), asof);
}

std::map<std::string, double> CurrencyHedgedEquityIndexDecomposition::fxSpotRiskFromForwards(
    const double quantity, const QuantLib::Date& asof, const boost::shared_ptr<ore::data::Market>& todaysMarket) const {

    std::map<std::string, double> fxRisks;
    auto indexCurve = todaysMarket->equityCurve(indexName());
    auto underlyingCurve = todaysMarket->equityCurve(underlyingIndexName());
    QuantLib::Date refDate = referenceDate(asof);
    double adjustmentFactor = 1.0;
    // If adjustement is daily, the fxForward notional will be adjusted by the relative return of the underlying index
    if (indexRefData_->hedgeAdjustmentRule() == CurrencyHedgedEquityIndexReferenceDatum::HedgeAdjustment::Daily) {
        adjustmentFactor = underlyingCurve->fixing(asof) / underlyingCurve->fixing(rebalancingDate(asof));
    }
    // Compute notionals and fxSpotRisks
    for (const auto& [ccy, weightAndIndex] : currencyWeightsAndFxIndexNames()) {
        double weight = weightAndIndex.first;
        auto fxIndexFamily = parseFxIndex("FX-" + fxIndexName())->familyName();
        auto fxIndex =
            todaysMarket->fxIndex("FX-" + fxIndexFamily + "-" + underlyingIndexCurrency_ + "-" + indexCurrency_);
        double forwardNotional =
            quantity * adjustmentFactor * weight * indexCurve->fixing(refDate) / fxIndex->fixing(refDate);
        fxRisks[ccy] = 0.01 * forwardNotional * fxIndex->fixing(asof);
    }
    return fxRisks;
}

double
CurrencyHedgedEquityIndexDecomposition::unhedgedDelta(double hedgedDelta, const double quantity,
                                                      const QuantLib::Date& asof,
                                                      const boost::shared_ptr<ore::data::Market>& todaysMarket) const {
    auto indexCurve = todaysMarket->equityCurve(indexName());
    auto underlyingCurve = todaysMarket->equityCurve(underlyingIndexName());
    QuantLib::Date rebalanceDt = rebalancingDate(asof);
    auto fxIndexFamily = parseFxIndex("FX-" + fxIndexName())->familyName();
    auto fxIndex = todaysMarket->fxIndex("FX-" + fxIndexFamily + "-" + underlyingIndexCurrency_ + "-" + indexCurrency_);
    // In case we have a option unit delta isnt one
    double scaling = (hedgedDelta * 100 / quantity) / indexCurve->fixing(asof); 
    double fxReturn = fxIndex->fixing(asof) / fxIndex->fixing(rebalanceDt);
    double underlyingIndexReturn = underlyingCurve->equitySpot()->value() / underlyingCurve->fixing(rebalanceDt);
    double unhedgedDelta= indexCurve->fixing(rebalanceDt) * underlyingIndexReturn * fxReturn;
    return scaling * 0.01 * quantity * unhedgedDelta;
}

void CurrencyHedgedEquityIndexDecomposition::addAdditionalFixingsForEquityIndexDecomposition(
    const QuantLib::Date& asof, std::map<std::string, std::set<QuantLib::Date>>& fixings) const {
    if (isValid()) {
        QuantLib::Date rebalancingDt = rebalancingDate(asof);
        QuantLib::Date referenceDt = referenceDate(asof);
        fixings[IndexNameTranslator::instance().oreName(indexName())].insert(rebalancingDt);
        fixings[IndexNameTranslator::instance().oreName(indexName())].insert(referenceDt);

        IndexNameTranslator::instance().add(underlyingIndexName(), "EQ-" + underlyingIndexName());
        fixings[IndexNameTranslator::instance().oreName(underlyingIndexName())].insert(rebalancingDt);
        fixings[IndexNameTranslator::instance().oreName(underlyingIndexName())].insert(referenceDt);

        fixings["FX-" + fxIndexName()].insert(referenceDt);
        fixings["FX-" + fxIndexName()].insert(rebalancingDt);

        for (const auto& [currency, name] : currencyWeightsAndFxIndexNames()) {
            fixings["FX-" + name.second].insert(referenceDt);
            fixings["FX-" + name.second].insert(rebalancingDt);
        }
    }
}

} // namespace data
} // namespace ore
