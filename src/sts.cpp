#include "sts.hpp"


Plugin *pluginInstance;

void init(rack::Plugin *p) 
{
	pluginInstance = p;
		
	//p->addModel(modelSwitch2to1x11);
	p->addModel(modelOdyssey);
	p->addModel(modelIlliad);
	p->addModel(modelPolySEQ16);
	p->addModel(modelRingModulator);
	p->addModel(modelWaveFolder);
	p->addModel(modelVU_Poly);
	p->addModel(modelLFOPoly);
	p->addModel(modelChords);

	p->addModel(modelMidiPlayer);
	//p->addModel(modelSEQEXP);
	
	

}


