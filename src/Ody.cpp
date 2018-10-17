#include "STS.hpp"
#include "dsp/digital.hpp"
#include "window.hpp"
//#include "torpedo.hpp"

struct Ody : Module 
{
	enum ParamIds
	{
		PITCHBEND_PARAM,
		PORTA_PARAM,
		OCTAVE_PARAM,

		ENUMS(OFFSET_PARAM, 33),
		ENUMS(SLIDER_PARAM, 33),
		ENUMS(SWITCH_PARAM, 22),
		
		NUM_PARAMS
	};
	enum InputIds
    {
        NUM_INPUTS
	};
	
	enum OutputIds {
		// Sliders
		ENUMS(OUT_OUTPUT, 33),

		// Pitch,Ports,Octave
		OUT_OUTPUT_PITCHBEND,
		OUT_OUTPUT_PORTA,
		OUT_OUTPUT_OCTAVE,
		//switches
		ENUMS(OUT_OUTPUT_SW, 22),
     	NUM_OUTPUTS
	};
	enum LightIds
    {
        TRIGGER_LED_1,
		NUM_LIGHTS
    };
    
	TextField* textField1;
    TextField* textField2;

    const float lightLambda = 0.075f;
    float resetLight1 = 0.0f;
    float resetLight2 = 0.0f;
	float out_[33];

    int label_num1 = 0;
    int label_num2 = 0;
	
	int panelStyle 		= 0;
	
	Ody() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	
	
	void step() override;

	struct OdyWidget : ModuleWidget {
    OdyWidget(Ody *module);
    void appendContextMenu(Menu *menu) override;
	};
};

void Ody::step()
{
	resetLight1 -= resetLight1 / lightLambda / engineGetSampleRate();
    lights[TRIGGER_LED_1].value = resetLight1;
	
	// Pitch bend
	outputs[OUT_OUTPUT_PITCHBEND].value = params[PITCHBEND_PARAM].value;
	// Portamento
	outputs[OUT_OUTPUT_PORTA].value = params[PORTA_PARAM].value;
	// Octave Switch
	outputs[OUT_OUTPUT_OCTAVE].value = params[OCTAVE_PARAM].value;

	// Sliders attenuverters Top + Bottom Row

	for(int i = 1; i < 34; i++)
    	{
        out_[i] = clamp(params[SLIDER_PARAM + i].value + params[OFFSET_PARAM + i].value, -10.0f, 10.0f);
		outputs[OUT_OUTPUT + i].value = out_[i];
	    }
	
	// Switches Bottom Row

	for(int i = 1; i < 23; i++)
    	{
        outputs[OUT_OUTPUT_SW  + i].value = params[SWITCH_PARAM + i].value;
	    }
	
};


struct OdyWidget : ModuleWidget {
    OdyWidget(Ody *module);
    void appendContextMenu(Menu *menu) override;
};


OdyWidget::OdyWidget(Ody *module) : ModuleWidget(module) {
	
	box.size = Vec(4 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

	//setPanel(SVG::load(assetPlugin(plugin, "res/Ody.svg")));

	
    
	DynamicPanelWidget *panel = new DynamicPanelWidget();
	panel->addPanel(SVG::load(assetPlugin(plugin, "res/Ody_Black.svg")));
	panel->addPanel(SVG::load(assetPlugin(plugin, "res/Ody_White.svg")));
	box.size = panel->box.size;
	panel->mode = &module->panelStyle;
	addChild(panel);
	
	
		

	addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 365)));

	
  	// Offset knobs top row

	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(215, 38), module, Ody::OFFSET_PARAM + 23, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(242, 38), module, Ody::OFFSET_PARAM + 24, -10.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(328, 38), module, Ody::OFFSET_PARAM + 25, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(355, 38), module, Ody::OFFSET_PARAM + 26, -10.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(522, 38), module, Ody::OFFSET_PARAM + 27, -10.0, 10.0, 0.0));
	
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(554, 38), module, Ody::OFFSET_PARAM + 28, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(581, 38), module, Ody::OFFSET_PARAM + 29, -10.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(689, 38), module, Ody::OFFSET_PARAM + 30, -10.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(743, 38), module, Ody::OFFSET_PARAM + 31, -10.0, 10.0, 0.0));
	
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(775, 38), module, Ody::OFFSET_PARAM + 32, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(802, 38), module, Ody::OFFSET_PARAM + 33, -10.0, 10.0, 0.0));
	
	// offset knobs bottom row

	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(215, 185), module, Ody::OFFSET_PARAM + 1, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(242, 185), module, Ody::OFFSET_PARAM + 2, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(269, 185), module, Ody::OFFSET_PARAM + 3, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(296, 185), module, Ody::OFFSET_PARAM + 4, -10.0, 10.0, 0.0));
    
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(328, 185), module, Ody::OFFSET_PARAM + 5, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(355, 185), module, Ody::OFFSET_PARAM + 6, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(382, 185), module, Ody::OFFSET_PARAM + 7, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(409, 185), module, Ody::OFFSET_PARAM + 8, -10.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(441, 185), module, Ody::OFFSET_PARAM + 9, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(468, 185), module, Ody::OFFSET_PARAM + 10, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(522, 185), module, Ody::OFFSET_PARAM + 11, -10.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(554, 185), module, Ody::OFFSET_PARAM + 12, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(581, 185), module, Ody::OFFSET_PARAM + 13, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(608, 185), module, Ody::OFFSET_PARAM + 14, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(635, 185), module, Ody::OFFSET_PARAM + 15, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(662, 185), module, Ody::OFFSET_PARAM + 16, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(689, 185), module, Ody::OFFSET_PARAM + 17, -10.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(743, 185), module, Ody::OFFSET_PARAM + 18, -10.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(775, 185), module, Ody::OFFSET_PARAM + 19, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(802, 185), module, Ody::OFFSET_PARAM + 20, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(829, 185), module, Ody::OFFSET_PARAM + 21, -10.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_Trimpot_Grey>(Vec(856, 185), module, Ody::OFFSET_PARAM + 22, -10.0, 10.0, 0.0));

	// Slider Output Ports
	
	addOutput(Port::create<PJ301MPort>(Vec(9, 20), Port::OUTPUT, module, Ody::OUT_OUTPUT + 1));
	addOutput(Port::create<PJ301MPort>(Vec(9, 58), Port::OUTPUT, module, Ody::OUT_OUTPUT + 2));
	addOutput(Port::create<PJ301MPort>(Vec(9, 96), Port::OUTPUT, module, Ody::OUT_OUTPUT + 3));
	addOutput(Port::create<PJ301MPort>(Vec(9, 134), Port::OUTPUT, module, Ody::OUT_OUTPUT + 4));
	addOutput(Port::create<PJ301MPort>(Vec(9, 172), Port::OUTPUT, module, Ody::OUT_OUTPUT + 5));
	addOutput(Port::create<PJ301MPort>(Vec(9, 210), Port::OUTPUT, module, Ody::OUT_OUTPUT + 6));
	addOutput(Port::create<PJ301MPort>(Vec(9, 248), Port::OUTPUT, module, Ody::OUT_OUTPUT + 7));
	addOutput(Port::create<PJ301MPort>(Vec(9, 286), Port::OUTPUT, module, Ody::OUT_OUTPUT + 8));
	addOutput(Port::create<PJ301MPort>(Vec(9, 324), Port::OUTPUT, module, Ody::OUT_OUTPUT + 9));
	
	addOutput(Port::create<PJ301MPort>(Vec(41, 20), Port::OUTPUT, module, Ody::OUT_OUTPUT + 10));
	addOutput(Port::create<PJ301MPort>(Vec(41, 58), Port::OUTPUT, module, Ody::OUT_OUTPUT + 11));
	addOutput(Port::create<PJ301MPort>(Vec(41, 96), Port::OUTPUT, module, Ody::OUT_OUTPUT + 12));
	addOutput(Port::create<PJ301MPort>(Vec(41, 134), Port::OUTPUT, module, Ody::OUT_OUTPUT + 13));
	addOutput(Port::create<PJ301MPort>(Vec(41, 172), Port::OUTPUT, module, Ody::OUT_OUTPUT + 14));
	addOutput(Port::create<PJ301MPort>(Vec(41, 210), Port::OUTPUT, module, Ody::OUT_OUTPUT + 15));
	addOutput(Port::create<PJ301MPort>(Vec(41, 248), Port::OUTPUT, module, Ody::OUT_OUTPUT + 16));
	addOutput(Port::create<PJ301MPort>(Vec(41, 286), Port::OUTPUT, module, Ody::OUT_OUTPUT + 17));
	addOutput(Port::create<PJ301MPort>(Vec(41, 324), Port::OUTPUT, module, Ody::OUT_OUTPUT + 18));

	addOutput(Port::create<PJ301MPort>(Vec(73, 20), Port::OUTPUT, module, Ody::OUT_OUTPUT + 19));
	addOutput(Port::create<PJ301MPort>(Vec(73, 58), Port::OUTPUT, module, Ody::OUT_OUTPUT + 20));
	addOutput(Port::create<PJ301MPort>(Vec(73, 96), Port::OUTPUT, module, Ody::OUT_OUTPUT + 21));
	addOutput(Port::create<PJ301MPort>(Vec(73, 134), Port::OUTPUT, module, Ody::OUT_OUTPUT + 22));
	addOutput(Port::create<PJ301MPort>(Vec(73, 172), Port::OUTPUT, module, Ody::OUT_OUTPUT + 23));
	addOutput(Port::create<PJ301MPort>(Vec(73, 210), Port::OUTPUT, module, Ody::OUT_OUTPUT + 24));
	addOutput(Port::create<PJ301MPort>(Vec(73, 248), Port::OUTPUT, module, Ody::OUT_OUTPUT + 25));
	addOutput(Port::create<PJ301MPort>(Vec(73, 286), Port::OUTPUT, module, Ody::OUT_OUTPUT + 26));
	addOutput(Port::create<PJ301MPort>(Vec(73, 324), Port::OUTPUT, module, Ody::OUT_OUTPUT + 27));
	
	addOutput(Port::create<PJ301MPort>(Vec(105, 20), Port::OUTPUT, module, Ody::OUT_OUTPUT + 28));
	addOutput(Port::create<PJ301MPort>(Vec(105, 58), Port::OUTPUT, module, Ody::OUT_OUTPUT + 29));
	addOutput(Port::create<PJ301MPort>(Vec(105, 96), Port::OUTPUT, module, Ody::OUT_OUTPUT + 30));
	addOutput(Port::create<PJ301MPort>(Vec(105, 134), Port::OUTPUT, module, Ody::OUT_OUTPUT + 31));
	addOutput(Port::create<PJ301MPort>(Vec(105, 172), Port::OUTPUT, module, Ody::OUT_OUTPUT + 32));
	addOutput(Port::create<PJ301MPort>(Vec(105, 210), Port::OUTPUT, module, Ody::OUT_OUTPUT + 33));

	addOutput(Port::create<PJ301MPort>(Vec(105, 248), Port::OUTPUT, module, Ody::OUT_OUTPUT_OCTAVE));
	addOutput(Port::create<PJ301MPort>(Vec(105, 286), Port::OUTPUT, module, Ody::OUT_OUTPUT_PITCHBEND));
	addOutput(Port::create<PJ301MPort>(Vec(105, 324), Port::OUTPUT, module, Ody::OUT_OUTPUT_PORTA));
	
	// Add Switch Outputs
	addOutput(Port::create<PJ301MPort>(Vec(128, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 20));
	addOutput(Port::create<PJ301MPort>(Vec(154, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 21));
	addOutput(Port::create<PJ301MPort>(Vec(180, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 22));

	addOutput(Port::create<PJ301MPort>(Vec(209, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 1));
	addOutput(Port::create<PJ301MPort>(Vec(236, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 2));
	addOutput(Port::create<PJ301MPort>(Vec(290, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 3));

	addOutput(Port::create<PJ301MPort>(Vec(322, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 4));
	addOutput(Port::create<PJ301MPort>(Vec(349, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 5));
	addOutput(Port::create<PJ301MPort>(Vec(403, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 6));

	addOutput(Port::create<PJ301MPort>(Vec(435, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 7));
	addOutput(Port::create<PJ301MPort>(Vec(462, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 8));
	addOutput(Port::create<PJ301MPort>(Vec(489, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 9));

	addOutput(Port::create<PJ301MPort>(Vec(548, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 10));
	addOutput(Port::create<PJ301MPort>(Vec(575, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 11));
	addOutput(Port::create<PJ301MPort>(Vec(602, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 12));
	addOutput(Port::create<PJ301MPort>(Vec(629, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 13));
	addOutput(Port::create<PJ301MPort>(Vec(656, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 14));
	addOutput(Port::create<PJ301MPort>(Vec(683, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 15));

	addOutput(Port::create<PJ301MPort>(Vec(737, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 16));

	addOutput(Port::create<PJ301MPort>(Vec(769, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 17));
	addOutput(Port::create<PJ301MPort>(Vec(796, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 18));
	addOutput(Port::create<PJ301MPort>(Vec(850, 340), Port::OUTPUT, module, Ody::OUT_OUTPUT_SW  + 19));


	// Octave 5 Way
	addParam(ParamWidget::create<CKSSThree>(Vec(176,215), module, Ody::OCTAVE_PARAM, 0.0f, 4.0f, 2.0f));
	// add Portamentp Slider
	addParam(ParamWidget::create<sts_SlidePotBlack>(Vec(145, 184), module, Ody::PITCHBEND_PARAM, 0.0, 10.0, 0.0)); 
	// Add Pitch Bend Knob
	addParam(ParamWidget::create<sts_Davies47_Grey>(Vec(147, 280), module, Ody::PORTA_PARAM, 0.0, 2.0, 1.0));
   	
	// Add sliders Top Row
	
	addParam(ParamWidget::create<sts_SlidePotBlue>(Vec(212, 57), module, Ody::SLIDER_PARAM + 23, 0.0, 2.0, 1.0));
	addParam(ParamWidget::create<sts_SlidePotBlue>(Vec(239, 57), module, Ody::SLIDER_PARAM + 24, 0.0, 0.25, 0.125));

	addParam(ParamWidget::create<sts_SlidePotGreen>(Vec(325, 57), module, Ody::SLIDER_PARAM + 25, 0.0, 2.0, 1.0));
	addParam(ParamWidget::create<sts_SlidePotGreen>(Vec(352, 57), module, Ody::SLIDER_PARAM + 26, 0.0, 0.25, 0.125));

	addParam(ParamWidget::create<sts_SlidePotPink>(Vec(519, 57), module, Ody::SLIDER_PARAM + 27, 0.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_SlidePotBlack>(Vec(551, 57), module, Ody::SLIDER_PARAM + 28, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotBlack>(Vec(578, 57), module, Ody::SLIDER_PARAM + 29, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotBlack>(Vec(686, 57), module, Ody::SLIDER_PARAM + 30, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotBlack>(Vec(740, 57), module, Ody::SLIDER_PARAM + 31, 0.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_SlidePotRed>(Vec(772, 57), module, Ody::SLIDER_PARAM + 32, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotRed>(Vec(799, 57), module, Ody::SLIDER_PARAM + 33, 0.0, 10.0, 0.0));
		
	// Add sliders Bottom Row
	
	addParam(ParamWidget::create<sts_SlidePotPink>(Vec(212, 204), module, Ody::SLIDER_PARAM + 1, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotYellow>(Vec(239, 204), module, Ody::SLIDER_PARAM + 2, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotBlue>(Vec(266, 204), module, Ody::SLIDER_PARAM + 3, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotPink>(Vec(293, 204), module, Ody::SLIDER_PARAM + 4, 0.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_SlidePotPink>(Vec(325, 204), module, Ody::SLIDER_PARAM + 5, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotYellow>(Vec(352, 204), module, Ody::SLIDER_PARAM + 6, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotGreen>(Vec(379, 204), module, Ody::SLIDER_PARAM + 7, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotPink>(Vec(406, 204), module, Ody::SLIDER_PARAM + 8, 0.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_SlidePotBlue>(Vec(438, 204), module, Ody::SLIDER_PARAM + 9, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotWhite>(Vec(465, 204), module, Ody::SLIDER_PARAM + 10, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotYellow>(Vec(519, 204), module, Ody::SLIDER_PARAM + 11, 0.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_SlidePotWhite>(Vec(551, 204), module, Ody::SLIDER_PARAM + 12, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotBlue>(Vec(578, 204), module, Ody::SLIDER_PARAM + 13, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotGreen>(Vec(605, 204), module, Ody::SLIDER_PARAM + 14, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotBlack>(Vec(632, 204), module, Ody::SLIDER_PARAM + 15, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotYellow>(Vec(659, 204), module, Ody::SLIDER_PARAM + 16, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotRed>(Vec(686, 204), module, Ody::SLIDER_PARAM + 17, 0.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_SlidePotRed>(Vec(740, 204), module, Ody::SLIDER_PARAM + 18, 0.0, 10.0, 0.0));

	addParam(ParamWidget::create<sts_SlidePotRed>(Vec(772, 204), module, Ody::SLIDER_PARAM + 19, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotRed>(Vec(799, 204), module, Ody::SLIDER_PARAM + 20, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotRed>(Vec(826, 204), module, Ody::SLIDER_PARAM + 21, 0.0, 10.0, 0.0));
	addParam(ParamWidget::create<sts_SlidePotRed>(Vec(853, 204), module, Ody::SLIDER_PARAM + 22, 0.0, 10.0, 0.0));
	
	
	// Switches Bottom row
	
	addParam(ParamWidget::create<CKSS>(Vec(215, 301), module, Ody::SWITCH_PARAM + 1, 0.0, 1.0, 0.0));
	addParam(ParamWidget::create<CKSS>(Vec(242, 301), module, Ody::SWITCH_PARAM + 2, 0.0, 1.0, 0.0));
	addParam(ParamWidget::create<CKSS>(Vec(296, 301), module, Ody::SWITCH_PARAM + 3, 0.0, 1.0, 0.0));
 
	addParam(ParamWidget::create<CKSS>(Vec(328, 301), module, Ody::SWITCH_PARAM + 4, 0.0, 1.0, 0.0)); 
	addParam(ParamWidget::create<CKSS>(Vec(355, 301), module, Ody::SWITCH_PARAM + 5, 0.0, 1.0, 0.0));
	addParam(ParamWidget::create<CKSS>(Vec(409, 301), module, Ody::SWITCH_PARAM + 6, 0.0, 1.0, 0.0));

	addParam(ParamWidget::create<CKSS>(Vec(441, 301), module, Ody::SWITCH_PARAM + 7, 0.0, 1.0, 0.0)); 
	addParam(ParamWidget::create<CKSS>(Vec(468, 301), module, Ody::SWITCH_PARAM + 8, 0.0, 1.0, 0.0));
	addParam(ParamWidget::create<CKSS>(Vec(495, 301), module, Ody::SWITCH_PARAM + 9, 0.0, 1.0, 0.0));

	addParam(ParamWidget::create<CKSS>(Vec(554, 301), module, Ody::SWITCH_PARAM + 10, 0.0, 1.0, 0.0)); 
	addParam(ParamWidget::create<CKSS>(Vec(581, 301), module, Ody::SWITCH_PARAM + 11, 0.0, 1.0, 0.0));
	addParam(ParamWidget::create<CKSS>(Vec(608, 301), module, Ody::SWITCH_PARAM + 12, 0.0, 1.0, 0.0));
	addParam(ParamWidget::create<CKSS>(Vec(635, 301), module, Ody::SWITCH_PARAM + 13, 0.0, 1.0, 0.0)); 
	addParam(ParamWidget::create<CKSS>(Vec(662, 301), module, Ody::SWITCH_PARAM + 14, 0.0, 1.0, 0.0));
	addParam(ParamWidget::create<CKSS>(Vec(689, 301), module, Ody::SWITCH_PARAM + 15, 0.0, 1.0, 0.0));

	addParam(ParamWidget::create<CKSS>(Vec(743, 301), module, Ody::SWITCH_PARAM + 16, 0.0, 1.0, 0.0));

	addParam(ParamWidget::create<CKSS>(Vec(775, 301), module, Ody::SWITCH_PARAM + 17, 0.0, 1.0, 0.0)); 
	addParam(ParamWidget::create<CKSS>(Vec(802, 301), module, Ody::SWITCH_PARAM + 18, 0.0, 1.0, 0.0));
	addParam(ParamWidget::create<CKSS>(Vec(856, 301), module, Ody::SWITCH_PARAM + 19, 0.0, 1.0, 0.0));
	// Switches Top Row
	addParam(ParamWidget::create<CKSS>(Vec(178, 57), module, Ody::SWITCH_PARAM + 20, 0.0, 1.0, 0.0));
	addParam(ParamWidget::create<CKSS>(Vec(265, 57), module, Ody::SWITCH_PARAM + 21, 0.0, 1.0, 0.0));
	addParam(ParamWidget::create<CKSS>(Vec(377, 57), module, Ody::SWITCH_PARAM + 22, 0.0, 1.0, 0.0));

};

struct OdyPanelStyleItem : MenuItem {
    Ody* module;
    int panelStyle;
    void onAction(EventAction &e) override {
        module->panelStyle = panelStyle;
    }
    void step() override {
        rightText = (module->panelStyle == panelStyle) ? "✔" : "";
        MenuItem::step();
    }
};

void OdyWidget::appendContextMenu(Menu *menu) {
    Ody *module = dynamic_cast<Ody*>(this->module);
    assert(module);

    // Panel style
    menu->addChild(construct<MenuLabel>());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Odyssey Homage color"));
    menu->addChild(construct<OdyPanelStyleItem>(&MenuItem::text, "Odyssey MKII Black",
    	&OdyPanelStyleItem::module, module, &OdyPanelStyleItem::panelStyle, 0));
    menu->addChild(construct<OdyPanelStyleItem>(&MenuItem::text, "Original Odyseey 2800 White",
    	&OdyPanelStyleItem::module, module, &OdyPanelStyleItem::panelStyle, 1));

}


Model *modelOdy = Model::create<Ody, OdyWidget>("STS", "Ody", "Illiad - Synth Controller", CONTROLLER_TAG);