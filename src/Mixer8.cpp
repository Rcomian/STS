#include "sts.hpp"


struct Mixer8 : Module {
	enum ParamIds {
		MIX_LVL_PARAM,
		ENUMS(LVL_PARAM, 8),
		NUM_PARAMS
	};
	enum InputIds {
		MIX_CV_INPUT,
		ENUMS(CH_INPUT, 8),
		ENUMS(CV_INPUT, 8),
		NUM_INPUTS
	};
	enum OutputIds {
		MIX_OUTPUT,
		ENUMS(CH_OUTPUT, 8),
		NUM_OUTPUTS
	};

	//int MAX_POLY_CHANNELS = 16;
	float mix[MAX_POLY_CHANNELS] = {};
	float in1[MAX_POLY_CHANNELS];
	float in2[MAX_POLY_CHANNELS];
	float in3[MAX_POLY_CHANNELS];
	float in4[MAX_POLY_CHANNELS];
	float in5[MAX_POLY_CHANNELS];
	float in6[MAX_POLY_CHANNELS];
	float in7[MAX_POLY_CHANNELS];
	float in8[MAX_POLY_CHANNELS];
	float cv[MAX_POLY_CHANNELS];
	int chanels = 16;

	Mixer8() 
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
		// x^1 scaling up to 6 dB
		configParam(MIX_LVL_PARAM, 0.0, 2.0, 1.0, "Master level", " dB", -10, 20);
		// x^2 scaling up to 6 dB
		configParam(LVL_PARAM + 0, 0.0, M_SQRT2, 1.0, "Ch 1 level", " dB", -10, 40);
		configParam(LVL_PARAM + 1, 0.0, M_SQRT2, 1.0, "Ch 2 level", " dB", -10, 40);
		configParam(LVL_PARAM + 2, 0.0, M_SQRT2, 1.0, "Ch 3 level", " dB", -10, 40);
		configParam(LVL_PARAM + 3, 0.0, M_SQRT2, 1.0, "Ch 4 level", " dB", -10, 40);
		configParam(LVL_PARAM + 4, 0.0, M_SQRT2, 1.0, "Ch 5 level", " dB", -10, 40);
		configParam(LVL_PARAM + 5, 0.0, M_SQRT2, 1.0, "Ch 6 level", " dB", -10, 40);
		configParam(LVL_PARAM + 6, 0.0, M_SQRT2, 1.0, "Ch 7 level", " dB", -10, 40);
		configParam(LVL_PARAM + 7, 0.0, M_SQRT2, 1.0, "Ch 8 level", " dB", -10, 40);
	}

	void process(const ProcessArgs &args) override 
	{
		//int MAX_POLY_CHANNELS = 16;
		float mix[MAX_POLY_CHANNELS] = {};
		float in1[MAX_POLY_CHANNELS];
		float in2[MAX_POLY_CHANNELS];
		float in3[MAX_POLY_CHANNELS];
		float in4[MAX_POLY_CHANNELS];
		float in5[MAX_POLY_CHANNELS];
		float in6[MAX_POLY_CHANNELS];
		float in7[MAX_POLY_CHANNELS];
		float in8[MAX_POLY_CHANNELS];
		int channels = 16;		

		for (int i = 0; i < 8; i++) 
		{
			for (int c = 0; c < 16; c++)
			{
				// Skip channel if not patched
				if (!inputs[CH_INPUT + i].isConnected())
					continue;

				float in[16] = {};
				//int channels = inputs[CH_INPUT + i].getChannels();
				
				//MAX_POLY_CHANNELS = std::max(MAX_POLY_CHANNELS, channels);

				// Get input
				in1[c] = inputs[CH_INPUT + i].getPolyVoltage(i);
				in2[c] = inputs[CH_INPUT + i].getPolyVoltage(i);
				in3[c] = inputs[CH_INPUT + i].getPolyVoltage(i);
				in4[c] = inputs[CH_INPUT + i].getPolyVoltage(i);
				in5[c] = inputs[CH_INPUT + i].getPolyVoltage(i);
				in6[c] = inputs[CH_INPUT + i].getPolyVoltage(i);
				in7[c] = inputs[CH_INPUT + i].getPolyVoltage(i);
				in8[c] = inputs[CH_INPUT + i].getPolyVoltage(i);
				
				//inputs[CH_INPUT + i].getVoltages(in);

				// Apply fader gain
				float gain = std::pow(params[LVL_PARAM + i].getValue(), 2.f);
				for (int c = 0; c < channels; c++) {
					in1[c] *= gain;
					in2[c] *= gain;
					in3[c] *= gain;
					in4[c] *= gain;
					in5[c] *= gain;
					in6[c] *= gain;
					in7[c] *= gain;
					in8[c] *= gain;
				
				}

				// Apply CV gain
				if (inputs[CV_INPUT + i].isConnected()) {
					for (int c = 0; c < channels; c++) {
						//cv = clamp(inputs[CV_INPUT + i].getPolyVoltage(c) / 10.f, 0.f, 1.f);
						in1[c] *= clamp(inputs[CV_INPUT + i].getPolyVoltage(c) / 10.f, 0.f, 1.f);
						in2[c] *= clamp(inputs[CV_INPUT + i].getPolyVoltage(c) / 10.f, 0.f, 1.f);
						in3[c] *= clamp(inputs[CV_INPUT + i].getPolyVoltage(c) / 10.f, 0.f, 1.f);
						in4[c] *= clamp(inputs[CV_INPUT + i].getPolyVoltage(c) / 10.f, 0.f, 1.f);
						in5[c] *= clamp(inputs[CV_INPUT + i].getPolyVoltage(c) / 10.f, 0.f, 1.f);
						in6[c] *= clamp(inputs[CV_INPUT + i].getPolyVoltage(c) / 10.f, 0.f, 1.f);
						in7[c] *= clamp(inputs[CV_INPUT + i].getPolyVoltage(c) / 10.f, 0.f, 1.f);
						in8[c] *= clamp(inputs[CV_INPUT + i].getPolyVoltage(c) / 10.f, 0.f, 1.f);
					
					}
				}

				// Set channel output
				//if (outputs[CH_OUTPUT + i].isConnected()) 
				//{
				//	outputs[CH_OUTPUT + i].setChannels(channels);
				//	outputs[CH_OUTPUT + i].writeVoltages(in);
				//}

				// Add to mix
				for (int c = 0; c < channels; c++) 
				{
					//////  FIX ///mix[c] = in1(c) + in2(c) + in3(c) ;
				}
			}
		}

		

		if (outputs[MIX_OUTPUT].isConnected()) 
		{
			// Apply mix knob gain
			float gain = params[MIX_LVL_PARAM].getValue();
			for (int c = 0; c < MAX_POLY_CHANNELS; c++) 
			{
				mix[c] *= gain;
			}

			// Apply mix CV gain
			if (inputs[MIX_CV_INPUT].isConnected()) 
			{
				for (int c = 0; c < MAX_POLY_CHANNELS; c++) 
				{
					float cv = clamp(inputs[MIX_CV_INPUT].getPolyVoltage(c) / 10.f, 0.f, 1.f);
					mix[c] *= cv;
				}
			}

			// Set mix output
			outputs[MIX_OUTPUT].setChannels(MAX_POLY_CHANNELS);
			outputs[MIX_OUTPUT].writeVoltages(mix);
		}
	}
};


struct Mixer8Widget : ModuleWidget {
	Mixer8Widget(Mixer8 *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/mixer8.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<RoundLargeBlackKnob>(mm2px(Vec(19.049999, 21.161154)), module, Mixer8::MIX_LVL_PARAM));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(5.8993969, 44.33149).plus(Vec(-2, 0))), module, Mixer8::LVL_PARAM + 0));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(17.899343, 44.331486).plus(Vec(-2, 0))), module, Mixer8::LVL_PARAM + 1));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(29.899292, 44.331486).plus(Vec(-2, 0))), module, Mixer8::LVL_PARAM + 2));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(41.90065, 44.331486).plus(Vec(-2, 0))), module, Mixer8::LVL_PARAM + 3));
        addParam(createParam<LEDSliderGreen>(mm2px(Vec(53.8993969, 44.33149).plus(Vec(-2, 0))), module, Mixer8::LVL_PARAM + 4));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(65.899343, 44.331486).plus(Vec(-2, 0))), module, Mixer8::LVL_PARAM + 5));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(77.899292, 44.331486).plus(Vec(-2, 0))), module, Mixer8::LVL_PARAM + 6));
		addParam(createParam<LEDSliderGreen>(mm2px(Vec(89.90065, 44.331486).plus(Vec(-2, 0))), module, Mixer8::LVL_PARAM + 7));

		// Use old interleaved order for backward compatibility with <0.6
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.2935331, 23.404598)), module, Mixer8::MIX_CV_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.2935331, 78.531639)), module, Mixer8::CH_INPUT + 0));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.2935331, 93.531586)), module, Mixer8::CV_INPUT + 0));
		addInput(createInput<PJ301MPort>(mm2px(Vec(15.29348, 78.531639)), module, Mixer8::CH_INPUT + 1));
		addInput(createInput<PJ301MPort>(mm2px(Vec(15.29348, 93.531586)), module, Mixer8::CV_INPUT + 1));
		addInput(createInput<PJ301MPort>(mm2px(Vec(27.293465, 78.531639)), module, Mixer8::CH_INPUT + 2));
		addInput(createInput<PJ301MPort>(mm2px(Vec(27.293465, 93.531586)), module, Mixer8::CV_INPUT + 2));
		addInput(createInput<PJ301MPort>(mm2px(Vec(39.293411, 78.531639)), module, Mixer8::CH_INPUT + 3));
		addInput(createInput<PJ301MPort>(mm2px(Vec(39.293411, 93.531586)), module, Mixer8::CV_INPUT + 3));

		addInput(createInput<PJ301MPort>(mm2px(Vec(48+3.2935331, 78.531639)), module, Mixer8::CH_INPUT + 4));
		addInput(createInput<PJ301MPort>(mm2px(Vec(48+3.2935331, 93.531586)), module, Mixer8::CV_INPUT + 4));
		addInput(createInput<PJ301MPort>(mm2px(Vec(48+15.29348, 78.531639)), module, Mixer8::CH_INPUT + 5));
		addInput(createInput<PJ301MPort>(mm2px(Vec(48+15.29348, 93.531586)), module, Mixer8::CV_INPUT + 5));
		addInput(createInput<PJ301MPort>(mm2px(Vec(48+27.293465, 78.531639)), module, Mixer8::CH_INPUT + 6));
		addInput(createInput<PJ301MPort>(mm2px(Vec(48+27.293465, 93.531586)), module, Mixer8::CV_INPUT + 6));
		addInput(createInput<PJ301MPort>(mm2px(Vec(48+39.293411, 78.531639)), module, Mixer8::CH_INPUT + 7));
		addInput(createInput<PJ301MPort>(mm2px(Vec(48+39.293411, 93.531586)), module, Mixer8::CV_INPUT + 7));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(39.293411, 23.4046)), module, Mixer8::MIX_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.2935331, 108.53153)), module, Mixer8::CH_OUTPUT + 0));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(15.29348, 108.53153)), module, Mixer8::CH_OUTPUT + 1));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(27.293465, 108.53153)), module, Mixer8::CH_OUTPUT + 2));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(39.293411, 108.53153)), module, Mixer8::CH_OUTPUT + 3));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(48+3.2935331, 108.53153)), module, Mixer8::CH_OUTPUT + 4));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(48+15.29348, 108.53153)), module, Mixer8::CH_OUTPUT + 5));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(48+27.293465, 108.53153)), module, Mixer8::CH_OUTPUT + 6));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(48+39.293411, 108.53153)), module, Mixer8::CH_OUTPUT + 7));
		
	}
};


Model *modelMixer8 = createModel<Mixer8, Mixer8Widget>("Mixer8");
