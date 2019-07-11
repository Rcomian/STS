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
extern Model *modelLFOPoly;
//extern Model *modelChords;
//extern Model *modelMidiFile;
//extern Model *modelMixer8;
//extern Model *modelSEQEXP;
//extern Model *modelOdy;
//extern Model *modelOdyssey;

#define MAX_POLY_CHANNELS 16
#define GTX__N 6
#define MAX_POLY_CHANNELS 16
#define GTX__2PI 6.283185307179586476925
#define GTX__IO_RADIUS 26.0
#define GTX__SAVE_SVG 0
#define GTX__WIDGET()

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


struct stsBigPushButton : app::SVGSwitch 
{
	stsBigPushButton() 
	{
		momentary = true;
		
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/comp/CKD6b_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/comp/CKD6b_1.svg")));	
	}
};
//============================================================================================================
//! Gratrix Widgets

struct PortInMed : SVGPort
{
	PortInMed()
	{
		//background->svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/PortInMedium.svg"));
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PortInMedium.svg")));
		//background->wrap();
		//box.size = background->box.size;
	}

	static Vec size() { return Vec(25.0, 25.0); } // Copied from SVG so no need to pre-load.
	static Vec pos() { return Vec(12.5, 12.5); }  // Copied from SVG so no need to pre-load.
};

struct PortOutMed : SVGPort
{
	PortOutMed()
	{
		//background->svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/PortOutMedium.svg"));
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PortOutMedium.svg")));
		//background->wrap();
		//box.size = background->box.size;
	}

	static Vec size() { return Vec(25.0, 25.0); } // Copied from SVG so no need to pre-load.
	static Vec pos() { return Vec(12.5, 12.5); }  // Copied from SVG so no need to pre-load.
};

struct KnobSnapSml : RoundKnob
{
	KnobSnapSml()
	{
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/KnobSnapSmall.svg")));
		//box.size = Vec(28, 28);
		snap = true;
	}

	static Vec size() { return Vec(28.0, 28.0); }  // Copied from SVG so no need to pre-load.
	static Vec pos()  { return Vec(14.0, 14.0); }  // Copied from SVG so no need to pre-load.
};
//! \brief Create output function that creates the UI component centered.

template <class Port>
Port *createInputGTX(Vec pos, Module *module, int outputId)
{
	return createInput<Port>(pos.minus(Port::pos()), module, outputId);
}

//============================================================================================================
//! \brief Create output function that creates the UI component centered.

template <class Port>
Port *createOutputGTX(Vec pos, Module *module, int outputId)
{
	return createOutput<Port>(pos.minus(Port::pos()), module, outputId);
}

//============================================================================================================
//! \brief Create param function that creates the UI component centered.

template <class ParamWidget>
ParamWidget *createParamGTX(Vec pos, Module *module, int paramId)     //, float minValue, float maxValue, float defaultValue)
{
	return createParam<ParamWidget>(pos.minus(ParamWidget::pos()), module, paramId);    //, minValue, maxValue, defaultValue);
}

//============================================================================================================


//! \brief ...

inline double dx(double i, std::size_t n) { return  std::sin(GTX__2PI * static_cast<double>(i) / static_cast<double>(n)); }
inline double dy(double i, std::size_t n) { return -std::cos(GTX__2PI * static_cast<double>(i) / static_cast<double>(n)); }
inline double dx(double i               ) { return dx(i, GTX__N); }
inline double dy(double i               ) { return dy(i, GTX__N); }

inline int    gx(double i) { return static_cast<int>(std::floor(0.5 + ((i+0.5) *  90))); }
inline int    gy(double i) { return static_cast<int>(std::floor(8.5 + ((i+1.0) * 102))); }
inline int    fx(double i) { return gx(i); }
inline int    fy(double i) { return gy(i - 0.1); }

inline double rad_n_b() { return 27;    }
inline double rad_n_m() { return 18;    }
inline double rad_n_s() { return 14;    }
inline double rad_l_m() { return  4.75; }
inline double rad_l_s() { return  3.25; }
inline double rad_but() { return  9;    }
inline double rad_scr() { return  8;    }
inline double rad_prt() { return 12.5;  }

inline Vec    tog(double x, double y) { return Vec(x - 7, y - 10); }
inline Vec    n_b(double x, double y) { return Vec(x - rad_n_b(), y - rad_n_b()); }
inline Vec    n_m(double x, double y) { return Vec(x - rad_n_m(), y - rad_n_m()); }
inline Vec    n_s(double x, double y) { return Vec(x - rad_n_s(), y - rad_n_s()); }
inline Vec    l_m(double x, double y) { return Vec(x - rad_l_m(), y - rad_l_m()); }
inline Vec    l_s(double x, double y) { return Vec(x - rad_l_s(), y - rad_l_s()); }
inline Vec    but(double x, double y) { return Vec(x - rad_but(), y - rad_but()); }
inline Vec    scr(double x, double y) { return Vec(x - rad_scr(), y - rad_scr()); }
inline Vec    prt(double x, double y) { return Vec(x - rad_prt(), y - rad_prt()); }

inline Vec    tog(const Vec &a)       { return tog(a.x, a.y); }
inline Vec    n_b(const Vec &a)       { return n_b(a.x, a.y); }
inline Vec    n_m(const Vec &a)       { return n_m(a.x, a.y); }
inline Vec    n_s(const Vec &a)       { return n_s(a.x, a.y); }
inline Vec    l_m(const Vec &a)       { return l_m(a.x, a.y); }
inline Vec    l_s(const Vec &a)       { return l_s(a.x, a.y); }
inline Vec    but(const Vec &a)       { return but(a.x, a.y); }
inline Vec    scr(const Vec &a)       { return scr(a.x, a.y); }
inline Vec    prt(const Vec &a)       { return prt(a.x, a.y); }

inline double px(          std::size_t i) { return GTX__IO_RADIUS * dx(i); }
inline double py(          std::size_t i) { return GTX__IO_RADIUS * dy(i); }
inline double px(double j, std::size_t i) { return gx(j) + px(i); }
inline double py(double j, std::size_t i) { return gy(j) + py(i); }
/*
void debug_out(vector<uint8_t>::const_iterator& iter, int lastSuccess = 0) const 
	{
        std::string s;
        auto start = std::max(&midi.front() + lastSuccess, &*iter - 25);
        auto end = std::min(&midi.back(), &*iter + 5);
        const char hex[] = "0123456789ABCDEF";
        for (auto i = start; i <= end; ++i) {
            if (i == &*iter) s += "[";
            s += "0x";
            s += hex[*i >> 4];
            s += hex[*i & 0xf];
            if (i == &*iter) s += "]";
            s += ", ";
        }
        DEBUG("%s", s.c_str());
    }
*/
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

struct sts_Davies20_Grey : app::SVGKnob {
	sts_Davies20_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Grey_20.svg")));
	}
};

struct sts_Davies25_Grey : app::SVGKnob {
	sts_Davies25_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Grey_25.svg")));
	}
};

struct sts_Davies30_Grey : app::SVGKnob {
	sts_Davies30_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Grey_30.svg")));
	}
};

struct sts_Davies30_Red : app::SVGKnob {
	sts_Davies30_Red() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Red_30.svg")));
	}
};

struct sts_Davies30_Yellow : app::SVGKnob {
	sts_Davies30_Yellow() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Yellow_30.svg")));
	}
};


struct sts_Davies30_Blue : app::SVGKnob {
	sts_Davies30_Blue() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Blue_30.svg")));
	}
};

struct sts_Davies30_Teal : app::SVGKnob {
	sts_Davies30_Teal() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Teal_30.svg")));
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

struct sts_Davies_snap_25_Grey : app::SVGKnob {
	sts_Davies_snap_25_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		snap = true;
		smooth = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Grey_25.svg")));
	}
};

struct sts_Davies_snap_35_Grey : app::SVGKnob {
	sts_Davies_snap_35_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		snap = true;
		smooth = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Grey_35.svg")));
	}
};
struct sts_Davies_snap_25_White : app::SVGKnob {
	sts_Davies_snap_25_White() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		snap = true;
		smooth = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_White_25.svg")));
	}
};
struct sts_Davies_snap_25_Yellow : app::SVGKnob {
	sts_Davies_snap_25_Yellow() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		snap = true;
		smooth = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Yellow_23.svg")));
	}
};
struct sts_Davies_25_Yellow : app::SVGKnob {
	sts_Davies_25_Yellow() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Yellow_23.svg")));
	}
};
struct sts_Davies_snap_25_Red : app::SVGKnob {
	sts_Davies_snap_25_Red() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		snap = true;
		smooth = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Red_23.svg")));
	}
};
struct sts_Davies_25_Red : app::SVGKnob {
	sts_Davies_25_Red() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Red_23.svg")));
	}
};
struct sts_Davies_snap_25_Blue : app::SVGKnob {
	sts_Davies_snap_25_Blue() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		snap = true;
		smooth = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Blue_23.svg")));
	}
};
struct sts_Davies_25_Blue : app::SVGKnob {
	sts_Davies_25_Blue() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Red_23.svg")));
	}
};
struct sts_Davies_snap_25_Teal : app::SVGKnob {
	sts_Davies_snap_25_Teal() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		snap = true;
		smooth = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Teal_23.svg")));
	}
};
struct sts_Davies_25_Teal : app::SVGKnob {
	sts_Davies_25_Teal() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Teal_23.svg")));
	}
};
struct sts_Davies_25_Grey : app::SVGKnob {
	sts_Davies_25_Grey() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/sts_Davies1900_Grey_25.svg")));
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
struct stsLEDButtonSmall : app::SvgSwitch {
	stsLEDButtonSmall() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/stsLEDButtonSmall.svg")));
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



