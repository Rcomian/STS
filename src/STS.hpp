#ifndef STS_HPP
#define STS_HPP

#include "rack.hpp"
#include "window.hpp"

using namespace rack;


extern Plugin *plugin;

extern Model *modelConvolutionReverb;
extern Model *modelOdy;
extern Model *modelswitch2to1x8;


////////////////////
// module widgets
////////////////////

// Dynamic Panel

enum DynamicViewMode {
    ACTIVE_HIGH_VIEW,
    ACTIVE_LOW_VIEW,
    ALWAYS_ACTIVE_VIEW
};
// old
struct PanelBorderWidget : TransparentWidget {
	void draw(NVGcontext *vg) override;
};

struct DynamicPanelWidget : FramebufferWidget {
    int* mode;
    int oldMode;
    std::vector<std::shared_ptr<SVG>> panels;
    SVGWidget* visiblePanel;
    PanelBorderWidget* border;

    DynamicPanelWidget();
    void addPanel(std::shared_ptr<SVG> svg);
    void step() override;
};

struct DynamicSVGPanel : FramebufferWidget { // like SVGPanel (in app.hpp and SVGPanel.cpp) but with dynmically assignable panel
    int* mode;
    int oldMode;
	std::vector<std::shared_ptr<SVG>> panels;
    SVGWidget* visiblePanel;
    PanelBorderWidget* border;
    DynamicSVGPanel();
    void addPanel(std::shared_ptr<SVG> svg);
    void step() override;
};



// for text display

struct BigLEDBezel : SVGSwitch, MomentarySwitch {
        BigLEDBezel() {
                addFrame(SVG::load(assetPlugin(plugin, "res/as_bigLEDBezel.svg")));
        }
};

struct Knurlie : SVGScrew {
	Knurlie() {
		sw->svg = SVG::load(assetPlugin(plugin, "res/Knurlie.svg"));
		sw->wrap();
		box.size = sw->box.size;
	}
};
// Befaco Slide Pots



struct stsB_SlidePotBlack : SVGSlider {
	stsB_SlidePotBlack() {
		Vec margin = Vec(3.5, 3.5);
		maxHandlePos = Vec(-1, -2).plus(margin);
		minHandlePos = Vec(-1, 87).plus(margin);
		setSVGs(SVG::load(assetGlobal("res/ComponentLibrary/BefacoSlidePot.svg")), SVG::load(assetGlobal("res/BefacoSlidePotHandle.svg")));
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
	}
};

struct stsB_SlidePotRed : SVGSlider {
	stsB_SlidePotRed() {
		Vec margin = Vec(3.5, 3.5);
		maxHandlePos = Vec(-1, -2).plus(margin);
		minHandlePos = Vec(-1, 87).plus(margin);
		setSVGs(SVG::load(assetGlobal("res/ComponentLibrary/BefacoSlidePot.svg")), SVG::load(assetGlobal("res/stsB-SlidePotHandleBlack.svg")));
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
	}
};

struct sts_Port : SVGPort {
	sts_Port() {
		setSVG(SVG::load(assetPlugin(plugin, "res/sts-Port20.svg")));
	}
};

struct sts_Switch : SVGSwitch, ToggleSwitch {
	sts_Switch() {
		addFrame(SVG::load(assetPlugin(plugin,"res/sts-switchv_0.svg")));
		addFrame(SVG::load(assetPlugin(plugin,"res/sts-switchv_1.svg")));
	}
};
struct as_CKSS : SVGSwitch, ToggleSwitch {
	as_CKSS() {
		addFrame(SVG::load(assetPlugin(plugin,"res/as_CKSS_0.svg")));
		addFrame(SVG::load(assetPlugin(plugin,"res/as_CKSS_1.svg")));
	}
};

struct as_CKSSH : SVGSwitch, ToggleSwitch {
	as_CKSSH() {
		addFrame(SVG::load(assetPlugin(plugin, "res/as_CKSSH_0.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/as_CKSSH_1.svg")));
		sw->wrap();
		box.size = sw->box.size;
	}
};

struct as_CKSSThree : SVGSwitch, ToggleSwitch {
	as_CKSSThree() {
		addFrame(SVG::load(assetPlugin(plugin,"res/as_CKSSThree_2.svg")));
		addFrame(SVG::load(assetPlugin(plugin,"res/as_CKSSThree_1.svg")));
		addFrame(SVG::load(assetPlugin(plugin,"res/as_CKSSThree_0.svg")));
	}
};

struct as_MuteBtn : SVGSwitch, ToggleSwitch {
	as_MuteBtn() {
		addFrame(SVG::load(assetPlugin(plugin,"res/as_mute-off.svg")));
		addFrame(SVG::load(assetPlugin(plugin,"res/as_mute-on.svg")));
	}
};


/////////
struct as_PJ301MPort : SVGPort {
	as_PJ301MPort() {
		setSVG(SVG::load(assetPlugin(plugin,"res/as-PJ301M.svg")));
		//background->svg = SVG::load(assetPlugin(plugin,"res/as-PJ301M.svg"));
		//background->wrap();
		//box.size = background->box.size;
	}
};

struct as_SlidePot : SVGFader {
	as_SlidePot() {
		Vec margin = Vec(4, 4);
		maxHandlePos = Vec(-1.5, -8).plus(margin);
		minHandlePos = Vec(-1.5, 87).plus(margin);
		background->svg = SVG::load(assetPlugin(plugin,"res/as-SlidePot.svg"));
		background->wrap();
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		handle->svg = SVG::load(assetPlugin(plugin,"res/as-SlidePotHandle.svg"));
		handle->wrap();
	}
};

struct as_FaderPot : SVGFader {
	as_FaderPot() {
		Vec margin = Vec(4, 4);
		maxHandlePos = Vec(-1.5, -8).plus(margin);
		minHandlePos = Vec(-1.5, 57).plus(margin);
		background->svg = SVG::load(assetPlugin(plugin,"res/as-FaderPot.svg"));
		background->wrap();
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		handle->svg = SVG::load(assetPlugin(plugin,"res/as-SlidePotHandle.svg"));
		handle->wrap();
	}
};
///////////////////////
// Knobs             //
///////////////////////

//  15 px trimpot
struct sts_Trimpot_Black : SVGKnob {
	sts_Trimpot_Black() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSVG(SVG::load(assetPlugin(plugin,"res/sts-Trimpot_Black.svg")));
	}
};
struct sts_Trimpot_Grey : SVGKnob {
	sts_Trimpot_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSVG(SVG::load(assetPlugin(plugin,"res/sts-Trimpot_Grey.svg")));
	}
};
struct sts_Davies47_Grey : SVGKnob {
	sts_Davies47_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSVG(SVG::load(assetPlugin(plugin,"res/sts_Davies1900_Grey_47_47.svg")));
	}
};

///////////////////////
// Conbo Output-Knob //
///////////////////////

struct sts_PJ301Port : SVGPort {
	sts_PJ301Port() {
		setSVG(SVG::load(assetPlugin(plugin,"res/PJ301C.svg")));
	}
};

struct sts_PJ301Knob : SVGKnob {
	sts_PJ301Knob() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSVG(SVG::load(assetPlugin(plugin,"res/PJ301O.svg")));
	}
};

///////////////////
//slider
///////////////////

struct sts_SlidePotPink : SVGFader
{
	sts_SlidePotPink()
	{
		Vec margin = Vec(0, 0);
		maxHandlePos = Vec(8, 0).plus(margin);
		minHandlePos = Vec(8, 67).plus(margin);
		background->svg = SVG::load(assetPlugin(plugin, "res/LEDSlider.svg"));
		background->wrap();
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		handle->svg = SVG::load(assetPlugin(plugin, "res/LEDSliderPinkHandle.svg"));
		handle->wrap();
	}
};

struct sts_SlidePotBlack : SVGFader
{
	sts_SlidePotBlack()
	{
		Vec margin = Vec(0, 0);
		maxHandlePos = Vec(8, 0).plus(margin);
		minHandlePos = Vec(8, 67).plus(margin);
		background->svg = SVG::load(assetPlugin(plugin, "res/LEDSlider.svg"));
		background->wrap();
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		handle->svg = SVG::load(assetPlugin(plugin, "res/LEDSliderBlackHandle.svg"));
		handle->wrap();
	}
};

struct sts_SlidePotWhite : SVGFader
{
	sts_SlidePotWhite()
	{
		Vec margin = Vec(0, 0);
		maxHandlePos = Vec(8, 0).plus(margin);
		minHandlePos = Vec(8, 67).plus(margin);
		background->svg = SVG::load(assetPlugin(plugin, "res/LEDSlider.svg"));
		background->wrap();
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		handle->svg = SVG::load(assetPlugin(plugin, "res/LEDSliderWhiteHandle.svg"));
		handle->wrap();
	}
};

struct sts_SlidePotGreen : SVGFader
{
	sts_SlidePotGreen()
	{
		Vec margin = Vec(0, 0);
		maxHandlePos = Vec(8, 0).plus(margin);
		minHandlePos = Vec(8, 67).plus(margin);
		background->svg = SVG::load(assetPlugin(plugin, "res/LEDSlider.svg"));
		background->wrap();
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		handle->svg = SVG::load(assetPlugin(plugin, "res/LEDSliderGreenHandle.svg"));
		handle->wrap();
	}
};

struct sts_SlidePotRed : SVGFader
{
	sts_SlidePotRed()
	{
		Vec margin = Vec(0, 0);
		maxHandlePos = Vec(8, 0).plus(margin);
		minHandlePos = Vec(8, 67).plus(margin);
		background->svg = SVG::load(assetPlugin(plugin, "res/LEDSlider.svg"));
		background->wrap();
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		handle->svg = SVG::load(assetPlugin(plugin, "res/LEDSliderRedHandle.svg"));
		handle->wrap();
	}
};

struct sts_SlidePotBlue : SVGFader
{
	sts_SlidePotBlue()
	{
		Vec margin = Vec(0, 0);
		maxHandlePos = Vec(8, 0).plus(margin);
		minHandlePos = Vec(8, 67).plus(margin);
		background->svg = SVG::load(assetPlugin(plugin, "res/LEDSlider.svg"));
		background->wrap();
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		handle->svg = SVG::load(assetPlugin(plugin, "res/LEDSliderBlueHandle.svg"));
		handle->wrap();
	}
};

struct sts_SlidePotYellow : SVGFader
{
	sts_SlidePotYellow()
	{
		Vec margin = Vec(0, 0);
		maxHandlePos = Vec(8, 0).plus(margin);
		minHandlePos = Vec(8, 67).plus(margin);
		background->svg = SVG::load(assetPlugin(plugin, "res/LEDSlider.svg"));
		background->wrap();
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		handle->svg = SVG::load(assetPlugin(plugin, "res/LEDSliderYellowHandle.svg"));
		handle->wrap();
	}
};

template <typename T> int sign(T val) {
    return (T(0) < val) - (val < T(0));
}


////////////////////
// Additional GUI stuff
////////////////////



#ifdef DEBUG
  #define dbgPrint(a) printf a
#else
  #define dbgPrint(a) (void)0
#endif


#endif

