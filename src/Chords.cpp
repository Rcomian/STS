//============================================================================================================
//!
//! \file Chord-G1.cpp
//!
//! \brief Chord-G1 genearates chords via a CV program selection and a fundamental bass note V/octave input.
//!
//============================================================================================================

#include "rack.hpp"
#include "sts.hpp"
#include "dsp/digital.hpp"

using namespace std;
using namespace rack;

namespace GTX {
namespace Chords {

//============================================================================================================
//! \brief Some settings.

enum Spec
{
	E = 12,    // ET
	T = 25,    // ET
	N = GTX__N
};

//============================================================================================================
//! \brief The module.

//struct GtxModule : Module
struct Chords : Module
{
	enum ParamIds {
		PROG_PARAM = 0,
		NOTE_PARAM  = PROG_PARAM + 1,
		NUM_PARAMS  = NOTE_PARAM + T
	};
	enum InputIds {
		PROG_INPUT,    // 1
		GATE_INPUT,    // 1
		VOCT_INPUT,    // 1
		NUM_INPUTS,
		OFF_INPUTS = VOCT_INPUT
	};
	enum OutputIds {
		GATE_OUTPUT,   // N
		VOCT_OUTPUT,   // N
		NUM_OUTPUTS,
		OFF_OUTPUTS = GATE_OUTPUT
	};
	enum LightIds {
		PROG_LIGHT = 0,
		NOTE_LIGHT = PROG_LIGHT + 2*E,
		FUND_LIGHT = NOTE_LIGHT + 2*T,
		NUM_LIGHTS = FUND_LIGHT + E
	};

	struct Decode
	{
		/*static constexpr*/ float e = static_cast<float>(E);  // Static constexpr gives
		/*static constexpr*/ float s = 1.0f / e;               // link error on Mac build.

		float in    = 0;  //!< Raw input.
		float out   = 0;  //!< Input quantized.
		int   note  = 0;  //!< Integer note (offset midi note).
		int   key   = 0;  //!< C, C#, D, D#, etc.
		int   oct   = 0;  //!< Octave (C4 = 0).

		void step(float input)
		{
			int safe, fnote;

			in    = input;
			fnote = std::floor(in * e + 0.5f);
			out   = fnote * s;
			note  = static_cast<int>(fnote);
			safe  = note + (E * 1000);  // push away from negative numbers
			key   = safe % E;
			oct   = (safe / E) - 1000;
		}
	};

	Decode prg_prm;
	Decode prg_cv;
	Decode input;

	dsp::SchmittTrigger note_trigger[T];
	bool  note_enable[E][T] = {};
	float gen[N] = {0,1,2,3,4,5};
    float outGate[MAX_POLY_CHANNELS];
    float outVoct[MAX_POLY_CHANNELS];
    int channels;
	std::string fileDesc = "";
	int patchNum = 1;

	//--------------------------------------------------------------------------------------------------------
	//! \brief Constructor.

	Chords()
	{
		config(NUM_PARAMS, NUM_INPUTS, N * NUM_OUTPUTS, NUM_LIGHTS);
        configParam(PROG_PARAM, 1.0f, 12.0f, 1.0f);    //, 0.0f, 12.0f, 12.0f));
	}

	//--------------------------------------------------------------------------------------------------------
	//! \brief Save data.

	json_t *dataToJson() override
	{
		json_t *rootJ = json_object();

		if (json_t *neJA = json_array())
		{
			for (int i = 0; i < E; ++i)
			{
				for (int j = 0; j < T; ++j)
				{
					if (json_t *neJI = json_integer((int) note_enable[i][j]))
					{
						json_array_append_new(neJA, neJI);
					}
				}
			}
			json_object_set_new(rootJ, "note_enable", neJA);
		}
		return rootJ;
	}

	//--------------------------------------------------------------------------------------------------------
	//! \brief Load data.

	void dataFromJson(json_t *rootJ) override
	{
		// Note enable
		if (json_t *neJA = json_object_get(rootJ, "note_enable"))
		{
			for (int i = 0; i < E; ++i)
			{
				for (int j = 0; j < T; ++j)
				{
					if (json_t *neJI = json_array_get(neJA, i*T+j))
					{
						note_enable[i][j] = !!json_integer_value(neJI);
					}
				}
			}
		}
	}

	//--------------------------------------------------------------------------------------------------------
	//! \brief Output map.

	static constexpr int omap(int port, int bank)
	{
		return port + bank * NUM_OUTPUTS;
	}

	//--------------------------------------------------------------------------------------------------------
	//! \brief Step function.

	void process(const ProcessArgs &args) override
	{
		// Clear all lights

		float leds[NUM_LIGHTS] = {};

		// Decode inputs and params

		bool act_prm = false;

        patchNum = clamp((params[PROG_PARAM].getValue() + inputs[PROG_INPUT].getVoltage()),1.f,12.f);
		fileDesc = "";
		fileDesc = std::to_string(patchNum);

		//if (patchNum < 12.0f)
		//{
			prg_prm.step(patchNum / 12.0f);
			act_prm = true;
		//}

		//prg_cv.step(inputs[PROG_INPUT].getVoltage());
		input .step(inputs[VOCT_INPUT].getVoltage());

		float gate = clamp(inputs[GATE_INPUT].getNormalVoltage(10.0f), 0.0f, 10.0f);

		// Input leds

		if (act_prm)
		{
			leds[PROG_LIGHT + prg_prm.key*2] = 1.0f;  // Green
		}
		else
		{
			leds[PROG_LIGHT + prg_cv.key*2+1] = 1.0f;  // Red
		}

		leds[FUND_LIGHT + input.key] = 1.0f;  // Red

		// Chord bit

		if (act_prm)
		{
			// Detect buttons and deduce what's enabled

			for (int j = 0; j < T; ++j)
			{
				if (note_trigger[j].process(params[j + NOTE_PARAM].getValue()))
				{
					note_enable[prg_prm.key][j] = !note_enable[prg_prm.key][j];
				}
			}
            channels = 0;
			for (int j = 0; j < T; ++j)
			{
				if (note_enable[prg_prm.key][j])
				{
					++channels;
                    //if (b++ >= N)
					//{
					//	note_enable[prg_prm.key][j] = false;
					//}
				}
			}
		}

		// Based on what's enabled turn on leds

		if (act_prm)
		{
			for (int j = 0; j < T; ++j)
			{
				if (note_enable[prg_prm.key][j])
				{
					leds[NOTE_LIGHT + j*2] = 1.0f; // Green
				}
			}
		}
		else
		{
			for (int j = 0; j < T; ++j)
			{
				if (note_enable[prg_cv.key][j])
				{
					leds[NOTE_LIGHT + j*2+1] = 1.0f; // Red
				}
			}
		}

		// Based on what's enabled generate output

		{
			int i = 0;
            
			for (int j = 0; j < T; ++j)
			{
				if (note_enable[prg_cv.key][j])
				{
                    //outputs[omap(GATE_OUTPUT, i)].setVoltage(gate);
					//outputs[omap(VOCT_OUTPUT, i)].setVoltage(input.out + static_cast<float>(j) / 12.0f);
					outGate[i] = gate;
					outVoct[i] = (input.out + static_cast<float>(j) / 12.0f);
					++i;
				}
			}

			while (i < N)
			{
				outputs[omap(GATE_OUTPUT, i)].setVoltage(0.0f);
				outputs[omap(VOCT_OUTPUT, i)].setVoltage(0.0f);  // is this a good value?
				++i;
                
			}
		}

        outputs[GATE_OUTPUT].setChannels(channels);
        outputs[GATE_OUTPUT].writeVoltages(outGate);
        outputs[VOCT_OUTPUT].setChannels(channels);
        outputs[VOCT_OUTPUT].writeVoltages(outVoct);

		// Write output in one go, seems to prevent flicker

		for (int i=0; i<NUM_LIGHTS; ++i)
		{
			lights[i].value = leds[i];
		}
	}
};

struct MainDisplayWidget : TransparentWidget
{
    Chords *module;
    std::shared_ptr<Font> font;
	static const int displaySize = 3;
	char text[displaySize + 1];

	MainDisplayWidget() 
	{
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/saxmono.ttf"));
	}
    
	std::string removeExtension(std::string filename)
    {
        size_t lastdot = filename.find_last_of(".");
        if (lastdot == std::string::npos)
            return filename;
        return filename.substr(0, lastdot);
    }
	void draw(const DrawArgs &args) override
    {
		
        NVGcolor backgroundColor = nvgRGB(0x38, 0x38, 0x38);
        NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
		//NVGcolor textColor = nvgRGB(0xff, 0x00, 0xff);
        nvgBeginPath(args.vg);
        //nvgRoundedRect(args.vg, 0.0, 0.0, 30, 30, 5.0);
        nvgFillColor(args.vg, backgroundColor);
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 1.0);
        nvgStrokeColor(args.vg, borderColor);
        nvgStroke(args.vg);
        nvgFontSize(args.vg, 18);

        
        nvgFontFaceId(args.vg, font->handle);
        Vec textPos = Vec(3, 3);
        nvgFillColor(args.vg, nvgRGBA(0xc8, 0xab, 0x37, 0xff));
        //std::string empty = std::string(displaySize, '~');
        //nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
        //nvgFillColor(args.vg, nvgRGBA(0x28, 0xb0, 0xf3, 0xc0));

		//snprintf(text, displaySize, "%s", (module->fileDesc).c_str());
		//std::string fileDesc = "1";
		//nvgTextBox(args.vg, 5, 5, 10, fileDesc.c_str(), NULL);
        //nvgText(args.vg, textPos.x, textPos.y, text, NULL);
		nvgText(args.vg, textPos.x, textPos.y, module->fileDesc.c_str(), NULL);
    }
	
};
//============================================================================================================

static double x0(double shift = 0) { return 6+6*15 + shift * 66; }
static double xd(double i, double radius = 37.0, double spill = 1.65) { return (x0()    + (radius + spill * i) * dx(i, E)); }
static double yd(double i, double radius = 37.0, double spill = 1.65) { return (gy(1.5) + (radius + spill * i) * dy(i, E)); }


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(Chords *module)    // : ModuleWidget(module)
	{
		GTX__WIDGET();
		//box.size = Vec(18*15, 380);

		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/chords_gold.svg")));

		static const float x1 = 35;
        static const float x2 = 155;
        static const float portY = 79;
        static const float y1 = 30;

        if (module != NULL)
        {
            // Main display
            MainDisplayWidget *display = new MainDisplayWidget();
            display->module = module;
            display->box.pos = Vec(146, 81);
            //display->box.size = Vec(137, 23.5f); // x characters
            display->box.size = Vec(30, 30); // x characters
            addChild(display);
        }

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParamCentered<sts_Davies_snap_35_Grey>(Vec(97.5f, portY), module, Chords::PROG_PARAM));    //, 0.0f, 12.0f, 12.0f));
		addInput(createInputCentered<sts_Port>(Vec(x1, portY ), module, Chords::PROG_INPUT));

		addInput(createInputCentered<sts_Port>(Vec(x1, 350 - y1), module, Chords::VOCT_INPUT));
		addOutput(createOutputCentered<sts_Port>(Vec(x2 ,350 - y1), module, Chords::VOCT_OUTPUT));
		addInput(createInputCentered<sts_Port>(Vec(x1, 350), module, Chords::GATE_INPUT)); 
		addOutput(createOutputCentered<sts_Port>(Vec(x2, 350), module, Chords::GATE_OUTPUT));  
        
		for (int i=0; i<T; ++i)
		{
			addParam(createParam<LEDButton>(but(xd(i)+6, yd(i)-38), module, i + Chords::NOTE_PARAM));    //, 0.0f, 1.0f, 0.0f));
			addChild(createLight<MediumLight<GreenRedLight>>(l_m(xd(i)+6, yd(i)-38), module, Chords::NOTE_LIGHT + i*2));
		}
        /* 
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0() - 30, fy(-0.28) + 5), module, Chords::PROG_LIGHT +  0*2));  // C
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0() - 25, fy(-0.28) - 5), module, Chords::PROG_LIGHT +  1*2));  // C#
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0() - 20, fy(-0.28) + 5), module, Chords::PROG_LIGHT +  2*2));  // D
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0() - 15, fy(-0.28) - 5), module, Chords::PROG_LIGHT +  3*2));  // Eb
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0() - 10, fy(-0.28) + 5), module, Chords::PROG_LIGHT +  4*2));  // E
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0()     , fy(-0.28) + 5), module, Chords::PROG_LIGHT +  5*2));  // F
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0() +  5, fy(-0.28) - 5), module, Chords::PROG_LIGHT +  6*2));  // Fs
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0() + 10, fy(-0.28) + 5), module, Chords::PROG_LIGHT +  7*2));  // G
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0() + 15, fy(-0.28) - 5), module, Chords::PROG_LIGHT +  8*2));  // Ab
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0() + 20, fy(-0.28) + 5), module, Chords::PROG_LIGHT +  9*2));  // A
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0() + 25, fy(-0.28) - 5), module, Chords::PROG_LIGHT + 10*2));  // Bb
		addChild(createLight<SmallLight<GreenRedLight>>(l_s(x0() + 30, fy(-0.28) + 5), module, Chords::PROG_LIGHT + 11*2));  // B
        */
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0() - 30, fy(+0.28) + 5), module, Chords::FUND_LIGHT +  0));  // C
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0() - 25, fy(+0.28) - 5), module, Chords::FUND_LIGHT +  1));  // C#
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0() - 20, fy(+0.28) + 5), module, Chords::FUND_LIGHT +  2));  // D
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0() - 15, fy(+0.28) - 5), module, Chords::FUND_LIGHT +  3));  // Eb
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0() - 10, fy(+0.28) + 5), module, Chords::FUND_LIGHT +  4));  // E
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0()     , fy(+0.28) + 5), module, Chords::FUND_LIGHT +  5));  // F
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0() +  5, fy(+0.28) - 5), module, Chords::FUND_LIGHT +  6));  // Fs
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0() + 10, fy(+0.28) + 5), module, Chords::FUND_LIGHT +  7));  // G
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0() + 15, fy(+0.28) - 5), module, Chords::FUND_LIGHT +  8));  // Ab
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0() + 20, fy(+0.28) + 5), module, Chords::FUND_LIGHT +  9));  // A
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0() + 25, fy(+0.28) - 5), module, Chords::FUND_LIGHT + 10));  // Bb
		addChild(createLight<SmallLight<     RedLight>>(l_s(x0() + 30, fy(+0.28) + 5), module, Chords::FUND_LIGHT + 11));  // B
	}
};


//Model *model = createModel<GtxModule, GtxWidget>("Chords"); 
Model *model = createModel<Chords, GtxWidget>("Chords");
} // Chord_G1
} // GTX
