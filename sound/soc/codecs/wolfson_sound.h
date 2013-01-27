/*
 * Author: andip71, 27.01.2013
 *
 * Version 1.4.8
 *
 * credits: Supercurio for ideas and partially code from his Voodoo
 * 	    sound implementation,
 *          Gokhanmoral for further modifications to the original code
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


/*****************************************/
// External function declarations
/*****************************************/

void Wolfson_sound_hook_wm8994_pcm_probe(struct snd_soc_codec *codec_pointer);
unsigned int Wolfson_sound_hook_wm8994_write(unsigned int reg, unsigned int value);


/*****************************************/
// Definitions
/*****************************************/

// Wolfson sound general
#define WOLFSON_SOUND_DEFAULT 	0
#define WOLFSON_SOUND_VERSION 	"1.4.8"

// Debug mode
#define DEBUG_DEFAULT 		1
#define DEBUG_OFF 		0
#define DEBUG_NORMAL 		1
#define DEBUG_VERBOSE 		2

// Debug register
#define DEBUG_REGISTER_KEY 	66

// EQ mode
#define EQ_DEFAULT 		0

#define EQ_OFF			0
#define EQ_NORMAL		1
#define EQ_NOSATPREVENT		2

// EQ gain
#define EQ_GAIN_DEFAULT 	0

#define EQ_GAIN_OFFSET 		12
#define EQ_GAIN_MIN 		-12
#define EQ_GAIN_MAX  		12

// EQ bands
#define BANDS_ALL		6

#define EQ_BAND_1_A_DEFAULT	0x0FCA
#define EQ_BAND_1_B_DEFAULT	0x0400
#define EQ_BAND_1_PG_DEFAULT	0x00D8
#define EQ_BAND_2_A_DEFAULT	0x1EB5
#define EQ_BAND_2_B_DEFAULT	0xF145
#define EQ_BAND_2_C_DEFAULT	0x0B75
#define EQ_BAND_2_PG_DEFAULT	0x01C5
#define EQ_BAND_3_A_DEFAULT	0x1C58
#define EQ_BAND_3_B_DEFAULT	0xF373
#define EQ_BAND_3_C_DEFAULT	0x0A54
#define EQ_BAND_3_PG_DEFAULT	0x0558
#define EQ_BAND_4_A_DEFAULT	0x168E
#define EQ_BAND_4_B_DEFAULT	0xF829
#define EQ_BAND_4_C_DEFAULT	0x07AD
#define EQ_BAND_4_PG_DEFAULT	0x1103
#define EQ_BAND_5_A_DEFAULT	0x0564
#define EQ_BAND_5_B_DEFAULT	0x0559
#define EQ_BAND_5_PG_DEFAULT	0x4000

// EQ saturation prevention
#define EQ_DRC_MAXGAIN_DEFAULT	0
#define EQ_DRC_DCY_DEFAULT	2
#define EQ_DRC_ATK_DEFAULT	4
#define EQ_DRC_HI_COMP_DEFAULT	5

#define EQ_DRC_MAXGAIN_PREVENT	3
#define EQ_DRC_DCY_PREVENT	4
#define EQ_DRC_ATK_PREVENT	1
#define EQ_DRC_HI_COMP_PREVENT	5

// FLL tuning loop gains
#define FLL_LOOP_GAIN_DEFAULT	0
#define FLL_LOOP_GAIN_TUNED	5

// headphone levels
#define HEADPHONE_DEFAULT 	50

#define HEADPHONE_MAX 		63
#define HEADPHONE_MIN 		20

// speaker levels
#define SPEAKER_DEFAULT 	57

#define SPEAKER_MAX 		63
#define SPEAKER_MIN 		57

// Microphone control
#define MIC_MODE_DEFAULT 	0
#define MIC_MODE_CONCERT 	1
#define MIC_MODE_NOISY 		2
#define MIC_MODE_LIGHT 		3

// Microphone control
#define MIC_CONCERT_LEFT_VALUE		271
#define MIC_CONCERT_RIGHT_VALUE		271
#define MIC_CONCERT_INPUT_MIXER_3	32
#define MIC_CONCERT_INPUT_MIXER_4	32
#define MIC_CONCERT_DRC1_1		156
#define MIC_CONCERT_DRC1_2		2118
#define MIC_CONCERT_DRC1_3		17
#define MIC_CONCERT_DRC1_4		201

#define MIC_NOISY_LEFT_VALUE		269
#define MIC_NOISY_RIGHT_VALUE		269
#define MIC_NOISY_INPUT_MIXER_3		32
#define MIC_NOISY_INPUT_MIXER_4		32
#define MIC_NOISY_DRC1_1		156
#define MIC_NOISY_DRC1_2		2117
#define MIC_NOISY_DRC1_3		153
#define MIC_NOISY_DRC1_4		364

#define MIC_LIGHT_LEFT_VALUE		268
#define MIC_LIGHT_RIGHT_VALUE		268
#define MIC_LIGHT_INPUT_MIXER_3		32
#define MIC_LIGHT_INPUT_MIXER_4		32
#define MIC_LIGHT_DRC1_1		156
#define MIC_LIGHT_DRC1_2		2116
#define MIC_LIGHT_DRC1_3		161
#define MIC_LIGHT_DRC1_4		462

// General
#define ON 	1
#define OFF 	0

