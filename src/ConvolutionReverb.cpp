#include <string.h>
#include "STS.hpp"
#include "dsp/functions.hpp"
#include "dsp/samplerate.hpp"
#include "dsp/ringbuffer.hpp"
#include "dsp/filter.hpp"
#include "dsp/fir.hpp"    
#include "window.hpp"
#include "osdialog.h"

float *springReverbIR;
size_t springReverbIRLen;


void springReverbInit(float** springReverbIR, size_t* springReverbIRLen) {

	//std::string dir = path.empty() ? assetLocal("") : stringDirectory(path);
	//std::string dir = play->lastPath.empty() ? assetLocal("") : stringDirectory(play->lastPath);
	//char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, NULL);
	std::string irFilename = assetPlugin(plugin, "res/SpringReverbIR.pcm");
	FILE *f = fopen(irFilename.c_str(), "rb");
	assert(f);
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f); 
	fseek(f, 0, SEEK_SET);

	*springReverbIRLen = size / sizeof(float);
	*springReverbIR = new float[*springReverbIRLen];
	fread(*springReverbIR, sizeof(float), *springReverbIRLen, f);
	fclose(f);
	}

static const size_t BLOCK_SIZE = 1024;  

struct ConvolutionReverb : Module {
	enum ParamIds {
		WET_PARAM,
		LEVEL1_PARAM,
		LEVEL2_PARAM,
		HPF_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV1_INPUT,
		CV2_INPUT,
		CV_HPF_INPUT,
		IN1_INPUT,
		IN2_INPUT,
		MIX_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MIX_OUTPUT,
		WET_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		PEAK_LIGHT,
		VU1_LIGHT,
		NUM_LIGHTS = VU1_LIGHT + 7
	}; 

	RealTimeConvolver *convolver = NULL;
	SampleRateConverter<1> inputSrc;
	SampleRateConverter<1> outputSrc;
	DoubleRingBuffer<Frame<1>, 16*BLOCK_SIZE> inputBuffer;
	DoubleRingBuffer<Frame<1>, 16*BLOCK_SIZE> outputBuffer;

	//std::string lastPath;
	//std::string waveFileName;
	//std::string waveExtension;
	//bool loading = false;

	RCFilter dryFilter;
	PeakFilter vuFilter;
	PeakFilter lightFilter;

	ConvolutionReverb();
	~ConvolutionReverb();
	void step() override;
};


ConvolutionReverb::ConvolutionReverb() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
	convolver = new RealTimeConvolver(BLOCK_SIZE);

	float *springReverbIR;
	size_t springReverbIRLen;

	springReverbInit(&springReverbIR,&springReverbIRLen); 

	convolver->setKernel(springReverbIR,springReverbIRLen);  
	delete [] springReverbIR;
}	

ConvolutionReverb::~ConvolutionReverb() {
	delete convolver;
}

struct ConvolutionReverbWidget : ModuleWidget {
    ConvolutionReverbWidget(ConvolutionReverb *module);
    void appendContextMenu(Menu *menu) override;
	};

void ConvolutionReverb::step() {
	float in1 = inputs[IN1_INPUT].value;
	float in2 = inputs[IN2_INPUT].value;
	const float levelScale = 0.030;
	const float levelBase = 25.0;
	float level1 = levelScale * exponentialBipolar(levelBase, params[LEVEL1_PARAM].value) * inputs[CV1_INPUT].normalize(10.0) / 10.0;
	float level2 = levelScale * exponentialBipolar(levelBase, params[LEVEL2_PARAM].value) * inputs[CV2_INPUT].normalize(10.0) / 10.0;
	float dry = in1 * level1 + in2 * level2;

	// HPF on dry
	float dryCutoff = 200.0 * (powf(20.0, params[HPF_PARAM].value + inputs[CV_HPF_INPUT].value)) * engineGetSampleTime();
	dryFilter.setCutoff(dryCutoff);
	dryFilter.process(dry);

	// Add dry to input buffer
	if (!inputBuffer.full()) {
		Frame<1> inputFrame;
		inputFrame.samples[0] = dryFilter.highpass();
		inputBuffer.push(inputFrame);
	}


	if (outputBuffer.empty()) {
		float input[BLOCK_SIZE] = {};
		float output[BLOCK_SIZE];
		// Convert input buffer
		{
			inputSrc.setRates(engineGetSampleRate(), 48000);
			int inLen = inputBuffer.size();
			int outLen = BLOCK_SIZE;
			inputSrc.process(inputBuffer.startData(), &inLen, (Frame<1>*) input, &outLen);
			inputBuffer.startIncr(inLen);
		}

		// Convolve block
		convolver->processBlock(input, output);

		// Convert output buffer
		{
			outputSrc.setRates(48000, engineGetSampleRate());
			int inLen = BLOCK_SIZE;
			int outLen = outputBuffer.capacity();
			outputSrc.process((Frame<1>*) output, &inLen, outputBuffer.endData(), &outLen);
			outputBuffer.endIncr(outLen);
		}
	}

	// Set output
	if (outputBuffer.empty())
		return;
	float wet = outputBuffer.shift().samples[0];
	float balance = clamp(params[WET_PARAM].value + inputs[MIX_CV_INPUT].value / 10.0f, 0.0f, 1.0f);
	float mix = crossfade(in1, wet, balance);

	outputs[WET_OUTPUT].value = clamp(wet, -10.0f, 10.0f);
	outputs[MIX_OUTPUT].value = clamp(mix, -10.0f, 10.0f);


	// Set lights
	float lightRate = 5.0 * engineGetSampleTime();
	vuFilter.setRate(lightRate);
	vuFilter.process(fabsf(wet));
	lightFilter.setRate(lightRate);
	lightFilter.process(fabsf(dry*50.0));

	float vuValue = vuFilter.peak();
	for (int i = 0; i < 7; i++) {
		float light = powf(1.413, i) * vuValue / 10.0 - 1.0;
		lights[VU1_LIGHT + i].value = clamp(light, 0.0f, 1.0f);
	}
	lights[PEAK_LIGHT].value = lightFilter.peak();
}


	ConvolutionReverbWidget::ConvolutionReverbWidget(ConvolutionReverb *module) : ModuleWidget(module) {  
	
		setPanel(SVG::load(assetPlugin(plugin, "res/ConvolutionReverb.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 365)));


		addParam(ParamWidget::create<BefacoBigKnob>(Vec(37, 28), module, ConvolutionReverb::WET_PARAM, 0.0, 1.0, 0.5));

		addParam(ParamWidget::create<BefacoSlidePot>(Vec(12, 116), module, ConvolutionReverb::LEVEL1_PARAM, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<BefacoSlidePot>(Vec(123, 116), module, ConvolutionReverb::LEVEL2_PARAM, 0.0, 1.0, 0.0));
		

		addParam(ParamWidget::create<Davies1900hWhiteKnob>(Vec(57, 209), module, ConvolutionReverb::HPF_PARAM, 0.0, 1.0, 0.5));

		addInput(Port::create<PJ301MPort>(Vec(7, 243), Port::INPUT, module, ConvolutionReverb::CV1_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(118, 243), Port::INPUT, module, ConvolutionReverb::CV2_INPUT));
		
		addInput(Port::create<PJ301MPort>(Vec(7, 281), Port::INPUT, module, ConvolutionReverb::IN1_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(62, 281), Port::INPUT, module, ConvolutionReverb::CV_HPF_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(118, 281), Port::INPUT, module, ConvolutionReverb::IN2_INPUT));

		addOutput(Port::create<PJ301MPort>(Vec(7, 322), Port::OUTPUT, module, ConvolutionReverb::MIX_OUTPUT));
		addInput(Port::create<PJ301MPort>(Vec(62, 322), Port::INPUT, module, ConvolutionReverb::MIX_CV_INPUT));
		addOutput(Port::create<PJ301MPort>(Vec(118, 322), Port::OUTPUT, module, ConvolutionReverb::WET_OUTPUT));

		addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(70, 265), module, ConvolutionReverb::PEAK_LIGHT));
		addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(70, 113), module, ConvolutionReverb::VU1_LIGHT + 0));
		addChild(ModuleLightWidget::create<MediumLight<YellowLight>>(Vec(70, 126), module, ConvolutionReverb::VU1_LIGHT + 1));
		addChild(ModuleLightWidget::create<MediumLight<YellowLight>>(Vec(70, 138), module, ConvolutionReverb::VU1_LIGHT + 2));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(70, 150), module, ConvolutionReverb::VU1_LIGHT + 3));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(70, 163), module, ConvolutionReverb::VU1_LIGHT + 4));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(70, 175), module, ConvolutionReverb::VU1_LIGHT + 5));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(70, 188), module, ConvolutionReverb::VU1_LIGHT + 6));
	}

/* void ConvolutionReverbWidget::appendContextMenu(Menu *menu) {
    ConvolutionReverb *module = dynamic_cast<ConvolutionReverb*>(this->module);
    assert(module);

    // Load IR File
    menu->addChild(construct<MenuLabel>());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Load IR file"));    menu->addChild(construct<ConvolutionReverbPanelStyleItem>(&MenuItem::text, "Load IR file",
  	&ConvolutionReverbPanelStyleItem::module, module, &ConvolutionReverbPanelStyleItem::panelStyle, 0));
   }
*/

/*	struct ConvolutionReverbItem : MenuItem {
	ConvolutionReverb *ConvolutionReverb;
	void onAction(EventAction &e) override {

		std::string dir = ConvolutionReverb->lastPath.empty() ? assetLocal("") : stringDirectory(ConvolutionReverb->lastPath);
		char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, NULL);
		
			/ConvolutionReverb->loadSample(path);
			ConvolutionReverb->samplePos = 0;
			ConvolutionReverb->lastPath = path;
			ConvolutionReverb->sliceIndex = -1;
			free(path);
			   springReverbInit(&springReverbIR,&springReverbIRLen); 

			convolver->setKernel(springReverbIR,springReverbIRLen);  
			delete [] springReverbIR;

		
	}
*/	


/* void ConvolutionReverbWidget::appendContextMenu(Menu *menu) {
    ConvolutionReverb *module = dynamic_cast<ConvolutionReverb*>(this->module);
    assert(module);

    // Load IR File
    menu->addChild(construct<MenuLabel>());
    menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Load IR file"));
    menu->addChild(construct<ConvolutionReverbItem>(&MenuItem::text, "Load IR file",
   	&ConvolutionReverbItem::module, module, &ConvolutionReverbItem::panelStyle, 0));

	ConvolutionReverbItem *sampleItem = new ConvolutionReverbItem();
	sampleItem->text = "Load sample";
	sampleItem->ConvolutionReverb = ConvolutionReverb;
	menu->addChild(sampleItem);
	}
*/


Model *modelConvolutionReverb = Model::create<ConvolutionReverb, ConvolutionReverbWidget>("STS", "ConvolutionReverb", "Convolution Reverb", REVERB_TAG);
