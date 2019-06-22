#include "sts.hpp"

using namespace std;
using namespace rack;

using simd::float_4;


template <typename T>
struct LowFrequencyOscillator {
	T phase = 0.f;
	T pw = 0.5f;
	T freq = 1.f;
	bool invert = false;
	bool bipolar = false;
	T resetState = T::mask();

	void setPitch(T pitch) {
		pitch = simd::fmin(pitch, 10.f);
		freq = simd::pow(2.f, pitch);
	}
	void setPulseWidth(T pw) {
		const T pwMin = 0.01f;
		this->pw = clamp(pw, pwMin, 1.f - pwMin);
	}
	void setReset(T reset) {
		reset = simd::rescale(reset, 0.1f, 2.f, 0.f, 1.f);
		T on = (reset >= 1.f);
		T off = (reset <= 0.f);
		T triggered = ~resetState & on;
		resetState = simd::ifelse(off, 0.f, resetState);
		resetState = simd::ifelse(on, T::mask(), resetState);
		phase = simd::ifelse(triggered, 0.f, phase);
	}
	void step(float dt) {
		T deltaPhase = simd::fmin(freq * dt, 0.5f);
		phase += deltaPhase;
		phase -= (phase >= 1.f) & 1.f;
	}
	T sin() {
		T p = phase;
		if (!bipolar) p -= 0.25f;
		T v = simd::sin(2*M_PI * p);
		if (invert) v *= -1.f;
		if (!bipolar) v += 1.f;
		return v;
	}
	T tri() {
		T p = phase;
		if (bipolar) p += 0.25f;
		T v = 4.f * simd::fabs(p - simd::round(p)) - 1.f;
		if (invert) v *= -1.f;
		if (!bipolar) v += 1.f;
		return v;
	}
	T saw() {
		T p = phase;
		if (!bipolar) p -= 0.5f;
		T v = 2.f * (p - simd::round(p));
		if (invert) v *= -1.f;
		if (!bipolar) v += 1.f;
		return v;
	}
	T sqr() {
		T v = simd::ifelse(phase < pw, 1.f, -1.f);
		if (invert) v *= -1.f;
		if (!bipolar) v += 1.f;
		return v;
	}
	T light() {
		return simd::sin(2 * T(M_PI) * phase);
	}
};

struct LFOPoly : Module {
	enum ParamIds {
		OFFSET_PARAM,
        ENUMS(FREQ_TRIM, 16),
        ENUMS(FREQ_PARAM, 16),
        ENUMS(WAVE_TRIM, 16),
        ENUMS(WAVE_PARAM, 16),
        ENUMS(PHASE_TRIM, 16),
        ENUMS(PW_PARAM,16),
        PHASE_PARAM,

		NUM_PARAMS
	};
	enum InputIds {
		FREQ_INPUT,
        WAVE_INPUT,
        PW_INPUT,
        RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		//ENUMS(PHASE_LIGHT, 3),
        //ENUMS(PHASE_LIGHT1, 3),
        //ENUMS(PHASE_POS_LIGHT, 16),
        //ENUMS(PHASE_NEG_LIGHT, 16),
        ENUMS(PHASE_LIGHTS, 16 * 2),
		NUM_LIGHTS
	};

	LowFrequencyOscillator<float_4> oscillators[4];
	dsp::ClockDivider lightDivider;

    int panelStyle = 2;
    int channels = 1;
    float waveParam[16];
    float freqParam[16];
    float pwParam[16];
    float fmParam = 0;
    float_4 pitch;
    float_4 wave;
    float_4 pw;
    

	LFOPoly() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < 16; ++i)
        {
            configParam(OFFSET_PARAM, 0.f, 1.f, 1.f);
            configParam(PHASE_PARAM, 0.f, 1.f, 1.f);

            configParam(FREQ_PARAM + i, -8.f, 10.f, 1.f,"LFO Frequency ");
            configParam(WAVE_PARAM + i, 0.f, 3.f, 0.f,"LFO Waveshape ");
            configParam(PW_PARAM + i, 0.f, 1.f, 0.5f,"LFO Pulse Width ",  "%", 0.f, 100.f);

            //configParam(FREQ_TRIM + i, 0.f, 1.f, 0.5f);
            //configParam(WAVE_TRIM + i, 0.f, 1.f, 0.5f);
            //configParam(PHASE_TRIM + i, 0.f, 1.f, 1.f);
        }
		lightDivider.setDivision(16);
	}

	json_t *dataToJson() override
    {
        json_t *rootJ = json_object();

        json_object_set_new(rootJ, "panelStyle", json_integer(panelStyle));
        json_object_set_new(rootJ, "channels", json_integer(channels));

        return rootJ;
    }

    void dataFromJson(json_t *rootJ) override
    {
        json_t *panelStyleJ = json_object_get(rootJ, "panelStyle");
        if (panelStyleJ)
            panelStyle = json_integer_value(panelStyleJ);

        json_t *channelsJ = json_object_get(rootJ, "channels");
        if (channelsJ)
            channels = json_integer_value(channelsJ);
    }	

	void process(const ProcessArgs &args) override 
    {
        if (inputs[FREQ_INPUT].isConnected()) 
            {
			    channels = std::max(1, inputs[FREQ_INPUT].getChannels());
            }
            else
            {
                channels = std::max(1, channels);
            }
        
        for (int i = 0; i < channels; ++i)
			{
		        waveParam[i] = params[WAVE_PARAM + i].getValue();    //&AUDIO_MIX[c]
                freqParam[i] = params[FREQ_PARAM + i].getValue();
                pwParam[i]= params[PW_PARAM + i].getValue();
            }

		for (int c = 0; c < channels; c += 4) 
        {
			auto *oscillator = &oscillators[c / 4];
			oscillator->invert = (params[PHASE_PARAM].getValue() == 0.f);
			oscillator->bipolar = (params[OFFSET_PARAM].getValue() == 0.f);  
			
            //oscillator->setOffset(float_4::load(&offsetParam[c]));
            //oscillator->bipolar = float_4::load(&offsetParam[c] );
            

            //float_4 pitch = freqParam + inputs[FM_INPUT].getVoltageSimd<float_4>(c) * fmParam;
			if (inputs[FREQ_INPUT].isConnected()) 
                {
                    pitch = float_4::load(&freqParam[c]);    //* fmParam;
                    pitch += inputs[FREQ_INPUT].getVoltageSimd<float_4>(c);    //* fmParam;
                }            
                else
                {
                    pitch = float_4::load(&freqParam[c]);     // float_4::load(&AUDIO_MIX[c])
                }
	        oscillator->setPitch(pitch);
            // Pulse width
            if (inputs[PW_INPUT].isConnected()) 
                {
                    pw = float_4::load(&pwParam[c]);    //* fmParam;
                    pw += inputs[PW_INPUT].getVoltageSimd<float_4>(c) /10;    //* fmParam;
                }            
                else
                {
                    pw = float_4::load(&pwParam[c]);     // float_4::load(&AUDIO_MIX[c])
                }
			//float_4 pw = float_4::load(&pwParam[c] /10);    // * pwmParam;
			oscillator->setPulseWidth(pw);
            
			oscillator->step(args.sampleTime);
			oscillator->setReset(inputs[RESET_INPUT].getPolyVoltageSimd<float_4>(c));

			// Outputs
			//if (outputs[POLY_OUTPUT].isConnected()) 
            //{
				if (inputs[WAVE_INPUT].isConnected()) 
                {
                    //wave = float_4::load(&waveParam[c]); 
                    wave = clamp(inputs[WAVE_INPUT + c ].getPolyVoltageSimd<float_4>(c) / 3.f, 0.f, 3.f);
                }
                else
                {
                    wave = float_4::load(&waveParam[c]);
                }
                
                float_4 v = 0.f;
				v += oscillator->sin() * simd::fmax(0.f, 1.f - simd::fabs(wave - 0.f));
				v += oscillator->tri() * simd::fmax(0.f, 1.f - simd::fabs(wave - 1.f));
				v += oscillator->saw() * simd::fmax(0.f, 1.f - simd::fabs(wave - 2.f));
				v += oscillator->sqr() * simd::fmax(0.f, 1.f - simd::fabs(wave - 3.f));
				outputs[POLY_OUTPUT].setVoltageSimd(5.f * v, c);
			//}
		}

		outputs[POLY_OUTPUT].setChannels(channels);

		
		if (lightDivider.process()) 
        {
			//lastChannel = inputs[POLY_INPUT].getChannels();
			float deltaTime = args.sampleTime * lightDivider.getDivision();

			for (int c = 0; c < 16; c++) 
            {
                if(c < channels)
                {
                    float v = outputs[POLY_OUTPUT].getVoltage(c) / 10.f;
                    lights[PHASE_LIGHTS + c * 2 + 0].setSmoothBrightness(v, deltaTime);
                    lights[PHASE_LIGHTS + c * 2 + 1].setSmoothBrightness(-v, deltaTime);
                }
                else
                {
                    lights[PHASE_LIGHTS + c * 2 + 0].setSmoothBrightness(0.f, deltaTime);
				    lights[PHASE_LIGHTS + c * 2 + 1].setSmoothBrightness(0.f, deltaTime);   
                }
                
			}
		}
	};


/** Resets performance state */
    void panic()
    {
        //pedal = false;
        for (int c = 0; c < 16; c++)
        {
            //notes[c] = 60;
            //gates[c] = false;
            //velocities[c] = 0;
            //aftertouches[c] = 0;
            //pitches[c] = 8192;
            //mods[c] = 0;
            //pitchFilters[c].reset();
            //modFilters[c].reset();
        }
        //pedal = false;
        //rotateIndex = -1;
        //heldNotes.clear();
    }

    void setChannels(int channels)
    {
        if (channels == this->channels)
            return;
        this->channels = channels;
        panic();
    }
};

struct ChannelValueItem : MenuItem
{
    LFOPoly *module;
    int channels;
    void onAction(const event::Action &e) override
    {
        module->setChannels(channels);
    }
};

struct ChannelItem : MenuItem
{
    LFOPoly *module;
    Menu *createChildMenu() override
    {
        Menu *menu = new Menu;
        for (int channels = 1; channels <= 16; channels++)
        {
            ChannelValueItem *item = new ChannelValueItem;
            if (channels == 1)
                item->text = "Monophonic";
            else
                item->text = string::f("%d", channels);
            item->rightText = CHECKMARK(module->channels == channels);
            item->module = module;
            item->channels = channels;
            menu->addChild(item);
        }
        return menu;
    }
};

struct LFOPolyWidget : ModuleWidget 
{
	SvgPanel *goldPanel;
    SvgPanel *blackPanel;
    SvgPanel *whitePanel;
    
	struct panelStyleItem : MenuItem
    {
        LFOPoly *module;
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
        LFOPoly *module = dynamic_cast<LFOPoly *>(this->module);

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

        ChannelItem *channelItem = new ChannelItem;
        channelItem->text = "Polyphony channels";
        channelItem->rightText = string::f("%d", module->channels) + " " + RIGHT_ARROW;
        channelItem->module = module;
        menu->addChild(channelItem);
    }
    
	LFOPolyWidget(LFOPoly *module) 
    {
		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/lfopoly_gold.svg")));
         
		goldPanel = new SvgPanel();
        goldPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/lfopoly_gold.svg")));
        box.size = goldPanel->box.size;
        addChild(goldPanel);
        blackPanel = new SvgPanel();
        blackPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/lfopoly_black.svg")));
        blackPanel->visible = false;
        addChild(blackPanel);
        whitePanel = new SvgPanel();
        whitePanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/lfopoly_white.svg")));
        whitePanel->visible = false;
        addChild(whitePanel);
		
		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		//static const float portx[16] = {35, 54, 82, 120, 158};
        static const float portx[16] = {40, 54, 71, 112, 151};
        static const float x2 = 145;
        static const float portY = 70;
        static const float y1 = 38;
        
        for (int i = 0; i < 8; ++i)
        {
            addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(portY + i * y1, portx[0]), module, LFOPoly::PHASE_LIGHTS + i *2));
            //addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(Vec(portY + i * y1, portx[0]), module, LFOPoly::PHASE_LIGHT));
            addParam(createParamCentered<sts_Davies30_Red>(Vec(portY + i * y1, portx[2]), module, LFOPoly::FREQ_PARAM + i));
            addParam(createParamCentered<sts_Davies30_Blue>(Vec(portY + i * y1, portx[3]), module, LFOPoly::WAVE_PARAM + i));
            addParam(createParamCentered<sts_Davies30_Teal>(Vec(portY + i * y1, portx[4]), module, LFOPoly::PW_PARAM + i));
        }
        for (int i = 8; i < 16; ++i)
        {
            addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(portY + (i - 8) * y1, portx[0] + x2), module, LFOPoly::PHASE_LIGHTS + i *2));
            addParam(createParamCentered<sts_Davies30_Red>(Vec(portY + (i - 8) * y1, portx[2] + x2), module, LFOPoly::FREQ_PARAM + i));
            addParam(createParamCentered<sts_Davies30_Blue>(Vec(portY + (i - 8) * y1, portx[3] + x2), module, LFOPoly::WAVE_PARAM + i));
            addParam(createParamCentered<sts_Davies30_Teal>(Vec(portY + (i - 8) * y1, portx[4] + x2), module, LFOPoly::PW_PARAM + i));
        }
        
        addInput(createInputCentered<sts_Port>(Vec(portY, 350), module, LFOPoly::RESET_INPUT));
        addInput(createInputCentered<sts_Port>(Vec(portY + y1, 350), module, LFOPoly::FREQ_INPUT));
        addInput(createInputCentered<sts_Port>(Vec(portY + y1 * 2, 350), module, LFOPoly::WAVE_INPUT));
        addInput(createInputCentered<sts_Port>(Vec(portY + y1 * 3, 350), module, LFOPoly::PW_INPUT));
        addParam(createParamCentered<sts_CKSS>(Vec(portY + y1 * 4, 350), module, LFOPoly::PHASE_PARAM));
        addParam(createParamCentered<sts_CKSS>(Vec(portY + y1 * 5, 350), module, LFOPoly::OFFSET_PARAM));
        addOutput(createOutputCentered<sts_Port>(Vec(portY + y1 * 7, 350), module, LFOPoly::POLY_OUTPUT));
	}

    void step() override
	{
		if (module)
		{
			goldPanel->visible = ((((LFOPoly *)module)->panelStyle) == 0);
			blackPanel->visible = ((((LFOPoly *)module)->panelStyle) == 1);
			whitePanel->visible = ((((LFOPoly *)module)->panelStyle) == 2);
		}
		Widget::step();
	}
};


Model *modelLFOPoly = createModel<LFOPoly, LFOPolyWidget>("LFOPoly");
