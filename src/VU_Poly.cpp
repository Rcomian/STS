//============================================================================================================
//!
//! \file VU-G1-Poly.cpp
//!
//! \brief VU-G1 is a simple polyphonic volume monitoring module.
//!
//============================================================================================================

#include "sts.hpp"
#include "rack.hpp"

using namespace std;
using namespace rack;

#define MAX_POLY_CHANNELS 16
#define VU_LEVELS 10

struct VU_Poly : Module
{
	enum ParamIds
	{
		NUM_PARAMS
	};
	enum InputIds
	{
		POLY_INPUT,
		NUM_INPUTS
	};
	enum OutputIds
	{
		NUM_OUTPUTS,
	};
	enum LightIds
	{
		ENUMS(CHANNEL_LIGHTS, MAX_POLY_CHANNELS),
		ENUMS(VU_LIGHTS, VU_LEVELS *MAX_POLY_CHANNELS),
		NUM_LIGHTS // N
	};

	int panelStyle = 0;

	dsp::VuMeter2 vuMeter[MAX_POLY_CHANNELS];
	dsp::ClockDivider vuDivider;
	dsp::ClockDivider lightDivider;

	VU_Poly()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		vuDivider.setDivision(16);
		lightDivider.setDivision(256);

		for (int c = 0; c < MAX_POLY_CHANNELS; c++)
		{
			vuMeter[c].lambda = 1 / 0.1f;
		}
	}

	json_t *dataToJson() override
	{
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "panelStyle", json_integer(panelStyle));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override
	{
		json_t *panelStyleJ = json_object_get(rootJ, "panelStyle");
		if (panelStyleJ)
			panelStyle = json_integer_value(panelStyleJ);
	}

	void process(const ProcessArgs &args) override
	{
		if (inputs[POLY_INPUT].isConnected())
		{
			float in[MAX_POLY_CHANNELS] = {};
			int channels = inputs[POLY_INPUT].getChannels();
			
			if (vuDivider.process())
			{
				for (int c = 0; c < channels; c++)
				{
					in[c] = inputs[POLY_INPUT].getPolyVoltage(c);

					vuMeter[c].process(args.sampleTime* vuDivider.getDivision(), in[c] / 10.f);
					// Set channel lights infrequently
					//if (lightDivider.process()) 
					//{
						//in[c] = inputs[POLY_INPUT].getPolyVoltage(c);	
						bool active = (c < channels);

						lights[CHANNEL_LIGHTS + c].setBrightness(active);

						lights[VU_LIGHTS + 0+ (VU_LEVELS * c)].setBrightness(vuMeter[c].getBrightness(0.f,  0.f));
						for (int i = 0; i < VU_LEVELS; i++)
						{
							lights[VU_LIGHTS + i + (VU_LEVELS * c)].setBrightness(vuMeter[c].getBrightness(-3.f * i, -3.f * (i - 1)));
						}
					//}
				}
			}
		}
		else
		{
			for (int c = 0; c < 16; c++)
			{
				lights[CHANNEL_LIGHTS + c].setBrightness(0);
				for (int i = 0; i < VU_LEVELS; i++)
					{
						lights[VU_LIGHTS + i + (VU_LEVELS * c)].setBrightness(0);
					}
			}
		}
		
	}
};

//============================================================================================================
//! \brief The widget.

struct VU_PolyWidget : ModuleWidget
{
	SvgPanel *goldPanel;
	SvgPanel *blackPanel;
	SvgPanel *whitePanel;

	struct panelStyleItem : MenuItem
	{
		VU_Poly *module;
		int theme;
		void onAction(const event::Action &e) override
		{
			module->panelStyle = theme;
		}
		void step() override
		{
			rightText = (module->panelStyle == theme) ? "✔" : "";
		}
	};

	void appendContextMenu(Menu *menu) override
	{
		VU_Poly *module = dynamic_cast<VU_Poly *>(this->module);

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		//assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		panelStyleItem *goldItem = new panelStyleItem();
		goldItem->text = "Bkack & Gold";
		goldItem->module = module;
		goldItem->theme = 0;
		menu->addChild(goldItem);

		panelStyleItem *blackItem = new panelStyleItem();
		blackItem->text = "Black & Orange";
		blackItem->module = module;
		blackItem->theme = 1;
		menu->addChild(blackItem);

		panelStyleItem *whiteItem = new panelStyleItem();
		whiteItem->text = "White & Black";
		whiteItem->module = module;
		whiteItem->theme = 2;
		menu->addChild(whiteItem);
	}

	VU_PolyWidget(VU_Poly *module) 
	{	
		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VUpoly.svg")));

		goldPanel = new SvgPanel();
        goldPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/vupoly_gold.svg")));
        box.size = goldPanel->box.size;
        addChild(goldPanel);
        blackPanel = new SvgPanel();
        blackPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/vupoly_black.svg")));
        blackPanel->visible = false;
        addChild(blackPanel);
        whitePanel = new SvgPanel();
        whitePanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/vupoly_white.svg")));
        whitePanel->visible = false;
        addChild(whitePanel);

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		addInput(createInputCentered<sts_Port>(Vec(60, 320), module, VU_Poly::POLY_INPUT));

		for (int j = 0; j < 8; ++j)
		{
			addChild(createLight<MediumLight<RedLight>>(Vec(10 + j * 13, 170), module, VU_Poly::CHANNEL_LIGHTS + j));
		}
		for (int j = 8; j < 16; ++j)
		{
			addChild(createLight<MediumLight<RedLight>>(Vec(10 + ((j - 8) * 13), 295), module, VU_Poly::CHANNEL_LIGHTS + j));
		}

		for (int channel = 0; channel < 8; channel++)
		{
			addChild(createLight<MediumLight<RedLight>>(Vec(10 + channel * 13, 70), module, VU_Poly::VU_LIGHTS + 0 + 10 * channel));
			addChild(createLight<MediumLight<YellowLight>>(Vec(10 + channel * 13, 80), module, VU_Poly::VU_LIGHTS + 1 + 10 * channel));
			addChild(createLight<MediumLight<YellowLight>>(Vec(10 + channel * 13, 90), module, VU_Poly::VU_LIGHTS + 2 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + channel * 13, 100), module, VU_Poly::VU_LIGHTS + 3 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + channel * 13, 110), module, VU_Poly::VU_LIGHTS + 4 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + channel * 13, 120), module, VU_Poly::VU_LIGHTS + 5 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + channel * 13, 130), module, VU_Poly::VU_LIGHTS + 6 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + channel * 13, 140), module, VU_Poly::VU_LIGHTS + 7 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + channel * 13, 150), module, VU_Poly::VU_LIGHTS + 8 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + channel * 13, 160), module, VU_Poly::VU_LIGHTS + 9 + 10 * channel));
		}
		for (int channel = 8; channel < 16; channel++)
		{
			addChild(createLight<MediumLight<RedLight>>(Vec(10 + ((channel - 8) * 13), 195), module, VU_Poly::VU_LIGHTS + 0 + 10 * channel));
			addChild(createLight<MediumLight<YellowLight>>(Vec(10 + ((channel -8) * 13), 205), module, VU_Poly::VU_LIGHTS + 1 + 10 * channel));
			addChild(createLight<MediumLight<YellowLight>>(Vec(10 + ((channel - 8) * 13), 215), module, VU_Poly::VU_LIGHTS + 2 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + ((channel - 8) * 13), 225), module, VU_Poly::VU_LIGHTS + 3 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + ((channel - 8) * 13), 235), module, VU_Poly::VU_LIGHTS + 4 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + ((channel - 8) * 13), 245), module, VU_Poly::VU_LIGHTS + 5 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + ((channel - 8) * 13), 255), module, VU_Poly::VU_LIGHTS + 6 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + ((channel - 8) * 13), 265), module, VU_Poly::VU_LIGHTS + 7 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + ((channel - 8) * 13), 275), module, VU_Poly::VU_LIGHTS + 8 + 10 * channel));
			addChild(createLight<MediumLight<GreenLight>>(Vec(10 + ((channel - 8) * 13), 285), module, VU_Poly::VU_LIGHTS + 9 + 10 * channel));
		}
	}
	void step() override
    {
        if (module)
        {
            goldPanel->visible = ((((VU_Poly *)module)->panelStyle) == 0);
            blackPanel->visible = ((((VU_Poly *)module)->panelStyle) == 1);
            whitePanel->visible = ((((VU_Poly *)module)->panelStyle) == 2);
        }
        Widget::step();
    }
};

Model *modelVU_Poly = createModel<VU_Poly, VU_PolyWidget>("VU_Poly");


