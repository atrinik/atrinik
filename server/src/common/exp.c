/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the Free Software           *
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
*                                                                       *
* The author can be reached at admin@atrinik.org                        *
************************************************************************/

/* there are some more exp calc funtions in skill_util.c - but thats part
 * of /server/server and not of crosslib.a - so we can't move then easily
 * on this place - another reason to kill the crosslib.a asap. */

#include <stdio.h>
#include <global.h>

float lev_exp[MAXLEVEL + 1] = {
	0.0f,
	1.0f, 		1.11f, 		1.75f,		3.2f,
	5.5f, 		10.0f, 		20.0f, 		35.25f, 	66.1f,
	137.0f, 	231.58f, 	240.00f, 	247.62f, 	254.55f,
	260.87f, 	266.67f, 	272.00f, 	276.92f, 	281.48f,
	285.71f, 	289.66f, 	293.33f, 	296.77f, 	300.00f,
	303.03f, 	305.88f, 	308.57f, 	311.11f, 	313.51f,
	315.79f, 	317.95f, 	320.00f, 	321.95f, 	323.81f,
	325.58f, 	327.27f, 	328.89f, 	330.43f, 	331.91f,
	333.33f, 	334.69f, 	336.00f, 	337.25f, 	338.46f,
	339.62f, 	340.74f, 	341.82f, 	342.86f, 	343.86f,
	344.83f, 	345.76f, 	346.67f, 	347.54f, 	348.39f,
	349.21f, 	350.00f, 	350.77f, 	351.52f, 	352.24f,
	352.94f, 	353.62f, 	354.29f, 	354.93f, 	355.56f,
	356.16f, 	356.76f, 	357.33f, 	357.89f, 	358.44f,
	358.97f, 	359.49f, 	360.00f, 	360.49f, 	360.98f,
	361.45f, 	361.90f, 	362.35f, 	362.79f, 	363.22f,
	363.64f, 	364.04f, 	364.44f, 	364.84f, 	365.22f,
	365.59f, 	365.96f, 	366.32f, 	366.67f, 	367.01f,
	367.35f, 	367.68f, 	368.00f, 	368.32f, 	368.63f,
	368.93f, 	369.23f, 	369.52f, 	369.81f, 	370.09f,
	370.37f, 	370.64f, 	370.91f, 	371.17f, 	371.43f,
	371.68f, 	371.93f, 	372.17f, 	372.41f, 	372.65f,
	372.88f
};

/* around level 11 you need 38+(2*(your_level-11)) yellow
 * mobs with a base exp of 125 to level up.
 * Every level >11 needs 100.000 exp more as the one before but
 * also one mob more to kill.
 * This avoid things like: "you geht 342.731.123 exp from this mob,
 * you have now 1.345.535.545.667 exp."
 * even here we have around 500.000.000 max exp - thats a pretty big
 * number. */
uint32 new_levels[MAXLEVEL + 2] = {
	0,
	0,			1500,		4000, 		8000,
	/* 9 */
	16000,		32000,		64000,		125000,		250000,
	500000,		1100000, 	2300000, 	3600000, 	5000000,
	/* 19 */
	6500000, 	8100000, 	9800000, 	11600000, 	13500000,
	15500000, 	17600000, 	19800000, 	22100000, 	24500000,
	/* 29 */
	27000000,	29600000, 	32300000, 	35100000, 	38000000,
	41000000, 	44100000, 	47300000, 	50600000, 	54000000,
	/* 39 */
	57500000, 	61100000, 	64800000, 	68600000, 	72500000,
	76500000,	80600000, 	84800000, 	89100000, 	93500000,
	/* 49 */
	98000000,	102600000, 	107300000, 	112100000, 	117000000,
	122000000,	127100000, 	132300000, 	137600000, 	143000000,
	/* 59 */
	148500000, 	154100000, 	159800000, 	165600000, 	171500000,
	177500000, 	183600000, 	189800000, 	196100000, 	202500000,
	/* 69 */
	209000000,	215600000, 	222300000, 	229100000, 	236000000,
	243000000,	250100000, 	257300000, 	264600000, 	272000000,
	/* 79 */
	279500000, 	287100000, 	294800000, 	302600000, 	310500000,
	318500000,	326600000, 	334800000, 	343100000, 	351500000,
	/* 89 */
	360000000,	368600000, 	377300000, 	386100000, 	395000000,
	404000000, 	413100000, 	422300000, 	431600000, 	441000000,
	/* 99 */
	450500000, 	460100000, 	469800000, 	479600000, 	489500000,
	499500000,	509600000, 	519800000, 	530100000, 	540500000,
	/* 109 */
	551000000, 	561600000, 	572300000, 	583100000, 	594000000,
	/* 111 is only a dummy */
	605000000, 	700000000
};

_level_color level_color[201] = {
	/* Level 0 */
	{-2,	-1,	0,	1,	2,	3},
	/* Level 1 */
	{-1,	0,	1,	2,	3,	4},
	/* Level 2 */
	{0,	1,	2,	3,	4,	5},
	/* Level 3 */
	{1,	2,	3,	4,	5,	6},
	/* Level 4 */
	{2,	3,	4,	5,	6,	7},
	/* Level 5 */
	{3,	4,	5,	6,	7,	8},
	/* Level 6 */
	{4,	5,	6,	7,	8,	9},
	/* Level 7 */
	{5,	6,	7,	8,	9,	10},
	/* Level 8 */
	{6,	7,	8,	9,	10,	11},
	/* Level 9 */
	{7,	8,	9,	10,	11,	12},
	/* Level 10 */
	{7,	9,	10,	11,	12,	14},
	/* Level 11 */
	{8,	9,	11,	12,	13,	15},
	/* Level 12 */
	{9,	10,	12,	13,	14,	16},
	/* Level 13 */
	{9,	11,	13,	14,	15,	17},
	/* Level 14 */
	{10,	11,	14,	15,	16,	18},
	/* Level 15 */
	{11,	12,	15,	16,	17,	19},
	/* Level 16 */
	{11,	13,	16,	17,	18,	20},
	/* Level 17 */
	{12,	14,	17,	18,	19,	21},
	/* Level 18 */
	{13,	15,	18,	19,	20,	22},
	/* Level 19 */
	{14,	16,	19,	20,	21,	23},
	/* Level 20 */
	{14,	17,	20,	21,	22,	24},
	/* Level 21 */
	{15,	17,	21,	22,	24,	26},
	/* Level 22 */
	{16,	18,	22,	23,	25,	27},
	/* Level 23 */
	{16,	19,	23,	24,	26,	28},
	/* Level 24 */
	{17,	19,	24,	25,	27,	30},
	/* Level 25 */
	{18,	20,	25,	26,	28,	31},
	/* Level 26 */
	{19,	21,	26,	27,	29,	32},
	/* Level 27 */
	{19,	22,	27,	28,	30,	33},
	/* Level 28 */
	{20,	23,	28,	29,	31,	35},
	/* Level 29 */
	{21,	24,	29,	30,	32,	36},
	/* Level 30 */
	{22,	25,	30,	31,	33,	37},
	/* Level 31 */
	{22,	25,	31,	32,	34,	38},
	/* Level 32 */
	{23,	26,	32,	33,	35,	39},
	/* Level 33 */
	{24,	27,	32,	35,	37,	41},
	/* Level 34 */
	{25,	28,	33,	36,	38,	42},
	/* Level 35 */
	{25,	28,	34,	37,	39,	43},
	/* Level 36 */
	{26,	29,	35,	38,	40,	44},
	/* Level 37 */
	{27,	30,	36,	39,	41,	45},
	/* Level 38 */
	{28,	31,	37,	40,	42,	46},
	/* Level 39 */
	{28,	32,	38,	41,	44,	48},
	/* Level 40 */
	{29,	33,	39,	42,	45,	49},
	/* Level 41 */
	{30,	34,	40,	43,	46,	50},
	/* Level 42 */
	{30,	34,	41,	44,	47,	52},
	/* Level 43 */
	{31,	35,	42,	45,	48,	53},
	/* Level 44 */
	{32,	36,	43,	46,	49,	54},
	/* Level 45 */
	{33,	37,	44,	47,	50,	55},
	/* Level 46 */
	{33,	37,	45,	48,	51,	57},
	/* Level 47 */
	{34,	38,	46,	49,	52,	58},
	/* Level 48 */
	{35,	39,	47,	50,	53,	59},
	/* Level 49 */
	{36,	40,	48,	51,	54,	60},
	/* Level 50 */
	{36,	41,	49,	52,	55,	61},
	/* Level 51 */
	{37,	42,	50,	53,	56,	62},
	/* Level 52 */
	{38,	43,	51,	54,	57,	63},
	/* Level 53 */
	{38,	43,	52,	55,	58,	65},
	/* Level 54 */
	{39,	44,	53,	56,	59,	66},
	/* Level 55 */
	{40,	45,	54,	57,	60,	67},
	/* Level 56 */
	{41,	46,	55,	58,	61,	68},
	/* Level 57 */
	{41,	47,	56,	59,	63,	70},
	/* Level 58 */
	{42,	48,	57,	60,	64,	71},
	/* Level 59 */
	{43,	49,	58,	61,	65,	72},
	/* Level 60 */
	{44,	50,	59,	62,	66,	73},
	/* Level 61 */
	{44,	50,	60,	63,	67,	75},
	/* Level 62 */
	{45,	51,	61,	64,	68,	76},
	/* Level 63 */
	{46,	52,	62,	65,	69,	77},
	/* Level 64 */
	{47,	53,	63,	66,	70,	78},
	/* Level 65 */
	{47,	53,	64,	67,	71,	79},
	/* Level 66 */
	{48,	54,	64,	69,	73,	81},
	/* Level 67 */
	{49,	55,	65,	70,	74,	82},
	/* Level 68 */
	{50,	56,	66,	71,	75,	83},
	/* Level 69 */
	{50,	56,	67,	72,	76,	84},
	/* Level 70 */
	{51,	57,	68,	73,	77,	85},
	/* Level 71 */
	{52,	58,	69,	74,	78,	86},
	/* Level 72 */
	{53,	59,	70,	75,	79,	87},
	/* Level 73 */
	{53,	60,	71,	76,	80,	89},
	/* Level 74 */
	{54,	61,	72,	77,	81,	90},
	/* Level 75 */
	{55,	62,	73,	78,	82,	91},
	/* Level 76 */
	{56,	63,	74,	79,	83,	92},
	/* Level 77 */
	{56,	63,	75,	80,	85,	94},
	/* Level 78 */
	{57,	64,	76,	81,	86,	95},
	/* Level 79 */
	{58,	65,	77,	82,	87,	96},
	/* Level 80 */
	{59,	66,	78,	83,	88,	97},
	/* Level 81 */
	{59,	67,	79,	84,	89,	99},
	/* Level 82 */
	{60,	68,	80,	85,	90,	100},
	/* Level 83 */
	{61,	69,	81,	86,	91,	101},
	/* Level 84 */
	{62,	70,	82,	87,	92,	102},
	/* Level 85 */
	{62,	70,	83,	88,	93,	103},
	/* Level 86 */
	{63,	71,	84,	89,	94,	104},
	/* Level 87 */
	{64,	72,	85,	90,	95,	105},
	/* Level 88 */
	{65,	73,	86,	91,	96,	106},
	/* Level 89 */
	{65,	73,	87,	92,	97,	108},
	/* Level 90 */
	{66,	74,	88,	93,	98,	109},
	/* Level 91 */
	{67,	75,	89,	94,	99,	110},
	/* Level 92 */
	{68,	76,	90,	95,	100,	111},
	/* Level 93 */
	{69,	77,	91,	96,	101,	112},
	/* Level 94 */
	{69,	78,	92,	97,	103,	114},
	/* Level 95 */
	{70,	79,	93,	98,	104,	115},
	/* Level 96 */
	{71,	80,	94,	99,	105,	116},
	/* Level 97 */
	{72,	81,	95,	100,	106,	117},
	/* Level 98 */
	{72,	81,	96,	101,	107,	119},
	/* Level 99 */
	{73,	82,	96,	103,	109,	120},
	/* Level 100 */
	{74,	83,	97,	104,	110,	121},
	/* Level 101 */
	{75,	84,	98,	105,	111,	122},
	/* Level 102 */
	{75,	84,	99,	106,	112,	124},
	/* Level 103 */
	{76,	85,	100,	107,	113,	125},
	/* Level 104 */
	{77,	86,	101,	108,	114,	126},
	/* Level 105 */
	{78,	87,	102,	109,	115,	127},
	/* Level 106 */
	{79,	88,	103,	110,	116,	128},
	/* Level 107 */
	{79,	89,	104,	111,	117,	129},
	/* Level 108 */
	{80,	90,	105,	112,	118,	130},
	/* Level 109 */
	{81,	91,	106,	113,	119,	131},
	/* Level 110 */
	{82,	92,	107,	114,	120,	132},
	/* Level 111 */
	{82,	92,	108,	115,	121,	134},
	/* Level 112 */
	{83,	93,	109,	116,	122,	135},
	/* Level 113 */
	{84,	94,	110,	117,	123,	136},
	/* Level 114 */
	{85,	95,	111,	118,	124,	137},
	/* Level 115 */
	{86,	96,	112,	119,	125,	138},
	/* Level 116 */
	{86,	96,	113,	120,	126,	140},
	/* Level 117 */
	{87,	97,	114,	121,	127,	141},
	/* Level 118 */
	{88,	98,	115,	122,	128,	142},
	/* Level 119 */
	{89,	99,	116,	123,	129,	143},
	/* Level 120 */
	{90,	100,	117,	124,	130,	144},
	/* Level 121 */
	{90,	101,	118,	125,	132,	146},
	/* Level 122 */
	{91,	102,	119,	126,	133,	147},
	/* Level 123 */
	{92,	103,	120,	127,	134,	148},
	/* Level 124 */
	{93,	104,	121,	128,	135,	149},
	/* Level 125 */
	{94,	105,	122,	129,	136,	150},
	/* Level 126 */
	{94,	105,	123,	130,	137,	151},
	/* Level 127 */
	{95,	106,	124,	131,	138,	152},
	/* Level 128 */
	{96,	107,	125,	132,	139,	153},
	/* Level 129 */
	{97,	108,	126,	133,	140,	154},
	/* Level 130 */
	{97,	109,	127,	134,	141,	156},
	/* Level 131 */
	{98,	110,	128,	135,	142,	157},
	/* Level 132 */
	{99,	110,	128,	137,	144,	158},
	/* Level 133 */
	{100,	111,	129,	138,	145,	159},
	/* Level 134 */
	{101,	112,	130,	139,	146,	160},
	/* Level 135 */
	{101,	113,	131,	140,	147,	162},
	/* Level 136 */
	{102,	114,	132,	141,	148,	163},
	/* Level 137 */
	{103,	115,	133,	142,	149,	164},
	/* Level 138 */
	{104,	116,	134,	143,	150,	165},
	/* Level 139 */
	{105,	117,	135,	144,	151,	166},
	/* Level 140 */
	{106,	118,	136,	145,	152,	167},
	/* Level 141 */
	{106,	118,	137,	146,	153,	169},
	/* Level 142 */
	{107,	119,	138,	147,	154,	170},
	/* Level 143 */
	{108,	120,	139,	148,	155,	171},
	/* Level 144 */
	{109,	121,	140,	149,	156,	172},
	/* Level 145 */
	{110,	122,	141,	150,	157,	173},
	/* Level 146 */
	{110,	122,	142,	151,	159,	175},
	/* Level 147 */
	{111,	123,	143,	152,	160,	176},
	/* Level 148 */
	{112,	124,	144,	153,	161,	177},
	/* Level 149 */
	{113,	125,	145,	154,	162,	178},
	/* Level 150 */
	{114,	126,	146,	155,	163,	179},
	/* Level 151 */
	{114,	127,	147,	156,	164,	180},
	/* Level 152 */
	{115,	128,	148,	157,	165,	181},
	/* Level 153 */
	{116,	129,	149,	158,	166,	182},
	/* Level 154 */
	{117,	130,	150,	159,	167,	183},
	/* Level 155 */
	{118,	131,	151,	160,	168,	184},
	/* Level 156 */
	{119,	132,	152,	161,	169,	185},
	/* Level 157 */
	{119,	132,	153,	162,	170,	187},
	/* Level 158 */
	{120,	133,	154,	163,	171,	188},
	/* Level 159 */
	{121,	134,	155,	164,	172,	189},
	/* Level 160 */
	{122,	135,	156,	165,	173,	190},
	/* Level 161 */
	{123,	136,	157,	166,	174,	191},
	/* Level 162 */
	{123,	137,	158,	167,	175,	193},
	/* Level 163 */
	{124,	138,	159,	168,	176,	194},
	/* Level 164 */
	{125,	139,	160,	169,	177,	195},
	/* Level 165 */
	{126,	139,	160,	171,	179,	196},
	/* Level 166 */
	{127,	140,	161,	172,	180,	197},
	/* Level 167 */
	{128,	141,	162,	173,	181,	198},
	/* Level 168 */
	{128,	142,	163,	174,	182,	200},
	/* Level 169 */
	{129,	143,	164,	175,	183,	201},
	/* Level 170 */
	{130,	144,	165,	176,	184,	202},
	/* Level 171 */
	{131,	145,	166,	177,	185,	203},
	/* Level 172 */
	{132,	146,	167,	178,	186,	204},
	/* Level 173 */
	{133,	147,	168,	179,	187,	205},
	/* Level 174 */
	{133,	147,	169,	180,	189,	207},
	/* Level 175 */
	{134,	148,	170,	181,	190,	208},
	/* Level 176 */
	{135,	149,	171,	182,	191,	209},
	/* Level 177 */
	{136,	150,	172,	183,	192,	210},
	/* Level 178 */
	{137,	151,	173,	184,	193,	211},
	/* Level 179 */
	{138,	152,	174,	185,	194,	212},
	/* Level 180 */
	{139,	153,	175,	186,	195,	213},
	/* Level 181 */
	{139,	153,	176,	187,	196,	214},
	/* Level 182 */
	{140,	154,	177,	188,	197,	215},
	/* Level 183 */
	{141,	155,	178,	189,	198,	216},
	/* Level 184 */
	{142,	156,	179,	190,	199,	217},
	/* Level 185 */
	{143,	157,	180,	191,	200,	218},
	/* Level 186 */
	{144,	158,	181,	192,	201,	219},
	/* Level 187 */
	{144,	159,	182,	193,	202,	221},
	/* Level 188 */
	{145,	160,	183,	194,	203,	222},
	/* Level 189 */
	{146,	161,	184,	195,	204,	223},
	/* Level 190 */
	{147,	162,	185,	196,	205,	224},
	/* Level 191 */
	{148,	163,	186,	197,	206,	225},
	/* Level 192 */
	{149,	164,	187,	198,	207,	226},
	/* Level 193 */
	{150,	165,	188,	199,	208,	227},
	/* Level 194 */
	{150,	165,	189,	200,	209,	229},
	/* Level 195 */
	{151,	166,	190,	201,	210,	230},
	/* Level 196 */
	{152,	167,	191,	202,	211,	231},
	/* Level 197 */
	{153,	168,	192,	203,	212,	232},
	/* Level 198 */
	{154,	169,	192,	205,	214,	233},
	/* Level 199 */
	{155,	170,	193,	206,	215,	234},
	/* Level 200 */
	{156,	171,	194,	207,	216,	235}
};

/* Since this is nowhere defined ...
 * Both come in handy at least in function add_exp() */
#define MAX_EXPERIENCE  new_levels[MAXLEVEL]

#define MAX_EXP_IN_OBJ new_levels[MAXLEVEL] / (MAX_EXP_CAT - 1)

/* Returns how much experience is needed for a player to become
 * the given level. */
uint32 level_exp(int level, double expmul)
{
	return (uint32)(expmul * (double)new_levels[level]);
}

/* add_exp() new algorithm. Revamped experience gain/loss routine.
 * Based on the old add_exp() function - but tailored to add experience
 * to experience objects. The way this works -- the code checks the
 * current skill readied by the player (chosen_skill) and uses that to
 * identify the appropriate experience object. Then the experience in
 * the object, and the player's overall score are updated. In the case
 * of exp loss, all exp categories which have experience are equally
 * reduced. The total experience score of the player == sum of all
 * exp object experience.  - b.t. thomas@astro.psu.edu  */

/* The old way to determinate the right skill which is used for exp gain
 * was broken. Best way to show this is, to cast some fire balls in a mob
 * and then changing the hand weapon some times. You will get some "no
 * ready skill warnings".
 * I reworked the whole system and the both main exp gain and add functions
 * add_exp() and adjust_exp(). Its now much faster, easier and more accurate. MT
 * exp lose by dead is handled from apply_death_exp_penalty(). */
sint32 add_exp(object *op, int exp, int skill_nr)
{
	/* the exp. object into which experience will go */
    object *exp_ob = NULL;
	/* the real skill object */
    object *exp_skill = NULL;
	/*int del_exp = 0;*/
    int limit = 0;

    /*LOG(llevBug, "ADD: add_exp() called for $d!\n", exp); */
    /* safety */
    if (!op)
    {
    	LOG(llevBug, "BUG: add_exp() called for null object!\n");
	    return 0;
    }

	/* no exp gain for mobs */
    if (op->type != PLAYER)
        return 0;

    /* ok, we have global exp gain or drain - we must grab a skill for it! */
    if (skill_nr == CHOSEN_SKILL_NO)
    {
        /* TODO: select skill */
        LOG(llevDebug, "TODO: add_exp(): called for %s with exp %d. CHOSEN_SKILL_NO set. TODO: select skill.\n", query_name(op, NULL), exp);
        return 0;
    }

    /* now we grab the skill exp. object from the player shortcut ptr array */
    exp_skill = CONTR(op)->skill_ptr[skill_nr];

	/* safety check */
    if (!exp_skill)
    {
        /* our player doesn't have this skill?
         * This can happen when group exp is devided.
         * We must get a useful sub or we skip the exp. */
        LOG(llevDebug, "TODO: add_exp(): called for %s with skill nr %d / %d exp - object has not this skill.\n", query_name(op, NULL), skill_nr, exp);
		/* TODO: groups comes later  - now we skip all times */
        return 0;
    }

    /* if we are full in this skill, then nothing is to do */
    if (exp_skill->level >= MAXLEVEL)
        return 0;

	/* we will sure change skill exp, mark for update */
    CONTR(op)->update_skills = 1;
    exp_ob = exp_skill->exp_obj;

    if (!exp_ob)
    {
		LOG(llevBug, "BUG: add_exp() skill:%s - no exp_op found!!\n", query_name(exp_skill, NULL));
		return 0;
    }

    /* General adjustments for playbalance */
    /* I set limit to 1/4 of a level - thats enormous much */
    limit = (new_levels[exp_skill->level + 1] - new_levels[exp_skill->level]) / 4;

    if (exp > limit)
        exp = limit;

	/* first we see what we can add to our skill */
    exp = adjust_exp(op, exp_skill, exp);

    /* adjust_exp has adjust the skill and all exp_obj and player exp */
    /* now lets check for level up in all categories */
    player_lvl_adj(op, exp_skill);
    player_lvl_adj(op, exp_ob);
    player_lvl_adj(op, NULL);

	/* reset the player exp_obj to NULL */
    /* I let this in but because we use single skill exp and skill nr now,
     * this broken exp_obj concept can be removed */
	if (op->exp_obj)
        op->exp_obj = NULL;

	/* thats the real exp we have added to our skill */
    return (sint32) exp;
}


/* player_lvl_adj() - for the new exp system. we are concerned with
 * whether the player gets more hp, sp and new levels.
 * -b.t. */
void player_lvl_adj(object *who, object *op)
{
	char buf[MAX_BUF];

	/* when rolling stats */
    if (!op)
		op = who;

	/* no exp gain for indirect skills */
	if (op->type == SKILL && !op->last_eat)
	{
		LOG(llevBug,"BUG: player_lvl_adj() called for indirect skill %s (who: %s)\n", query_name(op, NULL), who == NULL ? "<null>" : query_name(who, NULL));
		return;
	}

    /*LOG(llevDebug, "LEVEL: %s ob:%s l: %d e: %d\n", who == NULL ? "<null>" : who->name, op->name, op->level, op->stats.exp);*/
    if (op->level < MAXLEVEL && op->stats.exp >= (sint32)level_exp(op->level + 1, 1.0))
	{
		op->level++;

		/* show the player some effects... */
		if (op->type == SKILL && who && who->type == PLAYER && who->map)
		{
			object *effect_ob;

			play_sound_player_only(CONTR(who), SOUND_LEVEL_UP, SOUND_NORMAL, 0, 0);

			if (level_up_arch)
			{
				/* prepare effect */
				effect_ob = arch_to_object(level_up_arch);
				effect_ob->map = who->map;
				effect_ob->x = who->x;
				effect_ob->y = who->y;

				if (!insert_ob_in_map(effect_ob, effect_ob->map, NULL, INS_NO_MERGE | INS_NO_WALK_ON))
				{
					/* something is wrong - kill object */
					if (!QUERY_FLAG(effect_ob, FLAG_REMOVED))
						remove_ob(effect_ob);
				}
			}
		}

		if (op == who && op->stats.exp > 1 && is_dragon_pl(who))
			dragon_level_gain(who);

		if (who && who->type == PLAYER && op->type != EXPERIENCE && op->type != SKILL && who->level > 1)
		{
			if (who->level > 4)
				CONTR(who)->levhp[who->level] = (char)((RANDOM() % who->arch->clone.stats.maxhp) + 1);
			else if (who->level > 2)
				CONTR(who)->levhp[who->level] = (char)((RANDOM() % (who->arch->clone.stats.maxhp / 2)) + 1) + (who->arch->clone.stats.maxhp / 2);
			else
				CONTR(who)->levhp[who->level] = (char)who->arch->clone.stats.maxhp;
		}

		if (op->level>1 && op->type == EXPERIENCE)
		{
			if (who && who->type == PLAYER)
			{
				/* mana */
				if (op->stats.Pow)
				{
					if (op->level > 4)
						CONTR(who)->levsp[op->level] = (char)((RANDOM() % who->arch->clone.stats.maxsp) + 1);
					else
						CONTR(who)->levsp[op->level] = (char)who->arch->clone.stats.maxsp;
				}
				/* grace */
				else if (op->stats.Wis)
				{
					if (op->level > 4)
						CONTR(who)->levgrace[op->level] = (char)((RANDOM() % who->arch->clone.stats.maxgrace) + 1);
					else
						CONTR(who)->levgrace[op->level] = (char)who->arch->clone.stats.maxgrace;
				}
			}

			sprintf(buf, "You are now level %d in %s based skills.", op->level, op->name);
			if (who)
				(*draw_info_func)(NDI_UNIQUE | NDI_RED, 0, who, buf);
		}
		else if (op->level > 1 && op->type == SKILL)
		{
			sprintf(buf, "You are now level %d in the skill %s.", op->level, op->name);
			if (who)
				(*draw_info_func)(NDI_UNIQUE | NDI_RED, 0, who, buf);
		}
		else
		{
			sprintf(buf, "You are now level %d.", op->level);
			if (who)
				(*draw_info_func)(NDI_UNIQUE | NDI_RED, 0, who, buf);
		}

		if (who)
			fix_player(who);

		/* To increase more levels */
		player_lvl_adj(who, op);
    }
	else if (op->level > 1 && op->stats.exp < (sint32)level_exp(op->level, 1.0))
	{
		op->level--;

		if (who)
			fix_player(who);

		if (op->type == EXPERIENCE)
		{
			sprintf(buf, "-You are now level %d in %s based skills.", op->level, op->name);
			if (who)
				(*draw_info_func)(NDI_UNIQUE | NDI_RED, 0, who, buf);
		}
		else if (op->type == SKILL)
		{
			sprintf(buf, "-You are now level %d in the skill %s.", op->level, op->name);
			if (who)
				(*draw_info_func)(NDI_UNIQUE | NDI_RED, 0, who, buf);
		}
		else
		{
			sprintf(buf, "-You are now level %d.", op->level);
			if (who)
				(*draw_info_func)(NDI_UNIQUE | NDI_RED, 0, who, buf);
		}

		/* To decrease more levels */
		player_lvl_adj(who, op);
    }
}

/* Ensure that the permanent experience requirements in an exp object are met. */
/* GD */
void calc_perm_exp(object *op)
{
    int p_exp_min;

    /* Sanity checks. */
    if (op->type != EXPERIENCE)
	{
        LOG(llevBug, "BUG: calc_minimum_perm_exp called on a non-experience object!");
        return;
    }

    if (!(settings.use_permanent_experience))
	{
        LOG(llevBug, "BUG: calc_minimum_perm_exp called whilst permanent experience disabled!");
        return;
    }

    /* The following fields are used:
     *   stats.exp: Current exp in experience object.
     *   last_heal: Permanent experience earnt. */

    /* Ensure that our permanent experience minimum is met. */
    p_exp_min = (int)(PERM_EXP_MINIMUM_RATIO * (float)(op->stats.exp));
    /*LOG(llevBug, "BUG: Experience minimum check: %d p-min %d p-curr %d curr.\n", p_exp_min, op->last_heal, op->stats.exp);*/

    if (op->last_heal < p_exp_min)
        op->last_heal = p_exp_min;

    /* Cap permanent experience. */
    if (op->last_heal < 0)
        op->last_heal = 0;
    else if (op->last_heal > (sint32)MAX_EXP_IN_OBJ)
        op->last_heal = MAX_EXP_IN_OBJ;
}

/* adjust_exp() - make sure that we don't exceed max or min set on
 * experience
 * I use this function now as global experience add and sub routine.
 * it should only be called for the skills object from players.
 * This function adds or subs the exp and updates all skill objects and
 * the player global exp.
 * You need to call player_lvl_adj() after it.
 * This routine uses brute force and goes through the whole inventory. We should
 * use a kind of skill container to speed this up. MT  */
int adjust_exp(object *pl, object *op, int exp)
{
    object *tmp;
    int i, sk_nr;
    sint32 sk_exp, pl_exp;

    /* be sure this is a skill object from a active player */
    if (op->type != SKILL || !pl || pl->type != PLAYER)
    {
        LOG(llevBug, "BUG: adjust_exp() - called for non player or non skill: skill: %s -> player: %s\n", query_name(op, NULL), query_name(pl, NULL));
        return 0;
    }

    /* add or sub the exp and cap it. it must be >=0 and <= MAX_EXPERIENCE */
    op->stats.exp += exp;
    if (op->stats.exp < 0)
    {
        exp -= op->stats.exp;
        op->stats.exp = 0;
    }

    if (op->stats.exp>(sint32)MAX_EXPERIENCE)
    {
        exp = exp - (op->stats.exp - MAX_EXPERIENCE);
        op->stats.exp = MAX_EXPERIENCE;
    }

    /* now we collect the exp of all skills which are in the same exp. object category */
    sk_nr = skills[op->stats.sp].category;
    sk_exp = 0;

#if 0
	/* this is the old collection system - all skills of a exp group add
	 * we changed that to "best skill count"*/
    for (tmp = pl->inv; tmp; tmp = tmp->below)
    {
        if (tmp->type == SKILL && skills[tmp->stats.sp].category == sk_nr)
        {
            if ((sk_exp += tmp->stats.exp) > (sint32)MAX_EXPERIENCE)
                sk_exp = MAX_EXPERIENCE;
        }
    }
#endif

    for (tmp = pl->inv; tmp; tmp = tmp->below)
    {
        if (tmp->type == SKILL && skills[tmp->stats.sp].category == sk_nr)
        {
			if (tmp->stats.exp > sk_exp)
				sk_exp = tmp->stats.exp;
        }
    }

    /* set the exp of the exp. object to our best skill of this group */
    op->exp_obj->stats.exp = sk_exp;

    /* now we collect all exp. objects exp */
    pl_exp = 0;

#if 0
	/* this is old adding function - we use now the best group */
    for (i = 0; i < MAX_EXP_CAT - 1; i++)
    {
        if ((pl_exp += CONTR(pl)->last_skill_ob[i]->stats.exp) > (sint32)MAX_EXPERIENCE)
            pl_exp = MAX_EXPERIENCE;
    }
#endif

    for (i = 0; i < MAX_EXP_CAT - 1; i++)
    {
		if (CONTR(pl)->last_skill_ob[i]->stats.exp > pl_exp)
			pl_exp = CONTR(pl)->last_skill_ob[i]->stats.exp;
    }

    /* last action: set our player exp to highest group */
    pl->stats.exp = pl_exp;

	/* return the actual amount changed stats.exp we REALLY have added to our skill */
    return exp;
}

/* we are now VERY friendly - but not because we want. With the
 * new sytem, we never lose level, just % of the exp we gained for
 * the next level. Why? Because dropping the level on purpose by
 * dying again & again will allow under some special circumstances
 * rich players to use exploits.
 * This here is newbie friendly and it allows to make the higher
 * level simply harder. By losing increased levels at high levels
 * you need at last to make recover easy. Now you will not lose much
 * but it will be hard in any case to get exp in high levels.
 * This is a just a design adjustment. */
void apply_death_exp_penalty(object *op)
{
    object *tmp;
	float loss_p;
	long level_exp, loss_exp;

	/* we will sure change skill exp, mark for update */
    CONTR(op)->update_skills = 1;

    for (tmp = op->inv; tmp; tmp = tmp->below)
    {
		/* only adjust skills with level and a positive exp value - negative exp has special meaning */
        if (tmp->type == SKILL && tmp->level && tmp->last_eat == 1)
        {
			/* first, lets check there are exp we can drain. */
			level_exp = tmp->stats.exp - new_levels[tmp->level];

			/* just a sanity check */
			if (level_exp < 0)
				LOG(llevBug, "DEATH_EXP: Skill %s (%d %d) for player %s -> less exp as level need!\n", query_name(tmp, NULL), tmp->level, tmp->stats.exp, query_name(op, NULL));

			if (!level_exp)
				continue;

			if (tmp->level < 2)
			{
				loss_exp = level_exp - (int) ((float)level_exp * 0.9);
			}
			else if (tmp->level < 3)
			{
				loss_exp = level_exp - (int) ((float)level_exp * 0.85);
			}
			else
			{
				loss_p = 0.927f - (((float)tmp->level / 5.0f) * 0.00337f);
				loss_exp = (new_levels[tmp->level + 1] - new_levels[tmp->level]) - (int)((float)(new_levels[tmp->level + 1] - new_levels[tmp->level]) * loss_p);
			}

			if (loss_exp < 0)
				loss_exp = 0;

			if (loss_exp > level_exp)
				loss_exp = level_exp;

			/* again some sanity checks */
			if (loss_exp > 0)
			{
				adjust_exp(op, tmp, -loss_exp);
				player_lvl_adj(op, tmp);
			}
        }
    }

    for (tmp = op->inv; tmp; tmp = tmp->below)
    {
		/* adjust exp objects levels */
        if (tmp->type == EXPERIENCE && tmp->stats.exp)
            player_lvl_adj(op, tmp);
    }

	/* and at last adjust the player level */
    player_lvl_adj(op, NULL);
}

/* i reworked this...
 * We will get a mali or boni value here except the level match exactly.
 * Yellow means not always exactly same level but "in equal range".
 * If the target is in yellow range, the exp mul is between 0.8 and 1.1 (80%-110%)
 * If the target is in blue the exp is 40-60%. (0.4-0.6)
 * if the mob is green the range is between 25%-30% (0.25 to 0.3)
 * For orange mobs, the exp is between 1.2 and 1.4 .
 * For all over orange its 1.4 + 0.1% per level. */
float calc_level_difference(int who_lvl, int op_lvl)
{
	int r;
	float v, tmp = 1.0f;

	/* some sanity checks */
	if (who_lvl < 0 || who_lvl > 200 || op_lvl < 0 || op_lvl > 200)
	{
		LOG(llevBug, "BUG: calc_level_difference(): Level out of range! (%d - %d)\n", who_lvl, op_lvl);
		return 0.0f;
	}

	/* grey */
	if (op_lvl < level_color[who_lvl].green)
		return 0.0f;

	/* op is perhaps yellow, blue or green */
	if (who_lvl > op_lvl)
	{
		if (op_lvl >= level_color[who_lvl].yellow)
		{
			r = who_lvl-level_color[who_lvl].yellow;

			if (r < 1)
				r = 1;

			v = 0.2f / (float)r;
			tmp = 1.0f - (v *(float)(who_lvl - op_lvl));
		}
		else if (op_lvl >= level_color[who_lvl].blue)
		{
			r = level_color[who_lvl].yellow - level_color[who_lvl].blue;

			if (r < 1)
				r = 1;

			v = 0.3f / (float)r;
			tmp = 0.4f + (v *(float)(op_lvl - level_color[who_lvl].blue + 1));
		}
		/* green */
		else
		{
			r = level_color[who_lvl].blue - level_color[who_lvl].green;

			if (r < 1)
				r = 1;

			v = 0.05f / (float)r;
			tmp = 0.25f + (v *(float)(op_lvl - level_color[who_lvl].green + 1));
		}
	}
	/* check for orange - if red/purple use 1,6 + 0.1% per level */
	else if (who_lvl < op_lvl)
	{
		/* still yellow */
		if (op_lvl < level_color[who_lvl].orange)
		{
			r = level_color[who_lvl].orange-who_lvl-1;

			if (r < 1)
				r = 1;

			v = 0.1f / (float)r;
			tmp = 1.0f + (v *(float)(op_lvl - who_lvl));

		}
		/* op is orange */
		else if (op_lvl < level_color[who_lvl].red)
		{
			r = level_color[who_lvl].red-who_lvl - 1;

			if (r < 1)
				r = 1;

			v = 0.2f / (float)r;
			tmp = 1.2f + (v *(float)(op_lvl - who_lvl));
		}
		/* red or purple! */
		else
		{
			r = (op_lvl + 1) - level_color[who_lvl].red;
			v = 0.1f * (float)r;
			tmp = 1.4f + v;
		}
	}

	return tmp;
}
