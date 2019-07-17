#include "sts.hpp"
#include "rack.hpp"
#include <common/meta.hpp>
#include <ext/LambertW/LambertW.h>

// STL
#include <cmath>
#include <iostream>

/*
http://smc2017.aalto.fi/media/materials/proceedings/SMC17_p336.pdf
Original VCV Rack Module code by J Eres
*/

using namespace std;
using namespace rack;

struct WaveFolder : Module
{
	enum ParamIds
	{
		INPUT_GAIN_PARAM = 0,
		DC_OFFSET_PARAM,

		OUTPUT_GAIN_PARAM,

		RESISTOR_PARAM,
		LOAD_RESISTOR_PARAM,

		NUM_PARAMS
	};

	enum InputIds
	{
		INPUT_INPUT = 0,
		INPUT_GAIN_CV,
		DC_OFFSET_CV,
		RESISTOR_CV,
		LOAD_RESISTOR_CV,
		OUTPUT_GAIN_CV,

		NUM_INPUTS
	};

	enum OutputIds
	{
		OUTPUT_OUTPUT = 0,

		NUM_OUTPUTS
	};

	enum LightIds
	{
		NUM_LIGHTS = 0
	};

	int panelStyle = 1;

	WaveFolder()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(INPUT_GAIN_PARAM, 0.0, 1.0, 0.1);
		configParam(DC_OFFSET_PARAM, -5.0, 5.0, 0.0);
		configParam(OUTPUT_GAIN_PARAM, 0.0, 10.0, 1.0);
		configParam(RESISTOR_PARAM, 10000.f, 100000.f, 15000.f);
		configParam(LOAD_RESISTOR_PARAM, 1000.f, 10000.f, 7500.f);
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
		if (!outputs[OUTPUT_OUTPUT].isConnected())
			return false;

		if (!inputs[INPUT_INPUT].isConnected())
		{
			outputs[OUTPUT_OUTPUT].setVoltage(0.f);
			return false;
		}

		return true;
	}

	inline void updateResistors()
	{
		if (meta::updateIfDifferent(m_resistor, (params[RESISTOR_PARAM].getValue() + inputs[RESISTOR_CV].getVoltage()*1000) ))
		{
			m_alpha = m_loadResistor2 / m_resistor;
			m_beta = (m_resistor + m_loadResistor2) / (m_thermalVoltage * m_resistor);
		}

		if (meta::updateIfDifferent(m_loadResistor, (params[LOAD_RESISTOR_PARAM].getValue() + inputs[LOAD_RESISTOR_CV].getVoltage()*100) ))
		{
			m_loadResistor2 = m_loadResistor * 2.f;
			m_alpha = m_loadResistor2 / m_resistor;
			m_beta = (m_resistor + m_loadResistor2) / (m_thermalVoltage * m_resistor);
			m_delta = (m_loadResistor * m_saturationCurrent) / m_thermalVoltage;
		}
	}

	inline float getGainedOffsetedInputValue()
	{
		return (inputs[INPUT_INPUT].getVoltageSum() * meta::clamp(params[INPUT_GAIN_PARAM].getValue() + inputs[INPUT_GAIN_CV].getVoltage() / g_audioPeakVoltage, 0.f, 1.f)) + (params[DC_OFFSET_PARAM].getValue() + inputs[DC_OFFSET_CV].getVoltage()) / 2.f;
	}

	inline float waveFolder(float in)
	{
		const float theta = (in >= 0.f) ? 1.f : -1.f;
		return theta * m_thermalVoltage * utl::LambertW<0>(m_delta * meta::exp(theta * m_beta * in)) - m_alpha * in;
	}

	inline void process(const ProcessArgs &args) override
	{
		if (!needToStep())
			return;

		updateResistors();

		outputs[OUTPUT_OUTPUT].setVoltage(std::tanh(waveFolder(getGainedOffsetedInputValue())) * meta::clamp(params[OUTPUT_GAIN_PARAM].getValue() + inputs[OUTPUT_GAIN_CV].getVoltage(), 0.f, 10.f));
	}

private:
	const float m_thermalVoltage = 0.026f;
	const float m_saturationCurrent = 10e-17f;
	float m_resistor = 15000.f;
	float m_loadResistor = 7500.f;
	float m_loadResistor2 = m_loadResistor * 2.f;
	// Derived values
	float m_alpha = m_loadResistor2 / m_resistor;
	float m_beta = (m_resistor + m_loadResistor2) / (m_thermalVoltage * m_resistor);
	float m_delta = (m_loadResistor * m_saturationCurrent) / m_thermalVoltage;
};

struct WaveFolderWidget : ModuleWidget
{
	SvgPanel *goldPanel;
	SvgPanel *blackPanel;
	SvgPanel *whitePanel;

	struct panelStyleItem : MenuItem
	{
		WaveFolder *module;
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
		WaveFolder *module = dynamic_cast<WaveFolder *>(this->module);

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
	WaveFolderWidget(WaveFolder *module)
	{
		setModule(module);
		box.size = rack::Vec(15 * 7, 380);

		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WaveFolder.svg")));

		goldPanel = new SvgPanel();
		goldPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/wavefolder_gold.svg")));
		box.size = goldPanel->box.size;
		addChild(goldPanel);
		blackPanel = new SvgPanel();
		blackPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/wavefolder_black.svg")));
		blackPanel->visible = false;
		addChild(blackPanel);
		whitePanel = new SvgPanel();
		whitePanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/wavefolder_white.svg")));
		whitePanel->visible = false;
		addChild(whitePanel);

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		float x = 27;
		float x2 = x + 50;
		float y = 70;
		float yOffset = 52;

		addInput(createInputCentered<sts_Port>(rack::Vec(x, y), module, WaveFolder::INPUT_GAIN_CV));
		addParam(createParamCentered<sts_Davies_25_Grey>(rack::Vec(x2, y), module, WaveFolder::INPUT_GAIN_PARAM)); //, 0.0, 1.0, 0.1));

		y += yOffset;
		addInput(createInputCentered<sts_Port>(rack::Vec(x, y), module, WaveFolder::DC_OFFSET_CV));
		addParam(createParamCentered<sts_Davies_25_Grey>(rack::Vec(x2, y), module, WaveFolder::DC_OFFSET_PARAM)); //, -5.0, 5.0, 0.0));

		y += yOffset;
		addInput(createInputCentered<sts_Port>(rack::Vec(x, y), module, WaveFolder::OUTPUT_GAIN_CV));
		addParam(createParamCentered<sts_Davies_25_Grey>(rack::Vec(x2, y), module, WaveFolder::OUTPUT_GAIN_PARAM)); //, 0.0, 10.0, 1.0));

		y += yOffset;
		addInput(createInputCentered<sts_Port>(rack::Vec(x, y), module, WaveFolder::RESISTOR_CV));
		addParam(createParamCentered<sts_Davies_25_Grey>(rack::Vec(x2, y), module, WaveFolder::RESISTOR_PARAM)); //, 10000.f, 100000.f, 15000.f));

		y += yOffset;
		addInput(createInputCentered<sts_Port>(rack::Vec(x, y), module, WaveFolder::LOAD_RESISTOR_CV));
		addParam(createParamCentered<sts_Davies_25_Grey>(rack::Vec(x2, y), module, WaveFolder::LOAD_RESISTOR_PARAM)); //, 1000.f, 10000.f, 7500.f));

		y += yOffset;
		addInput(createInputCentered<sts_Port>(rack::Vec(x, y), module, WaveFolder::INPUT_INPUT));
		addOutput(createOutputCentered<sts_Port>(rack::Vec(x2, y), module, WaveFolder::OUTPUT_OUTPUT));
	}

	void step() override
	{
		if (module)
		{
			goldPanel->visible = ((((WaveFolder *)module)->panelStyle) == 0);
			blackPanel->visible = ((((WaveFolder *)module)->panelStyle) == 1);
			whitePanel->visible = ((((WaveFolder *)module)->panelStyle) == 2);
		}
		Widget::step();
	}
};

Model *modelWaveFolder = createModel<WaveFolder, WaveFolderWidget>("WaveFolder");
