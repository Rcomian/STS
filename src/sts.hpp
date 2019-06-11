#include "window.hpp"
#include "rack.hpp"


using namespace rack;

extern Plugin *pluginInstance;

//extern Model *modelSwitch2to1x11;
extern Model *modelOdyssey;
extern Model *modelIlliad;
extern Model *modelPolySEQ16;
extern Model *modelRingModulator;
extern Model *modelWaveFolder;
extern Model *modelVU_Poly;
//extern Model *modelLFOPoly;


//extern Model *modelMidiFile;
//extern Model *modelMixer8;
//extern Model *modelSEQEXP;
//extern Model *modelOdy;
//extern Model *modelOdyssey;

#define MAX_POLY_CHANNELS 16

static constexpr float g_audioPeakVoltage = 10.f;
static constexpr float g_controlPeakVoltage = 5.f;

// General constants
static const float lightLambda = 0.075f;
static const bool clockIgnoreOnRun = false;
static const bool retrigGatesOnReset = true;
static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
static const int displayAlpha = 23;
static const std::string lightPanelID = "Classic";
static const std::string darkPanelID = "Dark-valor";
static const unsigned int displayRefreshStepSkips = 256;
static const unsigned int userInputsStepSkipMask = 0xF;// sub interval of displayRefreshStepSkips, since inputs should be more responsive than lights
// above value should make it such that inputs are sampled > 1kHz so as to not miss 1ms triggers

// Component offset constants

static const int hOffsetCKSS = 5;
static const int vOffsetCKSS = 2;
static const int vOffsetCKSSThree = -2;
static const int hOffsetCKSSH = 2;
static const int vOffsetCKSSH = 5;
static const int offsetCKD6 = -1;//does both h and v
static const int offsetCKD6b = 0;//does both h and v
static const int vOffsetDisplay = -2;
static const int offsetIMBigKnob = -6;//does both h and v
static const int offsetIMSmallKnob = 0;//does both h and v
static const int offsetMediumLight = 9;
static const float offsetLEDbutton = 3.0f;//does both h and v
static const float offsetLEDbuttonLight = 4.4f;//does both h and v
static const int offsetTL1105 = 4;//does both h and v
static const int offsetLEDbezel = 1;//does both h and v
static const float offsetLEDbezelLight = 2.2f;//does both h and v
static const float offsetLEDbezelBig = -11;//does both h and v
static const int offsetTrimpot = 3;//does both h and v
static const float blurRadiusRatio = 0.06f;




/*
////////////////////////
//   IM widgets   //
////////////////////////




// Other functions

inline bool calcWarningFlash(long count, long countInit) {
	if ( (count > (countInit * 2l / 4l) && count < (countInit * 3l / 4l)) || (count < (countInit * 1l / 4l)) )
		return false;
	return true;
}	

NVGcolor prepareDisplay(NVGcontext *vg, Rect *box, int fontSize);

void printNote(float cvVal, char* text, bool sharp);

int moveIndex(int index, int indexNext, int numSteps);

};
*/

struct stsBigPushButton : app::SVGSwitch 
{
	stsBigPushButton() 
	{
		momentary = true;
		
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/comp/CKD6b_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/comp/CKD6b_1.svg")));	
	}
};
////////////////////////
//      Knobs         //
////////////////////////

struct sts_Switch : app::SVGSwitch {
	sts_Switch() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts-switchv_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts-switchv_1.svg")));
	}
};

struct sts_Trimpot_Grey : app::SVGKnob {
	sts_Trimpot_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts-Trimpot_Grey.svg")));
	}
};

/*
//  15 px trimpot
struct sts_Trimpot_Black : app::SVGKnob {
	sts_Trimpot_Black() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts-Trimpot_Black.svg")));
	}
};
*/

struct sts_Davies15_Grey : app::SVGKnob {
	sts_Davies15_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Grey_15.svg")));
	}
};

struct sts_Davies47_Grey : app::SVGKnob {
	sts_Davies47_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Grey_47.svg")));
	}
};

struct sts_Davies_snap_47_Grey : app::SVGKnob {
	sts_Davies_snap_47_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		snap = true;
		smooth = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Grey_47.svg")));
	}
};
//sts_Davies_snap_37_Grey
struct sts_Davies_snap_37_Grey : app::SVGKnob {
	sts_Davies_snap_37_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		snap = true;
		smooth = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Grey_37.svg")));
	}
};
struct sts_Davies_37_Grey : app::SVGKnob {
	sts_Davies_37_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Grey_37.svg")));
	}
};


struct sts_BigSnapKnob : app::SVGKnob
{
    sts_BigSnapKnob()
    {
		box.size = Vec(54, 54);
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/bigknob.svg")));
        snap = true;
		smooth = false;
    }
};

struct stsLEDButton : app::SvgSwitch {
	stsLEDButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/stsLEDButton.svg")));
	}
};
struct stsLEDButton1 : app::SvgSwitch {
	stsLEDButton1() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/stsLEDButton.svg")));
	}
};
struct sts_CKSS : app::SVGSwitch {
	sts_CKSS() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_CKSS_1.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_CKSS_0.svg")));
	}
};

struct sts_CKSS3 : app::SVGSwitch {
	sts_CKSS3() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_CKSS4_2.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_CKSS4_1.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_CKSS4_0.svg")));
	}
};

struct sts_CKSS4 : app::SVGSwitch {
	sts_CKSS4() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_CKSS4_3.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_CKSS4_2.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_CKSS4_1.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_CKSS4_0.svg")));
	}
};

/////////

struct sts_Port : SVGPort {
	sts_Port() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts-Port18.svg")));
		
	}
};

///////////////////
//slider
///////////////////

/*
struct BefacoSlidePot : app::SvgSlider {
	BefacoSlidePot() {
		math::Vec margin = math::Vec(3.5, 3.5);
		maxHandlePos = math::Vec(-1, -2).plus(margin);
		minHandlePos = math::Vec(-1, 87).plus(margin);
		setBackgroundSvg(APP->window->loadSvg(asset::system("res/ComponentLibrary/BefacoSlidePot.svg")));
		setHandleSvg(APP->window->loadSvg(asset::system("res/ComponentLibrary/BefacoSlidePotHandle.svg")));
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
	}
};
*/

/** Based on the size of 3mm LEDs */
template <typename BASE>
struct stsMediumLight : BASE {
	stsMediumLight() {
		this->box.size = app::mm2px(math::Vec(2.75, 2.75));
	}
};


struct sts_SlidePotPink : app::SVGSlider
{
	sts_SlidePotPink()
	{
		math::Vec margin = math::Vec(0, 0);
		maxHandlePos = math::Vec(8, 0).plus(margin);
		minHandlePos = math::Vec(8, 67).plus(margin);
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSlider.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSliderPinkHandle.svg")));
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
	}
};


struct sts_SlidePotBlack : app::SVGSlider
{
	sts_SlidePotBlack()
	{
		math::Vec margin = math::Vec(0, 0);
		maxHandlePos = math::Vec(8, 0).plus(margin);
		minHandlePos = math::Vec(8, 67).plus(margin);
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSlider.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSliderBlackHandle.svg")));
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
	}
};

struct sts_SlidePotTeal : app::SVGSlider
{
	sts_SlidePotTeal()
	{
		math::Vec margin = math::Vec(0, 0);
		maxHandlePos = math::Vec(8, 0).plus(margin);
		minHandlePos = math::Vec(8, 67).plus(margin);
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSlider.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSliderTealHandle.svg")));
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		
	}
};


struct sts_SlidePotWhite : app::SVGSlider
{
	sts_SlidePotWhite()
	{
		math::Vec margin = math::Vec(0, 0);
		maxHandlePos = math::Vec(8, 0).plus(margin);
		minHandlePos = math::Vec(8, 67).plus(margin);
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSlider.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSliderWhiteHandle.svg")));
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
	}
};

struct sts_SlidePotGreen : app::SVGSlider
{
	sts_SlidePotGreen()
	{
		math::Vec margin = math::Vec(0, 0);
		maxHandlePos = math::Vec(8, 0).plus(margin);
		minHandlePos = math::Vec(8, 67).plus(margin);
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSlider.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSliderGreenHandle.svg")));
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		
	}
};

struct sts_SlidePotRed : app::SVGSlider
{
	sts_SlidePotRed()
	{
		math::Vec margin = math::Vec(0, 0);
		maxHandlePos = math::Vec(8, 0).plus(margin);
		minHandlePos = math::Vec(8, 67).plus(margin);
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSlider.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSliderRedHandle.svg")));
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
		
	}
};

struct sts_SlidePotBlue : app::SVGSlider
{
	sts_SlidePotBlue()
	{
		math::Vec margin = math::Vec(0, 0);
		maxHandlePos = math::Vec(8, 0).plus(margin);
		minHandlePos = math::Vec(8, 67).plus(margin);
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSlider.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSliderBlueHandle.svg")));
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
	}
};

struct sts_SlidePotYellow : app::SVGSlider
{
	sts_SlidePotYellow()
	{
		math::Vec margin = math::Vec(0, 0);
		maxHandlePos = math::Vec(8, 0).plus(margin);
		minHandlePos = math::Vec(8, 67).plus(margin);
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSlider.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSliderYellowHandle.svg")));
		background->box.pos = margin;
		box.size = background->box.size.plus(margin.mult(2));
	}
};



