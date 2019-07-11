#include "sts.hpp"
#include "rack.hpp"
#include <common/diode.hpp>
// STL
#include <cmath>

/*
http://recherche.ircam.fr/pub/dafx11/Papers/66_e.pdf
Original VCV Rack Module code by J Eres
*/

using namespace std;
using namespace rack;

struct RingModulator : Module
{
	enum ParamIds
	{
		INPUT_LEVEL_PARAM,
		CARRIER_LEVEL_PARAM,
		CARRIER_OFFSET_PARAM,
		INPUT_POLARITY_PARAM,
		CARRIER_POLARITY_PARAM,
		DIODE_VB_PARAM,
		DIODE_VL_MINUS_VB_PARAM,
		DIODE_H_PARAM,

		NUM_PARAMS
	};

	enum InputIds
	{
		INPUT_INPUT,
		CARRIER_INPUT,
		CARRIER_OFFSET_INPUT,

		NUM_INPUTS
	};

	enum OutputIds
	{
		RING_OUTPUT,
		SUM_OUTPUT,
		DIFF_OUTPUT,
		MIN_OUTPUT,
		MAX_OUTPUT,

		NUM_OUTPUTS
	};

	enum LightIds
	{
		NUM_LIGHTS
	};

	int panelStyle = 0;

	RingModulator()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(INPUT_LEVEL_PARAM, 0.0, 1.0, 1.0);
		configParam(INPUT_POLARITY_PARAM, 0.0, 2.0, 1.0);

		configParam(CARRIER_LEVEL_PARAM, 0.0, 1.0, 1.0);
		configParam(CARRIER_POLARITY_PARAM, 0.0, 2.0, 1.0);

		configParam(CARRIER_OFFSET_PARAM, -g_controlPeakVoltage, g_controlPeakVoltage, 0.0);

		configParam(DIODE_VB_PARAM, std::numeric_limits<float>::epsilon(), g_controlPeakVoltage, 0.2);
		configParam(DIODE_VL_MINUS_VB_PARAM, std::numeric_limits<float>::epsilon(), g_controlPeakVoltage, 0.5);
		configParam(DIODE_H_PARAM, 0.0, 1.0, 0.9);
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

	inline bool needToStep()
	{
		if (!outputs[RING_OUTPUT].isConnected() &&
			!outputs[SUM_OUTPUT].isConnected() &&
			!outputs[DIFF_OUTPUT].isConnected() &&
			!outputs[MIN_OUTPUT].isConnected() &&
			!outputs[MAX_OUTPUT].isConnected())
			return false;

		if (!inputs[INPUT_INPUT].isConnected() || !inputs[CARRIER_INPUT].isConnected())
		{
			outputs[RING_OUTPUT].setVoltage(0.f);
			return false;
		}

		return true;
	}

	inline void updateDiodeCharacteristics()
	{
		m_diode.setVb(params[DIODE_VB_PARAM].getValue());
		m_diode.setVlMinusVb(params[DIODE_VL_MINUS_VB_PARAM].getValue());
		m_diode.setH(params[DIODE_H_PARAM].getValue());
	}
	
	//inline float getLeveledPolarizedInputValue(InputIds inId, ParamIds polarityPrmId, ParamIds levelPrmId)
	inline float getLeveledPolarizedInputValue(float inId,float inputPolarity, float levelPrmId)
	{
		
		//const float inputPolarity = getParameterValue(polarityPrmId);
		const float inputValue = inId * levelPrmId;
		
		if (inputPolarity < 0.5f)
			return (inputValue < 0.0f) ? -m_diode(inputValue) : 0.f;
		else if (inputPolarity > 1.5f)
			return (inputValue > 0.0f) ? m_diode(inputValue) : 0.f;
		return inputValue;
	}

	void process(const ProcessArgs &args) override
	{
		if (!needToStep())
			return;

		updateDiodeCharacteristics();

		//const float vhin = getLeveledPolarizedInputValue(INPUT_INPUT, INPUT_POLARITY_PARAM, INPUT_LEVEL_PARAM) * 0.5f;
		//const float vc = getLeveledPolarizedInputValue(CARRIER_INPUT, CARRIER_POLARITY_PARAM, CARRIER_LEVEL_PARAM) +
		//				 params[CARRIER_OFFSET_PARAM].getValue() + params[CARRIER_OFFSET_INPUT].getValue();
		const float vhin = getLeveledPolarizedInputValue(inputs[INPUT_INPUT].getVoltageSum(), 
														params[INPUT_POLARITY_PARAM].getValue(), 
														params[INPUT_LEVEL_PARAM].getValue()) * 0.5f;
		const float vc =   getLeveledPolarizedInputValue(inputs[CARRIER_INPUT].getVoltageSum(), 
														params[CARRIER_POLARITY_PARAM].getValue(), 
														params[CARRIER_LEVEL_PARAM].getValue()) +
						 								params[CARRIER_OFFSET_PARAM].getValue() + 
														inputs[CARRIER_OFFSET_INPUT].getVoltage();

		
		const float vc_plus_vhin = vc + vhin;
		const float vc_minus_vhin = vc - vhin;

		const float sum = m_diode(vc_plus_vhin);
		outputs[SUM_OUTPUT].setVoltage(sum);

		const float diff = m_diode(vc_minus_vhin);
		outputs[DIFF_OUTPUT].setVoltage(diff);

		outputs[MIN_OUTPUT].setVoltage(sum < diff ? sum : diff);
		outputs[MAX_OUTPUT].setVoltage(sum > diff ? sum : diff);

		const float out = sum - diff;
		outputs[RING_OUTPUT].setVoltage(out);
	}

private:
	Diode m_diode;
};

struct RingModulatorWidget : ModuleWidget
{
SvgPanel *goldPanel;
	SvgPanel *blackPanel;
	SvgPanel *whitePanel;

	struct panelStyleItem : MenuItem
	{
		RingModulator *module;
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
		RingModulator *module = dynamic_cast<RingModulator *>(this->module);

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		//assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		panelStyleItem *goldItem = new panelStyleItem();
		goldItem->text = "Black & Gold";
		goldItem->module = module;
		goldItem->theme = 0;
		menu->addChild(goldItem);

		panelStyleItem *blackItem = new panelStyleItem();
		blackItem->text = "Black & Orange";
		blackItem->module = module;
		blackItem->theme = 1;
		menu->addChild(blackItem);

		panelStyleItem *whiteItem = new panelStyleItem();
		whiteItem->text = "Black & White";
		whiteItem->module = module;
		whiteItem->theme = 2;
		menu->addChild(whiteItem);
	}

	RingModulatorWidget(RingModulator *module)
	{

		setModule(module);
		box.size = Vec(15 * 10, 380);

		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/CleanRingModulator.svg")));
		
		goldPanel = new SvgPanel();
		goldPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ringmodulator_gold.svg")));
		box.size = goldPanel->box.size;
		addChild(goldPanel);
		blackPanel = new SvgPanel();
		blackPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ringmodulator_black.svg")));
		blackPanel->visible = false;
		addChild(blackPanel);
		whitePanel = new SvgPanel();
		whitePanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ringmodulator_white.svg")));
		whitePanel->visible = false;
		addChild(whitePanel);

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
	    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
	    addChild(createWidget<ScrewSilver>(Vec(15, 365)));
	    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		float x = 27;
		float x2 = 75;
		float x3 = box.size.x - 28;
		float y = 75;
		float yOffset = 67;
		addInput(createInputCentered<sts_Port>(Vec(x, y), module, RingModulator::INPUT_INPUT));
		addParam(createParamCentered<sts_Davies_25_Grey>(Vec(x2, y), module, RingModulator::INPUT_LEVEL_PARAM));	//, 0.0, 1.0, 1.0));
		addParam(createParamCentered<CKSSThree>(Vec(x3, y), module, RingModulator::INPUT_POLARITY_PARAM)); //, 0.0, 2.0, 1.0));

		y += yOffset;
		addInput(createInputCentered<sts_Port>(Vec(x, y), module, RingModulator::CARRIER_INPUT));
		addParam(createParamCentered<sts_Davies_25_Grey>(Vec(x2, y), module, RingModulator::CARRIER_LEVEL_PARAM));	//, 0.0, 1.0, 1.0));
		addParam(createParamCentered<CKSSThree>(Vec(x3, y), module, RingModulator::CARRIER_POLARITY_PARAM)); //, 0.0, 2.0, 1.0));

		y += yOffset;
		addInput(createInputCentered<sts_Port>(Vec(x, y), module, RingModulator::CARRIER_OFFSET_INPUT));
		addParam(createParamCentered<sts_Davies_25_Grey>(Vec(x2, y), module, RingModulator::CARRIER_OFFSET_PARAM)); //, -g_controlPeakVoltage, g_controlPeakVoltage, 0.0));
		addOutput(createOutputCentered<sts_Port>(Vec(x3, y), module, RingModulator::RING_OUTPUT));

		y += yOffset;
		addParam(createParamCentered<sts_Davies_25_Grey>(Vec(x, y), module, RingModulator::DIODE_VB_PARAM)); //, std::numeric_limits<float>::epsilon(), g_controlPeakVoltage, 0.2));
		addParam(createParamCentered<sts_Davies_25_Grey>(Vec(x2, y), module, RingModulator::DIODE_VL_MINUS_VB_PARAM)); //, std::numeric_limits<float>::epsilon(), g_controlPeakVoltage, 0.5));
		addParam(createParamCentered<sts_Davies_25_Grey>(Vec(x3, y), module, RingModulator::DIODE_H_PARAM)); //, 0.0, 1.0, 0.9));
		
		y += yOffset;
		//x = 27;
		addOutput(createOutputCentered<sts_Port>(Vec(x, y), module, RingModulator::SUM_OUTPUT));
		addOutput(createOutputCentered<sts_Port>(Vec(x + 31.33, y), module, RingModulator::DIFF_OUTPUT));
		addOutput(createOutputCentered<sts_Port>(Vec(x3 - 31.33, y), module, RingModulator::MIN_OUTPUT));
		addOutput(createOutputCentered<sts_Port>(Vec(x3, y), module, RingModulator::MAX_OUTPUT));

	}
	
	void step() override
	{
		if (module)
		{
			goldPanel->visible = ((((RingModulator *)module)->panelStyle) == 0);
			blackPanel->visible = ((((RingModulator *)module)->panelStyle) == 1);
			whitePanel->visible = ((((RingModulator *)module)->panelStyle) == 2);
		}
		Widget::step();
	}
};

Model *modelRingModulator = createModel<RingModulator, RingModulatorWidget>("RingModulator");
