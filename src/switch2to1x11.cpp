#include "STS.hpp"

struct switch2to1x11 : Module 
{
	enum ParamIds 
	{
        NUM_PARAMS
	};
	enum InputIds 
	{
        INA1_INPUT,
        INA2_INPUT,
        INA3_INPUT,
        INA4_INPUT,
        INA5_INPUT,
        INA6_INPUT,
        INA7_INPUT,
        INA8_INPUT,
        INA9_INPUT,
        INA10_INPUT,
        INA11_INPUT,

        INB1_INPUT,
        INB2_INPUT,
        INB3_INPUT,
        INB4_INPUT,
        INB5_INPUT,
        INB6_INPUT,
        INB7_INPUT,
        INB8_INPUT,
        INB9_INPUT,
        INB10_INPUT,
        INB11_INPUT,

        CV1_INPUT,
        CV2_INPUT,
        CV3_INPUT,
        CV4_INPUT,
        CV5_INPUT,
        CV6_INPUT,
        CV7_INPUT,
        CV8_INPUT,
        CV9_INPUT,
        CV10_INPUT,
        CV11_INPUT,

		NUM_INPUTS
	};
	enum OutputIds 
	{
        OUT1_OUTPUT,
        OUT2_OUTPUT,
        OUT3_OUTPUT,
        OUT4_OUTPUT,
        OUT5_OUTPUT,
        OUT6_OUTPUT,
        OUT7_OUTPUT,
        OUT8_OUTPUT,
        OUT9_OUTPUT,
        OUT10_OUTPUT,
        OUT11_OUTPUT,

		NUM_OUTPUTS
    };
    enum LightIds 
	{
		NUM_LIGHTS
	};

    bool swState[8] = {};
    
	switch2to1x11() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) 
	{
		reset();
	}

    void step() override;

    void reset() override 
    {
        for (int i = 0; i < 11; i++) 
        {
            swState[i] = false;
		}
	}
    void randomize() override 
    {
        for (int i = 0; i < 11; i++) 
        {
            swState[i] = (randomUniform() < 0.5);
		}
    }
    
    json_t *toJson() override 
    {
		json_t *rootJ = json_object();
        json_t *swStatesJ = json_array();
        for (int i = 0; i < 11; i++) 
        {
			json_t *swStateJ = json_boolean(swState[i]);
            json_array_append_new(swStatesJ, swStateJ);
		}
        json_object_set_new(rootJ, "swStates", swStatesJ);
		return rootJ;
	}

    void fromJson(json_t *rootJ) override 
    {
        json_t *swStatesJ = json_object_get(rootJ, "swStates");
        if (swStatesJ) 
        {
            for (int i = 0; i < 11; i++) 
            {
				json_t *stateJ = json_array_get(swStatesJ, i);
				if (stateJ) {
					swState[i] = json_boolean_value(stateJ);
                }
			}
        }
	}
};

void switch2to1x11::step() 
{
    
    for(int i = 0; i < 11; i++)
    {
        swState[i] = inputs[CV1_INPUT + i].value;
        if ( !swState[i] ) {
            if(inputs[INA1_INPUT + i].active)
            {
                float ina = inputs[INA1_INPUT + i].value;
                outputs[OUT1_OUTPUT + i].value = ina;
                
            }
        } else {
            if(inputs[INB1_INPUT + i].active)
            {
                float inb = inputs[INB1_INPUT + i].value;
                outputs[OUT1_OUTPUT + i].value = inb;
                
            }
        } 
    }
}

struct switch2to1x11Widget : ModuleWidget { 
    switch2to1x11Widget(switch2to1x11 *module) : ModuleWidget(module) 
    {
        box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

        {
            auto *panel = new SVGPanel();
            panel->box.size = box.size;
            panel->setBackground(SVG::load(assetPlugin(plugin, "res/switch2to1x11.svg")));
            addChild(panel);
        }
        addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
	    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 0)));
	    addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
	    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 365)));


        const float x1 = 3.0;
        const float x2 = 4.+20.;
        const float x3 = 5.+41.;
        const float x4 = 5.+62.;

        float yPos = 22.;

        for(int i = 0; i < 11; i++)
        {
            yPos += 27.;
            addInput(Port::create<sts_Port>(Vec(x1, yPos), Port::INPUT, module, switch2to1x11::CV1_INPUT + i));
            addInput(Port::create<sts_Port>(Vec(x2, yPos), Port::INPUT, module, switch2to1x11::INA1_INPUT + i));
            addInput(Port::create<sts_Port>(Vec(x3, yPos), Port::INPUT, module, switch2to1x11::INB1_INPUT + i));
            addOutput(Port::create<sts_Port>(Vec(x4, yPos), Port::OUTPUT, module, switch2to1x11::OUT1_OUTPUT + i));
        }

        yPos += 48.;
       
    }

};

Model *modelswitch2to1x11 = Model::create<switch2to1x11,switch2to1x11Widget>("STS", "switch2to1x11", "2 -> 1 x 11 Switch - A/B Switch", SWITCH_TAG, MIXER_TAG);
