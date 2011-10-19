/* Copyright (C) 2009 LordGregGreg Back

   This is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   This is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the viewer; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "llviewerprecompiledheaders.h"
#include "lgghunspell_wrapper.h"
#include <boost/regex.hpp>
#include "llweb.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llwindow.h"
#include "lldiriterator.h"
#include "llfile.h"
#include "llhttpclient.h"
#include "lggdicdownload.h"

lggHunSpell_Wrapper *glggHunSpell = 0;
Hunspell* lggHunSpell_Wrapper::myHunspell = 0;
// do not insert empty lines after this line until the size calculation
#define COUNTRY_CODES_RAW_START_LINE (__LINE__ + 2)
static char * countryCodesraw[] = {
  (char*)"SL",(char*)"SecondLife",
  (char*)"AD",(char*)"Andorra",
  (char*)"AE",(char*)"United Arab Emirates",
  (char*)"AF",(char*)"Afghanistan",
  (char*)"AG",(char*)"Antigua & Barbuda",
  (char*)"AI",(char*)"Anguilla",
  (char*)"AL",(char*)"Albania",
  (char*)"AM",(char*)"Armenia",
  (char*)"AN",(char*)"Netherlands Antilles",
  (char*)"AO",(char*)"Angola",
  (char*)"AQ",(char*)"Antarctica",
  (char*)"AR",(char*)"Argentina",
  (char*)"AS",(char*)"American Samoa",
  (char*)"AT",(char*)"Austria",
  (char*)"AU",(char*)"Australia",
  (char*)"AW",(char*)"Aruba",
  (char*)"AZ",(char*)"Azerbaijan",
  (char*)"BA",(char*)"Bosnia and Herzegovina",
  (char*)"BB",(char*)"Barbados",
  (char*)"BD",(char*)"Bangladesh",
  (char*)"BE",(char*)"Belgium",
  (char*)"BF",(char*)"Burkina Faso",
  (char*)"BG",(char*)"Bulgaria",
  (char*)"BH",(char*)"Bahrain",
  (char*)"BI",(char*)"Burundi",
  (char*)"BJ",(char*)"Benin",
  (char*)"BM",(char*)"Bermuda",
  (char*)"BN",(char*)"Brunei Darussalam",
  (char*)"BO",(char*)"Bolivia",
  (char*)"BR",(char*)"Brazil",
  (char*)"BS",(char*)"Bahama",
  (char*)"BT",(char*)"Bhutan",
  (char*)"BU",(char*)"Burma",
  (char*)"BV",(char*)"Bouvet Island",
  (char*)"BW",(char*)"Botswana",
  (char*)"BY",(char*)"Belarus",
  (char*)"BZ",(char*)"Belize",
  (char*)"CA",(char*)"Canada",
  (char*)"CC",(char*)"Cocos (Keeling) Islands",
  (char*)"CF",(char*)"Central African Republic",
  (char*)"CG",(char*)"Congo",
  (char*)"CH",(char*)"Switzerland",
  (char*)"CI",(char*)"Côte D'ivoire (Ivory Coast)",
  (char*)"CK",(char*)"Cook Iislands",
  (char*)"CL",(char*)"Chile",
  (char*)"CM",(char*)"Cameroon",
  (char*)"CN",(char*)"China",
  (char*)"CO",(char*)"Colombia",
  (char*)"CR",(char*)"Costa Rica",
  (char*)"CS",(char*)"Czechoslovakia",
  (char*)"CU",(char*)"Cuba",
  (char*)"CV",(char*)"Cape Verde",
  (char*)"CX",(char*)"Christmas Island",
  (char*)"CY",(char*)"Cyprus",
  (char*)"CZ",(char*)"Czech Republic",
  (char*)"DD",(char*)"German Democratic Republic",
  (char*)"DE",(char*)"Germany",
  (char*)"DJ",(char*)"Djibouti",
  (char*)"DK",(char*)"Denmark",
  (char*)"DM",(char*)"Dominica",
  (char*)"DO",(char*)"Dominican Republic",
  (char*)"DZ",(char*)"Algeria",
  (char*)"EC",(char*)"Ecuador",
  (char*)"EE",(char*)"Estonia",
  (char*)"EG",(char*)"Egypt",
  (char*)"EH",(char*)"Western Sahara",
  (char*)"ER",(char*)"Eritrea",
  (char*)"ES",(char*)"Spain",
  (char*)"ET",(char*)"Ethiopia",
  (char*)"FI",(char*)"Finland",
  (char*)"FJ",(char*)"Fiji",
  (char*)"FK",(char*)"Falkland Islands (Malvinas)",
  (char*)"FM",(char*)"Micronesia",
  (char*)"FO",(char*)"Faroe Islands",
  (char*)"FR",(char*)"France",
  (char*)"FX",(char*)"France, Metropolitan",
  (char*)"GA",(char*)"Gabon",
  (char*)"GB",(char*)"United Kingdom (Great Britain)",
  (char*)"GD",(char*)"Grenada",
  (char*)"GE",(char*)"Georgia",
  (char*)"GF",(char*)"French Guiana",
  (char*)"GH",(char*)"Ghana",
  (char*)"GI",(char*)"Gibraltar",
  (char*)"GL",(char*)"Greenland",
  (char*)"GM",(char*)"Gambia",
  (char*)"GN",(char*)"Guinea",
  (char*)"GP",(char*)"Guadeloupe",
  (char*)"GQ",(char*)"Equatorial Guinea",
  (char*)"GR",(char*)"Greece",
  (char*)"GS",(char*)"South Georgia and the South Sandwich Islands",
  (char*)"GT",(char*)"Guatemala",
  (char*)"GU",(char*)"Guam",
  (char*)"GW",(char*)"Guinea-Bissau",
  (char*)"GY",(char*)"Guyana",
  (char*)"HK",(char*)"Hong Kong",
  (char*)"HM",(char*)"Heard & McDonald Islands",
  (char*)"HN",(char*)"Honduras",
  (char*)"HR",(char*)"Croatia",
  (char*)"HT",(char*)"Haiti",
  (char*)"HU",(char*)"Hungary",
  (char*)"ID",(char*)"Indonesia",
  (char*)"IE",(char*)"Ireland",
  (char*)"IL",(char*)"Israel",
  (char*)"IN",(char*)"India",
  (char*)"IO",(char*)"British Indian Ocean Territory",
  (char*)"IQ",(char*)"Iraq",
  (char*)"IR",(char*)"Islamic Republic of Iran",
  (char*)"IS",(char*)"Iceland",
  (char*)"IT",(char*)"Italy",
  (char*)"JM",(char*)"Jamaica",
  (char*)"JO",(char*)"Jordan",
  (char*)"JP",(char*)"Japan",
  (char*)"KE",(char*)"Kenya",
  (char*)"KG",(char*)"Kyrgyzstan",
  (char*)"KH",(char*)"Cambodia",
  (char*)"KI",(char*)"Kiribati",
  (char*)"KM",(char*)"Comoros",
  (char*)"KN",(char*)"St. Kitts and Nevis",
  (char*)"KP",(char*)"Korea, Democratic People's Republic of",
  (char*)"KR",(char*)"Korea, Republic of",
  (char*)"KW",(char*)"Kuwait",
  (char*)"KY",(char*)"Cayman Islands",
  (char*)"KZ",(char*)"Kazakhstan",
  (char*)"LA",(char*)"Lao People's Democratic Republic",
  (char*)"LB",(char*)"Lebanon",
  (char*)"LC",(char*)"Saint Lucia",
  (char*)"LI",(char*)"Liechtenstein",
  (char*)"LK",(char*)"Sri Lanka",
  (char*)"LR",(char*)"Liberia",
  (char*)"LS",(char*)"Lesotho",
  (char*)"LT",(char*)"Lithuania",
  (char*)"LU",(char*)"Luxembourg",
  (char*)"LV",(char*)"Latvia",
  (char*)"LY",(char*)"Libyan Arab Jamahiriya",
  (char*)"MA",(char*)"Morocco",
  (char*)"MC",(char*)"Monaco",
  (char*)"MD",(char*)"Moldova, Republic of",
  (char*)"MG",(char*)"Madagascar",
  (char*)"MH",(char*)"Marshall Islands",
  (char*)"ML",(char*)"Mali",
  (char*)"MN",(char*)"Mongolia",
  (char*)"MM",(char*)"Myanmar",
  (char*)"MO",(char*)"Macau",
  (char*)"MP",(char*)"Northern Mariana Islands",
  (char*)"MQ",(char*)"Martinique",
  (char*)"MR",(char*)"Mauritania",
  (char*)"MS",(char*)"Monserrat",
  (char*)"MT",(char*)"Malta",
  (char*)"MU",(char*)"Mauritius",
  (char*)"MV",(char*)"Maldives",
  (char*)"MW",(char*)"Malawi",
  (char*)"MX",(char*)"Mexico",
  (char*)"MY",(char*)"Malaysia",
  (char*)"MZ",(char*)"Mozambique",
  (char*)"NA",(char*)"Namibia",
  (char*)"NC",(char*)"New Caledonia",
  (char*)"NE",(char*)"Niger",
  (char*)"NF",(char*)"Norfolk Island",
  (char*)"NG",(char*)"Nigeria",
  (char*)"NI",(char*)"Nicaragua",
  (char*)"NL",(char*)"Netherlands",
  (char*)"NO",(char*)"Norway",
  (char*)"NP",(char*)"Nepal",
  (char*)"NR",(char*)"Nauru",
  (char*)"NT",(char*)"Neutral Zone",
  (char*)"NU",(char*)"Niue",
  (char*)"NZ",(char*)"New Zealand",
  (char*)"OM",(char*)"Oman",
  (char*)"PA",(char*)"Panama",
  (char*)"PE",(char*)"Peru",
  (char*)"PF",(char*)"French Polynesia",
  (char*)"PG",(char*)"Papua New Guinea",
  (char*)"PH",(char*)"Philippines",
  (char*)"PK",(char*)"Pakistan",
  (char*)"PL",(char*)"Poland",
  (char*)"PM",(char*)"St. Pierre & Miquelon",
  (char*)"PN",(char*)"Pitcairn",
  (char*)"PR",(char*)"Puerto Rico",
  (char*)"PT",(char*)"Portugal",
  (char*)"PW",(char*)"Palau",
  (char*)"PY",(char*)"Paraguay",
  (char*)"QA",(char*)"Qatar",
  (char*)"RE",(char*)"Réunion",
  (char*)"RO",(char*)"Romania",
  (char*)"RU",(char*)"Russian Federation",
  (char*)"RW",(char*)"Rwanda",
  (char*)"SA",(char*)"Saudi Arabia",
  (char*)"SB",(char*)"Solomon Islands",
  (char*)"SC",(char*)"Seychelles",
  (char*)"SD",(char*)"Sudan",
  (char*)"SE",(char*)"Sweden",
  (char*)"SG",(char*)"Singapore",
  (char*)"SH",(char*)"St. Helena",
  (char*)"SI",(char*)"Slovenia",
  (char*)"SJ",(char*)"Svalbard & Jan Mayen Islands",
  (char*)"SK",(char*)"Slovakia",
  (char*)"SL",(char*)"Sierra Leone",
  (char*)"SM",(char*)"San Marino",
  (char*)"SN",(char*)"Senegal",
  (char*)"SO",(char*)"Somalia",
  (char*)"SR",(char*)"Suriname",
  (char*)"ST",(char*)"Sao Tome & Principe",
  (char*)"SU",(char*)"Union of Soviet Socialist Republics",
  (char*)"SV",(char*)"El Salvador",
  (char*)"SY",(char*)"Syrian Arab Republic",
  (char*)"SZ",(char*)"Swaziland",
  (char*)"TC",(char*)"Turks & Caicos Islands",
  (char*)"TD",(char*)"Chad",
  (char*)"TF",(char*)"French Southern Territories",
  (char*)"TG",(char*)"Togo",
  (char*)"TH",(char*)"Thailand",
  (char*)"TJ",(char*)"Tajikistan",
  (char*)"TK",(char*)"Tokelau",
  (char*)"TM",(char*)"Turkmenistan",
  (char*)"TN",(char*)"Tunisia",
  (char*)"TO",(char*)"Tonga",
  (char*)"TP",(char*)"East Timor",
  (char*)"TR",(char*)"Turkey",
  (char*)"TT",(char*)"Trinidad & Tobago",
  (char*)"TV",(char*)"Tuvalu",
  (char*)"TW",(char*)"Taiwan, Province of China",
  (char*)"TZ",(char*)"Tanzania, United Republic of",
  (char*)"UA",(char*)"Ukraine",
  (char*)"UG",(char*)"Uganda",
  (char*)"UM",(char*)"United States Minor Outlying Islands",
  (char*)"US",(char*)"United States of America",
  (char*)"UY",(char*)"Uruguay",
  (char*)"UZ",(char*)"Uzbekistan",
  (char*)"VA",(char*)"Vatican City State",
  (char*)"VC",(char*)"St. Vincent & the Grenadines",
  (char*)"VE",(char*)"Venezuela",
  (char*)"VG",(char*)"British Virgin Islands",
  (char*)"VI",(char*)"United States Virgin Islands",
  (char*)"VN",(char*)"Viet Nam",
  (char*)"VU",(char*)"Vanuatu",
  (char*)"WF",(char*)"Wallis & Futuna Islands",
  (char*)"WS",(char*)"Samoa",
  (char*)"YD",(char*)"Democratic Yemen",
  (char*)"YE",(char*)"Yemen",
  (char*)"YT",(char*)"Mayotte",
  (char*)"YU",(char*)"Yugoslavia",
  (char*)"ZA",(char*)"South Africa",
  (char*)"ZM",(char*)"Zambia",
  (char*)"ZR",(char*)"Zaire",
  (char*)"ZW",(char*)"Zimbabwe",
  (char*)"ZZ",(char*)"Unknown or unspecified country"
};
//#define COUNTRY_CODES_RAW_SIZE ((__LINE__ - 1 - COUNTRY_CODES_RAW_START_LINE) * 2)
#define COUNTRY_CODES_RAW_SIZE 492
#define LANGUAGE_CODES_RAW_START_LINE (__LINE__ + 2)
static char * languageCodesraw[]={
  (char*)"aa",(char*)"Afar",
  (char*)"ab",(char*)"Abkhazian",
  (char*)"ae",(char*)"Avestan",
  (char*)"af",(char*)"Afrikaans",
  (char*)"ak",(char*)"Akan",
  (char*)"am",(char*)"Amharic",
  (char*)"an",(char*)"Aragonese",
  (char*)"ar",(char*)"Arabic",
  (char*)"as",(char*)"Assamese",
  (char*)"av",(char*)"Avaric",
  (char*)"ay",(char*)"Aymara",
  (char*)"az",(char*)"Azerbaijani",
  (char*)"ba",(char*)"Bashkir",
  (char*)"be",(char*)"Belarusian",
  (char*)"bg",(char*)"Bulgarian",
  (char*)"bh",(char*)"Bihari",
  (char*)"bi",(char*)"Bislama",
  (char*)"bm",(char*)"Bambara",
  (char*)"bn",(char*)"Bengali",
  (char*)"bo",(char*)"Tibetan",
  (char*)"br",(char*)"Breton",
  (char*)"bs",(char*)"Bosnian",
  (char*)"ca",(char*)"Catalan",
  (char*)"ce",(char*)"Chechen",
  (char*)"ch",(char*)"Chamorro",
  (char*)"co",(char*)"Corsican",
  (char*)"cr",(char*)"Cree",
  (char*)"cs",(char*)"Czech",
  (char*)"cu",(char*)"ChurchSlavic",
  (char*)"cv",(char*)"Chuvash",
  (char*)"cy",(char*)"Welsh",
  (char*)"da",(char*)"Danish",
  (char*)"de",(char*)"German",
  (char*)"dv",(char*)"Divehi",
  (char*)"dz",(char*)"Dzongkha",
  (char*)"ee",(char*)"Ewe",
  (char*)"el",(char*)"ModernGreek",
  (char*)"en",(char*)"English",
  (char*)"eo",(char*)"Esperanto",
  (char*)"es",(char*)"Spanish",
  (char*)"et",(char*)"Estonian",
  (char*)"eu",(char*)"Basque",
  (char*)"fa",(char*)"Persian",
  (char*)"ff",(char*)"Fulah",
  (char*)"fi",(char*)"Finnish",
  (char*)"fj",(char*)"Fijian",
  (char*)"fo",(char*)"Faroese",
  (char*)"fr",(char*)"French",
  (char*)"fy",(char*)"WesternFrisian",
  (char*)"ga",(char*)"Irish",
  (char*)"gd",(char*)"Gaelic",
  (char*)"gl",(char*)"Galician",
  (char*)"gn",(char*)"Guaraní",
  (char*)"gu",(char*)"Gujarati",
  (char*)"gv",(char*)"Manx",
  (char*)"ha",(char*)"Hausa",
  (char*)"he",(char*)"ModernHebrew",
  (char*)"hi",(char*)"Hindi",
  (char*)"ho",(char*)"HiriMotu",
  (char*)"hr",(char*)"Croatian",
  (char*)"ht",(char*)"Haitian",
  (char*)"hu",(char*)"Hungarian",
  (char*)"hy",(char*)"Armenian",
  (char*)"hz",(char*)"Herero",
  (char*)"ia",(char*)"Interlingua",
  (char*)"id",(char*)"Indonesian",
  (char*)"ie",(char*)"Interlingue",
  (char*)"ig",(char*)"Igbo",
  (char*)"ii",(char*)"SichuanYi",
  (char*)"ik",(char*)"Inupiaq",
  (char*)"io",(char*)"Ido",
  (char*)"is",(char*)"Icelandic",
  (char*)"it",(char*)"Italian",
  (char*)"iu",(char*)"Inuktitut",
  (char*)"ja",(char*)"Japanese",
  (char*)"jv",(char*)"Javanese",
  (char*)"ka",(char*)"Georgian",
  (char*)"kg",(char*)"Kongo",
  (char*)"ki",(char*)"Kikuyu",
  (char*)"kj",(char*)"Kwanyama",
  (char*)"kk",(char*)"Kazakh",
  (char*)"kl",(char*)"Kalaallisut",
  (char*)"km",(char*)"CentralKhmer",
  (char*)"kn",(char*)"Kannada",
  (char*)"ko",(char*)"Korean",
  (char*)"kr",(char*)"Kanuri",
  (char*)"ks",(char*)"Kashmiri",
  (char*)"ku",(char*)"Kurdish",
  (char*)"kv",(char*)"Komi",
  (char*)"kw",(char*)"Cornish",
  (char*)"ky",(char*)"Kirghiz",
  (char*)"la",(char*)"Latin",
  (char*)"lb",(char*)"Luxembourgish",
  (char*)"lg",(char*)"Ganda",
  (char*)"li",(char*)"Limburgish",
  (char*)"ln",(char*)"Lingala",
  (char*)"lo",(char*)"Lao",
  (char*)"lt",(char*)"Lithuanian",
  (char*)"lu",(char*)"Luba-Katanga",
  (char*)"lv",(char*)"Latvian",
  (char*)"mg",(char*)"Malagasy",
  (char*)"mh",(char*)"Marshallese",
  (char*)"mi",(char*)"Maori",
  (char*)"mk",(char*)"Macedonian",
  (char*)"ml",(char*)"Malayalam",
  (char*)"mn",(char*)"Mongolian",
  (char*)"mr",(char*)"Marathi",
  (char*)"ms",(char*)"Malay",
  (char*)"mt",(char*)"Maltese",
  (char*)"my",(char*)"Burmese",
  (char*)"na",(char*)"Nauru",
  (char*)"nb",(char*)"NorwegianBokmal",
  (char*)"nd",(char*)"NorthNdebele",
  (char*)"ne",(char*)"Nepali",
  (char*)"ng",(char*)"Ndonga",
  (char*)"nl",(char*)"Dutch/Flemish",
  (char*)"nn",(char*)"NorwegianNynorsk",
  (char*)"no",(char*)"Norwegian",
  (char*)"nr",(char*)"SouthNdebele",
  (char*)"nv",(char*)"Navajo/Navaho",
  (char*)"ny",(char*)"Nyanja",
  (char*)"oc",(char*)"Occitan",
  (char*)"oj",(char*)"Ojibwa",
  (char*)"om",(char*)"Oromo",
  (char*)"or",(char*)"Oriya",
  (char*)"os",(char*)"Ossetian",
  (char*)"pa",(char*)"Panjabi",
  (char*)"pi",(char*)"Pali",
  (char*)"pl",(char*)"Polish",
  (char*)"ps",(char*)"Pashto/Pushto",
  (char*)"pt",(char*)"Portuguese",
  (char*)"qu",(char*)"Quechua",
  (char*)"rm",(char*)"Romansh",
  (char*)"rn",(char*)"Rundi",
  (char*)"ro",(char*)"Romanian",
  (char*)"ru",(char*)"Russian",
  (char*)"rw",(char*)"Kinyarwanda",
  (char*)"sa",(char*)"Sanskrit",
  (char*)"sc",(char*)"Sardinian",
  (char*)"sd",(char*)"Sindhi",
  (char*)"se",(char*)"NorthernSami",
  (char*)"sg",(char*)"Sango",
  (char*)"si",(char*)"Sinhala",
  (char*)"sk",(char*)"Slovak",
  (char*)"sl",(char*)"Slovene",
  (char*)"sm",(char*)"Samoan",
  (char*)"sn",(char*)"Shona",
  (char*)"so",(char*)"Somali",
  (char*)"sq",(char*)"Albanian",
  (char*)"sr",(char*)"Serbian",
  (char*)"ss",(char*)"Swati",
  (char*)"st",(char*)"SouthernSotho",
  (char*)"su",(char*)"Sundanese",
  (char*)"sv",(char*)"Swedish",
  (char*)"sw",(char*)"Swahili",
  (char*)"ta",(char*)"Tamil",
  (char*)"te",(char*)"Telugu",
  (char*)"tg",(char*)"Tajik",
  (char*)"th",(char*)"Thai",
  (char*)"ti",(char*)"Tigrinya",
  (char*)"tk",(char*)"Turkmen",
  (char*)"tl",(char*)"Tagalog",
  (char*)"tn",(char*)"Tswana",
  (char*)"to",(char*)"Tonga",
  (char*)"tr",(char*)"Turkish",
  (char*)"ts",(char*)"Tsonga",
  (char*)"tt",(char*)"Tatar",
  (char*)"tw",(char*)"Twi",
  (char*)"ty",(char*)"Tahitian",
  (char*)"ug",(char*)"Uighur",
  (char*)"uk",(char*)"Ukrainian",
  (char*)"ur",(char*)"Urdu",
  (char*)"uz",(char*)"Uzbek",
  (char*)"ve",(char*)"Venda",
  (char*)"vi",(char*)"Vietnamese",
  (char*)"vo",(char*)"Volapük",
  (char*)"wa",(char*)"Walloon",
  (char*)"wo",(char*)"Wolof",
  (char*)"xh",(char*)"Xhosa",
  (char*)"yi",(char*)"Yiddish",
  (char*)"yo",(char*)"Yoruba",
  (char*)"za",(char*)"Zhuang",
  (char*)"zh",(char*)"Chinese",
  (char*)"zu",(char*)"Zulu"
};
//#define LANGUAGE_CODES_RAW_SIZE ((__LINE__ - 1 - LANGUAGE_CODES_RAW_START_LINE) * 2)
#define LANGUAGE_CODES_RAW_SIZE 368

lggHunSpell_Wrapper::lggHunSpell_Wrapper()
{
	//languageCodes(begin(languageCodesraw), end(languageCodesraw));    
//	mSpellCheckHighlight = rebind_llcontrol<BOOL>("SpellDisplay", &gSavedSettings, true);
}

lggHunSpell_Wrapper::~lggHunSpell_Wrapper()
{
}

BOOL lggHunSpell_Wrapper::getSpellCheckHighlight()
{
	static const LLCachedControl<bool> mSpellCheckHighlight("SpellDisplay", false);

    return mSpellCheckHighlight;
}

std::string lggHunSpell_Wrapper::getCorrectPath(std::string file)
{
	//finds out if it is in user dir, if not, takes it from app dir
	std::string dicpath1(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "dictionaries", file).c_str());
	if (!gDirUtilp->fileExists(dicpath1))
	{
		dicpath1=gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "dictionaries", file).c_str();
	}
	return dicpath1;
}

void lggHunSpell_Wrapper::setNewDictionary(std::string newDict)
{

	llinfos << "Setting new base dictionary long name is-> " << newDict.c_str() << llendl;

	currentBaseDic = newDict;
	
	//expecting a full name comming in
	newDict = fullName2DictName(newDict);

	if (myHunspell)
	{
		delete myHunspell;
	}

	std::string dicaffpath = getCorrectPath(newDict+".aff");
	std::string dicdicpath = getCorrectPath(newDict+".dic");
	
	llinfos << "Setting new base dictionary -> " << dicaffpath.c_str() << llendl;

	myHunspell = new Hunspell(dicaffpath.c_str(), dicdicpath.c_str());
	llinfos << "Adding custom dictionary " << llendl;
	createCustomDic();
	addDictionary("custom");
	std::vector<std::string> toInstall = getInstalledDicts();
	for (int i = 0; i < (int)toInstall.size(); i++)
	{
		addDictionary(toInstall[i]);
	}
}

void lggHunSpell_Wrapper::createCustomDic()
{
	std::string filename(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "dictionaries", "custom.dic"));
	if (!gDirUtilp->fileExists(filename))
	{
		llofstream export_file;	
		export_file.open(filename);
		std::string sizePart("1\nLordGregGreg\n");
		export_file.write(sizePart.c_str(),sizePart.length());
		export_file.close();
	}
}

void lggHunSpell_Wrapper::addWordToCustomDictionary(std::string wordToAdd)
{
	if (!myHunspell)
	{
		return;
	}

	myHunspell->add(wordToAdd.c_str());
	std::string filename(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "dictionaries", "custom.dic"));
	std::vector<std::string> lines;
	if (gDirUtilp->fileExists(filename))
	{
		//get words already there..
		llifstream importer(filename);
		std::string line;
		if (getline( importer, line ))//ignored the size
		{
			while ( getline( importer, line ) ) lines.push_back(line);
		}
		importer.close();
	}

	llofstream export_file;	
	export_file.open(filename);
	std::string sizePart(llformat("%i", (int)(lines.size()+1)) + "\n");
	export_file.write(sizePart.c_str(), sizePart.length());
	for (int i = 0; i < (int)lines.size() ;i++)
	{
		export_file.write(std::string(lines[i]+"\n").c_str(),lines[i].length()+1);
	}
	//LLStringUtil::toLower(wordToAdd);
	wordToAdd = wordToAdd+std::string("\n");
	export_file.write(wordToAdd.c_str(), wordToAdd.length());
	//export_file << std::hex << 10 ;
	export_file.close();
}

BOOL lggHunSpell_Wrapper::isSpelledRight(std::string wordToCheck)
{
	if (!myHunspell || wordToCheck.length() < 3)
	{
		return TRUE;
	}
	return myHunspell->spell(wordToCheck.c_str());
}

std::vector<std::string> lggHunSpell_Wrapper::getSuggestionList(std::string badWord)
{
	std::vector<std::string> toReturn;
	if (!myHunspell)
	{
		return toReturn;
	}

	char** suggestionList;	
	int numberOfSuggestions = myHunspell->suggest(&suggestionList, badWord.c_str());	
	if (numberOfSuggestions <= 0)
	{
		return toReturn;
	}
	for (int i = 0; i < numberOfSuggestions; i++) 
	{
		std::string tempSugg(suggestionList[i]);
		toReturn.push_back(tempSugg);
	}
	myHunspell->free_list(&suggestionList,numberOfSuggestions);	
	return toReturn;
}

void lggHunSpell_Wrapper::debugTest(std::string testWord)
{
	llinfos << "Testing to see if " << testWord.c_str() << " is spelled correct" << llendl;

	if (isSpelledRight(testWord))
	{
		llinfos << testWord.c_str() << " is spelled correctly" << llendl;
	}
	else
	{
		llinfos << testWord.c_str() << " is not spelled correctly, getting suggestions" << llendl;
		std::vector<std::string> suggList;
		suggList.clear();
		suggList = getSuggestionList(testWord);
		llinfos << "Got suggestions.. " << llendl;

		for (int i = 0; i < (int)suggList.size(); i++)
		{
			llinfos << "Suggestion for " << testWord.c_str() << ":" << suggList[i].c_str() << llendl;
		}
	}
}

void lggHunSpell_Wrapper::initSettings()
{
	glggHunSpell = new lggHunSpell_Wrapper();
	glggHunSpell->processSettings();
}

void lggHunSpell_Wrapper::processSettings()
{
	//expects everything to already be in saved settings
	//this will also reload and read the installed dicts
	setNewDictionary(gSavedSettings.getString("SpellBase"));
}

void lggHunSpell_Wrapper::addDictionary(std::string additionalDictionary)
{
	if (!myHunspell || additionalDictionary.empty())
	{
		return;
	}

	//expecting a full name here
	std::string dicpath = getCorrectPath(fullName2DictName(additionalDictionary)+".dic");
	if (gDirUtilp->fileExists(dicpath))
	{
		llinfos << "Adding additional dictionary -> " << dicpath.c_str() << llendl;
		myHunspell->add_dic(dicpath.c_str());
	}
}

std::string lggHunSpell_Wrapper::dictName2FullName(std::string dictName)
{
	if (dictName.empty())
	{
		return dictName;
	}

	std::string countryCode("");
	std::string languageCode("");

	//remove extension
	dictName = dictName.substr(0,dictName.find("."));

	//break it up by - or _
	S32 breakPoint = dictName.find("-");
	if (breakPoint == std::string::npos)
	{
		breakPoint = dictName.find("_");
	}
	if (breakPoint == std::string::npos)
	{
		//no country code given
		languageCode = dictName;
	}
	else
	{
		languageCode = dictName.substr(0,breakPoint);
		countryCode = dictName.substr(breakPoint+1);
	}

	//get long language code
	for (int i = 0; i<LANGUAGE_CODES_RAW_SIZE; i++)
	{		
		if (0 == LLStringUtil::compareInsensitive(languageCode, std::string(languageCodesraw[i])))
		{
			languageCode = languageCodesraw[i+1];
			break;
		}
	}

	//get long country code
	if (!countryCode.empty())
	{
		for (int i =0; i<COUNTRY_CODES_RAW_SIZE; i++)
		{		
			//llinfos << i << llendl;
			if (0 == LLStringUtil::compareInsensitive(countryCode, std::string(countryCodesraw[i])))
			{
				countryCode = countryCodesraw[i+1];
				break;
			}
		}
		countryCode = " (" + countryCode + ")";
	}
	
	return std::string(languageCode+countryCode);
}

std::string lggHunSpell_Wrapper::fullName2DictName(std::string fullName)
{
	std::string countryCode("");
	std::string languageCode("");
	S32 breakPoint = fullName.find(" (");
	if (breakPoint == std::string::npos)
	{
		languageCode = fullName;
	}
	else
	{
		languageCode = fullName.substr(0, breakPoint);
		countryCode = fullName.substr(breakPoint+2, fullName.length()-3-breakPoint);
	}
	//get long language code
	for (int i = 1; i<LANGUAGE_CODES_RAW_SIZE; i+=2)
	{
		//llinfos << i << llendl;
		if (0 == LLStringUtil::compareInsensitive(languageCode, std::string(languageCodesraw[i])))
		{
			languageCode = std::string(languageCodesraw[i-1]);
			break;
		}
	}
	//get long country code
	std::string toReturn = languageCode;
	if (!countryCode.empty())
	{
		for (int i = 1; i<COUNTRY_CODES_RAW_SIZE; i+=2)
		{
			//llinfos << i << " comparing " <<countryCode<<" and "<<std::string(countryCodesraw[i]).c_str()<< llendl;
			if (0 == LLStringUtil::compareInsensitive(countryCode, std::string(countryCodesraw[i])))
			{
				countryCode = std::string(countryCodesraw[i-1]);
				break;
			}
		}
		toReturn += "_" + countryCode;
	}

	LLStringUtil::toLower(toReturn);
	return toReturn;
}

std::vector <std::string> lggHunSpell_Wrapper::getDicts()
{
	std::vector<std::string> names;	
	std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "dictionaries", ""));
	std::string name;
	LLDirIterator app_iter(path_name, "*.aff");
	while (app_iter.next(name))
	{
		names.push_back(dictName2FullName(name));
	}
	path_name = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "dictionaries", "");
	LLDirIterator user_iter(path_name, "*.aff");
	while (user_iter.next(name))
	{
		names.push_back(dictName2FullName(name));
	}
	return names;
}

std::vector <std::string> lggHunSpell_Wrapper::getExtraDicts()
{
	std::vector<std::string> names;	
	std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "dictionaries", ""));
	std::string name;
	LLDirIterator app_iter(path_name, "*.dic");
	while (app_iter.next(name))
	{
		names.push_back(dictName2FullName(name));
	}
	path_name = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "dictionaries", "");
	LLDirIterator user_iter(path_name, "*.dic");
	while (user_iter.next(name))
	{
		names.push_back(dictName2FullName(name));
	}
	return names;
}

std::vector<std::string> lggHunSpell_Wrapper::getInstalledDicts()
{
	std::vector<std::string> toReturn;
	//expecting short names to be stored...
	std::vector<std::string> shortNames = CSV2VEC(gSavedSettings.getString("SpellInstalled"));
	for (int i =0; i < (int)shortNames.size(); i++)
	{
		toReturn.push_back(dictName2FullName(shortNames[i]));
	}
	return toReturn;
}

std::vector<std::string> lggHunSpell_Wrapper::getAvailDicts()
{
	std::vector<std::string> toReturn;
	std::vector<std::string> dics = getExtraDicts();
	std::vector<std::string> installedDics = getInstalledDicts();
	for (int i = 0; i < (int)dics.size(); i++)
	{
		bool found = false;
		for (int j = 0; j < (int)installedDics.size(); j++)
		{
			if (0 == LLStringUtil::compareInsensitive(dics[i], installedDics[j]))
			{
				found = true;//this dic is already installed
			}
		}
		if (0 == LLStringUtil::compareInsensitive(dics[i], currentBaseDic))
		{
			found = true;
		}
		if (0 == LLStringUtil::compareInsensitive(dics[i], "custom"))
		{
			found = true;
		}
		if (!found)
		{
			toReturn.push_back(dics[i]);
		}
	}
	return toReturn;
}

std::vector<std::string> lggHunSpell_Wrapper::CSV2VEC(std::string csv)
{
	std::vector<std::string> toReturn;
	boost::regex re(",");
	boost::sregex_token_iterator i(csv.begin(), csv.end(), re, -1);
	boost::sregex_token_iterator j;
	while (i != j)
	{
		toReturn.push_back(*i++);
	}
	return toReturn;
}

std::string lggHunSpell_Wrapper::VEC2CSV(std::vector<std::string> vec)
{
	std::string toReturn("");
	if (vec.size() < 1)
	{
		return toReturn;
	}

	for (int i = 0;i < (int)vec.size() ;i++)
	{
		toReturn += vec[i] + ",";
	}
	return toReturn.erase(toReturn.length()-1);
}

void lggHunSpell_Wrapper::addButton(std::string selection)
{
	if (selection.empty())
	{
		return;
	}
	addDictionary(selection);
	std::vector<std::string> alreadyInstalled = CSV2VEC(gSavedSettings.getString("SpellInstalled"));
	alreadyInstalled.push_back(fullName2DictName(selection));	
	gSavedSettings.setString("SpellInstalled", VEC2CSV(alreadyInstalled));
}

void lggHunSpell_Wrapper::removeButton(std::string selection)
{
	if (selection.empty())
	{
		return;
	}
	std::vector<std::string> newInstalledDics;
	std::vector<std::string> currentlyInstalled = getInstalledDicts();
	for (int i = 0; i < (int)currentlyInstalled.size(); i++)
	{
		if (0 != LLStringUtil::compareInsensitive(selection, currentlyInstalled[i]))
		{
			newInstalledDics.push_back(fullName2DictName(currentlyInstalled[i]));
		}
	}
	gSavedSettings.setString("SpellInstalled", VEC2CSV(newInstalledDics));
	processSettings();
}

void lggHunSpell_Wrapper::newDictSelection(std::string selection)
{
	currentBaseDic = selection;
	gSavedSettings.setString("SpellBase", selection);
	//better way to do this would be to check and see if there is a installed conflict
	//and then only remove that one.. messy
	gSavedSettings.setString("SpellInstalled", "en_sl");
	processSettings();
}

void lggHunSpell_Wrapper::getMoreButton(void* data)
{
	std::vector<std::string> shortNames;
	std::vector<std::string> longNames;
	LLSD response = LLHTTPClient::blockingGet(gSavedSettings.getString("SpellDownloadURL")+"dic_list.xml");
	if (response.has("body"))
	{
		const LLSD &dict_list = response["body"];
		if (dict_list.has("isComplete"))
		{
			LLSD dics = dict_list["data"];
			for (int i = 0; i < dics.size(); i++)
			{
				std::string dicFullName = dictName2FullName(dics[i].asString());
				longNames.push_back(dicFullName);
				shortNames.push_back(fullName2DictName(dicFullName));
			}
			LggDicDownload::show(true,shortNames,longNames, data);	
		}
	}
}

void lggHunSpell_Wrapper::editCustomButton()
{
	std::string dicdicpath(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "dictionaries", std::string("custom.dic")).c_str());

	if (!gDirUtilp->fileExists(dicdicpath))
	{
			createCustomDic();
			//glggHunSpell->addWordToCustomDictionary("temp");
	}
	
	gViewerWindow->getWindow()->ShellEx(dicdicpath);
}

void lggHunSpell_Wrapper::setSpellCheckHighlight(BOOL highlight)
{
	gSavedSettings.setBOOL("SpellDisplay", highlight);
}
