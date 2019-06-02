#include "sts.hpp"

using namespace std;
using namespace rack;

struct Switch2to1x11 : Module 
{
	enum ParamIds 
	{
        NUM_PARAMS
	};
	enum InputIds 
	{
        ENUMS(INA_INPUT, 11),
        ENUMS(INB_INPUT, 11),
        ENUMS(CV_INPUT,11),

		NUM_INPUTS
	};
	enum OutputIds 
	{
        ENUMS(OUT_OUTPUT, 11),
        
		NUM_OUTPUTS
    };
    enum LightIds 
	{
		NUM_LIGHTS
	};

    bool swState[11] = {};
    int panelStyle = 1;
    
	Switch2to1x11() 
    {
        // Configure the module
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        // Configure parameters
		// See engine/Param.hpp for config() arguments
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
        int channels = 1;
        for (int i = 0; i < 11; i++)
        {
            float out[16] = {};
            //float outa[16] = {};
            //float outb[16] = {}; 
                        
            if (inputs[INA_INPUT + i].isConnected() && inputs[INB_INPUT + i].isConnected() && inputs[CV_INPUT + i].isConnected() && outputs[OUT_OUTPUT + i].isConnected())
            {
                //outa = inputs[INA_INPUT + i].getPolyVoltage(i);
                channels = inputs[CV_INPUT + i].getChannels();
            

                if  (inputs[CV_INPUT + i].isConnected())
                {
                    swState[i] = inputs[CV_INPUT + i].getPolyVoltage(i);
                }
                else
                {
                    //swState[i] = inputs[CV_INPUT + (i - 1)].getVoltage();
                    //swState[i] = inputs[CV_INPUT + i].getVoltage();
                    swState[i] = swState[i - 1];
                }

                //if (inputs[INB_INPUT + i].isConnected())
                //{
                //    outb = inputs[INB_INPUT + i].getPolyVoltage(i);
                 //   channels = inputs[INB_INPUT + i].getChannels();
               // }
                              
               // outputs[OUT_OUTPUT + i].setChannels(channels);
               // if (swState[i])
                //{
                    //outputs[OUT_OUTPUT + i].writeVoltages(inputs[INA_INPUT + i].getPolyVoltage(i));
               //     out = inputs[INA_INPUT + i].getPolyVoltage(i));  
               // }
               // else
               // {
               //     out = inputs[INB_INPUT + i].getPolyVoltage(i));
               // }
               // outputs[OUT_OUTPUT + i].writeVoltages(out)
            out[i] = swState[i] ? inputs[INA_INPUT + i].getPolyVoltage(i) : inputs[INB_INPUT + i].getPolyVoltage(i);
            //outputs[OUT_OUTPUT + i].writeVoltages(swState[i] ? inputs[INA_INPUT + i].getPolyVoltage(i) : inputs[INB_INPUT + i].getPolyVoltage(i));                
            outputs[OUT_OUTPUT + i].writeVoltages(out);
            }
        }
    }

    
    
    
};

struct Switch2to1x11Widget : ModuleWidget 
{
    SvgPanel *goldPanel;
	SvgPanel *blackPanel;
	SvgPanel *whitePanel;

	struct panelStyleItem : MenuItem
	{
		Switch2to1x11 *module;
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
		Switch2to1x11 *module = dynamic_cast<Switch2to1x11 *>(this->module);

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

    Switch2to1x11Widget(Switch2to1x11 *module) 
    {
        setModule(module);
        //setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/switch2to1x11.svg")));

        goldPanel = new SvgPanel();
        goldPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/switch2to1x11_gold.svg")));
        box.size = goldPanel->box.size;
        addChild(goldPanel);
        blackPanel = new SvgPanel();
        blackPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/switch2to1x11_black.svg")));
        blackPanel->visible = false;
        addChild(blackPanel);
        whitePanel = new SvgPanel();
        whitePanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/switch2to1x11_white.svg")));
        whitePanel->visible = false;
        addChild(whitePanel);

        addChild(createWidget<ScrewSilver>(Vec(15, 0)));
	    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
	    addChild(createWidget<ScrewSilver>(Vec(15, 365)));
	    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));


        const float x1 = 3.0;
        const float x2 = 4.+20.;
        const float x3 = 5.+41.;
        const float x4 = 5.+62.;

        float yPos = 22;

        for(int i = 0; i < 11; i++)
        {
            yPos += 27;
            addInput(createInput<sts_Port>(Vec(x1, yPos), module, Switch2to1x11::CV_INPUT + i));
            addInput(createInput<sts_Port>(Vec(x2, yPos), module, Switch2to1x11::INA_INPUT + i));
            addInput(createInput<sts_Port>(Vec(x3, yPos), module, Switch2to1x11::INB_INPUT + i));
            addOutput(createOutput<sts_Port>(Vec(x4, yPos), module, Switch2to1x11::OUT_OUTPUT + i));
        }
        //yPos += 48;

    }

    void step() override
    {
        if (module)
        {
            goldPanel->visible = ((((Switch2to1x11 *)module)->panelStyle) == 0);
            blackPanel->visible = ((((Switch2to1x11 *)module)->panelStyle) == 1);
            whitePanel->visible = ((((Switch2to1x11 *)module)->panelStyle) == 2);
        }
        Widget::step();
    }
};

Model *modelSwitch2to1x11 = createModel<Switch2to1x11,Switch2to1x11Widget>("Switch2to1x11");
