/*
 * Java core library component.
 *
 * Copyright (c) 2002
 *      Dalibor Topic <robilad@yahoo.com>.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file.
 */

package kaffe.util.locale;

import java.util.ListResourceBundle;

/* The class for ISO 3166-Alpha-3 country codes.
 *
 * based on the ISO 3166-1 and ISO 3166-Alpha-2 code
 * lists available online on the DIN web site, as
 * well as the UN Statistics Department lists available
 * online at the UN web site.
 *
 * The list is in sync with the official list from
 * 2002-10-17. 
 *
 */
public class CountryCodes extends ListResourceBundle {
    public Object [] [] getContents() {
	return contents;
    }

    private Object [] [] contents = {
	{ "AF", "AFG" }, // AFGHANISTAN
	{ "AL", "ALB" }, // ALBANIA
	{ "DZ", "DZA" }, // ALGERIA
	{ "AS", "ASM" }, // AMERICAN SAMOA
	{ "AD", "AND" }, // ANDORRA
	{ "AO", "AGO" }, // ANGOLA
	{ "AI", "AIA" }, // ANGUILLA
	{ "AQ", "ATA" }, // ANTARCTICA
	{ "AG", "ATG" }, // ANTIGUA AND BARBUDA
	{ "AR", "ARG" }, // ARGENTINA
	{ "AM", "ARM" }, // ARMENIA
	{ "AW", "ABW" }, // ARUBA
	{ "AU", "AUS" }, // AUSTRALIA
	{ "AT", "AUT" }, // AUSTRIA
	{ "AZ", "AZE" }, // AZERBAIJAN

	{ "BS", "BHS" }, // BAHAMAS
	{ "BH", "BHR" }, // BAHRAIN
	{ "BD", "BGD" }, // BANGLADESH
	{ "BB", "BRB" }, // BARBADOS
	{ "BY", "BLR" }, // BELARUS
	{ "BE", "BEL" }, // BELGIUM
	{ "BZ", "BLZ" }, // BELIZE
	{ "BJ", "BEN" }, // BENIN
	{ "BM", "BMU" }, // BERMUDA
	{ "BT", "BTN" }, // BHUTAN
	{ "BO", "BOL" }, // BOLIVIA
	{ "BA", "BIH" }, // BOSNIA AND HERZEGOVINA
	{ "BW", "BWA" }, // BOTSWANA
	{ "BV", "BVT" }, // BOUVET ISLAND
	{ "BR", "BRA" }, // BRAZIL
	{ "IO", "IOT" }, // BRITISH INDIAN OCEAN TERRITORY
	{ "BN", "BRN" }, // BRUNEI DARUSSALAM
	{ "BG", "BGR" }, // BULGARIA
	{ "BF", "BFA" }, // BURKINA FASO
	{ "BI", "BDI" }, // BURUNDI

	{ "KH", "KHM" }, // CAMBODIA
	{ "CM", "CMR" }, // CAMEROON
	{ "CA", "CAN" }, // CANADA
	{ "CV", "CPV" }, // CAPE VERDE
	{ "KY", "CYM" }, // CAYMAN ISLANDS
	{ "CF", "CAF" }, // CENTRAL AFRICAN REPUBLIC
	{ "TD", "TCD" }, // CHAD
	{ "CL", "CHL" }, // CHILE
	{ "CN", "CHN" }, // CHINA
	{ "CX", "CXR" }, // CHRISTMAS ISLAND
	{ "CC", "CCK" }, // COCOS (KEELING) ISLANDS
	{ "CO", "COL" }, // COLOMBIA
	{ "KM", "COM" }, // COMOROS
	{ "CG", "COG" }, // CONGO
	{ "CD", "COD" }, // CONGO, THE DEMOCRATIC REPUBLIC OF THE
	{ "CK", "COK" }, // COOK ISLANDS
	{ "CR", "CRI" }, // COSTA RICA
	{ "CI", "CIV" }, // COTE D'IVOIRE
	{ "HR", "HRV" }, // CROATIA
	{ "CU", "CUB" }, // CUBA
	{ "CY", "CYP" }, // CYPRUS
	{ "CZ", "CZE" }, // CZECH REPUBLIC

	{ "DK", "DNK" }, // DENMARK
	{ "DJ", "DJI" }, // DJIBOUTI
	{ "DM", "DMA" }, // DOMINICA
	{ "DO", "DOM" }, // DOMINICAN REPUBLIC

	{ "TP", "TMP" }, // EAST TIMOR
	{ "EC", "ECU" }, // ECUADOR
	{ "EG", "EGY" }, // EGYPT
	{ "SV", "SLV" }, // EL SALVADOR
	{ "GQ", "GNQ" }, // EQUATORIAL GUINEA
	{ "ER", "ERI" }, // ERITREA
	{ "EE", "EST" }, // ESTONIA
	{ "ET", "ETH" }, // ETHIOPIA

	{ "FK", "FLK" }, // FALKLAND ISLANDS (MALVINAS)
	{ "FO", "FRO" }, // FAROE ISLANDS
	{ "FJ", "FJI" }, // FIJI
	{ "FI", "FIN" }, // FINLAND
	{ "FR", "FRA" }, // FRANCE
	{ "GF", "GUF" }, // FRENCH GUIANA
	{ "PF", "PYF" }, // FRENCH POLYNESIA
	{ "TF", "ATF" }, // FRENCH SOUTHERN TERRITORIES

	{ "GA", "GAB" }, // GABON
	{ "GM", "GMB" }, // GAMBIA
	{ "GE", "GEO" }, // GEORGIA
	{ "DE", "DEU" }, // GERMANY
	{ "GH", "GHA" }, // GHANA
	{ "GI", "GIB" }, // GIBRALTAR
	{ "GR", "GRC" }, // GREECE
	{ "GL", "GRL" }, // GREENLAND
	{ "GD", "GRD" }, // GRENADA
	{ "GP", "GLP" }, // GUADELOUPE
	{ "GU", "GUM" }, // GUAM
	{ "GT", "GTM" }, // GUATEMALA
	{ "GN", "GIN" }, // GUINEA
	{ "GW", "GNB" }, // GUINEA-BISSAU
	{ "GY", "GUY" }, // GUYANA

	{ "HT", "HTI" }, // HAITI
	{ "HM", "HMD" }, // HEARD ISLAND AND MCDONALD ISLANDS
	{ "VA", "VAT" }, // HOLY SEE (VATICAN CITY STATE)
	{ "HN", "HND" }, // HONDURAS
	{ "HK", "HKG" }, // HONG KONG
	{ "HU", "HUN" }, // HUNGARY

	{ "IS", "ISL" }, // ICELAND
	{ "IN", "IND" }, // INDIA
	{ "ID", "IDN" }, // INDONESIA
	{ "IR", "IRN" }, // IRAN, ISLAMIC REPUBLIC OF
	{ "IQ", "IRQ" }, // IRAQ
	{ "IE", "IRL" }, // IRELAND
	{ "IL", "ISR" }, // ISRAEL
	{ "IT", "ITA" }, // ITALY

	{ "JM", "JAM" }, // JAMAICA
	{ "JP", "JPN" }, // JAPAN
	{ "JO", "JOR" }, // JORDAN

	{ "KZ", "KAZ" }, // KAZAKHSTAN
	{ "KE", "KEN" }, // KENYA
	{ "KI", "KIR" }, // KIRIBATI
	{ "KP", "PRK" }, // KOREA, DEMOCRATIC PEOPLE'S REPUBLIC OF
	{ "KR", "KOR" }, // KOREA, REPUBLIC OF
	{ "KW", "KWT" }, // KUWAIT
	{ "KY", "KGZ" }, // KYRGYZSTAN

	{ "LA", "LAO" }, // LAO PEOPLE'S DEMOCRATIC REPUBLIC
	{ "LV", "LVA" }, // LATVIA
	{ "LB", "LBN" }, // LEBANON
	{ "LS", "LSO" }, // LESOTHO
	{ "LR", "LBR" }, // LIBERIA
	{ "LY", "LBY" }, // LIBYAN ARAB JAMAHIRIYA
	{ "LI", "LIE" }, // LIECHTENSTEIN
	{ "LT", "LTU" }, // LITHUANIA
	{ "LU", "LUX" }, // LUXEMBOURG

	{ "MO", "MAC" }, // MACAO
	{ "MK", "MKD" }, // MACEDONIA, THE FORMER YUGOSLAV REPUBLIC OF
	{ "MG", "MDG" }, // MADAGASCAR
	{ "MW", "MWI" }, // MALAWI
	{ "MY", "MYS" }, // MALAYSIA
	{ "MV", "MDV" }, // MALDIVES
	{ "ML", "MLI" }, // MALI
	{ "MT", "MLT" }, // MALTA
	{ "MP", "MNP" }, // MARIANA ISLANDS (NORTHERN)
	{ "MH", "MHL" }, // MARSHALL ISLANDS
	{ "MQ", "MTQ" }, // MARTINIQUE
	{ "MR", "MRT" }, // MAURITANIA
	{ "MU", "MUS" }, // MAURITIUS
	{ "YT", "MYT" }, // MAYOTTE
	{ "MX", "MEX" }, // MEXICO
	{ "FM", "FSM" }, // MICRONESIA, FEDERATED STATES OF
	{ "MD", "MDA" }, // MOLDOVA, REPUBLIC OF
	{ "MC", "MCO" }, // MONACO
	{ "MN", "MNG" }, // MONGOLIA
	{ "MS", "MSR" }, // MONTSERRAT
	{ "MA", "MAR" }, // MOROCCO
	{ "MZ", "MOZ" }, // MOZAMBIQUE
	{ "MM", "MMR" }, // MYANMAR

	{ "NA", "NAM" }, // NAMIBIA
	{ "NR", "NRU" }, // NAURU
	{ "NP", "NPL" }, // NEPAL
	{ "NL", "NLD" }, // NETHERLANDS
	{ "AN", "ANT" }, // NETHERLANDS ANTILLES
	{ "NC", "NCL" }, // NEW CALEDONIA
	{ "NZ", "NZL" }, // NEW ZEALAND
	{ "NI", "NIC" }, // NICARAGUA
	{ "NE", "NER" }, // NIGER
	{ "NG", "NGA" }, // NIGERIA
	{ "NU", "NIU" }, // NIUE
	{ "NF", "NFK" }, // NORFOLK ISLAND
	{ "NO", "NOR" }, // NORWAY

	{ "OM", "OMN" }, // OMAN

	{ "PK", "PAK" }, // PAKISTAN
	{ "PW", "PLW" }, // PALAU
	{ "PS", "PSE" }, // PALESTINIAN TERRITORY, OCCUPIED
	{ "PA", "PAN" }, // PANAMA
	{ "PG", "PNG" }, // PAPUA NEW GUINEA
	{ "PY", "PRY" }, // PARAGUAY
	{ "PE", "PER" }, // PERU
	{ "PH", "PHL" }, // PHILIPPINES
	{ "PN", "PCN" }, // PITCAIRN
	{ "PL", "POL" }, // POLAND
	{ "PT", "PRT" }, // PORTUGAL
	{ "PR", "PRI" }, // PUERTO RICO

	{ "QA", "QAT" }, // QATAR

	{ "RE", "REU" }, // REUNION
	{ "RO", "ROM" }, // ROMANIA
	{ "RU", "RUS" }, // RUSSIAN FEDERATION
	{ "RW", "RWA" }, // RWANDA

	{ "SH", "SHN" }, // SAINT HELENA
	{ "KN", "KNA" }, // SAINT KITTS AND NEVIS
	{ "LC", "LCA" }, // SAINT LUCIA
	{ "PM", "SPM" }, // SAINT PIERRE AND MIQUELON
	{ "VC", "VCT" }, // SAINT VINCENT AND THE GRENADINES
	{ "WS", "WSM" }, // SAMOA
	{ "SM", "SMR" }, // SAN MARINO
	{ "ST", "STP" }, // SAO TOME AND PRINCIPE
	{ "SA", "SAU" }, // SAUDI ARABIA
	{ "SN", "SEN" }, // SENEGAL
	{ "SC", "SYC" }, // SEYCHELLES
	{ "SL", "SLE" }, // SIERRA LEONE
	{ "SG", "SGP" }, // SINGAPORE
	{ "SK", "SVK" }, // SLOVAKIA
	{ "SI", "SVN" }, // SLOVENIA
	{ "SB", "SLB" }, // SOLOMON ISLANDS
	{ "SO", "SOM" }, // SOMALIA
	{ "ZA", "ZAF" }, // SOUTH AFRICA
	{ "GS", "SGS" }, // SOUTH GEORGIA AND THE SOUTH SANDWICH ISLANDS
	{ "ES", "ESP" }, // SPAIN
	{ "LK", "LKA" }, // SRI LANKA
	{ "SD", "SDN" }, // SUDAN
	{ "SR", "SUR" }, // SURINAME
	{ "SJ", "SJM" }, // SVALBARD AND JAN MAYEN
	{ "SZ", "SWZ" }, // SWAZILAND
	{ "SE", "SWE" }, // SWEDEN
	{ "CH", "CHE" }, // SWITZERLAND
	{ "SY", "SYR" }, // SYRIAN ARAB REPUBLIC

	{ "TW", "TWN" }, // TAIWAN, PROVINCE OF CHINA
	{ "TJ", "TJK" }, // TAJIKISTAN
	{ "TZ", "TZA" }, // TANZANIA, UNITED REPUBLIC OF
	{ "TH", "THA" }, // THAILAND
	{ "TG", "TGO" }, // TOGO
	{ "TK", "TKL" }, // TOKELAU
	{ "TO", "TON" }, // TONGA
	{ "TT", "TTO" }, // TRINIDAD AND TOBAGO
	{ "TN", "TUN" }, // TUNISIA
	{ "TR", "TUR" }, // TURKEY
	{ "TM", "TKM" }, // TURKMENISTAN
	{ "TC", "TCA" }, // TURKS AND CAICOS ISLANDS
	{ "TV", "TUV" }, // TUVALU

	{ "UG", "UGA" }, // UGANDA
	{ "UA", "UKR" }, // UKRAINE
	{ "AE", "ARE" }, // UNITED ARAB EMIRATES
	{ "GB", "GBR" }, // UNITED KINGDOM
	{ "US", "USA" }, // UNITED STATES
	{ "UM", "UMI" }, // UNITED STATES MINOR OUTLYING ISLANDS
	{ "UY", "URY" }, // URUGUAY
	{ "UZ", "UZB" }, // UZBEKISTAN

	{ "VU", "VUT" }, // VANUATU
	{ "VE", "VEN" }, // VENEZUELA
	{ "VN", "VNM" }, // VIET NAM
	{ "VG", "VGB" }, // VIRGIN ISLANDS, BRITISH
	{ "VI", "VIR" }, // VIRGIN ISLANDS, U.S

	{ "WF", "WLF" }, // WALLIS AND FUTUNA
	{ "EH", "ESH" }, // WESTERN SAHARA

	{ "YE", "YEM" }, // YEMEN
	{ "YU", "YUG" }, // YUGOSLAVIA

	{ "ZM", "ZMB" }, // ZAMBIA
	{ "ZW", "ZWE" }, // ZIMBABWE
    };
}
