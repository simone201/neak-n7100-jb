/*
 * Author: andip71, 30.11.2012
 *
 * Version 1.2
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

#include <sound/soc.h>
#include <sound/core.h>
#include <sound/jack.h>

#include <linux/miscdevice.h>
#include <linux/switch.h>
#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/gpio.h>

#include "wm8994.h"

#include "wolfson_sound.h"

#define CALL_ACTIVE_REGISTER 0

/*****************************************/
// Static variables
/*****************************************/

// pointer to codec structure
static struct snd_soc_codec *codec;
static struct wm8994_priv *wm8994;

// internal wolfson sound variables
static int wolfson_sound;
static int debug_level;

static int headphone_l, headphone_r;

static int speaker_l, speaker_r;

static int eq;

static int eq_gains[5];

static unsigned int eq_bands[5][4];

static int dac_direct;
static int dac_oversampling;
static int fll_tuning;
static int privacy_mode;

static int mic_mode;

static unsigned int debug_register;

// internal state variables
static bool is_call;
static bool is_headphone;
static bool is_socket;
static bool is_fmradio;
static bool is_eq;


/*****************************************/
// Internal function declarations
/*****************************************/

static unsigned int wm8994_read(struct snd_soc_codec *codec, unsigned int reg);
static int wm8994_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value);

extern struct switch_dev android_switch;

static bool debug(int level);
static bool check_for_call(unsigned int val);
static bool check_for_socket(unsigned int val);
static bool check_for_headphone(void);
static bool check_for_fmradio(void);

static void set_headphone(void);
static unsigned int get_headphone_l(unsigned int val);
static unsigned int get_headphone_r(unsigned int val);

static void set_speaker(void);
static unsigned int get_speaker_l(unsigned int val);
static unsigned int get_speaker_r(unsigned int val);

static void set_eq(void);
static void set_eq_gains(void);
static void set_eq_bands(int band);
static void set_eq_satprevention(void);
static unsigned int get_eq_satprevention(int reg_index, unsigned int val);

static void set_dac_direct(void);
static unsigned int get_dac_direct_l(unsigned int val);
static unsigned int get_dac_direct_r(unsigned int val);

static void set_dac_oversampling(void);
static void set_fll_tuning(void);

static void set_mic_mode(void);
static unsigned int get_mic_mode(int reg_index);
static unsigned int get_mic_mode_for_hook(int reg_index, unsigned int value);


/*****************************************/
// wolfson sound hook functions for
// original wm8994 alsa driver
/*****************************************/

void Wolfson_sound_hook_wm8994_pcm_probe(struct snd_soc_codec *codec_pointer)
{
	// store a copy of the pointer to the codec, we need
	// that for internal calls to the audio hub
	codec = codec_pointer;

	// store pointer to codecs driver data
	wm8994 = snd_soc_codec_get_drvdata(codec);

	// Print debug info
	printk("Wolfson-Sound: codec pointer received\n");
}


unsigned int Wolfson_sound_hook_wm8994_write(unsigned int reg, unsigned int val)
{
	unsigned int newval;

	// Terminate instantly if wolfson sound is not enabled and return
	// original value back
	if (!wolfson_sound)
		return val;

	// If the write request of the original driver is for specific registers,
	// change value to wolfson sound values accordingly as new return value
	newval = val;

	switch (reg)
	{

		// call detection
		case WM8994_AIF2_CONTROL_2:
		{
			if (is_call != check_for_call(val))
			{
				is_call = !is_call;

				if (debug(DEBUG_NORMAL))
					printk("Wolfson-Sound: Call detection new status %d\n", is_call);

				// switch equalizer and mic mode
				set_eq();
				set_mic_mode();
			}
			break;
		}

		// socket connection/disconnection detection (incl. headphone un-plug)
		// (see headphone detection below for plug-in)
		case WM1811_JACKDET_CTRL:
		{
			if (check_for_socket(val))
			{
				is_socket = true;

				if (debug(DEBUG_NORMAL))
					printk("Wolfson-Sound: Socket plugged-in\n");
			}
			else
			{
				is_socket = false;
				is_headphone = false;

				if (debug(DEBUG_NORMAL))
					printk("Wolfson-Sound: Socket un-plugged\n");

				// Handler: switch equalizer and set speaker volume (for privacy mode)
				set_eq();
				set_speaker();
			}
			break;
		}

		// left headphone volume
		case WM8994_LEFT_OUTPUT_VOLUME:
		{
			newval = get_headphone_l(val);
			break;
		}

		// right headphone volume
		case WM8994_RIGHT_OUTPUT_VOLUME:
		{
			newval = get_headphone_r(val);
			break;
		}

		// left speaker volume
		case WM8994_SPEAKER_VOLUME_LEFT:
		{
			newval = get_speaker_l(val);
			break;
		}

		// right speaker volume
		case WM8994_SPEAKER_VOLUME_RIGHT:
		{
			newval = get_speaker_r(val);
			break;
		}

		// dac_direct left channel
		case WM8994_OUTPUT_MIXER_1:
		{
			newval = get_dac_direct_l(val);
			break;
		}

		// dac_direct right channel
		case WM8994_OUTPUT_MIXER_2:
		{
			newval = get_dac_direct_r(val);
			break;
		}

		// EQ saturation prevention: dynamic range control 1_1
		case WM8994_AIF1_DRC1_1:
		{
			newval = get_eq_satprevention(1, val);
			break;
		}

		// EQ saturation prevention: dynamic range control 1_2
		case WM8994_AIF1_DRC1_2:
		{
			newval = get_eq_satprevention(2, val);
			break;
		}

		// EQ saturation prevention: dynamic range control 1_3
		case WM8994_AIF1_DRC1_3:
		{
			newval = get_eq_satprevention(3, val);
			break;
		}

		// Microphone: left input volume
		case WM8994_LEFT_LINE_INPUT_1_2_VOLUME:
		{
			newval = get_mic_mode_for_hook(1, val);
			break;
		}


		// Microphone: right input volume
		case WM8994_RIGHT_LINE_INPUT_1_2_VOLUME:
		{
			newval = get_mic_mode_for_hook(2, val);
			break;
		}

		// Microphone: input mixer 3 = left channel
		case WM8994_INPUT_MIXER_3:
		{
			newval = get_mic_mode_for_hook(3, val);
			break;
		}

		// Microphone: input mixer 4 = right channel
		case WM8994_INPUT_MIXER_4:
		{
			newval = get_mic_mode_for_hook(4, val);
			break;
		}

		// Microphone: dynamic range control 2_1
		case WM8994_AIF1_DRC2_1:
		{
			newval = get_mic_mode_for_hook(5, val);
			break;
		}

		// Microphone: dynamic range control 2_2
		case WM8994_AIF1_DRC2_2:
		{
			newval = get_mic_mode_for_hook(6, val);
			break;
		}

		// Microphone: dynamic range control 2_3
		case WM8994_AIF1_DRC2_3:
		{
			newval = get_mic_mode_for_hook(7, val);
			break;
		}

		// Microphone: dynamic range control 2_4
		case WM8994_AIF1_DRC2_4:
		{
			newval = get_mic_mode_for_hook(8, val);
			break;
		}
	}

	// Headphone plug-in detection
	// ( for un-plug detection see above, this is covered by checking a register)
	if (is_socket && !is_headphone)
	{
		if (check_for_headphone())
		{
			is_headphone = true;

			if (debug(DEBUG_NORMAL))
				printk("Wolfson-Sound: Headphone or headset found\n");

			// Handler: switch equalizer and set speaker volume (for privacy mode)
			set_eq();
			set_speaker();
		}
	}

	// FM radio detection
	// Important note: We need to absolutely make sure we do not do this detection if one of the 
	// two output mixers are called in this hook (as they can potentially be modified again in the
	// set_dac_direct call). Otherwise this adds strange value overwriting effects.
	if (is_fmradio != check_for_fmradio() &&
		(reg != WM8994_OUTPUT_MIXER_1) && (reg != WM8994_OUTPUT_MIXER_2))
	{
		is_fmradio = !is_fmradio;

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: FM radio detection new status %d\n", is_fmradio);

		// Switch dac_direct
		set_dac_direct();
	}

	// print debug info
	if (debug(DEBUG_VERBOSE))
		printk("Wolfson-Sound: write hook %d -> %d (Orig:%d), c:%d, h:%d, r:%d\n", 
				reg, newval, val, is_call, is_headphone, is_fmradio);

	return newval;
}


/*****************************************/
// Internal functions copied over from
// original wm8994 alsa driver,
// enriched by some debug prints
/*****************************************/

static int wm8994_readable(struct snd_soc_codec *codec, unsigned int reg)
{
	//struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	struct wm8994 *control = codec->control_data;

	switch (reg) {
	case WM8994_GPIO_1:
	case WM8994_GPIO_2:
	case WM8994_GPIO_3:
	case WM8994_GPIO_4:
	case WM8994_GPIO_5:
	case WM8994_GPIO_6:
	case WM8994_GPIO_7:
	case WM8994_GPIO_8:
	case WM8994_GPIO_9:
	case WM8994_GPIO_10:
	case WM8994_GPIO_11:
	case WM8994_INTERRUPT_STATUS_1:
	case WM8994_INTERRUPT_STATUS_2:
	case WM8994_INTERRUPT_STATUS_1_MASK:
	case WM8994_INTERRUPT_STATUS_2_MASK:
	case WM8994_INTERRUPT_RAW_STATUS_2:
		return 1;

	case WM8958_DSP2_PROGRAM:
	case WM8958_DSP2_CONFIG:
	case WM8958_DSP2_EXECCONTROL:
		if (control->type == WM8958)
			return 1;
		else
			return 0;

	default:
		break;
	}

	if (reg >= WM8994_CACHE_SIZE)
		return 0;
	return wm8994_access_masks[reg].readable != 0;
}


static int wm8994_volatile(struct snd_soc_codec *codec, unsigned int reg)
{
	if (reg >= WM8994_CACHE_SIZE)
		return 1;

	switch (reg) {
	case WM8994_SOFTWARE_RESET:
	case WM8994_CHIP_REVISION:
	case WM8994_DC_SERVO_1:
	case WM8994_DC_SERVO_READBACK:
	case WM8994_RATE_STATUS:
	case WM8994_LDO_1:
	case WM8994_LDO_2:
	case WM8958_DSP2_EXECCONTROL:
	case WM8958_MIC_DETECT_3:
	case WM8994_DC_SERVO_4E:
		return 1;
	default:
		return 0;
	}
}


static int wm8994_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	int ret;

	BUG_ON(reg > WM8994_MAX_REGISTER);

	if (!wm8994_volatile(codec, reg)) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d",
				reg, ret);
	}

	// print debug info
	if (debug(DEBUG_VERBOSE))
		printk("Wolfson-Sound: write register %d -> %d\n", reg, value);

	return wm8994_reg_write(codec->control_data, reg, value);
}


static unsigned int wm8994_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	unsigned int val;
	int ret;

	BUG_ON(reg > WM8994_MAX_REGISTER);

	if (!wm8994_volatile(codec, reg) && wm8994_readable(codec, reg) &&
	    reg < codec->driver->reg_cache_size) {
		ret = snd_soc_cache_read(codec, reg, &val);
		if (ret >= 0)
		{
			// print debug info
			if (debug(DEBUG_VERBOSE))
				printk("Wolfson-Sound: read register from cache %d -> %d\n", reg, val);

			return val;
		}
		else
			dev_err(codec->dev, "Cache read from %x failed: %d",
				reg, ret);
	}

	val = wm8994_reg_read(codec->control_data, reg);

	// print debug info
	if (debug(DEBUG_VERBOSE))
		printk("Wolfson-Sound: read register %d -> %d\n", reg, val);

	return val;
}


/*****************************************/
// Internal helper functions
/*****************************************/

static bool check_for_call(unsigned int val)
{
	// check via register WM8994_AIF2DACR if currently call active
	if (!(val & CALL_ACTIVE_REGISTER))
		return true;

	return false;
}


static bool check_for_socket(unsigned int val)
{
	// check via register WM1811_JACKDET if something is plugged in currently
	if (val & WM1811_JACKDET_DB_MASK)
		return false;

	return true;
}


static bool check_for_headphone(void)
{
	// check status of micdet zero jacket to find out whether headphone
	// or headset is currently connected
	// Note: This always shows status delayed after something has been plugged in or
	// unplugged !!!
	return (switch_get_state(&android_switch) > 0);
}


static bool check_for_fmradio(void)
{
	struct snd_soc_dapm_widget *w;

	// loop through widget list to find widget for FM radio and check
	// power state of it
	list_for_each_entry(w, &codec->card->widgets, list)
	{
		if (w->dapm != &codec->dapm)
			continue;

		switch (w->id) 
		{
			case snd_soc_dapm_line:
				if (w->name)
				{
					if(strstr(w->name,"FM In") != 0)
					{
						if((w->power) != 0)
							return true;
						else
							return false;
					}
				}
				break;
			case snd_soc_dapm_mic:
			case snd_soc_dapm_hp:
			case snd_soc_dapm_spk:
			case snd_soc_dapm_micbias:
			case snd_soc_dapm_dac:
			case snd_soc_dapm_adc:
			case snd_soc_dapm_pga:
			case snd_soc_dapm_out_drv:
			case snd_soc_dapm_mixer:
			case snd_soc_dapm_mixer_named_ctl:
			case snd_soc_dapm_supply:
				break;
			default:
				break;
		}
	}

	return false;
}


static bool debug (int level)
{
	// determine whether a debug information should be printed according to currently
	// configured debug level, or not
	if (level <= debug_level)
		return true;

	return false;
}



/*****************************************/
// Internal set/get/restore functions
/*****************************************/

// Headphone volume

static void set_headphone(void)
{
	unsigned int val;

	// get current register value, unmask volume bits, merge with wolfson sound volume and write back
	val = wm8994_read(codec, WM8994_LEFT_OUTPUT_VOLUME);
	val = (val & ~WM8994_HPOUT1L_VOL_MASK) | headphone_l;
        wm8994_write(codec, WM8994_LEFT_OUTPUT_VOLUME, val);

	val = wm8994_read(codec, WM8994_RIGHT_OUTPUT_VOLUME);
	val = (val & ~WM8994_HPOUT1R_VOL_MASK) | headphone_r;
        wm8994_write(codec, WM8994_RIGHT_OUTPUT_VOLUME, val | WM8994_HPOUT1_VU);

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: set_headphone %d %d\n", headphone_l, headphone_r);

}


static unsigned int get_headphone_l(unsigned int val)
{
	// return register value for left headphone volume back
        return (val & ~WM8994_HPOUT1L_VOL_MASK) | headphone_l;
}


static unsigned int get_headphone_r(unsigned int val)
{
	// return register value for right headphone volume back
        return (val & ~WM8994_HPOUT1R_VOL_MASK) | headphone_r;
}


// Speaker volume

static void set_speaker(void)
{
	unsigned int val;

	// read current register values, get corrected value and write back to audio hub
	val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_LEFT);
	val = get_speaker_l(val);
        wm8994_write(codec, WM8994_SPEAKER_VOLUME_LEFT, val);

	val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_RIGHT);
	val = get_speaker_r(val);
        wm8994_write(codec, WM8994_SPEAKER_VOLUME_RIGHT, val | WM8994_SPKOUT_VU);

	// print debug info
	if (debug(DEBUG_NORMAL))
	{
	if((privacy_mode == ON) && is_headphone)
			printk("Wolfson-Sound: set_speaker to mute (privacy mode)\n");
		else
			printk("Wolfson-Sound: set_speaker %d %d\n", speaker_l, speaker_r);
	}
}


static unsigned int get_speaker_l(unsigned int val)
{
	// if privacy mode is on, we set value to zero, otherwise to configured speaker volume
	if((privacy_mode == ON) && is_headphone)
	{
		return (val & ~WM8994_SPKOUTL_VOL_MASK);
	}
	return (val & ~WM8994_SPKOUTL_VOL_MASK) | speaker_l;
}


static unsigned int get_speaker_r(unsigned int val)
{
	// if privacy mode is on, we set value to zero, otherwise to configured speaker volume
	if((privacy_mode == ON) && is_headphone)
	{
		return (val & ~WM8994_SPKOUTR_VOL_MASK);
	}
	return (val & ~WM8994_SPKOUTR_VOL_MASK) | speaker_r;
}


// Equalizer on/off

static void set_eq(void)
{
	unsigned int val;

	// read current register value from audio hub
	val = wm8994_read(codec, WM8994_AIF1_DAC1_EQ_GAINS_1);

	// Equalizer will only be switched on in fact if there is no call and if there is
	// a headphone connected
	if (is_call || !is_headphone || eq == EQ_OFF)
	{
		// switch EQ off + print debug
		val &= ~WM8994_AIF1DAC1_EQ_ENA_MASK;
		is_eq = OFF;

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: set_eq off\n");
	}
	else
	{
		// switch EQ on + print debug
		val |= WM8994_AIF1DAC1_EQ_ENA_MASK;
		is_eq = ON;

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: set_eq on\n");
	}

	// write value back to audio hub
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_1, val);

	// in the end set saturation prevention according to configuration
	set_eq_satprevention();
}


// Equalizer gains

static void set_eq_gains(void)
{
	unsigned int val;

	// First register
	// read current value from audio hub and mask all bits apart from equalizer enabled bit
	val = wm8994_read(codec, WM8994_AIF1_DAC1_EQ_GAINS_1);
	val &= WM8994_AIF1DAC1_EQ_ENA_MASK;

	// add individual gains and write back to audio hub
	val = val | ((eq_gains[0] + EQ_GAIN_OFFSET) << WM8994_AIF1DAC1_EQ_B1_GAIN_SHIFT);
	val = val | ((eq_gains[1] + EQ_GAIN_OFFSET) << WM8994_AIF1DAC1_EQ_B2_GAIN_SHIFT);
	val = val | ((eq_gains[2] + EQ_GAIN_OFFSET) << WM8994_AIF1DAC1_EQ_B3_GAIN_SHIFT);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_1, val);

	// second register
	// set individual gains and write back to audio hub
	val = ((eq_gains[3] + EQ_GAIN_OFFSET) << WM8994_AIF1DAC1_EQ_B4_GAIN_SHIFT);
	val = val | ((eq_gains[4] + EQ_GAIN_OFFSET) << WM8994_AIF1DAC1_EQ_B5_GAIN_SHIFT);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_2, val);

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: set_eq_gains %d %d %d %d %d\n",
			eq_gains[0], eq_gains[1], eq_gains[2], eq_gains[3], eq_gains[4]);
}


// Equalizer bands

static void set_eq_bands(int band)
{
	// depending on which band is supposed to be updated, update values and print debug info,
	// or update all bands if requested
	if((band == 1) || (band == BANDS_ALL))
	{
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_A, eq_bands[0][0]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_B, eq_bands[0][1]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_PG, eq_bands[0][3]);

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: set_eq_bands 1 %d %d %d\n",
				eq_bands[0][0], eq_bands[0][1], eq_bands[0][3]);
	}

	if((band == 2) || (band == BANDS_ALL))
	{
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_A, eq_bands[1][0]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_B, eq_bands[1][1]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_C, eq_bands[1][2]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_PG, eq_bands[1][3]);

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: set_eq_bands 2 %d %d %d %d\n",
				eq_bands[1][0], eq_bands[1][1], eq_bands[1][2], eq_bands[1][3]);
	}

	if((band == 3) || (band == BANDS_ALL))
	{
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_A, eq_bands[2][0]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_B, eq_bands[2][1]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_C, eq_bands[2][2]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_PG, eq_bands[2][3]);

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: set_eq_bands 3 %d %d %d %d\n",
				eq_bands[2][0], eq_bands[2][1], eq_bands[2][2], eq_bands[2][3]);
	}

	if((band == 4) || (band == BANDS_ALL))
	{
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_4_A, eq_bands[3][0]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_4_B, eq_bands[3][1]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_4_C, eq_bands[3][2]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_4_PG, eq_bands[3][3]);

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: set_eq_bands 4 %d %d %d %d\n",
				eq_bands[3][0], eq_bands[3][1], eq_bands[3][2], eq_bands[3][3]);
	}

	if((band == 5) || (band == BANDS_ALL))
	{
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_5_A, eq_bands[4][0]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_5_B, eq_bands[4][1]);
		wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_5_PG, eq_bands[4][3]);

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: set_eq_bands 5 %d %d %d\n",
				eq_bands[4][0], eq_bands[4][1], eq_bands[4][3]);
	}
}


// EQ saturation prevention

static void set_eq_satprevention(void)
{
	unsigned int val;

	// read current value for DRC1_3 register, modify value and write back to audio hub
	val = wm8994_read(codec, WM8994_AIF1_DRC1_3);
	val = get_eq_satprevention(3, val);
	wm8994_write(codec, WM8994_AIF1_DRC1_3, val);

	// read current value for DRC1_2 register, modify value and write back to audio hub
	val = wm8994_read(codec, WM8994_AIF1_DRC1_2);
	val = get_eq_satprevention(2, val);
	wm8994_write(codec, WM8994_AIF1_DRC1_2, val);

	// read current value for DRC1_1 register, modify value and write back to audio hub
	val = wm8994_read(codec, WM8994_AIF1_DRC1_1);
	val = get_eq_satprevention(1, val);
	wm8994_write(codec, WM8994_AIF1_DRC1_1, val);

	// print debug information
	if (debug(DEBUG_NORMAL))
	{
		// check whether saturation prevention is switched on or off based on
		// real status of EQ and configured EQ mode
		if (is_eq && (eq == EQ_NORMAL))
		{
			printk("Wolfson-Sound: set_eq_satprevention to on\n");
		}
		else
		{
			printk("Wolfson-Sound: set_eq_satprevention to off\n");
		}
	}
}


static unsigned int get_eq_satprevention(int reg_index, unsigned int val)
{
	// EQ mode is with saturation prevention and EQ is in fact on
	if ((is_eq) && (eq == EQ_NORMAL))
	{
		switch(reg_index)
		{
			case 1: 
			{
				// register WM8994_AIF1_DRC1_1
				// disable quick release and anticlip, enable drc
				val &= ~WM8994_AIF1DRC1_QR_MASK;
				val &= ~WM8994_AIF1DRC1_ANTICLIP_MASK;
				val |= WM8994_AIF1DAC1_DRC_ENA;
				return val;
			}

			case 2:
			{
				// register WM8994_AIF1_DRC1_2
				// set new values for attack, decay and maxgain
				val &= ~(WM8994_AIF1DRC1_ATK_MASK);
				val &= ~(WM8994_AIF1DRC1_DCY_MASK);
				val &= ~(WM8994_AIF1DRC1_MAXGAIN_MASK);
				val |= (EQ_DRC_ATK_PREVENT << WM8994_AIF1DRC1_ATK_SHIFT);
				val |= (EQ_DRC_DCY_PREVENT << WM8994_AIF1DRC1_DCY_SHIFT);
				val |= (EQ_DRC_MAXGAIN_PREVENT << WM8994_AIF1DRC1_MAXGAIN_SHIFT);
				return val;
			}

			case 3:
			{
				// register WM8994_AIF1_DRC1_3
				// set new value for hi_comp above knee
				val &= ~(WM8994_AIF1DRC1_HI_COMP_MASK);
				val |= (EQ_DRC_HI_COMP_PREVENT << WM8994_AIF1DRC1_HI_COMP_SHIFT);
				return val;
			}
		}
	}

	// EQ is in fact off or mode is without saturation prevention
	switch(reg_index)
	{
		case 1:
		{
			// register WM8994_AIF1_DRC1_1
			// enable quick release and anticlip, disable drc
			val |= WM8994_AIF1DRC1_QR;
			val |= WM8994_AIF1DRC1_ANTICLIP;
			val &= ~(WM8994_AIF1DAC1_DRC_ENA_MASK);
			return val;
		}

		case 2:
		{
			// register WM8994_AIF1_DRC1_2
			// set default values for attack, decay and maxgain
			val &= ~WM8994_AIF1DRC1_ATK_MASK;
			val &= ~WM8994_AIF1DRC1_DCY_MASK;
			val &= ~WM8994_AIF1DRC1_MAXGAIN_MASK;
			val |= (EQ_DRC_ATK_DEFAULT << WM8994_AIF1DRC1_ATK_SHIFT);
			val |= (EQ_DRC_DCY_DEFAULT << WM8994_AIF1DRC1_DCY_SHIFT);
			val |= (EQ_DRC_MAXGAIN_DEFAULT << WM8994_AIF1DRC1_MAXGAIN_SHIFT);
			return val;
		}

		case 3:
		{
			// register WM8994_AIF1_DRC1_3
			// set default value for hi_comp above knee
			val &= ~(WM8994_AIF1DRC1_HI_COMP_MASK);
			val |= (EQ_DRC_HI_COMP_DEFAULT << WM8994_AIF1DRC1_HI_COMP_SHIFT);
			return val;
		}
	}

	// We should in fact never reach this last return, only in case of errors
	return val;
}


// DAC direct

static void set_dac_direct(void)
{
	unsigned int val;

	// get current values for output mixer 1 and 2 (l + r) from audio hub
	// modify the data accordingly and write back to audio hub
	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
	val = get_dac_direct_l(val);
	wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
	val = get_dac_direct_r(val);
	wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

	// take the value of the right channel as reference, check for the bypass bit
	// and print debug information
	if (debug(DEBUG_NORMAL))
	{
		if (val & WM8994_DAC1R_TO_HPOUT1R)
			printk("Wolfson-Sound: set_dac_direct on\n");
		else
			printk("Wolfson-Sound: set_dac_direct off\n");
	}

}

static unsigned int get_dac_direct_l(unsigned int val)
{
	// dac direct is only enabled if fm radio is not active
	if ((dac_direct == ON) && (!is_fmradio))
	{
		// enable dac_direct: bypass for both channels, mute output mixer
		return((val & ~WM8994_DAC1L_TO_MIXOUTL) | WM8994_DAC1L_TO_HPOUT1L);
	}

	// disable dac_direct: enable bypass for both channels, mute output mixer
	return((val & ~WM8994_DAC1L_TO_HPOUT1L) | WM8994_DAC1L_TO_MIXOUTL);
}

static unsigned int get_dac_direct_r(unsigned int val)
{
	// dac direct is only enabled if fm radio is not active
	if ((dac_direct == ON) && (!is_fmradio))
	{
		// enable dac_direct: bypass for both channels, mute output mixer
		return((val & ~WM8994_DAC1R_TO_MIXOUTR) | WM8994_DAC1R_TO_HPOUT1R);
	}

	// disable dac_direct: enable bypass for both channels, mute output mixer
	return((val & ~WM8994_DAC1R_TO_HPOUT1R) | WM8994_DAC1R_TO_MIXOUTR);
}


// DAC oversampling

static void set_dac_oversampling()
{
	int val;

	// read current value of oversampling register
	val = wm8994_read(codec, WM8994_OVERSAMPLING);

	// toggle oversampling bit depending on status + print debug
	if (dac_oversampling == ON)
	{
		val |= WM8994_DAC_OSR128;

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: set_oversampling on\n");
	}
	else
	{
		val &= ~WM8994_DAC_OSR128;

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: set_oversampling off\n");
	}

	// write value back to audio hub
	wm8994_write(codec, WM8994_OVERSAMPLING, val);
}


// FLL tuning

static void set_fll_tuning(void)
{
	int val;

	// read current value of FLL control register 4 and mask out loop gain value
	val = wm8994_read(codec, WM8994_FLL1_CONTROL_4);
	val &= ~WM8994_FLL1_LOOP_GAIN_MASK;

	// depending on whether fll tuning is on or off, modify value accordingly
	// and print debug
	if (fll_tuning == ON)
	{
		val |= FLL_LOOP_GAIN_TUNED;

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: set_fll_tuning on\n");
	}
	else
	{
		val |= FLL_LOOP_GAIN_DEFAULT;

		if (debug(DEBUG_NORMAL))
			printk("Wolfson-Sound: set_fll_tuning off\n");
	}

	// write value back to audio hub
	wm8994_write(codec, WM8994_FLL1_CONTROL_4, val);
}


// MIC mode

static void set_mic_mode(void)
{
	unsigned int reg_value[9];
	int i;

	// get current register values for selected mic mode
	for (i=1; i<=8; i++)
	{
		reg_value[i] = get_mic_mode(i);
	}

	// write values for selected mic_mode to audio hub
	wm8994_write(codec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME, reg_value[1]);
	wm8994_write(codec, WM8994_RIGHT_LINE_INPUT_1_2_VOLUME, reg_value[2]);
	wm8994_write(codec, WM8994_INPUT_MIXER_3, reg_value[3]);
	wm8994_write(codec, WM8994_INPUT_MIXER_4, reg_value[4]);
	wm8994_write(codec, WM8994_AIF1_DRC2_1, reg_value[5]);
	wm8994_write(codec, WM8994_AIF1_DRC2_2, reg_value[6]);
	wm8994_write(codec, WM8994_AIF1_DRC2_3, reg_value[7]);
	wm8994_write(codec, WM8994_AIF1_DRC2_4, reg_value[8]);

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: set_mic_mode %d %d %d %d %d %d %d %d\n",
			reg_value[1], reg_value[2], reg_value[3], reg_value[4],
			reg_value[5], reg_value[6], reg_value[7], reg_value[8]);
}


static unsigned int get_mic_mode(int reg_index)
{
	// Mic mode is default or we have an active call
	if ((mic_mode == MIC_MODE_DEFAULT) || is_call)
	{
		switch(reg_index)
		{
			case 1:
				return MIC_DEFAULT_LEFT_VALUE;
			case 2:
				return MIC_DEFAULT_RIGHT_VALUE;
			case 3:
				return MIC_DEFAULT_INPUT_MIXER_3;
			case 4:
				return MIC_DEFAULT_INPUT_MIXER_4;
			case 5:
				return MIC_DEFAULT_DRC1_1;
			case 6:
				return MIC_DEFAULT_DRC1_2;
			case 7:
				return MIC_DEFAULT_DRC1_3;
			case 8:
				return MIC_DEFAULT_DRC1_4;
		}
	}

	// Mic mode is concert
	if (mic_mode == MIC_MODE_CONCERT)
	{
		switch(reg_index)
		{
			case 1:
				return MIC_CONCERT_LEFT_VALUE;
			case 2:
				return MIC_CONCERT_RIGHT_VALUE;
			case 3:
				return MIC_CONCERT_INPUT_MIXER_3;
			case 4:
				return MIC_CONCERT_INPUT_MIXER_4;
			case 5:
				return MIC_CONCERT_DRC1_1;
			case 6:
				return MIC_CONCERT_DRC1_2;
			case 7:
				return MIC_CONCERT_DRC1_3;
			case 8:
				return MIC_CONCERT_DRC1_4;
		}
	}

	// Mic mode is noisy
	if (mic_mode == MIC_MODE_NOISY)
	{
		switch(reg_index)
		{
			case 1:
				return MIC_NOISY_LEFT_VALUE;
			case 2:
				return MIC_NOISY_RIGHT_VALUE;
			case 3:
				return MIC_NOISY_INPUT_MIXER_3;
			case 4:
				return MIC_NOISY_INPUT_MIXER_4;
			case 5:
				return MIC_NOISY_DRC1_1;
			case 6:
				return MIC_NOISY_DRC1_2;
			case 7:
				return MIC_NOISY_DRC1_3;
			case 8:
				return MIC_NOISY_DRC1_4;
		}
	}

	// Mic mode is light
	if (mic_mode == MIC_MODE_LIGHT)
	{
		switch(reg_index)
		{
			case 1:
				return MIC_LIGHT_LEFT_VALUE;
			case 2:
				return MIC_LIGHT_RIGHT_VALUE;
			case 3:
				return MIC_LIGHT_INPUT_MIXER_3;
			case 4:
				return MIC_LIGHT_INPUT_MIXER_4;
			case 5:
				return MIC_LIGHT_DRC1_1;
			case 6:
				return MIC_LIGHT_DRC1_2;
			case 7:
				return MIC_LIGHT_DRC1_3;
			case 8:
				return MIC_LIGHT_DRC1_4;
		}
	}

	// we should never reach this, but if so in error case, return zero
	return 0;
}


static unsigned int get_mic_mode_for_hook(int reg_index, unsigned int value)
{
	// if mic mode is default or we have an active call -> return value back to hook
	// otherwise, request value for selected mic mode
	if ((mic_mode == MIC_MODE_DEFAULT) || is_call)
		return value;

	return get_mic_mode(reg_index);
}


// Initialization functions

static void initialize_global_variables(void)
{
	// set global variables to standard values

	headphone_l = HEADPHONE_DEFAULT;
	headphone_r = HEADPHONE_DEFAULT;

	speaker_l = SPEAKER_DEFAULT;
	speaker_r = SPEAKER_DEFAULT;

	eq = EQ_DEFAULT;

	eq_gains[0] = EQ_GAIN_DEFAULT;
	eq_gains[1] = EQ_GAIN_DEFAULT;
	eq_gains[2] = EQ_GAIN_DEFAULT;
	eq_gains[3] = EQ_GAIN_DEFAULT;
	eq_gains[4] = EQ_GAIN_DEFAULT;

	eq_bands[0][0] = EQ_BAND_1_A_DEFAULT;
	eq_bands[0][1] = EQ_BAND_1_B_DEFAULT;
	eq_bands[0][3] = EQ_BAND_1_PG_DEFAULT;
	eq_bands[1][0] = EQ_BAND_2_A_DEFAULT;
	eq_bands[1][1] = EQ_BAND_2_B_DEFAULT,
	eq_bands[1][2] = EQ_BAND_2_C_DEFAULT,
	eq_bands[1][3] = EQ_BAND_2_PG_DEFAULT;
	eq_bands[2][0] = EQ_BAND_3_A_DEFAULT;
	eq_bands[2][1] = EQ_BAND_3_B_DEFAULT;
	eq_bands[2][2] = EQ_BAND_3_C_DEFAULT;
	eq_bands[2][3] = EQ_BAND_3_PG_DEFAULT;
	eq_bands[3][0] = EQ_BAND_4_A_DEFAULT;
	eq_bands[3][1] = EQ_BAND_4_B_DEFAULT;
	eq_bands[3][2] = EQ_BAND_4_C_DEFAULT;
	eq_bands[3][3] = EQ_BAND_4_PG_DEFAULT;
	eq_bands[4][0] = EQ_BAND_5_A_DEFAULT;
	eq_bands[4][1] = EQ_BAND_5_B_DEFAULT;
	eq_bands[4][3] = EQ_BAND_5_PG_DEFAULT;

	dac_direct = OFF;

	dac_oversampling = OFF;

	fll_tuning = OFF;

	privacy_mode = OFF;

	mic_mode = MIC_MODE_DEFAULT;

	debug_register = 0;

	is_call = false;
	is_socket = false;
	is_headphone = false;
	is_fmradio = false;
	is_eq = false;

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: initialize_global_variables completed\n");

}


static void reset_wolfson_sound(void)
{
	unsigned int val;

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: reset_wolfson_sound started\n");

	// load all default values
	initialize_global_variables();

	// set headphone volumes to defaults
	set_headphone();

	// set speaker volumes to defaults
	set_speaker();

	// reset equalizer mode (incl. saturation prevention)
	set_eq();

	// reset equalizer gains
	set_eq_gains();

	// reset all equalizer bands
	set_eq_bands(BANDS_ALL);

	// reset DAC_direct
	set_dac_direct();

	// reset DAC oversampling
	set_dac_oversampling();

	// reset FLL tuning
	set_fll_tuning();

	// reset mic settings
	set_mic_mode();

	// initialize jacket status
	val = wm8994_read(codec, WM1811_JACKDET_CTRL);
	is_socket = check_for_socket(val);

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: reset_wolfson_sound completed\n");

}



/*****************************************/
// sysfs interface functions
/*****************************************/

// wolfson sound master switch

static ssize_t wolfson_sound_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	// print current value
	return sprintf(buf, "wolfson sound status: %d\n", wolfson_sound);
}


static ssize_t wolfson_sound_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	int val;

	// read values from input buffer
	ret = sscanf(buf, "%d", &val);

	// store if valid data and only if status has changed, reset all values
	if (((val == OFF) || (val == ON))&& (val != wolfson_sound))
	{
		wolfson_sound = val;
		reset_wolfson_sound();
	}

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: status %d\n", wolfson_sound);

	return count;

}


// Headphone volume

static ssize_t headphone_volume_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return 0;

	// print current values
	return sprintf(buf, "Headphone volume:\nLeft: %d\nRight: %d\n", headphone_l, headphone_r);
}


static ssize_t headphone_volume_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	int val_l;
	int val_r;

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return count;

	// read values from input buffer
	ret = sscanf(buf, "%d %d", &val_l, &val_r);

	// check whether values are within the valid ranges and adjust accordingly
	if (val_l > HEADPHONE_MAX)
	{
		val_l = HEADPHONE_MAX;
	}

	if (val_l < HEADPHONE_MIN)
	{
		val_l = HEADPHONE_MIN;
	}

	if (val_r > HEADPHONE_MAX)
	{
		val_r = HEADPHONE_MAX;
	}

	if (val_r < HEADPHONE_MIN)
	{
		val_r = HEADPHONE_MIN;
	}

	// store values into global variables
	headphone_l = val_l;
	headphone_r = val_r;

	// set new values
	set_headphone();

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: headphone volume L=%d R=%d\n", headphone_l, headphone_r);

	return count;
}


// Speaker volume

static ssize_t speaker_volume_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return 0;

	// print current values
	return sprintf(buf, "Speaker volume:\nLeft: %d\nRight: %d\n", speaker_l, speaker_r);

}

static ssize_t speaker_volume_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	int val_l;
	int val_r;

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return count;

	// read values from input buffer
	ret = sscanf(buf, "%d %d", &val_l, &val_r);

	// check whether values are within the valid ranges and adjust accordingly
	if (val_l > SPEAKER_MAX)
	{
		val_l = SPEAKER_MAX;
	}

	if (val_l < SPEAKER_MIN)
	{
		val_l = SPEAKER_MIN;
	}

	if (val_r > SPEAKER_MAX)
	{
		val_r = SPEAKER_MAX;
	}

	if (val_r < SPEAKER_MIN)
	{
		val_r = SPEAKER_MIN;
	}

	// store values into global variables
	speaker_l = val_l;
	speaker_r = val_r;

	// set new values
	set_speaker();

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: speaker volume L=%d R=%d\n", speaker_l, speaker_r);

	return count;
}


// Equalizer mode

static ssize_t eq_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return 0;

	// print current value
	return sprintf(buf, "EQ: %d\n", eq);
}


static ssize_t eq_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	int val;

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return count;

	// read values from input buffer and update audio hub
	ret = sscanf(buf, "%d", &val);

	if (((val >= EQ_OFF) && (val <= EQ_NOSATPREVENT)) && (val != eq))
	{
		eq = val;
		set_eq();
	}

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: EQ %d\n", eq);

	return count;
}


// Equalizer gains

static ssize_t eq_gains_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return 0;

	// print current values
	return sprintf(buf, "EQ gains: %d %d %d %d %d\n",
			eq_gains[0], eq_gains[1], eq_gains[2], eq_gains[3], eq_gains[4]);
}


static ssize_t eq_gains_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	int gains[5];
	int i;

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return count;

	// read values from input buffer
	ret = sscanf(buf, "%d %d %d %d %d", &gains[0], &gains[1], &gains[2], &gains[3], &gains[4]);

	// check validity of gain values and adjust
	for (i=0; i<=4; i++)
	{
		if (gains[i] < EQ_GAIN_MIN)
			gains[i] = EQ_GAIN_MIN;

		if (gains[i] > EQ_GAIN_MAX)
			gains[i] = EQ_GAIN_MAX;

		eq_gains[i] = gains[i];
	}

	// set new values
	set_eq_gains();

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: EQ gains %d %d %d %d %d\n",
			eq_gains[0], eq_gains[1], eq_gains[2], eq_gains[3], eq_gains[4]);

	return count;
}


// Equalizer bands

static ssize_t eq_bands_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return 0;

	// print current values
	return sprintf(buf, 
		"band a b c pg\n1: %d %d %d %d\n2: %d %d %d %d\n3: %d %d %d %d\n4: %d %d %d %d\n5: %d %d %d %d\n", 
			eq_bands[0][0], eq_bands[0][1], 0, eq_bands[0][3],
			eq_bands[1][0], eq_bands[1][1], eq_bands[1][2], eq_bands[1][3],
			eq_bands[2][0], eq_bands[2][1], eq_bands[2][2], eq_bands[2][3],
			eq_bands[3][0], eq_bands[3][1], eq_bands[3][2], eq_bands[3][3],
			eq_bands[4][0], eq_bands[4][1], 0, eq_bands[4][3]);
}


static ssize_t eq_bands_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int band, v1, v2, v3, v4;

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return count;

	// read values from input buffer
	ret = sscanf(buf, "%d %d %d %d %d", &band, &v1, &v2, &v3, &v4);

	// check input data for validity, terminate if not valid
	if ((band < 1) || (band > 5))
		return count;

	eq_bands[band-1][0] = v1;
	eq_bands[band-1][1] = v2;
	eq_bands[band-1][2] = v3;
	eq_bands[band-1][3] = v4;

	// set new values
	set_eq_bands(band);

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: EQ bands %d -> %d %d %d %d\n",
			band, v1, v2, v3, v4);

	return count;
}


// DAC direct

static ssize_t dac_direct_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return 0;

	return sprintf(buf, "DAC direct: %d\n", dac_direct);
}


static ssize_t dac_direct_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int val;

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return count;

	// read values from input buffer, check validity and update audio hub
	ret = sscanf(buf, "%d", &val);

	if ((val == ON) || (val == OFF))
	{
		dac_direct = val;
		set_dac_direct();
	}

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: DAC direct %d\n", dac_direct);

	return count;
}


// DAC oversampling

static ssize_t dac_oversampling_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return 0;

	return sprintf(buf, "DAC oversampling: %d\n", dac_oversampling);
}


static ssize_t dac_oversampling_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int val;

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return count;

	// read values from input buffer, check validity and update audio hub
	ret = sscanf(buf, "%d", &val);

	if ((val == ON) || (val == OFF))
	{
		dac_oversampling = val;
		set_dac_oversampling();
	}

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: DAC oversampling %d\n", dac_oversampling);

	return count;
}


// FLL tuning

static ssize_t fll_tuning_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return 0;

	return sprintf(buf, "FLL tuning: %d\n", fll_tuning);
}


static ssize_t fll_tuning_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int val;

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return count;

	// read values from input buffer, check validity and update audio hub
	ret = sscanf(buf, "%d", &val);

	if ((val == ON) || (val == OFF))
	{
		fll_tuning = val;
		set_fll_tuning();
	}

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: FLL tuning %d\n", fll_tuning);

	return count;
}


// Privacy mode

static ssize_t privacy_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return 0;

	return sprintf(buf, "Privacy mode: %d\n", privacy_mode);
}


static ssize_t privacy_mode_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int val;

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return count;

	// read values from input buffer, check validity and update audio hub
	ret = sscanf(buf, "%d", &val);

	if ((val == ON) || (val == OFF))
	{
		privacy_mode = val;
		set_speaker();
	}

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: Privacy mode %d\n", privacy_mode);

	return count;
}


// Microphone mode

static ssize_t mic_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return 0;

	return sprintf(buf, "Mic mode: %d\n", mic_mode);
}


static ssize_t mic_mode_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int val;

	// Terminate instantly if wolfson sound is not enabled
	if (!wolfson_sound)
		return count;

	// read value for mic_mode from input buffer
	ret = sscanf(buf, "%d", &val);

	// check validity of data and update audio hub
	if ((val >= MIC_MODE_DEFAULT) && (val <= MIC_MODE_LIGHT))
	{
		mic_mode = val;
		set_mic_mode();
	}

	// print debug info
	if (debug(DEBUG_NORMAL))
		printk("Wolfson-Sound: Mic mode %d\n", mic_mode);

	return count;
}


// Debug level

static ssize_t debug_level_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	// return current debug level back
	// (this also works when Wolfson-Sound is disabled)
	return sprintf(buf, "Debug level: %d\n", debug_level);
}


static ssize_t debug_level_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int val;

	// check data and store if valid
	ret = sscanf(buf, "%d", &val);

	if ((val >= 0) && (val <= 2))
	{
		debug_level = val;
	}

	return count;
}


// Debug info

static ssize_t debug_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int val;

	// start with version info
	sprintf(buf, "Wolfson-Sound version: %s\n\n", WOLFSON_SOUND_VERSION);

	// read values of some interesting registers and put them into a string
	val = wm8994_read(codec, WM8994_AIF2_CONTROL_2);
	sprintf(buf+strlen(buf), "WM8994_AIF2_CONTROL_2: %d\n", val);

	val = wm8994_read(codec, WM8994_LEFT_OUTPUT_VOLUME);
	sprintf(buf+strlen(buf), "WM8994_LEFT_OUTPUT_VOLUME: %d\n", val);

	val = wm8994_read(codec, WM8994_RIGHT_OUTPUT_VOLUME);
	sprintf(buf+strlen(buf), "WM8994_RIGHT_OUTPUT_VOLUME: %d\n", val);

	val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_LEFT);
	sprintf(buf+strlen(buf), "WM8994_SPEAKER_VOLUME_LEFT: %d\n", val);

	val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_RIGHT);
	sprintf(buf+strlen(buf), "WM8994_SPEAKER_VOLUME_RIGHT: %d\n", val);

	val = wm8994_read(codec, WM8994_AIF1_DAC1_EQ_GAINS_1);
	sprintf(buf+strlen(buf), "WM8994_AIF1_DAC1_EQ_GAINS_1: %d\n", val);

	val = wm8994_read(codec, WM8994_AIF1_DAC1_EQ_GAINS_2);
	sprintf(buf+strlen(buf), "WM8994_AIF1_DAC1_EQ_GAINS_2: %d\n", val);

	val = wm8994_read(codec, WM8994_AIF1_DRC1_1);
	sprintf(buf+strlen(buf), "WM8994_AIF1_DRC1_1: %d\n", val);

	val = wm8994_read(codec, WM8994_AIF1_DRC1_2);
	sprintf(buf+strlen(buf), "WM8994_AIF1_DRC1_2: %d\n", val);

	val = wm8994_read(codec, WM8994_AIF1_DRC1_3);
	sprintf(buf+strlen(buf), "WM8994_AIF1_DRC1_3: %d\n", val);

	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
	sprintf(buf+strlen(buf), "WM8994_OUTPUT_MIXER_1: %d\n", val);

	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
	sprintf(buf+strlen(buf), "WM8994_OUTPUT_MIXER_2: %d\n", val);

	val = wm8994_read(codec, WM8994_OVERSAMPLING);
	sprintf(buf+strlen(buf), "WM8994_OVERSAMPLING: %d\n", val);

	val = wm8994_read(codec, WM8994_FLL1_CONTROL_4);
	sprintf(buf+strlen(buf), "WM8994_FLL1_CONTROL_4: %d\n", val);

	val = wm8994_read(codec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME);
	sprintf(buf+strlen(buf), "WM8994_LEFT_LINE_INPUT_1_2_VOLUME: %d\n", val);

	val = wm8994_read(codec, WM8994_RIGHT_LINE_INPUT_1_2_VOLUME);
	sprintf(buf+strlen(buf), "WM8994_RIGHT_LINE_INPUT_1_2_VOLUME: %d\n", val);

	val = wm8994_read(codec, WM8994_INPUT_MIXER_3);
	sprintf(buf+strlen(buf), "WM8994_INPUT_MIXER_3: %d\n", val);

	val = wm8994_read(codec, WM8994_INPUT_MIXER_4);
	sprintf(buf+strlen(buf), "WM8994_INPUT_MIXER_4: %d\n", val);

	val = wm8994_read(codec, WM8994_AIF1_DRC2_1);
	sprintf(buf+strlen(buf), "WM8994_AIF1_DRC2_1: %d\n", val);

	val = wm8994_read(codec, WM8994_AIF1_DRC2_2);
	sprintf(buf+strlen(buf), "WM8994_AIF1_DRC2_2: %d\n", val);

	val = wm8994_read(codec, WM8994_AIF1_DRC2_3);
	sprintf(buf+strlen(buf), "WM8994_AIF1_DRC2_3: %d\n", val);

	val = wm8994_read(codec, WM8994_AIF1_DRC2_4);
	sprintf(buf+strlen(buf), "WM8994_AIF1_DRC2_4: %d\n\n", val);

	// finally add the current states of call, headphone and fmradio
	sprintf(buf+strlen(buf), "is_call:%d is_socket: %d is_headphone:%d is_fmradio:%d is_eq:%d\n", 
				is_call, is_socket, is_headphone, is_fmradio, is_eq);

	// return buffer length back
	return strlen(buf);
}


static ssize_t debug_info_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	// this function has no function, but can be misused for some debugging/testing

	return count;
}


// Debug register

static ssize_t debug_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int val;

	// read current debug register value from audio hub and return value back
	val = wm8994_read(codec, debug_register);
	return sprintf(buf, "%d -> %d", debug_register, val);
}


static ssize_t debug_reg_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned int val1 = 0;
	unsigned int val2;

	// read values from input buffer and update audio hub (if requested via key)
	ret = sscanf(buf, "%d %d %d", &debug_register, &val1, &val2);

	if (val1 == DEBUG_REGISTER_KEY)
	{
		wm8994_write(codec, debug_register, val2);
	}

	return count;
}

// Wolfson Audio Version

static ssize_t wolfson_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", WOLFSON_SOUND_VERSION);
}

/*****************************************/
// Initialize wolfson sound sysfs folder
/*****************************************/

// define objects
static DEVICE_ATTR(wolfson_sound, S_IRUGO | S_IWUGO, wolfson_sound_show, wolfson_sound_store);
static DEVICE_ATTR(headphone_volume, S_IRUGO | S_IWUGO, headphone_volume_show, headphone_volume_store);
static DEVICE_ATTR(speaker_volume, S_IRUGO | S_IWUGO, speaker_volume_show, speaker_volume_store);
static DEVICE_ATTR(privacy_mode, S_IRUGO | S_IWUGO, privacy_mode_show, privacy_mode_store);
static DEVICE_ATTR(eq, S_IRUGO | S_IWUGO, eq_show, eq_store);
static DEVICE_ATTR(eq_gains, S_IRUGO | S_IWUGO, eq_gains_show, eq_gains_store);
static DEVICE_ATTR(eq_bands, S_IRUGO | S_IWUGO, eq_bands_show, eq_bands_store);
static DEVICE_ATTR(dac_direct, S_IRUGO | S_IWUGO, dac_direct_show, dac_direct_store);
static DEVICE_ATTR(dac_oversampling, S_IRUGO | S_IWUGO, dac_oversampling_show, dac_oversampling_store);
static DEVICE_ATTR(fll_tuning, S_IRUGO | S_IWUGO, fll_tuning_show, fll_tuning_store);
static DEVICE_ATTR(mic_mode, S_IRUGO | S_IWUGO, mic_mode_show, mic_mode_store);
static DEVICE_ATTR(debug_level, S_IRUGO | S_IWUGO, debug_level_show, debug_level_store);
static DEVICE_ATTR(debug_info, S_IRUGO | S_IWUGO, debug_info_show, debug_info_store);
static DEVICE_ATTR(debug_reg, S_IRUGO | S_IWUGO, debug_reg_show, debug_reg_store);
static DEVICE_ATTR(wolfson_version, S_IRUGO, wolfson_version_show, NULL);

// define attributes
static struct attribute *wolfson_sound_attributes[] = {
	&dev_attr_wolfson_sound.attr,
	&dev_attr_headphone_volume.attr,
	&dev_attr_speaker_volume.attr,
	&dev_attr_privacy_mode.attr,
	&dev_attr_eq.attr,
	&dev_attr_eq_gains.attr,
	&dev_attr_eq_bands.attr,
	&dev_attr_dac_direct.attr,
	&dev_attr_dac_oversampling.attr,
	&dev_attr_fll_tuning.attr,
	&dev_attr_mic_mode.attr,
	&dev_attr_debug_level.attr,
	&dev_attr_debug_info.attr,
	&dev_attr_debug_reg.attr,
	&dev_attr_wolfson_version.attr,
	NULL
};

// define attribute group
static struct attribute_group wolfson_sound_control_group = {
	.attrs = wolfson_sound_attributes,
};

// define control device
static struct miscdevice wolfson_sound_control_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "wolfson_sound",
};


/*****************************************/
// Driver init and exit functions
/*****************************************/

static int wolfson_sound_init(void)
{
	// register wolfson sound control device
	misc_register(&wolfson_sound_control_device);
	if (sysfs_create_group(&wolfson_sound_control_device.this_device->kobj,
				&wolfson_sound_control_group) < 0) {
		printk("Wolfson-Sound: failed to create sys fs object.\n");
		return 0;
	}

	// Print debug info
	printk("Wolfson-Sound: engine version %s started\n", WOLFSON_SOUND_VERSION);

	// Initialize wolfson sound master switch and default debug level
	wolfson_sound = WOLFSON_SOUND_DEFAULT;
	debug_level = DEBUG_DEFAULT;

	// initialize global variables
	initialize_global_variables();

	return 0;
}


static void wolfson_sound_exit(void)
{
	// remove wolfson sound control device
	sysfs_remove_group(&wolfson_sound_control_device.this_device->kobj,
                           &wolfson_sound_control_group);

	// Print debug info
	printk("Wolfson-Sound: engine stopped\n");
}


/* define driver entry points */

module_init(wolfson_sound_init);
module_exit(wolfson_sound_exit);
