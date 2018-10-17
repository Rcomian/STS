#include "STS.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	plugin->slug = TOSTRING(SLUG);
	p->version = TOSTRING(VERSION);
	
	p->addModel(modelswitch2to1x8);
	p->addModel(modelConvolutionReverb);
	p->addModel(modelOdy);
}
