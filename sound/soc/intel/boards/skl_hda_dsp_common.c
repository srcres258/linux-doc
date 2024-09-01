// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2015-18 Intel Corporation.

/*
 * Common functions used in different Intel machine drivers
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "skl_hda_dsp_common.h"

#include <sound/hda_codec.h>
#include "../../codecs/hdac_hda.h"

#define NAME_SIZE	32

int skl_hda_hdmi_add_pcm(struct snd_soc_card *card, int device)
{
	struct skl_hda_private *ctx = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *dai;
	char dai_name[NAME_SIZE];

	snprintf(dai_name, sizeof(dai_name), "intel-hdmi-hifi%d",
		 ctx->dai_index);
	dai = snd_soc_card_get_codec_dai(card, dai_name);
	if (!dai)
		return -EINVAL;

	ctx->hdmi.hdmi_comp = dai->component;

	return 0;
}

SND_SOC_DAILINK_DEF(idisp1_cpu,
	DAILINK_COMP_ARRAY(COMP_CPU("iDisp1 Pin")));
SND_SOC_DAILINK_DEF(idisp1_codec,
	DAILINK_COMP_ARRAY(COMP_CODEC("ehdaudio0D2", "intel-hdmi-hifi1")));

SND_SOC_DAILINK_DEF(idisp2_cpu,
	DAILINK_COMP_ARRAY(COMP_CPU("iDisp2 Pin")));
SND_SOC_DAILINK_DEF(idisp2_codec,
	DAILINK_COMP_ARRAY(COMP_CODEC("ehdaudio0D2", "intel-hdmi-hifi2")));

SND_SOC_DAILINK_DEF(idisp3_cpu,
	DAILINK_COMP_ARRAY(COMP_CPU("iDisp3 Pin")));
SND_SOC_DAILINK_DEF(idisp3_codec,
	DAILINK_COMP_ARRAY(COMP_CODEC("ehdaudio0D2", "intel-hdmi-hifi3")));

SND_SOC_DAILINK_DEF(analog_cpu,
	DAILINK_COMP_ARRAY(COMP_CPU("Analog CPU DAI")));
SND_SOC_DAILINK_DEF(analog_codec,
	DAILINK_COMP_ARRAY(COMP_CODEC("ehdaudio0D0", "Analog Codec DAI")));

SND_SOC_DAILINK_DEF(digital_cpu,
	DAILINK_COMP_ARRAY(COMP_CPU("Digital CPU DAI")));
SND_SOC_DAILINK_DEF(digital_codec,
	DAILINK_COMP_ARRAY(COMP_CODEC("ehdaudio0D0", "Digital Codec DAI")));

SND_SOC_DAILINK_DEF(dmic_pin,
	DAILINK_COMP_ARRAY(COMP_CPU("DMIC01 Pin")));

SND_SOC_DAILINK_DEF(dmic_codec,
	DAILINK_COMP_ARRAY(COMP_CODEC("dmic-codec", "dmic-hifi")));

SND_SOC_DAILINK_DEF(dmic16k,
	DAILINK_COMP_ARRAY(COMP_CPU("DMIC16k Pin")));

SND_SOC_DAILINK_DEF(bt_offload_pin,
	DAILINK_COMP_ARRAY(COMP_CPU(""))); /* initialized in driver probe function */
SND_SOC_DAILINK_DEF(dummy,
	DAILINK_COMP_ARRAY(COMP_DUMMY()));

SND_SOC_DAILINK_DEF(platform,
	DAILINK_COMP_ARRAY(COMP_PLATFORM("0000:00:1f.3")));

/* skl_hda_digital audio interface glue - connects codec <--> CPU */
struct snd_soc_dai_link skl_hda_be_dai_links[HDA_DSP_MAX_BE_DAI_LINKS] = {
	/* Back End DAI links */
	{
		.name = "iDisp1",
		.id = 1,
		.dpcm_playback = 1,
		.no_pcm = 1,
		SND_SOC_DAILINK_REG(idisp1_cpu, idisp1_codec, platform),
	},
	{
		.name = "iDisp2",
		.id = 2,
		.dpcm_playback = 1,
		.no_pcm = 1,
		SND_SOC_DAILINK_REG(idisp2_cpu, idisp2_codec, platform),
	},
	{
		.name = "iDisp3",
		.id = 3,
		.dpcm_playback = 1,
		.no_pcm = 1,
		SND_SOC_DAILINK_REG(idisp3_cpu, idisp3_codec, platform),
	},
	{
		.name = "Analog Playback and Capture",
		.id = 4,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
		SND_SOC_DAILINK_REG(analog_cpu, analog_codec, platform),
	},
	{
		.name = "Digital Playback and Capture",
		.id = 5,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
		SND_SOC_DAILINK_REG(digital_cpu, digital_codec, platform),
	},
	{
		.name = "dmic01",
		.id = 6,
		.dpcm_capture = 1,
		.no_pcm = 1,
		SND_SOC_DAILINK_REG(dmic_pin, dmic_codec, platform),
	},
	{
		.name = "dmic16k",
		.id = 7,
		.dpcm_capture = 1,
		.no_pcm = 1,
		SND_SOC_DAILINK_REG(dmic16k, dmic_codec, platform),
	},
	{
		.name = NULL, /* initialized in driver probe function */
		.id = 8,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
		SND_SOC_DAILINK_REG(bt_offload_pin, dummy, platform),
	},
};

int skl_hda_hdmi_jack_init(struct snd_soc_card *card)
{
	struct skl_hda_private *ctx = snd_soc_card_get_drvdata(card);

	/* HDMI disabled, do not create controls */
	if (!ctx->hdmi.idisp_codec)
		return 0;

	if (!ctx->hdmi.hdmi_comp)
		return -EINVAL;

	return hda_dsp_hdmi_build_controls(card, ctx->hdmi.hdmi_comp);
}
