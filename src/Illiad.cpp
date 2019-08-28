#include "sts.hpp"
#include "dsp/digital.hpp"
#include "window.hpp"

struct Illiad : Module
{
	enum ParamIds
	{
		OCTAVE_PARAM,
		PORTA_PARAM,
		PITCHBEND_PARAM,
		ENUMS(OFFSET_PARAM, 35),
		ENUMS(SLIDER_PARAM, 35),
		ENUMS(SWITCH_PARAM, 23),
		NUM_PARAMS
	};
	enum InputIds
	{
		NUM_INPUTS
	};

	enum OutputIds
	{
		// Sliders
		ENUMS(OUT_OUTPUT, 35),
		// Pitch,Ports,Octave
		OUT1_OUTPUT_OCTAVE,
		OUT1_OUTPUT_PORTA,
		OUT1_OUTPUT_PITCHBEND,

		//switches
		ENUMS(OUT_OUTPUT_SW, 23),
		NUM_OUTPUTS
	};
	enum LightIds
	{
		TRIGGER_LED_1,
		NUM_LIGHTS
	};

	float out_[33];
	int panelStyle = 0;

	Illiad()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(PORTA_PARAM, 0.0, 10.0, 0.0, "Portamento", " V", 0.0, 1.0, 0.0);
		configParam(OCTAVE_PARAM, -4.0f, 2.0f, -1.0f, "Octave Switch", " Oct", 0.0, 1.0, 0.0);
		configParam(PITCHBEND_PARAM, 0.0, 2.0, 1.0, "Pitch Bend", " ", 0.0, 1.0, 0.0);

		configParam(SLIDER_PARAM + 24, 0.0, 2.0, 1.0, " ");
		configParam(SLIDER_PARAM + 25, 0.0, 0.25, 0.125, " ");

		configParam(SWITCH_PARAM + 0, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 1, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 2, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 3, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 4, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 5, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 6, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 7, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 8, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 9, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 10, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 11, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 12, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 13, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 14, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 15, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 16, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 17, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 18, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 19, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 20, 0.0, 1.0, 0.0, " ");
		configParam(SWITCH_PARAM + 21, 0.0, 1.0, 0.0, " ");

		configParam(OFFSET_PARAM + 0, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 1, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 2, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 3, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 4, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 5, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 6, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 7, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 8, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 9, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 10, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 11, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 12, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 13, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 14, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 15, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 16, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 17, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 18, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 19, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 20, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 21, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 22, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 23, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 24, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 25, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 26, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 27, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 28, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 29, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 30, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 31, -10.0, 10.0, 0.0, " ");
		configParam(OFFSET_PARAM + 32, -10.0, 10.0, 0.0, " ");
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
		// Octave Switch
		outputs[OUT1_OUTPUT_OCTAVE].value = params[OCTAVE_PARAM].value;
		// Portamento
		outputs[OUT1_OUTPUT_PORTA].value = params[PORTA_PARAM].value;
		// Pitch bend
		outputs[OUT1_OUTPUT_PITCHBEND].value = params[PITCHBEND_PARAM].value;

		// Sliders attenuverters Top + Bottom Row

		for (int i = 0; i < 33; i++)
		{
			out_[i] = clamp(params[SLIDER_PARAM + i].value + params[OFFSET_PARAM + i].value, -10.0f, 10.0f);
			outputs[OUT_OUTPUT + i].value = out_[i];
		}

		// Switches Bottom Row

		for (int i = 0; i < 22; i++)
		{
			outputs[OUT_OUTPUT_SW + i].value = params[SWITCH_PARAM + i].value;
		}
	}
};

/*
struct IlliadWidget : ModuleWidget 
{
    IlliadWidget(Illiad *module);
    void appendContextMenu(Menu *menu) override;
};
*/

struct IlliadWidget : ModuleWidget
{
	SvgPanel *goldPanel;
	SvgPanel *blackPanel;
	SvgPanel *whitePanel;

	struct panelStyleItem : MenuItem
	{
		Illiad *module;
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
		Illiad *module = dynamic_cast<Illiad *>(this->module);

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		//assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		panelStyleItem *goldItem = new panelStyleItem();
		goldItem->text = "Odyssey MKII 2810 Gold";
		goldItem->module = module;
		goldItem->theme = 0;
		menu->addChild(goldItem);

		panelStyleItem *blackItem = new panelStyleItem();
		blackItem->text = "Odyssey MKIII 2820 Orange";
		blackItem->module = module;
		blackItem->theme = 1;
		menu->addChild(blackItem);

		panelStyleItem *whiteItem = new panelStyleItem();
		whiteItem->text = "Original Odyssey 2800 White";
		whiteItem->module = module;
		whiteItem->theme = 2;
		menu->addChild(whiteItem);
	}

	IlliadWidget(Illiad *module)
	{

		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/illiad_black.svg")));

		goldPanel = new SvgPanel();
		goldPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/illiad_gold.svg")));
		box.size = goldPanel->box.size;
		addChild(goldPanel);
		blackPanel = new SvgPanel();
		blackPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/illiad_black.svg")));
		blackPanel->visible = false;
		addChild(blackPanel);
		whitePanel = new SvgPanel();
		whitePanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/illiad_white.svg")));
		whitePanel->visible = false;
		addChild(whitePanel);

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		//// Offset knobs top row
		addParam(createParam<sts_Davies15_Grey>(Vec(215, 38), module, Illiad::OFFSET_PARAM + 22));
		addParam(createParam<sts_Davies15_Grey>(Vec(242, 38), module, Illiad::OFFSET_PARAM + 23));

		addParam(createParam<sts_Davies15_Grey>(Vec(328, 38), module, Illiad::OFFSET_PARAM + 24));
		addParam(createParam<sts_Davies15_Grey>(Vec(355, 38), module, Illiad::OFFSET_PARAM + 25));

		addParam(createParam<sts_Davies15_Grey>(Vec(522, 38), module, Illiad::OFFSET_PARAM + 26));

		addParam(createParam<sts_Davies15_Grey>(Vec(554, 38), module, Illiad::OFFSET_PARAM + 27));
		addParam(createParam<sts_Davies15_Grey>(Vec(581, 38), module, Illiad::OFFSET_PARAM + 28));

		addParam(createParam<sts_Davies15_Grey>(Vec(689, 38), module, Illiad::OFFSET_PARAM + 29));

		addParam(createParam<sts_Davies15_Grey>(Vec(743, 38), module, Illiad::OFFSET_PARAM + 30));

		addParam(createParam<sts_Davies15_Grey>(Vec(775, 38), module, Illiad::OFFSET_PARAM + 31));
		addParam(createParam<sts_Davies15_Grey>(Vec(802, 38), module, Illiad::OFFSET_PARAM + 32));

		//// offset knobs bottom row

		addParam(createParam<sts_Davies15_Grey>(Vec(215, 185), module, Illiad::OFFSET_PARAM + 0));
		addParam(createParam<sts_Davies15_Grey>(Vec(242, 185), module, Illiad::OFFSET_PARAM + 1));
		addParam(createParam<sts_Davies15_Grey>(Vec(269, 185), module, Illiad::OFFSET_PARAM + 2));
		addParam(createParam<sts_Davies15_Grey>(Vec(296, 185), module, Illiad::OFFSET_PARAM + 3));

		addParam(createParam<sts_Davies15_Grey>(Vec(328, 185), module, Illiad::OFFSET_PARAM + 4));
		addParam(createParam<sts_Davies15_Grey>(Vec(355, 185), module, Illiad::OFFSET_PARAM + 5));
		addParam(createParam<sts_Davies15_Grey>(Vec(382, 185), module, Illiad::OFFSET_PARAM + 6));
		addParam(createParam<sts_Davies15_Grey>(Vec(409, 185), module, Illiad::OFFSET_PARAM + 7));

		addParam(createParam<sts_Davies15_Grey>(Vec(441, 185), module, Illiad::OFFSET_PARAM + 8));
		addParam(createParam<sts_Davies15_Grey>(Vec(468, 185), module, Illiad::OFFSET_PARAM + 9));
		addParam(createParam<sts_Davies15_Grey>(Vec(522, 185), module, Illiad::OFFSET_PARAM + 10));

		addParam(createParam<sts_Davies15_Grey>(Vec(554, 185), module, Illiad::OFFSET_PARAM + 11));
		addParam(createParam<sts_Davies15_Grey>(Vec(581, 185), module, Illiad::OFFSET_PARAM + 12));
		addParam(createParam<sts_Davies15_Grey>(Vec(608, 185), module, Illiad::OFFSET_PARAM + 13));
		addParam(createParam<sts_Davies15_Grey>(Vec(635, 185), module, Illiad::OFFSET_PARAM + 14));
		addParam(createParam<sts_Davies15_Grey>(Vec(662, 185), module, Illiad::OFFSET_PARAM + 15));
		addParam(createParam<sts_Davies15_Grey>(Vec(689, 185), module, Illiad::OFFSET_PARAM + 16));

		addParam(createParam<sts_Davies15_Grey>(Vec(743, 185), module, Illiad::OFFSET_PARAM + 17));

		addParam(createParam<sts_Davies15_Grey>(Vec(775, 185), module, Illiad::OFFSET_PARAM + 18));
		addParam(createParam<sts_Davies15_Grey>(Vec(802, 185), module, Illiad::OFFSET_PARAM + 19));
		addParam(createParam<sts_Davies15_Grey>(Vec(829, 185), module, Illiad::OFFSET_PARAM + 20));
		addParam(createParam<sts_Davies15_Grey>(Vec(856, 185), module, Illiad::OFFSET_PARAM + 21));

		//// Slider Output Ports

		addOutput(createOutput<sts_Port>(Vec(9, 20), module, Illiad::OUT_OUTPUT + 0));
		addOutput(createOutput<sts_Port>(Vec(9, 58), module, Illiad::OUT_OUTPUT + 1));
		addOutput(createOutput<sts_Port>(Vec(9, 96), module, Illiad::OUT_OUTPUT + 2));
		addOutput(createOutput<sts_Port>(Vec(9, 134), module, Illiad::OUT_OUTPUT + 3));
		addOutput(createOutput<sts_Port>(Vec(9, 172), module, Illiad::OUT_OUTPUT + 4));
		addOutput(createOutput<sts_Port>(Vec(9, 210), module, Illiad::OUT_OUTPUT + 5));
		addOutput(createOutput<sts_Port>(Vec(9, 248), module, Illiad::OUT_OUTPUT + 6));
		addOutput(createOutput<sts_Port>(Vec(9, 286), module, Illiad::OUT_OUTPUT + 7));
		addOutput(createOutput<sts_Port>(Vec(9, 324), module, Illiad::OUT_OUTPUT + 8));

		addOutput(createOutput<sts_Port>(Vec(41, 20), module, Illiad::OUT_OUTPUT + 9));
		addOutput(createOutput<sts_Port>(Vec(41, 58), module, Illiad::OUT_OUTPUT + 10));
		addOutput(createOutput<sts_Port>(Vec(41, 96), module, Illiad::OUT_OUTPUT + 11));
		addOutput(createOutput<sts_Port>(Vec(41, 134), module, Illiad::OUT_OUTPUT + 12));
		addOutput(createOutput<sts_Port>(Vec(41, 172), module, Illiad::OUT_OUTPUT + 13));
		addOutput(createOutput<sts_Port>(Vec(41, 210), module, Illiad::OUT_OUTPUT + 14));
		addOutput(createOutput<sts_Port>(Vec(41, 248), module, Illiad::OUT_OUTPUT + 15));
		addOutput(createOutput<sts_Port>(Vec(41, 286), module, Illiad::OUT_OUTPUT + 16));
		addOutput(createOutput<sts_Port>(Vec(41, 324), module, Illiad::OUT_OUTPUT + 17));

		addOutput(createOutput<sts_Port>(Vec(73, 20), module, Illiad::OUT_OUTPUT + 18));
		addOutput(createOutput<sts_Port>(Vec(73, 58), module, Illiad::OUT_OUTPUT + 19));
		addOutput(createOutput<sts_Port>(Vec(73, 96), module, Illiad::OUT_OUTPUT + 20));
		addOutput(createOutput<sts_Port>(Vec(73, 134), module, Illiad::OUT_OUTPUT + 21));
		addOutput(createOutput<sts_Port>(Vec(73, 172), module, Illiad::OUT_OUTPUT + 22));
		addOutput(createOutput<sts_Port>(Vec(73, 210), module, Illiad::OUT_OUTPUT + 23));
		addOutput(createOutput<sts_Port>(Vec(73, 248), module, Illiad::OUT_OUTPUT + 24));
		addOutput(createOutput<sts_Port>(Vec(73, 286), module, Illiad::OUT_OUTPUT + 25));
		addOutput(createOutput<sts_Port>(Vec(73, 324), module, Illiad::OUT_OUTPUT + 26));

		addOutput(createOutput<sts_Port>(Vec(105, 20), module, Illiad::OUT_OUTPUT + 27));
		addOutput(createOutput<sts_Port>(Vec(105, 58), module, Illiad::OUT_OUTPUT + 28));
		addOutput(createOutput<sts_Port>(Vec(105, 96), module, Illiad::OUT_OUTPUT + 29));
		addOutput(createOutput<sts_Port>(Vec(105, 134), module, Illiad::OUT_OUTPUT + 30));
		addOutput(createOutput<sts_Port>(Vec(105, 172), module, Illiad::OUT_OUTPUT + 31));
		addOutput(createOutput<sts_Port>(Vec(105, 210), module, Illiad::OUT_OUTPUT + 32));

		addOutput(createOutput<sts_Port>(Vec(105, 248), module, Illiad::OUT1_OUTPUT_OCTAVE));
		addOutput(createOutput<sts_Port>(Vec(105, 286), module, Illiad::OUT1_OUTPUT_PORTA));
		addOutput(createOutput<sts_Port>(Vec(105, 324), module, Illiad::OUT1_OUTPUT_PITCHBEND));

		//// Add Switch Outputs
		addOutput(createOutput<sts_Port>(Vec(128, 340), module, Illiad::OUT_OUTPUT_SW + 19));
		addOutput(createOutput<sts_Port>(Vec(154, 340), module, Illiad::OUT_OUTPUT_SW + 20));
		addOutput(createOutput<sts_Port>(Vec(180, 340), module, Illiad::OUT_OUTPUT_SW + 21));

		addOutput(createOutput<sts_Port>(Vec(209, 340), module, Illiad::OUT_OUTPUT_SW + 0));
		addOutput(createOutput<sts_Port>(Vec(236, 340), module, Illiad::OUT_OUTPUT_SW + 1));
		addOutput(createOutput<sts_Port>(Vec(290, 340), module, Illiad::OUT_OUTPUT_SW + 2));

		addOutput(createOutput<sts_Port>(Vec(322, 340), module, Illiad::OUT_OUTPUT_SW + 3));
		addOutput(createOutput<sts_Port>(Vec(349, 340), module, Illiad::OUT_OUTPUT_SW + 4));
		addOutput(createOutput<sts_Port>(Vec(403, 340), module, Illiad::OUT_OUTPUT_SW + 5));

		addOutput(createOutput<sts_Port>(Vec(435, 340), module, Illiad::OUT_OUTPUT_SW + 6));
		addOutput(createOutput<sts_Port>(Vec(462, 340), module, Illiad::OUT_OUTPUT_SW + 7));
		addOutput(createOutput<sts_Port>(Vec(489, 340), module, Illiad::OUT_OUTPUT_SW + 8));

		addOutput(createOutput<sts_Port>(Vec(548, 340), module, Illiad::OUT_OUTPUT_SW + 9));
		addOutput(createOutput<sts_Port>(Vec(575, 340), module, Illiad::OUT_OUTPUT_SW + 10));
		addOutput(createOutput<sts_Port>(Vec(602, 340), module, Illiad::OUT_OUTPUT_SW + 11));
		addOutput(createOutput<sts_Port>(Vec(629, 340), module, Illiad::OUT_OUTPUT_SW + 12));
		addOutput(createOutput<sts_Port>(Vec(656, 340), module, Illiad::OUT_OUTPUT_SW + 13));
		addOutput(createOutput<sts_Port>(Vec(683, 340), module, Illiad::OUT_OUTPUT_SW + 14));

		addOutput(createOutput<sts_Port>(Vec(737, 340), module, Illiad::OUT_OUTPUT_SW + 15));

		addOutput(createOutput<sts_Port>(Vec(769, 340), module, Illiad::OUT_OUTPUT_SW + 16));
		addOutput(createOutput<sts_Port>(Vec(796, 340), module, Illiad::OUT_OUTPUT_SW + 17));
		addOutput(createOutput<sts_Port>(Vec(850, 340), module, Illiad::OUT_OUTPUT_SW + 18));

		//// Add sliders Top Row

		addParam(createParam<sts_SlidePotBlue>(Vec(212, 57), module, Illiad::SLIDER_PARAM + 22)); //, 0.0, 2.0, 1.0
		addParam(createParam<sts_SlidePotBlue>(Vec(239, 57), module, Illiad::SLIDER_PARAM + 23)); //, 0.0, 0.25, 0.125

		addParam(createParam<sts_SlidePotGreen>(Vec(325, 57), module, Illiad::SLIDER_PARAM + 24)); //, 0.0, 2.0, 1.0));
		addParam(createParam<sts_SlidePotGreen>(Vec(352, 57), module, Illiad::SLIDER_PARAM + 25)); //, 0.0, 0.25, 0.125));

		addParam(createParam<sts_SlidePotPink>(Vec(519, 57), module, Illiad::SLIDER_PARAM + 26)); //, 0.0, 10.0, 0.0

		addParam(createParam<sts_SlidePotBlack>(Vec(551, 57), module, Illiad::SLIDER_PARAM + 27)); //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotBlack>(Vec(578, 57), module, Illiad::SLIDER_PARAM + 28)); // , 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotBlack>(Vec(686, 57), module, Illiad::SLIDER_PARAM + 29)); //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotBlack>(Vec(740, 57), module, Illiad::SLIDER_PARAM + 30)); // , 0.0, 10.0, 0.0

		addParam(createParam<sts_SlidePotRed>(Vec(772, 57), module, Illiad::SLIDER_PARAM + 31)); //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotRed>(Vec(799, 57), module, Illiad::SLIDER_PARAM + 32)); //, 0.0, 10.0, 0.0

		// Add sliders Bottom Row

		addParam(createParam<sts_SlidePotPink>(Vec(212, 204), module, Illiad::SLIDER_PARAM + 0));   //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotYellow>(Vec(239, 204), module, Illiad::SLIDER_PARAM + 1)); //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotBlue>(Vec(266, 204), module, Illiad::SLIDER_PARAM + 2));   // , 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotPink>(Vec(293, 204), module, Illiad::SLIDER_PARAM + 3));   //, 0.0, 10.0, 0.0

		addParam(createParam<sts_SlidePotPink>(Vec(325, 204), module, Illiad::SLIDER_PARAM + 4));   //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotYellow>(Vec(352, 204), module, Illiad::SLIDER_PARAM + 5)); //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotGreen>(Vec(379, 204), module, Illiad::SLIDER_PARAM + 6));  //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotPink>(Vec(406, 204), module, Illiad::SLIDER_PARAM + 7));   //, 0.0, 10.0, 0.0

		addParam(createParam<sts_SlidePotBlue>(Vec(438, 204), module, Illiad::SLIDER_PARAM + 8));	//, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotWhite>(Vec(465, 204), module, Illiad::SLIDER_PARAM + 9));   //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotYellow>(Vec(519, 204), module, Illiad::SLIDER_PARAM + 10)); //, 0.0, 10.0, 0.0

		addParam(createParam<sts_SlidePotWhite>(Vec(551, 204), module, Illiad::SLIDER_PARAM + 11));  //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotBlue>(Vec(578, 204), module, Illiad::SLIDER_PARAM + 12));   //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotGreen>(Vec(605, 204), module, Illiad::SLIDER_PARAM + 13));  //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotBlack>(Vec(632, 204), module, Illiad::SLIDER_PARAM + 14));  //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotYellow>(Vec(659, 204), module, Illiad::SLIDER_PARAM + 15)); //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotRed>(Vec(686, 204), module, Illiad::SLIDER_PARAM + 16));	// , 0.0, 10.0, 0.0

		addParam(createParam<sts_SlidePotRed>(Vec(740, 204), module, Illiad::SLIDER_PARAM + 17)); //, 0.0, 10.0, 0.0

		addParam(createParam<sts_SlidePotRed>(Vec(772, 204), module, Illiad::SLIDER_PARAM + 18)); //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotRed>(Vec(799, 204), module, Illiad::SLIDER_PARAM + 19)); //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotRed>(Vec(826, 204), module, Illiad::SLIDER_PARAM + 20)); //, 0.0, 10.0, 0.0
		addParam(createParam<sts_SlidePotRed>(Vec(853, 204), module, Illiad::SLIDER_PARAM + 21)); //, 0.0, 10.0, 0.0

		//// Switches Bottom row

		addParam(createParam<sts_CKSS>(Vec(215, 301), module, Illiad::SWITCH_PARAM + 0));
		addParam(createParam<sts_CKSS>(Vec(242, 301), module, Illiad::SWITCH_PARAM + 1));
		addParam(createParam<sts_CKSS>(Vec(296, 301), module, Illiad::SWITCH_PARAM + 2));

		addParam(createParam<sts_CKSS>(Vec(328, 301), module, Illiad::SWITCH_PARAM + 3));
		addParam(createParam<sts_CKSS>(Vec(355, 301), module, Illiad::SWITCH_PARAM + 4));
		addParam(createParam<sts_CKSS>(Vec(409, 301), module, Illiad::SWITCH_PARAM + 5));

		addParam(createParam<sts_CKSS>(Vec(441, 301), module, Illiad::SWITCH_PARAM + 6));
		addParam(createParam<sts_CKSS>(Vec(468, 301), module, Illiad::SWITCH_PARAM + 7));
		addParam(createParam<sts_CKSS>(Vec(495, 301), module, Illiad::SWITCH_PARAM + 8));

		addParam(createParam<sts_CKSS>(Vec(554, 301), module, Illiad::SWITCH_PARAM + 9));
		addParam(createParam<sts_CKSS>(Vec(581, 301), module, Illiad::SWITCH_PARAM + 10));
		addParam(createParam<sts_CKSS>(Vec(608, 301), module, Illiad::SWITCH_PARAM + 11));
		addParam(createParam<sts_CKSS>(Vec(635, 301), module, Illiad::SWITCH_PARAM + 12));
		addParam(createParam<sts_CKSS>(Vec(662, 301), module, Illiad::SWITCH_PARAM + 13));
		addParam(createParam<sts_CKSS>(Vec(689, 301), module, Illiad::SWITCH_PARAM + 14));

		addParam(createParam<sts_CKSS>(Vec(743, 301), module, Illiad::SWITCH_PARAM + 15));

		addParam(createParam<sts_CKSS>(Vec(775, 301), module, Illiad::SWITCH_PARAM + 16));
		addParam(createParam<sts_CKSS>(Vec(802, 301), module, Illiad::SWITCH_PARAM + 17));
		addParam(createParam<sts_CKSS>(Vec(856, 301), module, Illiad::SWITCH_PARAM + 18));
		// Switches Top Row
		addParam(createParam<sts_CKSS>(Vec(165, 57), module, Illiad::SWITCH_PARAM + 19));
		addParam(createParam<sts_CKSS>(Vec(265, 57), module, Illiad::SWITCH_PARAM + 20));
		addParam(createParam<sts_CKSS>(Vec(377, 57), module, Illiad::SWITCH_PARAM + 21));

		// add Portamentp Slider
		addParam(createParam<sts_SlidePotBlack>(Vec(161, 110), module, Illiad::PORTA_PARAM));
		addParam(createParam<sts_Davies_snap_47_Grey>(Vec(147, 215), module, Illiad::OCTAVE_PARAM));
		addParam(createParam<sts_Davies47_Grey>(Vec(147, 280), module, Illiad::PITCHBEND_PARAM));
	}

	void step() override
	{
		if (module)
		{
			goldPanel->visible = ((((Illiad *)module)->panelStyle) == 0);
			blackPanel->visible = ((((Illiad *)module)->panelStyle) == 1);
			whitePanel->visible = ((((Illiad *)module)->panelStyle) == 2);
		}
		Widget::step();
	}
};

Model *modelIlliad = createModel<Illiad, IlliadWidget>("Illiad");
