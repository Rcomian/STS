#include "sts.hpp"
#include "app.hpp"
#include "dsp/digital.hpp"
#include "dsp/filter.hpp"
#include "dsp/resampler.hpp"
#include "dsp/ode.hpp"
#include "dsp/minblep.hpp"
#include "window.hpp"
#include "osdialog.h"
#include "asset.hpp"
#include <jansson.h>
#include <random>
#include <cmath>
#include <sstream>
#include <iomanip>
#include "rack.hpp"
#define pi 3.14159265359
#include <string.h>
#include "pffft.h"
#include "engine/Module.hpp"

using namespace std;
using namespace rack;

static const int UPSAMPLE = 2;

struct Mix_Loop2
{
	///  vars
	float output;

	void step(float in0,
			  float in1,
			  float sl0, float sl1)
	{
		// Apply fader gain
		float gain0 = std::pow(sl0, 2.f);
		float gain1 = std::pow(sl1, 2.f);

		output = ((in0 *= gain0) + (in1 *= gain1));
	}
};

struct Mix_Loop3
{
	///  vars
	float output;

	void step(float in0,
			  float in1,
			  float in2,
			  float sl0, float sl1, float sl2)
	{
		// Apply fader gain
		float gain0 = std::pow(sl0, 2.f);
		float gain1 = std::pow(sl1, 2.f);
		float gain2 = std::pow(sl2, 2.f);

		output = ((in0 *= gain0) + (in1 *= gain1) + (in2 *= gain2));
	}
};

struct Mix_Loop4
{
	///  vars
	float output;

	void step(float in0,
			  float in1,
			  float in2,
			  float in3,
			  float sl0, float sl1, float sl2, float sl3)
	{
		// Apply fader gain
		float gain0 = std::pow(sl0, 2.f);
		float gain1 = std::pow(sl1, 2.f);
		float gain2 = std::pow(sl2, 2.f);
		float gain3 = std::pow(sl3, 2.f);

		output = ((in0 *= gain0) + (in1 *= gain1) + (in2 *= gain2) + (in3 *= gain3));
	}
};

struct RMOD
{
	float wd = 1.0f;
	float A;
	float B;

	float dPA;
	float dMA;
	float dPB;
	float dMB;
	float vin;
	float vc;
	float res_RM;

	float diode_sim(float in)
	{
		if (in < 0)
			return 0;
		else
			return 0.2 * log(1.0 + exp(10 * (in - 1)));
	}

	void step(float vin, float vc)
	{
		A = 0.5 * vin + vc;
		B = vc - 0.5 * vin;

		dPA = diode_sim(A);
		dMA = diode_sim(-A);
		dPB = diode_sim(B);
		dMB = diode_sim(-B);

		res_RM = dPA + dMA - dPB - dMB;
	}
};

//  HPF    Bidoo Perco
struct MultiFilter
{
	float q;
	float freq;
	float smpRate;
	float hp = 0.0f, bp = 0.0f, lp = 0.0f, mem1 = 0.0f, mem2 = 0.0f;

	void setParams(float freq, float q, float smpRate)
	{
		this->freq = freq;
		this->q = q;
		this->smpRate = smpRate;
	}

	void calcOutput(float sample)
	{
		float g = tan(pi * freq / smpRate);
		float R = 1.0f / (2.0f * q);
		hp = (sample - (2.0f * R + g) * mem1 - mem2) / (1.0f + 2.0f * R * g + g * g);
		bp = g * hp + mem1;
		lp = g * bp + mem2;
		mem1 = g * hp + bp;
		mem2 = g * bp + lp;
	}
};

////  NOISE start
struct NoiseGenerator
{
	std::mt19937 rng;
	std::uniform_real_distribution<float> uniform;

	NoiseGenerator() : uniform(-1.0f, 1.0f)
	{
		rng.seed(std::random_device()());
	}

	float white()
	{
		return uniform(rng);
	}
};

struct PinkFilter
{
	float b0, b1, b2, b3, b4, b5, b6; // Coefficients
	float y;						  // Out
	void process(float x)
	{
		b0 = 0.99886f * b0 + x * 0.0555179f;
		b1 = 0.99332f * b1 + x * 0.0750759f;
		b2 = 0.96900f * b2 + x * 0.1538520f;
		b3 = 0.86650f * b3 + x * 0.3104856f;
		b4 = 0.55000f * b4 + x * 0.5329522f;
		b5 = -0.7616f * b5 - x * 0.0168980f;
		y = b0 + b1 + b2 + b3 + b4 + b5 + b6 + x * 0.5362f;
		b6 = x * 0.115926f;
	}

	float pink()
	{
		return y;
	}
};

struct NotchFilter
{
	float freq, bandwidth;	// Params
	float a0, a1, a2, b1, b2; // Coefficients
	float x1, x2;			  // In
	float y1, y2;			  // out

	void setFreq(float value)
	{
		freq = value;
		computeCoefficients();
	}

	void setBandwidth(float value)
	{
		bandwidth = value;
		computeCoefficients();
	}

	void process(float x)
	{
		float y = a0 * x + a1 * x1 + a2 * x2 + b1 * y1 + b2 * y2;

		x2 = x1;
		x1 = x;
		y2 = y1;
		y1 = y;
	}

	float notch()
	{
		return y1;
	}

	void computeCoefficients()
	{
		float c2pf = cos(2.0f * M_PI * freq);
		float r = 1.0f - 3.0f * bandwidth;
		float r2 = r * r;
		float k = (1.0f - (2.0f * r * c2pf) + r2) / (2.0f - 2.0f * c2pf);

		a0 = k;
		a1 = -2.0f * k * c2pf;
		a2 = k;
		b1 = 2.0f * r * c2pf;
		b2 = -r2;
	}
};

/////////  VCF SIMD ////////////////////////////

template <typename T>
static T clip(T x)
{
	// return std::tanh(x);
	// Pade approximant of tanh
	x = simd::clamp(x, -3.f, 3.f);
	return x * (27 + x * x) / (27 + 9 * x * x);
}

template <typename T>
struct LadderFilter
{
	T omega0;
	T resonance = 1;
	T state[4];
	T input;

	LadderFilter()
	{
		reset();
		setCutoff(0);
	}

	void reset()
	{
		for (int i = 0; i < 4; i++)
		{
			state[i] = 0;
		}
	}

	void setCutoff(T cutoff)
	{
		omega0 = 2 * T(M_PI) * cutoff;
	}

	void process(T input, T dt)
	{
		dsp::stepRK4(T(0), dt, state, 4, [&](T t, const T x[], T dxdt[]) {
			T inputc = clip(input - resonance * x[3]);
			T yc0 = clip(x[0]);
			T yc1 = clip(x[1]);
			T yc2 = clip(x[2]);
			T yc3 = clip(x[3]);

			dxdt[0] = omega0 * (inputc - yc0);
			dxdt[1] = omega0 * (yc0 - yc1);
			dxdt[2] = omega0 * (yc1 - yc2);
			dxdt[3] = omega0 * (yc2 - yc3);
		});

		this->input = input;
	}

	T lowpass()
	{
		return state[3];
	}
	T highpass()
	{
		// TODO This is incorrect when `resonance > 0`. Is the math wrong?
		return clip((input - resonance * state[3]) - 4 * state[0] + 6 * state[1] - 4 * state[2] + state[3]);
	}
};

//////    SLEW  //////////////

struct SLEW_STEP
{
	float OUT_OUTPUT_SLEW;
	float out_SLEW = 0.0;
	float shape = 0.05; // params[SHAPE_PARAM_SLEW].getValue();
	// minimum and maximum slopes in volts per second
	const float slewMin = 0.1;
	const float slewMax = 10000.0;
	// Amount of extra slew per voltage difference
	const float shapeScale = 1 / 10.0;

	void step(float in_SLEW,
			  float slew_slider,
			  float sTime)
	{

		// Risewe know the nxt note is
		if (in_SLEW > out_SLEW)
		{
			float rise = slew_slider;
			float slew = slewMax * std::pow(slewMin / slewMax, rise);
			out_SLEW += slew * crossfade(1.0f, shapeScale * (in_SLEW - out_SLEW), shape) * sTime;
			if (out_SLEW > in_SLEW)
				out_SLEW = in_SLEW;
		}
		// Fall
		else if (in_SLEW < out_SLEW)
		{
			float fall = slew_slider;
			float slew = slewMax * std::pow(slewMin / slewMax, fall);
			out_SLEW -= slew * crossfade(1.0f, shapeScale * (out_SLEW - in_SLEW), shape) * sTime;
			if (out_SLEW < in_SLEW)
				out_SLEW = in_SLEW;
		}
		//OUT_OUTPUT_SLEW = out_SLEW;
	}
};

//  ADSR
struct ADSR
{

	float env = 0.f;
	bool sustaining = false;
	bool resting = false;
	bool gated = false;
	bool decaying = false;
	dsp::SchmittTrigger adsrTrigger;

	void step(float gate,
			  float trigger,
			  float attack,
			  float decay,
			  float sustain,
			  float release,
			  float sTime)
	{

		gated = gate >= 1.f;
		if (adsrTrigger.process(trigger))
			decaying = false;
		const float base = 20000.0f;
		const float maxTime = 10.0f;
		if (gated)
		{
			if (decaying)
			{
				// Decay
				if (decay < 1e-4)
				{
					env = sustain;
				}
				else
				{
					env += std::pow(base, 1 - decay) / maxTime * (sustain - env) * sTime;
				}
			}
			else
			{
				// Attack
				// Skip ahead if attack is all the way down (infinitely fast)
				if (attack < 1e-4)
				{
					env = 1.f;
				}
				else
				{
					env += std::pow(base, 1 - attack) / maxTime * (1.01f - env) * sTime;
				}
				if (env >= 1.f)
				{
					env = 1.f;
					decaying = true;
				}
			}
		}
		else
		{
			// Release
			if (release < 1e-4)
			{
				env = 0.0f;
			}
			else
			{
				env += std::pow(base, 1 - release) / maxTime * (0.f - env) * sTime;
			}
			decaying = false;
		}

		sustaining = isNear(env, sustain, 1e-3);
		resting = isNear(env, 0.0f, 1e-3);
	}
};

/////////////  LFO STRUCT   /////////////////
template <typename T>
struct LowFrequencyOscillator
{
	T phase = 0.f;
	T pw = 0.5f;
	T freq = 1.f;
	bool invert = false;
	bool bipolar = false;
	T resetState = T::mask();

	void setPitch(T pitch)
	{
		pitch = simd::fmin(pitch, 10.f);
		freq = simd::pow(2.f, pitch);
	}
	void setPulseWidth(T pw)
	{
		const T pwMin = 0.01f;
		this->pw = clamp(pw, pwMin, 1.f - pwMin);
	}
	void setReset(T reset)
	{
		reset = simd::rescale(reset, 0.1f, 2.f, 0.f, 1.f);
		T on = (reset >= 1.f);
		T off = (reset <= 0.f);
		T triggered = ~resetState & on;
		resetState = simd::ifelse(off, 0.f, resetState);
		resetState = simd::ifelse(on, T::mask(), resetState);
		phase = simd::ifelse(triggered, 0.f, phase);
	}
	void step(float dt)
	{
		T deltaPhase = simd::fmin(freq * dt, 0.5f);
		phase += deltaPhase;
		phase -= (phase >= 1.f) & 1.f;
	}
	T sin()
	{
		T p = phase;
		if (!bipolar)
			p -= 0.25f;
		T v = simd::sin(2 * M_PI * p);
		if (invert)
			v *= -1.f;
		if (!bipolar)
			v += 1.f;
		return v;
	}
	T tri()
	{
		T p = phase;
		if (bipolar)
			p += 0.25f;
		T v = 4.f * simd::fabs(p - simd::round(p)) - 1.f;
		if (invert)
			v *= -1.f;
		if (!bipolar)
			v += 1.f;
		return v;
	}
	T saw()
	{
		T p = phase;
		if (!bipolar)
			p -= 0.5f;
		T v = 2.f * (p - simd::round(p));
		if (invert)
			v *= -1.f;
		if (!bipolar)
			v += 1.f;
		return v;
	}
	T sqr()
	{
		T v = simd::ifelse(phase < pw, 1.f, -1.f);
		if (invert)
			v *= -1.f;
		if (!bipolar)
			v += 1.f;
		return v;
	}
	T light()
	{
		return 1.f - 2.f * phase;
	}
};

////////  Oscillator  /////  EvenVCO  ///
struct Oscillator
{

	Oscillator()
	{
	}

	float phase = 0.0;

	float sync = 0.0;
	//float crossing;
	float tri = 0.0;
	float sine;
	float saw;
	float square;
	bool halfPhase = false;

	dsp::MinBlepGenerator<16, 32> triSquareMinBlep;
	dsp::MinBlepGenerator<16, 32> triMinBlep;
	dsp::MinBlepGenerator<16, 32> sineMinBlep;
	dsp::MinBlepGenerator<16, 32> doubleSawMinBlep;
	dsp::MinBlepGenerator<16, 32> sawMinBlep;
	dsp::MinBlepGenerator<16, 32> squareMinBlep;

	//dsp::RCFilter triFilter;

	void step(float pwm_param,
			  float pwm_input,
			  float fine_tune,
			  float freq_osc,
			  float fm_input,
			  float pitch_input,
			  float sTime) //Module::ProcessArgs &args)
	{

		// Compute frequency, pitch is 1V/oct
		float pitch = 1.f + std::round(freq_osc) + fine_tune / 12.f;
		pitch += pitch_input;
		pitch += fm_input / 4.f;
		float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);
		freq = clamp(freq, 0.f, 20000.f);

		// Pulse width
		float pw = pwm_param + pwm_input / 5.f;
		const float minPw = 0.05;
		pw = rescale(clamp(pw, -1.f, 1.f), -1.f, 1.f, minPw, 1.f - minPw);

		// Advance phase
		//float deltaPhase = clamp(freq * args.sampleTime, 1e-6f, 0.5f);
		float deltaPhase = clamp(freq * sTime, 1e-6f, 0.5f);
		float oldPhase = phase;
		phase += deltaPhase;

		if (oldPhase < 0.5 && phase >= 0.5)
		{
			float crossing = -(phase - 0.5) / deltaPhase;
			triSquareMinBlep.insertDiscontinuity(crossing, 2.f);
			doubleSawMinBlep.insertDiscontinuity(crossing, -2.f);
		}

		if (!halfPhase && phase >= pw)
		{
			float crossing = -(phase - pw) / deltaPhase;
			squareMinBlep.insertDiscontinuity(crossing, 2.f);
			halfPhase = true;
		}

		// Reset phase if at end of cycle
		if (phase >= 1.f)
		{
			phase -= 1.f;
			float crossing = -phase / deltaPhase;
			triSquareMinBlep.insertDiscontinuity(crossing, -2.f);
			doubleSawMinBlep.insertDiscontinuity(crossing, -2.f);
			squareMinBlep.insertDiscontinuity(crossing, -2.f);
			sawMinBlep.insertDiscontinuity(crossing, -2.f);
			halfPhase = false;
		}
		// Outputs
		float triSquare = (phase < 0.5) ? -1.f : 1.f;
		triSquare += triSquareMinBlep.process();

		// Integrate square for triangle
		tri += 4.f * triSquare * freq * sTime;
		tri *= (1.f - 40.f * sTime);
		//tri += 4.f * triSquare * freq * args.sampleTime;
		//tri *= (1.f - 40.f *args.sampleTime);
		//float even = 0.55 * (doubleSaw + 1.27 * sine);
		sine = -std::cos(2 * M_PI * phase);
		float doubleSaw = (phase < 0.5) ? (-1.f + 4.f * phase) : (-1.f + 4.f * (phase - 0.5));
		doubleSaw += doubleSawMinBlep.process();
		saw = -1.f + 2.f * phase;
		saw += sawMinBlep.process();
		square = (phase < pw) ? -1.f : 1.f;
		square += squareMinBlep.process();
	}
};

struct VCA
{
	float v;

	void step(float in,
			  float gainSlider,
			  float lin,
			  float exp)
	{
		v = in * gainSlider;

		v *= clamp(lin / 10.0f, 0.0f, 1.0f);
		const float expBase = 50.0f;

		v *= rescale(powf(expBase, clamp(exp / 10.0f, 0.0f, 1.0f)), 1.0f, expBase, 0.0f, 1.0f);
		//out.value = v;
	}
};

struct Odyssey : Module
{
	enum ParamIds
	{
		OCTAVE_PARAM,
		PITCHBEND_PARAM,
		ATTEN_PB,
		PARAM_PORTA,
		PARAM_PORTA_SHAPE,
		SLIDER_PARAM_LAG,
		SWITCH_PARAM_SH_LFO_KEY,
		SWITCH_PARAM_LFO_TYPE,
		SWITCH_PARAM_OSC1_TYPE,
		SWITCH_PARAM_OSC2_TYPE,
		SWITCH_PARAM_OSC3_TYPE,

		ENUMS(SLIDER_PARAM_FM_OSC1_LVL, 2),
		ENUMS(SLIDER_PARAM_FM_OSC2_LVL, 2),
		ENUMS(SLIDER_PARAM_FM_OSC3_LVL, 2),
		SLIDER_PARAM_PWM_OSC1_LVL,
		SLIDER_PARAM_PWM_OSC2_LVL,
		SLIDER_PARAM_PWM_OSC3_LVL,
		ENUMS(SLIDER_PARAM_SH_LVL, 2),
		ENUMS(SLIDER_PARAM_TO_AUDIO_LVL, 4),
		ENUMS(SLIDER_PARAM_TO_FILTER_LVL, 3),
		SLIDER_PARAM_AR_ADSR_LVL,

		SWITCH_PARAM_LFO_MOD_FM_OSC1,
		SWITCH_PARAM_LFO_MOD_PWM_OSC1,
		SWITCH_PARAM_LFO_MOD_FM_OSC2,
		SWITCH_PARAM_LFO_MOD_PWM_OSC2,
		SWITCH_PARAM_LFO_MOD_FM_OSC3,
		SWITCH_PARAM_LFO_MOD_PWM_OSC3,
		SWITCH_PARAM_LFO_MOD_VCF,

		ENUMS(SWITCH_PARAM_FM_OSC1, 2),
		ENUMS(SWITCH_PARAM_FM_OSC2, 2),
		ENUMS(SWITCH_PARAM_FM_OSC3, 2),
		ENUMS(SWITCH_PARAM_PWM_OSC1, 2),
		ENUMS(SWITCH_PARAM_PWM_OSC2, 2),
		ENUMS(SWITCH_PARAM_PWM_OSC3, 2),
		ENUMS(SWITCH_PARAM_SH, 2),
		ENUMS(SWITCH_PARAM_AUDIO, 4),
		ENUMS(SWITCH_PARAM_FILTER, 3),
		ENUMS(SWITCH_PARAM_AR_ADSR, 2),

		// PRESETS
		PARAM_SAVE_PRESET,
		PARAM_LOAD_PRESET,
		// ADSR
		ATTACK_PARAM_1,
		DECAY_PARAM_1,
		SUSTAIN_PARAM_1,
		RELEASE_PARAM_1,
		ATTACK_PARAM_2,
		DECAY_PARAM_2,
		SUSTAIN_PARAM_2,
		RELEASE_PARAM_2,
		//ADSR switches
		SWITCH_PARAM_ADSR1_SW1,
		SWITCH_PARAM_ADSR1_SW2,
		SWITCH_PARAM_ADSR2_SW1,
		SWITCH_PARAM_ADSR2_SW2,
		// VCF
		FREQ_PARAM_VCF,
		FINE_PARAM_VCF,
		RES_PARAM_VCF,
		FREQ_CV_PARAM_VCF,
		DRIVE_PARAM_VCF,
		// HPF
		HPF_FILTER_FREQ,
		HPF_FMOD_PARAM,
		// OSC1
		FREQ_PARAM_OSC1,
		FINE_PARAM_OSC1,
		//FM_PARAM_OSC1,
		PW_PARAM_OSC1,
		PWM_PARAM_OSC1,
		WAVE_ATTEN_OSC1,
		// OSC2
		FREQ_PARAM_OSC2,
		FINE_PARAM_OSC2,
		SYNC_PARAM_OSC2,
		//FM_PARAM_OSC2,
		PW_PARAM_OSC2,
		PWM_PARAM_OSC2,
		WAVE_ATTEN_OSC2,
		/// OSC3
		FREQ_PARAM_OSC3,
		FINE_PARAM_OSC3,
		//FM_PARAM_OSC2,
		PW_PARAM_OSC3,
		PWM_PARAM_OSC3,
		WAVE_ATTEN_OSC3,
		// LFO
		LFO_FM_PARAM,
		FREQ_PARAM_LFO,
		WAVE_ATTEN_LFO,
		// VCA
		LEVEL1_PARAM_VCA,
		LEVEL2_PARAM_VCA,
		//Noise
		QUANTA_PARAM,
		SWITCH_PARAM_NOISE_SEL,
		//SLEW
		SHAPE_PARAM_SLEW,

		NUM_PARAMS
	};
	enum InputIds
	{
		// Need for everything
		IN_VOLT_OCTAVE_INPUT_1,
		IN_GATE_INPUT_1,
		IN_TRIGGER_INPUT_1,
		IN_VELOCITY,
		IN_CV_LFO,
		IN_CV_PB,
		IN_CV_MOD,
		IN_CV_1,
		IN_CV_2,
		IN_PORTA_ON_OFF,
		IN_CV_OCTAVE,
		IN_CV_HPF_CUTOFF,
		INPUT_EXT_VCF,
		WAVE_CV_OSC1,
		WAVE_CV_OSC2,
		WAVE_CV_OSC3,
		WAVE_CV_LFO,

		ENUMS(FILTER_IN, 18),

		IN_INPUT_1,

		NUM_INPUTS
	};

	enum OutputIds
	{
		MAIN_AUDIO_OUT,
		MONO_OUTPUT,
		OUTPUT_TO_EXT_VCF,
		OUT_FILTER_MIX_OUTPUT,
		OUT_FREQ_PARAM_VCF,
		OUT_RES_PARAM_VCF,
		OUT_DRIVE_PARAM_VCF,
		OUT_OUTPUT_1,

		NUM_OUTPUTS
	};
	enum LightIds
	{
		// LFO
		ENUMS(PHASE_LIGHT, 3),
		// HPF
		LEARN_LIGHT,
		// ADSR
		ATTACK_LIGHT_1,
		DECAY_LIGHT_1,
		SUSTAIN_LIGHT_1,
		RELEASE_LIGHT_1,
		ATTACK_LIGHT_2,
		DECAY_LIGHT_2,
		SUSTAIN_LIGHT_2,
		RELEASE_LIGHT_2,

		ENUMS(VU1_LIGHT, 10),

		NUM_LIGHTS
	};

	//switches
	float IN_FM_OSC1_SW_1[2];
	float IN_FM_OSC2_SW_1[2];
	float IN_FM_OSC3_SW_1[2];
	float IN_PWM_OSC1_SW_1[1];
	float IN_PWM_OSC2_SW_1[1];
	float IN_PWM_OSC3_SW_1[1];
	float IN_SH_SW_1[2];
	float IN_AUDIO_SW_1[4];
	float IN_FILTER_SW_1[3];
	float IN_AR_ADSR_SW_1[1];

	float IN_FM_OSC1_SW_2[2];
	float IN_FM_OSC2_SW_2[2];
	float IN_FM_OSC3_SW_2[2];
	float IN_PWM_OSC1_SW_2[1];
	float IN_PWM_OSC2_SW_2[1];
	float IN_PWM_OSC3_SW_2[1];
	float IN_SH_SW_2[2];
	float IN_AUDIO_SW_2[4];
	float IN_FILTER_SW_2[3];
	float IN_AR_ADSR_SW_2[1];

	int panelStyle = 0;

	float OUT_OUTPUT_TO_FM_OSC1[MAX_POLY_CHANNELS];
	float OUT_OUTPUT_TO_FM_OSC2[MAX_POLY_CHANNELS];
	float OUT_OUTPUT_TO_FM_OSC3[MAX_POLY_CHANNELS];
	float PWM_TO_OSC1[MAX_POLY_CHANNELS];
	float PWM_TO_OSC2[MAX_POLY_CHANNELS];
	float PWM_TO_OSC3[MAX_POLY_CHANNELS];
	float SH_MIX[MAX_POLY_CHANNELS];
	float AUDIO_MIX[MAX_POLY_CHANNELS];
	float FILTER_MIX[MAX_POLY_CHANNELS];
	float OUT_OUTPUT_AR_ADSR[MAX_POLY_CHANNELS];
	// VCA

	// VCF
	float FREQ_INPUT_VCF[MAX_POLY_CHANNELS];
	float RES_INPUT_VCF;
	float DRIVE_INPUT_VCF;
	float VCF_INPUT[MAX_POLY_CHANNELS];
	float LPF_OUTPUT_VCF[MAX_POLY_CHANNELS];
	float F_IN0[MAX_POLY_CHANNELS];
	float F_IN1[MAX_POLY_CHANNELS];
	float F_IN2[MAX_POLY_CHANNELS];
	float F_IN3[MAX_POLY_CHANNELS];

	// Upsampler<UPSAMPLE, 8> inputUpsampler;
	// Decimator<UPSAMPLE, 8> lowpassDecimator;
	// Decimator<UPSAMPLE, 8> highpassDecimator;

	//HPF
	float HPFin[MAX_POLY_CHANNELS];
	float Finput[MAX_POLY_CHANNELS];
	float IN_HPF[MAX_POLY_CHANNELS];
	float Q_INPUT_HPF;
	float OUT_HP[MAX_POLY_CHANNELS];
	float CUTOFF_INPUT_HPF;
	float HPF_Q_PARAM;

	//// Pitch,Ports,Octave
	float PORTA_OUT[MAX_POLY_CHANNELS];
	float OUT_OUTPUT_OCTAVE;
	float OUT_OUTPUT_PITCHBEND;
	float WHITE_OUTPUT;
	float PINK_OUTPUT;
	float NOISE_OUT[MAX_POLY_CHANNELS];

	//  ADSR
	float ENVELOPE_OUTPUT_ADSR_1[MAX_POLY_CHANNELS];
	float ENVELOPE_OUTPUT_ADSR_2[MAX_POLY_CHANNELS];
	float mix_AR_ADSR;
	float ch_AR_ADSR;
	float gate_signal_ADSR1[MAX_POLY_CHANNELS];
	float gate_signal_ADSR2[MAX_POLY_CHANNELS];
	float trigger_signal_in[MAX_POLY_CHANNELS];

	// Ring Mod
	bool RM_ON;
	float SIGNAL_INPUT;
	float CARRIER_INPUT;
	float RM_MODULATED_OUTPUT[MAX_POLY_CHANNELS] = {};

	// OSC1
	float PITCH_INPUT_OSC1;
	float FM_INPUT_OSC1[MAX_POLY_CHANNELS];
	float PW_INPUT_OSC1[MAX_POLY_CHANNELS];
	float PW_OSC1[MAX_POLY_CHANNELS] = {};
	float SIN_OUTPUT_OSC1[MAX_POLY_CHANNELS] = {};
	float TRI_OUTPUT_OSC1[MAX_POLY_CHANNELS] = {};
	float SAW_OUTPUT_OSC1[MAX_POLY_CHANNELS] = {};
	float SQR_OUTPUT_OSC1[MAX_POLY_CHANNELS] = {};
	float OSC1_OUTPUT[MAX_POLY_CHANNELS] = {};

	// OSC2
	float PITCH_INPUT_OSC2;
	float FM_INPUT_OSC2[MAX_POLY_CHANNELS];
	float PW_INPUT_OSC2[MAX_POLY_CHANNELS];
	float PW_OSC2[MAX_POLY_CHANNELS] = {};
	float SIN_OUTPUT_OSC2[MAX_POLY_CHANNELS] = {};
	float TRI_OUTPUT_OSC2[MAX_POLY_CHANNELS] = {};
	float SAW_OUTPUT_OSC2[MAX_POLY_CHANNELS] = {};
	float SQR_OUTPUT_OSC2[MAX_POLY_CHANNELS] = {};
	float OSC2_OUTPUT[MAX_POLY_CHANNELS] = {};

	// OSC3
	float PITCH_INPUT_OSC3;
	float FM_INPUT_OSC3[MAX_POLY_CHANNELS];
	float PW_INPUT_OSC3[MAX_POLY_CHANNELS];
	float PW_OSC3[MAX_POLY_CHANNELS] = {};
	float SIN_OUTPUT_OSC3[MAX_POLY_CHANNELS] = {};
	float TRI_OUTPUT_OSC3[MAX_POLY_CHANNELS] = {};
	float SAW_OUTPUT_OSC3[MAX_POLY_CHANNELS] = {};
	float SQR_OUTPUT_OSC3[MAX_POLY_CHANNELS] = {};
	float OSC3_OUTPUT[MAX_POLY_CHANNELS] = {};

	float LFO_SIN_OUTPUT[MAX_POLY_CHANNELS];
	float LFO_SQR_OUTPUT[MAX_POLY_CHANNELS];
	float LFO_SAW_OUTPUT[MAX_POLY_CHANNELS];
	float LFO_TRI_OUTPUT[MAX_POLY_CHANNELS];
	float LFO_OUTPUT[MAX_POLY_CHANNELS];
	float LFO_FM_IN[MAX_POLY_CHANNELS];

	// SLEW
	float OUT_OUTPUT_SLEW;

	// S/H
	float TRIGGER_SH_INPUT[MAX_POLY_CHANNELS];
	float IN_SH_INPUT;
	float IN_SW_SH_LFO_KEY;
	float OUT_SH_OUTPUT[MAX_POLY_CHANNELS];
	float sh_value;
	float OUT_TO_LAG[MAX_POLY_CHANNELS];
	float OUTPUT_SH[MAX_POLY_CHANNELS];

	float new_slider;
	float inCV1[MAX_POLY_CHANNELS];
	float inCV2[MAX_POLY_CHANNELS];
	//  POLY
	float sum;
	float vel_in[MAX_POLY_CHANNELS] = {};
	float PITCH_INPUT;
	int channels = 1;
	float oddy_poly_out[MAX_POLY_CHANNELS] = {};

	Oscillator oscillator1[MAX_POLY_CHANNELS];
	Oscillator oscillator2[MAX_POLY_CHANNELS];
	Oscillator oscillator3[MAX_POLY_CHANNELS];
	float wave[MAX_POLY_CHANNELS];
	float wave2[MAX_POLY_CHANNELS];
	float wave3[MAX_POLY_CHANNELS];

	//LowFrequencyOscillator LFoscillator[MAX_POLY_CHANNELS];
	LowFrequencyOscillator<simd::float_4> oscillators[4];
	RMOD rmod1[MAX_POLY_CHANNELS];
	MultiFilter hpfFilter[MAX_POLY_CHANNELS];
	ADSR adsr1[MAX_POLY_CHANNELS];
	ADSR adsr2[MAX_POLY_CHANNELS];
	NoiseGenerator noise;
	PinkFilter pinkFilter;
	//LadderFilter filter[MAX_POLY_CHANNELS];
	LadderFilter<simd::float_4> filters[4];
	VCA vca1[MAX_POLY_CHANNELS];

	Mix_Loop3 FilterMix;
	Mix_Loop4 AudioMix;
	Mix_Loop2 S_H_Mix;
	Mix_Loop2 FM_1_Mix;
	Mix_Loop2 FM_2_Mix;
	Mix_Loop2 FM_3_Mix;

	dsp::SchmittTrigger sh_trigger[MAX_POLY_CHANNELS];

	SLEW_STEP porta_slew[MAX_POLY_CHANNELS];
	SLEW_STEP sh_slew[MAX_POLY_CHANNELS];
	dsp::SlewLimiter portaM;

	dsp::VuMeter2 vuMeter;
	dsp::ClockDivider vuDivider;
	dsp::ClockDivider lightDivider;
	dsp::ClockDivider lightDividerLFO;

	Odyssey()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(SWITCH_PARAM_NOISE_SEL, 0.f, 1.f, 1.f, "Noise Select");
		configParam(PARAM_PORTA, 0.0f, 1.f, 0.0f, "Portamento", "%", 0, 100); //.4
		//configParam(PARAM_PORTA_SHAPE, 0.f, 1.f, 0.5f, "Portamento Shape");
		configParam(OCTAVE_PARAM, -4.f, 2.f, -1.f, "Octaves");			  //, " ", 0.0, 1.0, 0.0);
		configParam(PITCHBEND_PARAM, -1.f, 1.f, 0.f, "Pitch Bend");		  //, " ", 0.0, 1.0, 0.0);
		configParam(ATTEN_PB, 0.f, 1.f, 0.0f, "Pitch Bend Attenverter "); //, " ", 0.0, 1.0, 0.0);
		//////////////////
		// Sliders Top Row
		//////////////////

		configParam(WAVE_ATTEN_OSC1, 0.f, 1.f, 0.1f, "Wave CV Attenverter OSC1 ");
		configParam(WAVE_ATTEN_OSC1, 0.f, 1.f, 0.1f, "Wave CV Attenverter OSC2 ");
		configParam(WAVE_ATTEN_OSC1, 0.f, 1.f, 0.1f, "Wave CV Attenverter OSC3 ");
		configParam(WAVE_ATTEN_LFO, 0.f, 1.f, 0.1f, "Wave CV Attenverter LFO ");

		configParam(FREQ_PARAM_OSC1, -5.f, 4.f, 0.f, "Frequency VCO 1"); //," ", 0.5);
		configParam(FINE_PARAM_OSC1, -7.f, 7.f, 0.f, "Fine Frequency VCO1", " semitones");
		configParam(SWITCH_PARAM_OSC1_TYPE, 0.f, 3, 0.f, "VCO1 Wave Select");
		configParam(FREQ_PARAM_OSC2, -4.f, 3.f, 0.f, "Frequency VCO 2"); //," ", 0.5);
		configParam(FINE_PARAM_OSC2, -7.f, 7.f, 0.f, "Fine Frequency VCO2", " semitones");
		configParam(SWITCH_PARAM_OSC2_TYPE, 0.f, 3, 0.f, "VCO2 Wave Select");
		configParam(FREQ_PARAM_OSC3, -4.f, 3.f, 0.f, "Frequency VCO 3"); //," ", 0.5);
		configParam(FINE_PARAM_OSC3, -7.f, 7.f, 0.f, "Fine Frequency VCO3", " semitones");
		configParam(SWITCH_PARAM_OSC3_TYPE, 0.f, 3, 0.f, "VCO3 Wave Select");
		configParam(FREQ_PARAM_LFO, -8.f, 10.f, 1.f, "LFO Frequency");
		configParam(LFO_FM_PARAM, 0.f, 1.f, 0.f, "LFO FM Attenuverter");
		configParam(SWITCH_PARAM_LFO_TYPE, 0.f, 3, 0.f, "LFO Wave Select");

		configParam(SLIDER_PARAM_LAG, 0.f, 1.f, 0.f, "S & H Lag");
		configParam(SLIDER_PARAM_SH_LVL + 0, 0.f, 1.f, 0.f, "S & H Mixer 1 ");
		;
		configParam(SLIDER_PARAM_SH_LVL + 1, 0.f, 1.f, 0.f, "S & H Mixer 2 ");
		configParam(FREQ_PARAM_VCF, 0.f, 1.f, 1.f, "VCF Cutoff Frequency");
		configParam(RES_PARAM_VCF, 0.f, 1.f, 0.f, "VCF Resonance", "%", 0.f, 100.f);
		configParam(DRIVE_PARAM_VCF, 0.f, 1.f, 0.f, "VCF Drive ");
		configParam(FREQ_CV_PARAM_VCF, -1.f, 1.f, 0.f, "VCF Cutoff Frequency Modulation", "%", 0.f, 100.f);

		//configParam(HPF_Q_PARAM, 0.f, 1.f, 0.f, "HPF Resonance ");
		configParam(HPF_FILTER_FREQ, 0.0f, 1.0f, 0.0f, "HPF Cutoff Frequency ");						 //, " ",0.0, 1.0, 0.0);
		configParam(HPF_FMOD_PARAM, -1.f, 1.f, 0.f, "HPF Cutoff Frequency Modulation", "%", 0.f, 100.f); //, " ",0.0, 1.0, 0.0);

		//configParam(LEVEL1_PARAM_VCA, 0.0f, 1.0f, 0.0f, "VCA Level NO ADSR", "%", 0, 100);

		configParam(ATTACK_PARAM_2, 0.03f, 1.f, 0.f, "Attack ADSR 2");   //, " ",0.0, 1.0, 0.0);
		configParam(DECAY_PARAM_2, 0.f, 1.f, 0.f, "Decay ADSR 2");	 //, " ",0.0, 1.0, 0.0);
		configParam(SUSTAIN_PARAM_2, 0.f, 1.f, 1.f, "Sustain ADSR 2"); //, " ",0.0, 1.0, 0.0);
		configParam(RELEASE_PARAM_2, 0.03f, 1.f, 0.f, "Release ADSR 2"); //, " ",0.0, 1.0, 0.0);
		/////////////////////
		// Sliders Bottom Row
		/////////////////////
		configParam(SLIDER_PARAM_FM_OSC1_LVL + 0, 0.f, 1.f, 0.f, "FM Level 1 VCO1");   //, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_FM_OSC1_LVL + 1, 0.f, 1.f, 0.f, "FM Level 2 VCO1");   //, " ",0.0, 1.0, 0.0);
		configParam(PW_PARAM_OSC1, -1.0f, 1.0f, 0.0f, "Pulse Width VCO1");			   //, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_PWM_OSC1_LVL, 0.f, 1.f, 0.f, "Pulse Width MOD VCO1"); //, " ",0.0, 1.0, 0.0);

		configParam(SLIDER_PARAM_FM_OSC2_LVL + 0, 0.f, 1.f, 0.f, "FM Level 1 VCO2");   //, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_FM_OSC2_LVL + 1, 0.f, 1.f, 0.f, "FM Level 2 VCO2");   //, " ",0.0, 1.0, 0.0);
		configParam(PW_PARAM_OSC2, -1.0f, 1.0f, 0.0f, "Pulse Width VCO2");			   //, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_PWM_OSC2_LVL, 0.f, 1.f, 0.f, "Pulse Width MOD VCO2"); //, " ",0.0, 1.0, 0.0);

		configParam(SLIDER_PARAM_FM_OSC3_LVL + 0, 0.0, 1.0, 0.0, "FM Level 1 VCO3");   //, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_FM_OSC3_LVL + 1, 0.0, 1.0, 0.0, "FM Level 2 VCO3");   //, " ",0.0, 1.0, 0.0);
		configParam(PW_PARAM_OSC3, -1.0f, 1.0f, 0.0f, "Pulse Width VCO3");			   //, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_PWM_OSC3_LVL, 0.0, 1.0, 0.0, "Pulse Width MOD VCO3"); //, " ",0.0, 1.0, 0.0);

		configParam(SLIDER_PARAM_TO_AUDIO_LVL + 0, 0.0, 1.0, 0.0, "Noise / Ring Mod Level"); //, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_TO_AUDIO_LVL + 1, 0.0, 1.0, 1.0, "VCO1 Mix Level");		 //, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_TO_AUDIO_LVL + 2, 0.0, 1.0, 0.0, "VCO2 Mix Level");		 //, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_TO_AUDIO_LVL + 3, 0.0, 1.0, 0.0, "VCO3 Mix Level");		 //, " ",0.0, 1.0, 0.0);

		configParam(SLIDER_PARAM_TO_FILTER_LVL + 0, 0.0, 1.0, 0.0, "Keyboard CV level to VCF"); //, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_TO_FILTER_LVL + 1, 0.0, 1.0, 0.0, "S & H CV level to VCF");	//, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_TO_FILTER_LVL + 2, 0.0, 1.0, 0.0, "ADSR CV level to VCF");		//, " ",0.0, 1.0, 0.0);
		configParam(SLIDER_PARAM_AR_ADSR_LVL, 0.0, 1.0, 0.9f, "ADSR to VCA Level", "%", 0, 100);

		configParam(ATTACK_PARAM_1, 0.03f, 1.0, 0.0, "Attack ADSR 1");   //, " ",0.0, 1.0, 0.0);
		configParam(DECAY_PARAM_1, 0.0, 1.0, 0.0, "Decay ADSR 1");	 //, " ",0.0, 1.0, 0.0);
		configParam(SUSTAIN_PARAM_1, 0.0, 1.0, 1.0, "Sustain ADSR 1"); //, " ",0.0, 1.0, 0.0);
		configParam(RELEASE_PARAM_1, 0.03f, 1.0, 0.0, "Release ADSR 1"); //, " ",0.0, 1.0, 0.0);

		////   OSC2
		//configParam(SYNC_PARAM_OSC2, 0.0f, 1.0f, 1.0f, " "); //, " ",0.0, 1.0, 0.0);

		/////////////////// Switches ////////////////////
		configParam(SWITCH_PARAM_FM_OSC1 + 0, 0.0, 1.0, 1.0, " ");			//, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_LFO_MOD_FM_OSC1 + 0, 0.0, 1.0, 1.0, " ");  //, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_FM_OSC1 + 1, 0.0, 1.0, 0.0, " ");			//, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_PWM_OSC1 + 0, 0.0, 1.0, 1.0, " ");			//, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_LFO_MOD_PWM_OSC1 + 0, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);

		configParam(SWITCH_PARAM_FM_OSC2 + 0, 0.0, 1.0, 1.0, " ");			//, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_LFO_MOD_FM_OSC2 + 0, 0.0, 1.0, 1.0, " ");  //, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_FM_OSC2 + 1, 0.0, 1.0, 0.0, " ");			//, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_PWM_OSC2 + 0, 0.0, 1.0, 1.0, " ");			//, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_LFO_MOD_PWM_OSC2 + 0, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);

		configParam(SWITCH_PARAM_FM_OSC3 + 0, 0.0, 1.0, 1.0, " ");			//, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_LFO_MOD_FM_OSC3 + 0, 0.0, 1.0, 1.0, " ");  //, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_FM_OSC3 + 1, 0.0, 1.0, 0.0, " ");			//, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_PWM_OSC3 + 0, 0.0, 1.0, 1.0, " ");			//, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_LFO_MOD_PWM_OSC3 + 0, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);

		configParam(SWITCH_PARAM_SH_LFO_KEY, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_SH + 0, 0.0, 1.0, 1.0, " ");	 //, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_SH + 1, 0.0, 1.0, 1.0, " ");	 //, " ",0.0, 1.0, 0.0);

		configParam(SWITCH_PARAM_AUDIO + 0, 0.0, 1.0, 1.0, "Noise / Ring Mod Switch "); //, " ",0.0, 1.0, 0.0);
		//configParam(SWITCH_PARAM_AUDIO + 1, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);
		//configParam(SWITCH_PARAM_AUDIO + 2, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);
		//configParam(SWITCH_PARAM_AUDIO + 3, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);

		configParam(SWITCH_PARAM_FILTER + 0, 0.0, 1.0, 1.0, " ");  //, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_FILTER + 1, 0.0, 1.0, 1.0, " ");  //, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_LFO_MOD_VCF, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_FILTER + 2, 0.0, 1.0, 1.0, " ");  //, " ",0.0, 1.0, 0.0);

		configParam(SWITCH_PARAM_AR_ADSR + 0, 0.0, 1.0, 1.0, " ");

		configParam(SWITCH_PARAM_ADSR1_SW1, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);
		//configParam(SWITCH_PARAM_ADSR1_SW2, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);
		configParam(SWITCH_PARAM_ADSR2_SW1, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);
		//configParam(SWITCH_PARAM_ADSR2_SW2, 0.0, 1.0, 1.0, " "); //, " ",0.0, 1.0, 0.0);

		vuMeter.lambda = 1 / 0.1f;
		vuDivider.setDivision(16);
		lightDivider.setDivision(256);
		//vuCounter.setPeriod(16);
		//lightCounter.setPeriod(256);

		onReset();
	};

	// persistence

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

	// *********************************************************************************
	// *********************************************************************************
	// ********************************  STEP STARTS HERE  *****************************
	// *********************************************************************************
	// *********************************************************************************

	void process(const ProcessArgs &args) override
	{

		//if (outputs[MAIN_AUDIO_OUT].isConnected() || outputs[MONO_OUTPUT].isConnected() || outputs[OUTPUT_TO_EXT_VCF].isConnected()) 
		if (inputs[IN_VOLT_OCTAVE_INPUT_1].isConnected())
		{
			//channels = inputs[IN_VOLT_OCTAVE_INPUT_1].getChannels();
			int channels = std::max(1, inputs[IN_VOLT_OCTAVE_INPUT_1].getChannels());
		
			using simd::float_4;

			////////////////////////////////////////////////////////
			//////////               VCA 1 ADSR      ////////////
			////////////////////////////////////////////////////////

			if (inputs[IN_VELOCITY].isConnected())

			{
				for (int i = 0; i < channels; ++i)
				{
					vel_in[i] = inputs[IN_VELOCITY].getPolyVoltage(i);
				}
			}
			else
			{
				for (int i = 0; i < channels; ++i)
				{
					vel_in[i] = 10.0f;
				}
			}

			for (int i = 0; i < channels; ++i)
			{
				vca1[i].step(OUT_HP[i], params[SLIDER_PARAM_AR_ADSR_LVL].getValue(), vel_in[i], OUT_OUTPUT_AR_ADSR[i]); //exp
				oddy_poly_out[i] = vca1[i].v;																			// / 10;
			}
			outputs[MAIN_AUDIO_OUT].setChannels(channels);
			outputs[MAIN_AUDIO_OUT].writeVoltages(oddy_poly_out);

			sum = 0.f;
			for (int i = 0; i < channels; i++)
			{
				sum += oddy_poly_out[i];
			}
			outputs[MONO_OUTPUT].setVoltage(sum);

			////////////////////////////////////////////////////////
			///////////////////////  VU Meter //////////////////////
			////////////////////////////////////////////////////////

			if (vuDivider.process())
			{
				vuMeter.process(args.sampleTime * vuDivider.getDivision(), outputs[MONO_OUTPUT].value / 10.f);
			}

			// Set channel lights infrequently
			if (lightDivider.process())
			{
				lights[VU1_LIGHT + 0].setBrightness(vuMeter.getBrightness(0.f, 0.f));
				for (int i = 1; i < 10; i++)
				{
					lights[VU1_LIGHT + i].setBrightness(vuMeter.getBrightness(-3.f * i, -3.f * (i - 1)));
				}
			}

			/////////////////////////////////////////////////////////
			///////////////  ADSR  Input switches   /////////////////
			//////////  SWITCH_PARAM_ADSR1_SW1 ADSR2_SW1 ////////////

			//if (inputs[IN_TRIGGER_INPUT_1].isConnected())
			//{
			//	inputs[IN_TRIGGER_INPUT_1].setChannels(channels);
			//	inputs[IN_TRIGGER_INPUT_1].writeVoltages(trigger_signal_in);
			//}

			if (params[SWITCH_PARAM_ADSR1_SW1].getValue() && !inputs[IN_GATE_INPUT_1].isConnected())
			{
				for (int i = 0; i < channels; ++i)
				{
					gate_signal_ADSR1[i] = LFO_SQR_OUTPUT[i];
				}
			}

			if (params[SWITCH_PARAM_ADSR2_SW1].getValue() && !inputs[IN_GATE_INPUT_1].isConnected())
			{
				for (int i = 0; i < channels; ++i)
				{
					gate_signal_ADSR2[i] = LFO_SQR_OUTPUT[i];
				}
			}
			////////////////////////////////////////////////////////
			//////////////////  ADSR 1 Bottom      /////////////////
			////////////////////////////////////////////////////////

			for (int i = 0; i < channels; ++i)
			{
				adsr1[i].step(
					inputs[IN_GATE_INPUT_1].getPolyVoltage(i),
					inputs[IN_TRIGGER_INPUT_1].getPolyVoltage(i),
					//trigger_signal_in[i],
					params[ATTACK_PARAM_1].getValue(),
					params[DECAY_PARAM_1].getValue(),
					params[SUSTAIN_PARAM_1].getValue(),
					params[RELEASE_PARAM_1].getValue(),
					args.sampleTime);

				ENVELOPE_OUTPUT_ADSR_1[i] = 10.0f * adsr1[i].env;

				// Lights
				lights[ATTACK_LIGHT_1].value = (adsr1[0].gated && !adsr1[0].decaying) ? 1.0f : 0.0f;
				lights[DECAY_LIGHT_1].value = (adsr1[0].gated && adsr1[0].decaying && !adsr1[0].sustaining) ? 1.0f : 0.0f;
				lights[SUSTAIN_LIGHT_1].value = (adsr1[0].gated && adsr1[0].decaying && adsr1[0].sustaining) ? 1.0f : 0.0f;
				lights[RELEASE_LIGHT_1].value = (!adsr1[0].gated && !adsr1[0].resting) ? 1.0f : 0.0f;
			}

			/////////////////////////////////////////////////////////
			///////////////////     ADSR 2 Top      /////////////////
			/////////////////////////////////////////////////////////

			for (int i = 0; i < channels; ++i)
			{
				adsr2[i].step(
					inputs[IN_GATE_INPUT_1].getPolyVoltage(i),
					inputs[IN_TRIGGER_INPUT_1].getPolyVoltage(i),
					//trigger_signal_in[i],
					params[ATTACK_PARAM_2].getValue(),
					params[DECAY_PARAM_2].getValue(),
					params[SUSTAIN_PARAM_2].getValue(),
					params[RELEASE_PARAM_2].getValue(),
					args.sampleTime);

				ENVELOPE_OUTPUT_ADSR_2[i] = 10.0f * adsr2[i].env;

				// Lights
				lights[ATTACK_LIGHT_2].value = (adsr2[id].gated && !adsr2[i].decaying) ? 1.0f : 0.0f;
				lights[DECAY_LIGHT_2].value = (adsr2[i].gated && adsr2[i].decaying && !adsr2[i].sustaining) ? 1.0f : 0.0f;
				lights[SUSTAIN_LIGHT_2].value = (adsr2[i].gated && adsr2[i].decaying && adsr2[i].sustaining) ? 1.0f : 0.0f;
				lights[RELEASE_LIGHT_2].value = (!adsr2[i].gated && !adsr2[i].resting) ? 1.0f : 0.0f;
			}
			////////////////////////////////////////////////////////
			///////////////  ADSR TO VCA slider  ///////////////////
			////////////////////////////////////////////////////////

			for (int i = 0; i < channels; ++i)
			{
				if (params[SWITCH_PARAM_AR_ADSR].getValue())
				{
					OUT_OUTPUT_AR_ADSR[i] = ENVELOPE_OUTPUT_ADSR_2[i];
				}
				else
				{
					OUT_OUTPUT_AR_ADSR[i] = ENVELOPE_OUTPUT_ADSR_1[i];
				}

				OUT_OUTPUT_AR_ADSR[i] *= std::pow(params[SLIDER_PARAM_AR_ADSR_LVL].getValue(), 2.f);
			}

			// Octave Switch
			OUT_OUTPUT_OCTAVE = params[OCTAVE_PARAM].getValue();
			// Pitch bend
			if (inputs[IN_CV_PB].isConnected())
			{
				OUT_OUTPUT_PITCHBEND = inputs[IN_CV_PB].getVoltage() * params[ATTEN_PB].getValue();
			}
			else
			{
				OUT_OUTPUT_PITCHBEND = params[PITCHBEND_PARAM].getValue();
			}

			//////////// Sample and hold mixer   (2)  ////////////
			///////////  SWITCH_PARAM_FILTER +0,1,2  /////////////
			////////////  SWITCH_PARAM_LFO_MOD_VCF   /////////////
			if (params[SLIDER_PARAM_SH_LVL + 0].getValue() > 0.1f || params[SLIDER_PARAM_SH_LVL + 1].getValue() > 0.1f)
			{
				for (int i = 0; i < channels; ++i)
				{
					///  #0 - Slider 1
					if (params[SWITCH_PARAM_SH + 0].getValue())
					{
						F_IN0[i] = SAW_OUTPUT_OSC1[i];
					}
					else
					{
						F_IN0[i] = SQR_OUTPUT_OSC1[i];
					}

					///  #1 - Slider 2
					if (params[SWITCH_PARAM_SH + 1].getValue())
					{
						F_IN1[i] = NOISE_OUT[i];
					}
					else
					{
						F_IN1[i] = SQR_OUTPUT_OSC2[i];
					}
					///////////  Call Mix_Loop 2
					S_H_Mix.step(F_IN0[i],
								 F_IN1[i],
								 params[SLIDER_PARAM_SH_LVL + 0].getValue(),
								 params[SLIDER_PARAM_SH_LVL + 1].getValue());

					SH_MIX[i] = S_H_Mix.output;
				}

				////////////////////////////////////////////////////////
				/////////////////    Sample & Hold    //////////////////
				//////////////////////////////////Thanks to Southpole///

				for (int i = 0; i < channels; ++i)
				{
					if (params[SWITCH_PARAM_SH_LFO_KEY].getValue())
					{
						TRIGGER_SH_INPUT[i] = LFO_OUTPUT[i];
					}
					else
					{
						TRIGGER_SH_INPUT[i] = inputs[IN_GATE_INPUT_1].getVoltage();
					}

					if (sh_trigger[i].process(TRIGGER_SH_INPUT[i]))
						OUT_TO_LAG[i] = SH_MIX[i];
					//OUTPUT_SH[i] = SH_MIX[i];
				}

				//	S/H lag
				for (int i = 0; i < channels; ++i)
				{
					if (params[SLIDER_PARAM_LAG].value > 0.1f)
					{

						sh_slew[i].step(
							OUT_TO_LAG[i],
							params[SLIDER_PARAM_LAG].getValue(),
							args.sampleTime);
						OUTPUT_SH[i] = sh_slew[i].out_SLEW;
					}
					else
					{
						OUTPUT_SH[i] = OUT_TO_LAG[i];
					}
				}
			}

			////////////////////////////////////////////////////////
			//////////////////     Portamento     //////////////////
			////////////////////////////////////////////////////////

			//inputs[IN_VOLT_OCTAVE_INPUT_1].getVoltages(voct);

			if (inputs[IN_PORTA_ON_OFF].isConnected())
			{
				for (int i = 0; i < channels; ++i)
				{
					if (OUT_OUTPUT_AR_ADSR[i] > .1)
					{
						if (params[PARAM_PORTA].getValue() > 0 && inputs[IN_PORTA_ON_OFF].getPolyVoltage(i) > 0)
						{
							porta_slew[i].step(inputs[IN_VOLT_OCTAVE_INPUT_1].getPolyVoltage(i), params[PARAM_PORTA].getValue(), args.sampleTime);
							PORTA_OUT[i] = porta_slew[i].out_SLEW;
						}
						else
						{
							PORTA_OUT[i] = inputs[IN_VOLT_OCTAVE_INPUT_1].getPolyVoltage(i);
						}
					}
				}
			}
			else
			{
				for (int i = 0; i < channels; ++i)
				{
					if (OUT_OUTPUT_AR_ADSR[i] > .1)
					{
						if (params[PARAM_PORTA].getValue() > 0)
						{
							porta_slew[i].step(inputs[IN_VOLT_OCTAVE_INPUT_1].getPolyVoltage(i), params[PARAM_PORTA].getValue(), args.sampleTime);
							PORTA_OUT[i] = porta_slew[i].out_SLEW;
						}
						else
						{
							PORTA_OUT[i] = inputs[IN_VOLT_OCTAVE_INPUT_1].getPolyVoltage(i);
						}
					}
				}
			}

			////////////////////////////////////////////////////////
			//////////////////////     LFO     /////////////////////
			////////////////////////////////////////////////////////
			float freqParam = params[FREQ_PARAM_LFO].getValue();
			float waveParam = params[WAVE_ATTEN_LFO].getValue();
			float fmParam = params[LFO_FM_PARAM].getValue();

			for (int c = 0; c < channels; c += 4)
			{
				auto *oscillator = &oscillators[c / 4];
				float_4 pitch = freqParam;
				// FM, polyphonic
				pitch += float_4::load(inputs[IN_CV_LFO].getVoltages(c)) * fmParam;
				//pitch += inputs[IN_CV_LFO].gePolyVoltageSimd<float_4>(c) * fmParam;
				oscillator->setPitch(pitch);

				// Wave
				float_4 wave = waveParam;
				inputs[WAVE_CV_LFO].getVoltage();
				if (inputs[WAVE_CV_LFO].getChannels() == 1)
					wave += inputs[WAVE_CV_LFO].getVoltage() / 10.f * 3.f;
				else
					wave += float_4::load(inputs[WAVE_CV_LFO].getVoltages(c)) / 10.f * 3.f;
				//wave += inputs[WAVE_CV_LFO].getPolyVoltageSimd<float_4>(c) / 10.f * 3.f;
				wave = clamp(wave, 0.f, 3.f);

				// Settings
				//oscillator->invert = (params[INVERT_PARAM].getValue() == 0.f);
				//oscillator->bipolar = (params[OFFSET_PARAM].getValue() == 0.f);
				oscillator->step(args.sampleTime);
				//	oscillator->setReset(inputs[IN_TRIGGER_INPUT_1].getVoltages(c));

				// Outputs
				if (inputs[WAVE_CV_LFO].isConnected())
				{
					//outputs[OUT_OUTPUT_1].setChannels(channels);
					float_4 v = 0.f;
					v += oscillator->sin() * simd::fmax(0.f, 1.f - simd::fabs(wave - 0.f));
					v += oscillator->tri() * simd::fmax(0.f, 1.f - simd::fabs(wave - 1.f));
					v += oscillator->saw() * simd::fmax(0.f, 1.f - simd::fabs(wave - 2.f));
					v += oscillator->sqr() * simd::fmax(0.f, 1.f - simd::fabs(wave - 3.f));
					v *= 5.f;
					//v.store(outputs[OUT_OUTPUT_1].getVoltages(c));
					v.store(LFO_OUTPUT);
				}
				else
				{
					if (params[SWITCH_PARAM_LFO_TYPE].getValue() == 0)
					{
						//outputs[OUT_OUTPUT_1].setChannels(channels);
						float_4 v = 5.f * oscillator->sin();
						//v.store(outputs[OUT_OUTPUT_1].getVoltages(c));
						v.store(LFO_OUTPUT);
					}
					else if (params[SWITCH_PARAM_LFO_TYPE].getValue() == 1)
					{
						//outputs[OUT_OUTPUT_1].setChannels(channels);
						float_4 v = 5.f * oscillator->tri();
						//v.store(outputs[OUT_OUTPUT_1].getVoltages(c));
						v.store(LFO_OUTPUT);
					}
					else if (params[SWITCH_PARAM_LFO_TYPE].getValue() == 2)
					{
						//outputs[OUT_OUTPUT_1].setChannels(channels);
						float_4 v = 5.f * oscillator->saw();
						//v.store(outputs[OUT_OUTPUT_1].getVoltages(c));
						v.store(LFO_OUTPUT);
					}
					else if (params[SWITCH_PARAM_LFO_TYPE].getValue() == 3)
					{
						//outputs[OUT_OUTPUT_1].setChannels(channels);
						float_4 v = 5.f * oscillator->sqr();
						//v.store(outputs[OUT_OUTPUT_1].getVoltages(c));
						v.store(LFO_OUTPUT);
					}
				}
			}

			// Light
			if (lightDividerLFO.process())
			{
				if (channels == 1)
				{
					float lightValue = oscillators[0].light().s[0];
					lights[PHASE_LIGHT + 0].setSmoothBrightness(-lightValue, args.sampleTime * lightDivider.getDivision());
					lights[PHASE_LIGHT + 1].setSmoothBrightness(lightValue, args.sampleTime * lightDivider.getDivision());
					lights[PHASE_LIGHT + 2].setBrightness(0.f);
				}
				else
				{
					lights[PHASE_LIGHT + 0].setBrightness(0.f);
					lights[PHASE_LIGHT + 1].setBrightness(0.f);
					lights[PHASE_LIGHT + 2].setBrightness(1.f);
				}
			}

			//channels = inputs[IN_VOLT_OCTAVE_INPUT_1].getChannels();
			////////////////////////////////////////////////////////
			//////////////    FM IN OSC1         ///////////////////
			////////////////////////////////////////////////////////

			for (int i = 0; i < channels; ++i)
			{
				if (OUT_OUTPUT_AR_ADSR[i] > .1)
				{
					if (params[SLIDER_PARAM_FM_OSC1_LVL + 0].getValue() > 0.1f || params[SLIDER_PARAM_FM_OSC1_LVL + 1].getValue() > 0.1f)
					{
						///  #1 - MW, LFO,  ADSR to VCO
						if (params[SWITCH_PARAM_FM_OSC1 + 0].getValue() && !inputs[IN_CV_1].isConnected())
						{
							F_IN0[i] = SH_MIX[i]; //SH Mix
						}
						else if (params[SWITCH_PARAM_FM_OSC1 + 0].getValue() && inputs[IN_CV_1].isConnected())
						{
							F_IN0[i] = inputs[IN_CV_1].getPolyVoltage(i); //CV_IN_1
						}
						else if (!params[SWITCH_PARAM_FM_OSC1 + 0].getValue() && !params[SWITCH_PARAM_LFO_MOD_FM_OSC1].getValue() && inputs[IN_CV_MOD].isConnected())
						{
							F_IN0[i] = inputs[IN_CV_MOD].getPolyVoltage(i); //Mod Wheel
						}
						else
						{
							F_IN0[i] = LFO_OUTPUT[i]; //   LFO_OUTPUT
						}

						///  #2 - S&H & ADSR12 to Filter
						if (params[SWITCH_PARAM_FM_OSC1 + 1].getValue())
						{
							F_IN1[i] = OUTPUT_SH[i]; // S&H
						}
						else
						{
							F_IN1[i] = ENVELOPE_OUTPUT_ADSR_1[i]; // ADSR 1
						}

						///////////  Call Mix_Loop 2
						FM_1_Mix.step(F_IN0[i],
									  F_IN1[i],
									  params[SLIDER_PARAM_FM_OSC1_LVL + 0].getValue(),
									  params[SLIDER_PARAM_FM_OSC1_LVL + 1].getValue());

						FM_INPUT_OSC1[i] = FM_1_Mix.output;
						//FM_INPUT_OSC1[i] = clamp(FM_INPUT_OSC1[i], 0.f, 10.f);
					}
					////////////////////////////////////////////////////////
					///////////////   PWM IN To OSC1 1 (1)   ///////////////
					////////////////////////////////////////////////////////

					///  #1 - MW, LFO,  ADSR to VCO
					if (params[SWITCH_PARAM_PWM_OSC1].getValue())
					{
						PWM_TO_OSC1[i] = ENVELOPE_OUTPUT_ADSR_1[i]; //ENVELOPE_OUTPUT_ADSR_1
					}
					else if (!params[SWITCH_PARAM_PWM_OSC1].getValue() && !params[SWITCH_PARAM_LFO_MOD_FM_OSC1].getValue() && inputs[IN_CV_MOD].isConnected())
					{
						PWM_TO_OSC1[i] = inputs[IN_CV_MOD].getPolyVoltage(i); //Mod Wheel
					}
					else
					{
						PWM_TO_OSC1[i] = LFO_OUTPUT[i]; //   LFO_OUTPUT
					}

					PWM_TO_OSC1[i] *= std::pow(params[SLIDER_PARAM_PWM_OSC1_LVL].getValue(), 2.f);

					//////////////////////////////////////////////////////////////
					/////////////////////  VCO 1   ///////////////////////////////
					//////////////////////////////////////////////////////////////

					PITCH_INPUT = PORTA_OUT[i] + OUT_OUTPUT_TO_FM_OSC1[i] + OUT_OUTPUT_OCTAVE + OUT_OUTPUT_PITCHBEND; ////////  +porta_out

					oscillator1[i].step(
						params[PW_PARAM_OSC1].getValue(),
						PWM_TO_OSC1[i],
						params[FINE_PARAM_OSC1].getValue(),
						params[FREQ_PARAM_OSC1].getValue(),
						FM_INPUT_OSC1[i],
						PITCH_INPUT,
						args.sampleTime);
					// Set outputs

					if (inputs[WAVE_CV_OSC1].isConnected())
					{
						wave[i] = params[WAVE_ATTEN_OSC1].getValue() + (inputs[WAVE_CV_OSC1].getPolyVoltage(i) / 3);
						wave[i] = clamp(wave[i], 0.f, 3.f);

						if (wave[i] < 1.f)
							OSC1_OUTPUT[i] = (crossfade(oscillator1[i].sine, oscillator1[i].tri, wave[i])) * 5.f;
						else if (wave[i] < 2.f)
							OSC1_OUTPUT[i] = (crossfade(oscillator1[i].tri, oscillator1[i].saw, wave[i] - 1.f)) * 5.f;
						else
							OSC1_OUTPUT[i] = (crossfade(oscillator1[i].saw, oscillator1[i].square, wave[i] - 2.f)) * 5.f;
					}
					else
					{
						SIN_OUTPUT_OSC1[i] = 5.f * oscillator1[i].sine;
						TRI_OUTPUT_OSC1[i] = 5.f * oscillator1[i].tri;
						//EVEN_OUTPUT_OSC1 = 5.f * oscillator1[i].even;
						SAW_OUTPUT_OSC1[i] = 5.f * oscillator1[i].saw;
						SQR_OUTPUT_OSC1[i] = 5.f * oscillator1[i].square;
						if (params[SWITCH_PARAM_OSC1_TYPE].getValue() == 0)
						{
							OSC1_OUTPUT[i] = 5.f * oscillator1[i].sine;
						}
						else if (params[SWITCH_PARAM_OSC1_TYPE].getValue() == 1)
						{
							OSC1_OUTPUT[i] = 5.f * oscillator1[i].tri;
						}
						else if (params[SWITCH_PARAM_OSC1_TYPE].getValue() == 2)
						{
							OSC1_OUTPUT[i] = 5.f * oscillator1[i].saw;
						}
						else if (params[SWITCH_PARAM_OSC1_TYPE].getValue() == 3)
						{
							OSC1_OUTPUT[i] = 5.f * oscillator1[i].square;
						}
					}
				}
			}
			////////////////////////////////////////////////////////
			///////////////    FM IN OSC2    ///////////////////
			////////////////////////////////////////////////////////

			for (int i = 0; i < channels; ++i)
			{
				if (OUT_OUTPUT_AR_ADSR[i] > .1)
				{
					if (params[SLIDER_PARAM_FM_OSC2_LVL + 0].getValue() > 0.1f || params[SLIDER_PARAM_FM_OSC2_LVL + 1].getValue() > 0.1f)
					{
						///  #1 - MW, LFO,  ADSR to VCO
						if (params[SWITCH_PARAM_FM_OSC2 + 0].getValue() && !inputs[IN_CV_1].isConnected())
						{
							F_IN0[i] = SH_MIX[i]; //SH Mix
						}
						else if (params[SWITCH_PARAM_FM_OSC2 + 0].getValue() && inputs[IN_CV_1].isConnected())
						{
							F_IN0[i] = inputs[IN_CV_1].getPolyVoltage(i); //CV_IN_1
						}
						else if (!params[SWITCH_PARAM_FM_OSC2 + 0].getValue() && !params[SWITCH_PARAM_LFO_MOD_FM_OSC2].getValue() && inputs[IN_CV_MOD].isConnected())
						{
							F_IN0[i] = inputs[IN_CV_MOD].getPolyVoltage(i); //Mod Wheel
						}
						else
						{
							F_IN0[i] = LFO_OUTPUT[i]; //   LFO_OUTPUT
						}

						///  #2 - S&H & ADSR12 to Filter
						if (params[SWITCH_PARAM_FM_OSC2 + 1].getValue())
						{
							F_IN1[i] = OUTPUT_SH[i]; // S&H
						}
						else
						{
							F_IN1[i] = ENVELOPE_OUTPUT_ADSR_1[i]; // ADSR 1
						}

						///////////  Call Mix_Loop 2
						FM_2_Mix.step(F_IN0[i],
									  F_IN1[i],
									  params[SLIDER_PARAM_FM_OSC2_LVL + 0].getValue(),
									  params[SLIDER_PARAM_FM_OSC2_LVL + 1].getValue());

						FM_INPUT_OSC2[i] = FM_2_Mix.output;

						//FM_INPUT_OSC2[i] = clamp(FM_INPUT_OSC2[i], 0.f, 10.f);
					}
					////////////////////////////////////////////////////////
					///////////////   PWM IN To OSC2        ///////////////
					////////////////////////////////////////////////////////

					///  #1 - MW, LFO,  ADSR to VCO
					if (params[SWITCH_PARAM_PWM_OSC2].getValue())
					{
						PWM_TO_OSC2[i] = ENVELOPE_OUTPUT_ADSR_1[i]; //ENVELOPE_OUTPUT_ADSR_1
					}
					else if (!params[SWITCH_PARAM_PWM_OSC2].getValue() && !params[SWITCH_PARAM_LFO_MOD_VCF].getValue() && inputs[IN_CV_MOD].isConnected())
					{
						PWM_TO_OSC2[i] = inputs[IN_CV_MOD].getPolyVoltage(i); //Mod Wheel
					}
					else
					{
						PWM_TO_OSC2[i] = LFO_OUTPUT[i]; //   LFO_OUTPUT
					}

					PWM_TO_OSC2[i] *= std::pow(params[SLIDER_PARAM_PWM_OSC2_LVL].getValue(), 2.f);

					//////////////////////////////////////////////////////////////
					/////////////////////  VCO 2 /////////////////////////////////
					//////////////////////////////////////////////////////////////

					PITCH_INPUT = PORTA_OUT[i] + OUT_OUTPUT_TO_FM_OSC2[i] + OUT_OUTPUT_OCTAVE + OUT_OUTPUT_PITCHBEND; ///+porta_out

					oscillator2[i].step(
						params[PW_PARAM_OSC2].getValue(),
						PWM_TO_OSC2[i],
						params[FINE_PARAM_OSC2].getValue(),
						params[FREQ_PARAM_OSC2].getValue(),
						FM_INPUT_OSC2[i],
						PITCH_INPUT,
						args.sampleTime);
					// Set outputs

					if (inputs[WAVE_CV_OSC2].isConnected())
					{
						wave2[i] = params[WAVE_ATTEN_OSC2].getValue() + (inputs[WAVE_CV_OSC2].getPolyVoltage(i) / 3);
						wave2[i] = clamp(wave2[i], 0.f, 3.f);

						if (wave2[i] < 1.f)
							OSC2_OUTPUT[i] = (crossfade(oscillator2[i].sine, oscillator2[i].tri, wave2[i])) * 5.f;
						else if (wave2[i] < 2.f)
							OSC2_OUTPUT[i] = (crossfade(oscillator2[i].tri, oscillator2[i].saw, wave2[i] - 1.f)) * 5.f;
						else
							OSC2_OUTPUT[i] = (crossfade(oscillator2[i].saw, oscillator2[i].square, wave2[i] - 2.f)) * 5.f;
					}
					else
					{
						SIN_OUTPUT_OSC2[i] = 5.f * oscillator2[i].sine;
						TRI_OUTPUT_OSC2[i] = 5.f * oscillator2[i].tri;
						//EVEN_OUTPUT_OSC2 = 5.f * oscillator2[i].even;
						SAW_OUTPUT_OSC2[i] = 5.f * oscillator2[i].saw;
						SQR_OUTPUT_OSC2[i] = 5.f * oscillator2[i].square;
						if (params[SWITCH_PARAM_OSC2_TYPE].getValue() == 0)
						{
							OSC2_OUTPUT[i] = 5.f * oscillator2[i].sine;
						}
						else if (params[SWITCH_PARAM_OSC2_TYPE].getValue() == 1)
						{
							OSC2_OUTPUT[i] = 5.f * oscillator2[i].tri;
						}
						else if (params[SWITCH_PARAM_OSC2_TYPE].getValue() == 2)
						{
							OSC2_OUTPUT[i] = 5.f * oscillator2[i].saw;
						}
						else if (params[SWITCH_PARAM_OSC2_TYPE].getValue() == 3)
						{
							OSC2_OUTPUT[i] = 5.f * oscillator2[i].square;
						}
					}
				}
			}

			////////////////////////////////////////////////////////
			//////////////    FM IN OSC3        ////////////////////
			////////////////////////////////////////////////////////
			for (int i = 0; i < channels; ++i)
			{
				if (OUT_OUTPUT_AR_ADSR[i] > .1)
				{
					if (params[SLIDER_PARAM_FM_OSC3_LVL + 0].getValue() > 0.1f || params[SLIDER_PARAM_FM_OSC3_LVL + 1].getValue() > 0.1f)
					{
						///  #1 - MW, LFO,  ADSR to VCO
						if (params[SWITCH_PARAM_FM_OSC3 + 0].getValue() && !inputs[IN_CV_1].isConnected())
						{
							F_IN0[i] = SH_MIX[i]; //SH Mix
						}
						else if (params[SWITCH_PARAM_FM_OSC3 + 0].getValue() && inputs[IN_CV_1].isConnected())
						{
							F_IN0[i] = inputs[IN_CV_1].getPolyVoltage(i); //CV_IN_1
						}
						else if (!params[SWITCH_PARAM_FM_OSC3 + 0].getValue() && !params[SWITCH_PARAM_LFO_MOD_FM_OSC3].getValue() && inputs[IN_CV_MOD].isConnected())
						{
							F_IN0[i] = inputs[IN_CV_MOD].getPolyVoltage(i); //Mod Wheel
						}
						else
						{
							F_IN0[i] = LFO_OUTPUT[i]; //   LFO_OUTPUT
						}

						///  #2 - S&H & ADSR12 to Filter
						if (params[SWITCH_PARAM_FM_OSC3 + 1].getValue())
						{
							F_IN1[i] = OUTPUT_SH[i]; // S&H
						}
						else
						{
							F_IN1[i] = ENVELOPE_OUTPUT_ADSR_1[i]; // ADSR 1
						}

						///////////  Call Mix_Loop 3
						FM_3_Mix.step(F_IN0[i],
									  F_IN1[i],
									  params[SLIDER_PARAM_FM_OSC3_LVL + 0].getValue(),
									  params[SLIDER_PARAM_FM_OSC3_LVL + 1].getValue());
						FM_INPUT_OSC3[i] = FM_3_Mix.output;

						//FM_INPUT_OSC3[i] = clamp(FM_INPUT_OSC3[i], 0.f, 10.f);
					}
					////////////////////////////////////////////////////////
					///////////////   PWM IN To OSC3 1 (1)   ///////////////
					////////////////////////////////////////////////////////

					///  #1 - MW, LFO,  ADSR to VCO
					if (params[SWITCH_PARAM_PWM_OSC3].getValue())
					{
						PWM_TO_OSC3[i] = ENVELOPE_OUTPUT_ADSR_1[i]; //ENVELOPE_OUTPUT_ADSR_1
					}
					else if (!params[SWITCH_PARAM_PWM_OSC3].getValue() && !params[SWITCH_PARAM_LFO_MOD_VCF].getValue() && inputs[IN_CV_MOD].isConnected())
					{
						PWM_TO_OSC3[i] = inputs[IN_CV_MOD].getPolyVoltage(i); //Mod Wheel
					}
					else
					{
						PWM_TO_OSC3[i] = LFO_OUTPUT[i]; //   LFO_OUTPUT
					}

					PWM_TO_OSC3[i] *= std::pow(params[SLIDER_PARAM_PWM_OSC3_LVL].getValue(), 2.f);

					//////////////////////////////////////////////////////////////
					///////////////////////  VCO 3  //////////////////////////////
					//////////////////////////////////////////////////////////////

					PITCH_INPUT = PORTA_OUT[i] + OUT_OUTPUT_TO_FM_OSC3[i] + OUT_OUTPUT_OCTAVE + OUT_OUTPUT_PITCHBEND; ///////  +porta_out

					oscillator3[i].step(
						params[PW_PARAM_OSC3].getValue(),
						PWM_TO_OSC3[i],
						params[FINE_PARAM_OSC3].getValue(),
						params[FREQ_PARAM_OSC3].getValue(),
						FM_INPUT_OSC3[i],
						PITCH_INPUT,
						args.sampleTime);
					// Set outputs
					if (inputs[WAVE_CV_OSC3].isConnected())
					{
						wave3[i] = params[WAVE_ATTEN_OSC3].getValue() + (inputs[WAVE_CV_OSC3].getPolyVoltage(i) / 3);
						wave3[i] = clamp(wave3[i], 0.f, 3.f);

						if (wave3[i] < 1.f)
							OSC3_OUTPUT[i] = (crossfade(oscillator3[i].sine, oscillator3[i].tri, wave3[i])) * 5.f;
						else if (wave2[i] < 2.f)
							OSC3_OUTPUT[i] = (crossfade(oscillator3[i].tri, oscillator3[i].saw, wave3[i] - 1.f)) * 5.f;
						else
							OSC3_OUTPUT[i] = (crossfade(oscillator3[i].saw, oscillator3[i].square, wave3[i] - 2.f)) * 5.f;
					}
					else
					{
						SIN_OUTPUT_OSC3[i] = 5.f * oscillator3[i].sine;
						TRI_OUTPUT_OSC3[i] = 5.f * oscillator3[i].tri;
						//EVEN_OUTPUT_OSC3 = 5.f * oscillator3[i].even;
						SAW_OUTPUT_OSC3[i] = 5.f * oscillator3[i].saw;
						SQR_OUTPUT_OSC3[i] = 5.f * oscillator3[i].square;
						if (params[SWITCH_PARAM_OSC3_TYPE].getValue() == 0)
						{
							OSC3_OUTPUT[i] = 5.f * oscillator3[i].sine;
						}
						else if (params[SWITCH_PARAM_OSC3_TYPE].getValue() == 1)
						{
							OSC3_OUTPUT[i] = 5.f * oscillator3[i].tri;
						}
						else if (params[SWITCH_PARAM_OSC3_TYPE].getValue() == 2)
						{
							OSC3_OUTPUT[i] = 5.f * oscillator3[i].saw;
						}
						else if (params[SWITCH_PARAM_OSC3_TYPE].getValue() == 3)
						{
							OSC3_OUTPUT[i] = 5.f * oscillator3[i].square;
						}
					}
				}
			}

			////////////////////////////////////////////////////////
			//////////////////   Ring Modulation   /////////////////
			////////////////////////////////////////////////////////

			// only run when switch is in ring mod position, AND level is >0

			RM_ON = false;
			if (params[SLIDER_PARAM_TO_AUDIO_LVL + 0].getValue() > 0.1f)
			{
				RM_ON = true;
			}

			{
				for (int i = 0; i < channels; ++i)
				{
					if (OUT_OUTPUT_AR_ADSR[i] > .1)
					{
						rmod1[i].step(OSC1_OUTPUT[i], OSC2_OUTPUT[i]);

						RM_MODULATED_OUTPUT[i] = rmod1[i].wd * rmod1[i].res_RM + (1.0 - rmod1[i].wd) * rmod1[i].vin;
					}
				}
			}

			////////////////////////////////////////////////////////
			//////////////     Noise select     ////////////////////
			////////////////////////////////////////////////////////
			float white = noise.white();
			for (int i = 0; i < channels; ++i)
			{
				if (OUT_OUTPUT_AR_ADSR[i] > .1)
				{
					if (params[SWITCH_PARAM_NOISE_SEL].getValue())
					{
						NOISE_OUT[i] = 10.0f * white;
						NOISE_OUT[i] *= std::pow(1, 3.f);
					}
					else
					{
						pinkFilter.process(white);
						NOISE_OUT[i] = 10.0f * clamp(0.18f * pinkFilter.pink(), -1.0f, 1.0f);
						NOISE_OUT[i] *= std::pow(1, 3.f);
					}
				}
			}
			///////////////////////////////////////////////////
			//////////////    Audio inputs (4)   //////////////
			///////////////////////////////////////////////////

			for (int i = 0; i < channels; ++i)
			{
				///  #0 - Noise / RM
				if (params[SWITCH_PARAM_AUDIO + 0].getValue())
				{
					F_IN0[i] = NOISE_OUT[i];
				}
				else
				{
					F_IN0[i] = RM_MODULATED_OUTPUT[i];
				}

				///////////  Call Mix_Loop 4
				AudioMix.step(F_IN0[i],
							  OSC1_OUTPUT[i],
							  OSC2_OUTPUT[i],
							  OSC3_OUTPUT[i],
							  params[SLIDER_PARAM_TO_AUDIO_LVL + 0].getValue(),
							  params[SLIDER_PARAM_TO_AUDIO_LVL + 1].getValue(),
							  params[SLIDER_PARAM_TO_AUDIO_LVL + 2].getValue(),
							  params[SLIDER_PARAM_TO_AUDIO_LVL + 3].getValue());

				AUDIO_MIX[i] = AudioMix.output;
			}

			outputs[OUTPUT_TO_EXT_VCF].setChannels(channels);
			outputs[OUTPUT_TO_EXT_VCF].writeVoltages(AUDIO_MIX);

			//////////////////////////////////////////////////////////////
			/////////////////    Filter Inputs   (3)  ////////////////////
			//////////////////////////////////////////////////////////////
			if (params[SLIDER_PARAM_TO_FILTER_LVL + 0].getValue() > 0.1f || params[SLIDER_PARAM_TO_FILTER_LVL + 1].getValue() > 0.1f || params[SLIDER_PARAM_TO_FILTER_LVL + 2].getValue() > 0.1f)
			{
				for (int i = 0; i < channels; ++i)
				{
					///  #1 - KBRD, CV2, SH MIX to Filter
					if (params[SWITCH_PARAM_FILTER + 0].getValue())
					{
						F_IN0[i] = inputs[IN_VOLT_OCTAVE_INPUT_1].getPolyVoltage(i); // kb cv
					}
					else
					{
						if (inputs[IN_CV_2].isConnected()) // CV In2)
						{
							F_IN0[i] = inputs[IN_CV_2].getPolyVoltage(i); // CV In2
						}
						else
						{
							F_IN0[i] = SH_MIX[i];
						}
					}
					///  #2 - S&H, LFO, MW to Filter
					if (params[SWITCH_PARAM_FILTER + 1].getValue())
					{
						F_IN1[i] = OUTPUT_SH[i];
					}
					else
					{
						if (params[SWITCH_PARAM_LFO_MOD_VCF].getValue()) ////LFO / MW
						{
							F_IN1[i] = LFO_OUTPUT[i];
						}
						else
						{
							if (inputs[IN_CV_MOD].isConnected()) //PW MOD IN
							{
								F_IN1[i] = inputs[IN_CV_MOD].getPolyVoltage(i);
							}
						}
					}
					///  #3 - ADSR 1 or 2 to Filter
					if (params[SWITCH_PARAM_FILTER + 2].getValue())
					{
						F_IN2[i] = ENVELOPE_OUTPUT_ADSR_1[i];
					}
					else
					{
						F_IN2[i] = ENVELOPE_OUTPUT_ADSR_2[i];
					}
					///////////  Call Mix_Loop 3
					FilterMix.step(F_IN0[i],
								   F_IN1[i],
								   F_IN2[i],
								   params[SLIDER_PARAM_TO_FILTER_LVL + 0].getValue(),
								   params[SLIDER_PARAM_TO_FILTER_LVL + 1].getValue(),
								   params[SLIDER_PARAM_TO_FILTER_LVL + 2].getValue());

					FILTER_MIX[i] = FilterMix.output;
				}
			}

			outputs[OUT_FILTER_MIX_OUTPUT].setChannels(channels);
			outputs[OUT_FILTER_MIX_OUTPUT].writeVoltages(FILTER_MIX);

			////////////////////////////////////////////////////////
			////////////////////////   VCF        //////////////////
			////////////////////////////////////////////////////////

			if (outputs[OUT_FREQ_PARAM_VCF].isConnected())
			{
				outputs[OUT_FREQ_PARAM_VCF].value = params[FREQ_PARAM_VCF].getValue() * 10;
			}
			if (outputs[OUT_RES_PARAM_VCF].isConnected())
			{
				outputs[OUT_RES_PARAM_VCF].value = params[RES_PARAM_VCF].getValue() * 10;
			}
			if (outputs[OUT_DRIVE_PARAM_VCF].isConnected())
			{
				outputs[OUT_DRIVE_PARAM_VCF].value = params[DRIVE_PARAM_VCF].getValue() * 10;
			}

			if (!inputs[INPUT_EXT_VCF].isConnected())
			{

				for (int c = 0; c < channels; c += 4)
				{

					float driveParam = params[DRIVE_PARAM_VCF].getValue(); //DRIVE_PARAM
					float resParam = params[RES_PARAM_VCF].getValue();	 //RES_PARAM
					float fineParam = 0.5f;
					fineParam = dsp::quadraticBipolar(fineParam * 2.f - 1.f) * 7.f / 12.f;
					float freqCvParam = params[FREQ_CV_PARAM_VCF].getValue(); //FREQ_CV_PARAM                 // FREQ_CV_PARAM
					freqCvParam = dsp::quadraticBipolar(freqCvParam);
					float freqParam = params[FREQ_PARAM_VCF].getValue(); //FREQ_PARAM
					freqParam = freqParam * 10.f - 5.f;

					auto *filter = &filters[c / 4];

					float_4 input = float_4::load(&AUDIO_MIX[c]) / 5.f;

					// Drive gain
					float_4 drive = driveParam;
					drive = clamp(drive, 0.f, 1.f);
					float_4 gain = simd::pow(1.f + drive, 5);
					input *= gain;

					// Add -120dB noise to bootstrap self-oscillation
					input += 1e-6f * (2.f * random::uniform() - 1.f);

					// Set resonance
					//float_4 resonance = resParam + inputs[RES_PARAM_VCF].getPolyVoltageSimd<float_4>(c) / 10.f;
					float_4 resonance = resParam;
					resonance = clamp(resonance, 0.f, 1.f);
					filter->resonance = simd::pow(resonance, 2) * 10.f;

					// Get pitch
					//float_4 pitch = freqParam + fineParam + inputs[FREQ_PARAM_VCF].getPolyVoltageSimd<float_4>(c) * freqCvParam;
					float_4 pitch = float_4::load(&FILTER_MIX[c]) * freqCvParam;
					pitch += freqParam + fineParam;
					//float_4 pitch += (freqParam + fineParam + (FILTER_MIX[c] + FILTER_MIX[c+1] + FILTER_MIX[c+2] + FILTER_MIX[c+3]) * freqCvParam);

					// Set cutoff
					float_4 cutoff = dsp::FREQ_C4 * simd::pow(2.f, pitch);
					cutoff = clamp(cutoff, 1.f, 8000.f);
					filter->setCutoff(cutoff);

					// Set outputs
					filter->process(input, args.sampleTime);
					float_4 lowpass = 5.f * filter->lowpass();
					lowpass.store(&IN_HPF[c]);
				}
			}

			//////////////////////////////////////////////////
			//////////////////   HPF     /////////////////////
			/////////     Bidoo Perco       //////////////////
			//inputs[IN_VOLT_OCTAVE_INPUT_1].getVoltages(voct);

			for (int i = 0; i < channels; ++i)
			{
				//IN_CV_HPF_CUTOFF
				//params[CMOD_PARAM].getValue() = 0.5f;
				//params[Q_PARAM].getValue() = 0.0f;
				HPF_Q_PARAM = 0.0f;
				//CUTOFF_INPUT_HPF = 0.0f;
				float cfreq = pow(2.0f, rescale(clamp(params[HPF_FILTER_FREQ].getValue() + params[HPF_FMOD_PARAM].getValue() * inputs[IN_CV_HPF_CUTOFF].getVoltage() / 5.0f, 0.0f, 1.0f), 0.0f, 1.0f, 4.5f, 13.0f));
				float q = 10.0f * clamp(0.f + HPF_Q_PARAM * 0.2f, 0.1f, 1.0f);
				hpfFilter[i].setParams(cfreq, q, args.sampleRate);
				if (!inputs[INPUT_EXT_VCF].isConnected())
				{
					HPFin[i] = IN_HPF[i] * 0.3f; //normalise to -1/+1 we consider VCV Rack standard is #+5/-5V on VCO1
				}
				else
				{
					HPFin[i] = inputs[INPUT_EXT_VCF].getVoltage(i);
					//hpfFilter[i].calcOutput(HPFin[i]);
				}
				hpfFilter[i].calcOutput(HPFin[i]);
				OUT_HP[i] = hpfFilter[i].hp * 5.0f;
			}
		}
		else
		{
			outputs[MAIN_AUDIO_OUT].setChannels(0);

			sum = 0.f;
			outputs[MONO_OUTPUT].setVoltage(sum);
		}
	}
};
//void Odyssey::onReset() {
//	sh_trigger.reset();
//	sh_value = 0.0;
//};

//Original Odyssey 2800 White
//Odyssey MKII 2810 Black and Gold
//Odyssey MKIII 2820 Black and Orange

struct OdysseyWidget : ModuleWidget
{

	SvgPanel *goldPanel;
	SvgPanel *blackPanel;
	SvgPanel *whitePanel;

	struct panelStyleItem : MenuItem
	{
		Odyssey *module;
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
		Odyssey *module = dynamic_cast<Odyssey *>(this->module);

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		//assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		panelStyleItem *goldItem = new panelStyleItem();
		goldItem->text = "Odyssey MKII 2810 Gold";
		goldItem->module = module;
		goldItem->theme = 0;
		menu->addChild(goldItem);

		panelStyleItem *blackItem = new panelStyleItem();
		blackItem->text = "Odyssey MKIII 2820 Orange";
		blackItem->module = module;
		blackItem->theme = 1;
		menu->addChild(blackItem);

		panelStyleItem *whiteItem = new panelStyleItem();
		whiteItem->text = "Original Odyssey 2800 White";
		whiteItem->module = module;
		whiteItem->theme = 2;
		menu->addChild(whiteItem);
	}

	OdysseyWidget(Odyssey *module)
	//    void appendContextMenu(Menu *menu) override;
	{
		setModule(module);
		//setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/odyssey_gold.svg")));   //Single Panel

		goldPanel = new SvgPanel();
		goldPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/odyssey_gold.svg")));
		box.size = goldPanel->box.size;
		addChild(goldPanel);
		blackPanel = new SvgPanel();
		blackPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/odyssey_black.svg")));
		blackPanel->visible = false;
		addChild(blackPanel);
		whitePanel = new SvgPanel();
		whitePanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/odyssey_white.svg")));
		whitePanel->visible = false;
		addChild(whitePanel);

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		// noise select
		addParam(createParam<sts_CKSS>(Vec(166, 57), module, Odyssey::SWITCH_PARAM_NOISE_SEL)); //, 0.0, 1.0, 1.0));
		// aPortamentp Slider
		addParam(createParam<sts_SlidePotBlack>(Vec(161, 110), module, Odyssey::PARAM_PORTA)); //, 0.4, 1.0, 0.4));
		//addParam(createParam<sts_Davies15_Grey>(Vec(164, 91), module, Odyssey::PARAM_PORTA_SHAPE));

		// Octave 5 Way Rotary with detents each volt
		addParam(createParam<sts_Davies_snap_47_Grey>(Vec(149, 215), module, Odyssey::OCTAVE_PARAM)); //, -4.0f, 2.0f, -1.0f));
		// Add Pitch Bend Knob
		addParam(createParam<sts_Davies47_Grey>(Vec(149, 280), module, Odyssey::PITCHBEND_PARAM)); //, -1.0, 1.0, 0.0));
		addParam(createParam<sts_Davies15_Grey>(Vec(164.5, 345), module, Odyssey::ATTEN_PB));

		////   LFO
		addChild(createLight<SmallLight<RedGreenBlueLight>>(Vec(499, 50), module, Odyssey::PHASE_LIGHT));
		////   VU
		addChild(createLight<SmallLight<RedLight>>(Vec(734, 55), module, Odyssey::VU1_LIGHT + 0));
		addChild(createLight<SmallLight<YellowLight>>(Vec(734, 63), module, Odyssey::VU1_LIGHT + 1));
		addChild(createLight<SmallLight<YellowLight>>(Vec(734, 71), module, Odyssey::VU1_LIGHT + 2));
		addChild(createLight<SmallLight<YellowLight>>(Vec(734, 79), module, Odyssey::VU1_LIGHT + 3));
		addChild(createLight<SmallLight<GreenLight>>(Vec(734, 87), module, Odyssey::VU1_LIGHT + 4));
		addChild(createLight<SmallLight<GreenLight>>(Vec(734, 95), module, Odyssey::VU1_LIGHT + 5));
		addChild(createLight<SmallLight<GreenLight>>(Vec(734, 103), module, Odyssey::VU1_LIGHT + 6));
		addChild(createLight<SmallLight<GreenLight>>(Vec(734, 111), module, Odyssey::VU1_LIGHT + 7));
		addChild(createLight<SmallLight<GreenLight>>(Vec(734, 119), module, Odyssey::VU1_LIGHT + 8));
		addChild(createLight<SmallLight<GreenLight>>(Vec(734, 127), module, Odyssey::VU1_LIGHT + 9));

		////   ADSR lights Bottom Red
		addChild(createLight<SmallLight<RedLight>>(Vec(779, 197), module, Odyssey::ATTACK_LIGHT_1));
		addChild(createLight<SmallLight<RedLight>>(Vec(806, 197), module, Odyssey::DECAY_LIGHT_1));
		addChild(createLight<SmallLight<RedLight>>(Vec(833, 197), module, Odyssey::SUSTAIN_LIGHT_1));
		addChild(createLight<SmallLight<RedLight>>(Vec(860, 197), module, Odyssey::RELEASE_LIGHT_1));

		////   ADSR lights TOP Pink
		addChild(createLight<SmallLight<RedLight>>(Vec(779, 50), module, Odyssey::ATTACK_LIGHT_2));
		addChild(createLight<SmallLight<RedLight>>(Vec(806, 50), module, Odyssey::DECAY_LIGHT_2));
		addChild(createLight<SmallLight<RedLight>>(Vec(833, 50), module, Odyssey::SUSTAIN_LIGHT_2));
		addChild(createLight<SmallLight<RedLight>>(Vec(860, 50), module, Odyssey::RELEASE_LIGHT_2));

		////   OSC2
		//addParam(createParam<sts_CKSS>(Vec(377, 57), module, Odyssey::SYNC_PARAM_OSC2, 0.0f, 1.0f, 1.0f));

		/////   All Outpus / Inputs    338    8  ,  44  , 79  ,   114
		int x1 = 10;
		int x2 = 62;
		int x3 = 114;
		;
		//int x4 = 114;
		addOutput(createOutput<sts_Port>(Vec(x1, 20), module, Odyssey::MAIN_AUDIO_OUT));	 // Main Output Poly
		addOutput(createOutput<sts_Port>(Vec(x1, 70), module, Odyssey::MONO_OUTPUT));		 // Main Output Mono
		addOutput(createOutput<sts_Port>(Vec(x1, 120), module, Odyssey::OUTPUT_TO_EXT_VCF)); // AUDIO MIX OUT
		addOutput(createOutput<sts_Port>(Vec(x1, 170), module, Odyssey::OUT_FILTER_MIX_OUTPUT));
		addOutput(createOutput<sts_Port>(Vec(x1, 220), module, Odyssey::OUT_FREQ_PARAM_VCF));  // filter freq
		addOutput(createOutput<sts_Port>(Vec(x1, 270), module, Odyssey::OUT_RES_PARAM_VCF));   // filter res
		addOutput(createOutput<sts_Port>(Vec(x1, 320), module, Odyssey::OUT_DRIVE_PARAM_VCF)); // filter drive
		//

		addInput(createInput<sts_Port>(Vec(x2, 20), module, Odyssey::INPUT_EXT_VCF));	 // INPUT_VCF_INPUT from ext VCF
		addInput(createInput<sts_Port>(Vec(x2, 70), module, Odyssey::IN_CV_1));			  // CV 1 IN
		addInput(createInput<sts_Port>(Vec(x2, 120), module, Odyssey::IN_CV_2));		  // CV 2 IN
		addInput(createInput<sts_Port>(Vec(x2, 170), module, Odyssey::IN_CV_HPF_CUTOFF)); // CV hpf
		addInput(createInput<sts_Port>(Vec(x2, 220), module, Odyssey::WAVE_CV_LFO));
		addInput(createInput<sts_Port>(Vec(x2, 270), module, Odyssey::IN_CV_LFO));		 // CV LFO IN
		addInput(createInput<sts_Port>(Vec(x2, 320), module, Odyssey::IN_PORTA_ON_OFF)); // Portamento on/off

		addInput(createInput<sts_Port>(Vec(x3, 20), module, Odyssey::IN_VOLT_OCTAVE_INPUT_1)); // V/O input
		addInput(createInput<sts_Port>(Vec(x3, 70), module, Odyssey::IN_GATE_INPUT_1));		   //  Gate in
		addInput(createInput<sts_Port>(Vec(x3, 120), module, Odyssey::IN_TRIGGER_INPUT_1));	// Trigger in
		addInput(createInput<sts_Port>(Vec(x3, 170), module, Odyssey::IN_VELOCITY));		   // Velocity In
		addInput(createInput<sts_Port>(Vec(x3, 220), module, Odyssey::IN_CV_PB));			   // CV Pitch Bend
		addInput(createInput<sts_Port>(Vec(x3, 270), module, Odyssey::IN_CV_MOD));			   // CV MOD IN
		addInput(createInput<sts_Port>(Vec(x3, 320), module, Odyssey::IN_CV_OCTAVE));		   // CV OCTAVE IN

		//  3 wave CV VCO inputs
		addInput(createInput<sts_Port>(Vec(579, 296), module, Odyssey::WAVE_CV_OSC1)); //	 Wave CV IN VCO1, 2 ,3
		addInput(createInput<sts_Port>(Vec(606, 296), module, Odyssey::WAVE_CV_OSC2)); // CV MOD IN
		addInput(createInput<sts_Port>(Vec(633, 296), module, Odyssey::WAVE_CV_OSC3));
		addParam(createParam<sts_Davies15_Grey>(Vec(223, 170), module, Odyssey::WAVE_ATTEN_OSC1));
		addParam(createParam<sts_Davies15_Grey>(Vec(336, 170), module, Odyssey::WAVE_ATTEN_OSC2));
		addParam(createParam<sts_Davies15_Grey>(Vec(450, 170), module, Odyssey::WAVE_ATTEN_OSC3));
		addParam(createParam<sts_Davies15_Grey>(Vec(495, 170), module, Odyssey::WAVE_ATTEN_LFO));

		/////////////////////  input & output for testing   far left column    //////////////////////
		//addOutput(createOutput<sts_Port>(Vec(35, 325), module, Odyssey::OUT_OUTPUT_1));
		//addInput(createInput<sts_Port>(Vec(35, 275), module, Odyssey::IN_INPUT_1));

		//////////////////////////////////////// Add sliders Top Row   ///////////////////////

		addParam(createParam<sts_SlidePotBlue>(Vec(212, 57), module, Odyssey::FREQ_PARAM_OSC1));  //, -4.0f, 3.0f, 0.0f));   //1
		addParam(createParam<sts_SlidePotBlue>(Vec(239, 57), module, Odyssey::FINE_PARAM_OSC1));  //, -7.0f, 7.0f, 0.0f));
		addParam(createParam<sts_CKSS4>(Vec(226, 140), module, Odyssey::SWITCH_PARAM_OSC1_TYPE)); //, 0.0, 3.0, 0.0));  //wave  +14

		addParam(createParam<sts_SlidePotGreen>(Vec(325, 57), module, Odyssey::FREQ_PARAM_OSC2)); //, -4.0f, 3.0f, 0.0f));    // 2
		addParam(createParam<sts_SlidePotGreen>(Vec(352, 57), module, Odyssey::FINE_PARAM_OSC2)); //, -7.0f, 7.0f, 0.0f));
		addParam(createParam<sts_CKSS4>(Vec(339, 140), module, Odyssey::SWITCH_PARAM_OSC2_TYPE)); //, 0.0, 3.0, 0.0));  //wave

		addParam(createParam<sts_SlidePotTeal>(Vec(439, 57), module, Odyssey::FREQ_PARAM_OSC3));  //, -4.0, 3.0, 0.0));     //3
		addParam(createParam<sts_SlidePotTeal>(Vec(465, 57), module, Odyssey::FINE_PARAM_OSC3));  //, -7.0, 7.0, 0.0));
		addParam(createParam<sts_CKSS4>(Vec(453, 140), module, Odyssey::SWITCH_PARAM_OSC3_TYPE)); //, 0.0, 3.0, 0.0));  //  wave

		addParam(createParam<sts_SlidePotPink>(Vec(492, 57), module, Odyssey::FREQ_PARAM_LFO)); //, -8.0, 10.0, 1.0));
		addParam(createParam<sts_Davies15_Grey>(Vec(495, 15), module, Odyssey::LFO_FM_PARAM));
		addParam(createParam<sts_CKSS4>(Vec(498, 140), module, Odyssey::SWITCH_PARAM_LFO_TYPE)); //, 0.0, 3.0, 0.0));   //wave

		addParam(createParam<sts_SlidePotYellow>(Vec(519, 57), module, Odyssey::SLIDER_PARAM_LAG));		  //, 0.0, 0.5, 0.0));
		addParam(createParam<sts_SlidePotBlue>(Vec(551, 57), module, Odyssey::SLIDER_PARAM_SH_LVL + 0));  //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotWhite>(Vec(578, 57), module, Odyssey::SLIDER_PARAM_SH_LVL + 1)); //, 0.0, 1.0, 0.0));

		addParam(createParam<sts_SlidePotBlack>(Vec(605, 57), module, Odyssey::FREQ_PARAM_VCF)); //, 0.f, 1.f, 1.0f));
		addParam(createParam<sts_Davies15_Grey>(Vec(608, 142), module, Odyssey::FREQ_CV_PARAM_VCF));
		addParam(createParam<sts_SlidePotBlack>(Vec(632, 57), module, Odyssey::RES_PARAM_VCF));   //, 0.f, 1.f, 0.f));
		addParam(createParam<sts_SlidePotBlack>(Vec(659, 57), module, Odyssey::DRIVE_PARAM_VCF)); //, 0.f, 1.f, 0.f));
		addParam(createParam<sts_SlidePotBlack>(Vec(686, 57), module, Odyssey::HPF_FILTER_FREQ));
		addParam(createParam<sts_Davies15_Grey>(Vec(689, 142), module, Odyssey::HPF_FMOD_PARAM)); //, 0.0f, 1.0f, 0.0f));
		//addParam(createParam<sts_SlidePotBlack>(Vec(713, 57), module, Odyssey::HPF_Q_PARAM)); //, -1.f, 1.f, 0.f));    BLANK
		addParam(createParam<sts_SlidePotRed>(Vec(740, 57), module, Odyssey::SLIDER_PARAM_AR_ADSR_LVL)); //, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(746, 153), module, Odyssey::SWITCH_PARAM_AR_ADSR + 0));		 //, 0.0, 1.0, 0.0));

		addParam(createParam<sts_SlidePotPink>(Vec(772, 57), module, Odyssey::ATTACK_PARAM_2));  //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotPink>(Vec(799, 57), module, Odyssey::DECAY_PARAM_2));   //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotPink>(Vec(826, 57), module, Odyssey::SUSTAIN_PARAM_2)); //, 0.0, 1.0, 1.0));
		addParam(createParam<sts_SlidePotPink>(Vec(853, 57), module, Odyssey::RELEASE_PARAM_2)); //, 0.0, 1.0, 0.0));

		//////////////////////////////////////////// Add sliders Bottom Row
		addParam(createParam<sts_SlidePotPink>(Vec(212, 204), module, Odyssey::SLIDER_PARAM_FM_OSC1_LVL + 0));   //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotYellow>(Vec(239, 204), module, Odyssey::SLIDER_PARAM_FM_OSC1_LVL + 1)); //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotBlue>(Vec(266, 204), module, Odyssey::PW_PARAM_OSC1));					 //, -1.0f, 1.0f, 0.0f));
		addParam(createParam<sts_SlidePotPink>(Vec(293, 204), module, Odyssey::SLIDER_PARAM_PWM_OSC1_LVL));		 //, 0.0, 1.0, 0.0));

		addParam(createParam<sts_SlidePotPink>(Vec(325, 204), module, Odyssey::SLIDER_PARAM_FM_OSC2_LVL + 0));   //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotYellow>(Vec(352, 204), module, Odyssey::SLIDER_PARAM_FM_OSC2_LVL + 1)); //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotGreen>(Vec(379, 204), module, Odyssey::PW_PARAM_OSC2));				 //, -1.0f, 1.0f, 0.0));
		addParam(createParam<sts_SlidePotPink>(Vec(406, 204), module, Odyssey::SLIDER_PARAM_PWM_OSC2_LVL));		 //, 0.0, 1.0, 0.0));

		addParam(createParam<sts_SlidePotPink>(Vec(438, 204), module, Odyssey::SLIDER_PARAM_FM_OSC3_LVL + 0));   //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotYellow>(Vec(465, 204), module, Odyssey::SLIDER_PARAM_FM_OSC3_LVL + 1)); //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotTeal>(Vec(492, 204), module, Odyssey::PW_PARAM_OSC3));					 //, -1.0f, 1.0f, 0.0));
		addParam(createParam<sts_SlidePotPink>(Vec(519, 204), module, Odyssey::SLIDER_PARAM_PWM_OSC3_LVL));		 //, 0.0, 1.0, 0.0));

		addParam(createParam<sts_SlidePotWhite>(Vec(551, 204), module, Odyssey::SLIDER_PARAM_TO_AUDIO_LVL + 0)); //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotBlue>(Vec(578, 204), module, Odyssey::SLIDER_PARAM_TO_AUDIO_LVL + 1));  //, 0.0, 1.0, 1.0));
		addParam(createParam<sts_SlidePotGreen>(Vec(605, 204), module, Odyssey::SLIDER_PARAM_TO_AUDIO_LVL + 2)); //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotTeal>(Vec(632, 204), module, Odyssey::SLIDER_PARAM_TO_AUDIO_LVL + 3));  //, 0.0, 1.0, 0.0));

		addParam(createParam<sts_SlidePotBlack>(Vec(659, 204), module, Odyssey::SLIDER_PARAM_TO_FILTER_LVL + 0));  //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotYellow>(Vec(686, 204), module, Odyssey::SLIDER_PARAM_TO_FILTER_LVL + 1)); //, 0.0, 1.0, 0.0));
		addParam(createParam<sts_SlidePotRed>(Vec(713, 204), module, Odyssey::SLIDER_PARAM_TO_FILTER_LVL + 2));	//, 0.0, 1.0, 0.0));
		//addParam(createParam<sts_SlidePotBlack>(Vec(740, 204), module, Odyssey::LEVEL1_PARAM_VCA)); //, 0.0f, 1.0f, 0.0f));   BLANK

		addParam(createParam<sts_SlidePotRed>(Vec(772, 204), module, Odyssey::ATTACK_PARAM_1));  //, 0.0f, 1.0f, 0.0f));
		addParam(createParam<sts_SlidePotRed>(Vec(799, 204), module, Odyssey::DECAY_PARAM_1));   //, 0.0f, 1.0f, 0.0f));
		addParam(createParam<sts_SlidePotRed>(Vec(826, 204), module, Odyssey::SUSTAIN_PARAM_1)); //, 0.0f, 1.0f, 1.0f));
		addParam(createParam<sts_SlidePotRed>(Vec(853, 204), module, Odyssey::RELEASE_PARAM_1)); //, 0.0f, 1.0f, 0.0f));

		/////////////////////////////////// Switches Bottom row   +3  -18   215  320     SWITCH_PARAM_LFO-MOD-FM_OSC1, SWITCH_PARAM_LFO-MOD-PWM_OSC1
		addParam(createParam<sts_CKSS>(Vec(218, 302), module, Odyssey::SWITCH_PARAM_FM_OSC1 + 0));			//, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(218, 334), module, Odyssey::SWITCH_PARAM_LFO_MOD_FM_OSC1 + 0));  //, 0.0, 1.0, 1.0));  //LFO_MOD
		addParam(createParam<sts_CKSS>(Vec(245, 302), module, Odyssey::SWITCH_PARAM_FM_OSC1 + 1));			//, 0.0, 1.0, 0.0));
		addParam(createParam<sts_CKSS>(Vec(299, 302), module, Odyssey::SWITCH_PARAM_PWM_OSC1 + 0));			//, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(299, 334), module, Odyssey::SWITCH_PARAM_LFO_MOD_PWM_OSC1 + 0)); //, 0.0, 1.0, 1.0));//LFO_MOD

		addParam(createParam<sts_CKSS>(Vec(331, 302), module, Odyssey::SWITCH_PARAM_FM_OSC2 + 0));			//, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(331, 334), module, Odyssey::SWITCH_PARAM_LFO_MOD_FM_OSC2 + 0));  //, 0.0, 1.0, 1.0));  //LFO_MOD
		addParam(createParam<sts_CKSS>(Vec(358, 302), module, Odyssey::SWITCH_PARAM_FM_OSC2 + 1));			//, 0.0, 1.0, 0.0));
		addParam(createParam<sts_CKSS>(Vec(412, 302), module, Odyssey::SWITCH_PARAM_PWM_OSC2 + 0));			//, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(412, 334), module, Odyssey::SWITCH_PARAM_LFO_MOD_PWM_OSC2 + 0)); //, 0.0, 1.0, 1.0));//LFO_MOD

		addParam(createParam<sts_CKSS>(Vec(444, 302), module, Odyssey::SWITCH_PARAM_FM_OSC3 + 0));			//, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(444, 334), module, Odyssey::SWITCH_PARAM_LFO_MOD_FM_OSC3 + 0));  //, 0.0, 1.0, 1.0));  //LFO_MOD
		addParam(createParam<sts_CKSS>(Vec(471, 302), module, Odyssey::SWITCH_PARAM_FM_OSC3 + 1));			//, 0.0, 1.0, 0.0));
		addParam(createParam<sts_CKSS>(Vec(525, 302), module, Odyssey::SWITCH_PARAM_PWM_OSC3 + 0));			//, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(525, 334), module, Odyssey::SWITCH_PARAM_LFO_MOD_PWM_OSC3 + 0)); //, 0.0, 1.0, 1.0));//LFO_MOD

		addParam(createParam<sts_CKSS>(Vec(525, 153), module, Odyssey::SWITCH_PARAM_SH_LFO_KEY)); //, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(557, 153), module, Odyssey::SWITCH_PARAM_SH + 0));	 //, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(584, 153), module, Odyssey::SWITCH_PARAM_SH + 1));	 //, 0.0, 1.0, 1.0));

		addParam(createParam<sts_CKSS>(Vec(557, 302), module, Odyssey::SWITCH_PARAM_AUDIO + 0)); //, 0.0, 1.0, 1.0));
		//addParam(createParam<sts_CKSS>(Vec(584, 302), module, Odyssey::SWITCH_PARAM_AUDIO + 1));   //, 0.0, 1.0, 1.0));
		//addParam(createParam<sts_CKSS>(Vec(611, 302), module, Odyssey::SWITCH_PARAM_AUDIO + 2));   //, 0.0, 1.0, 1.0));
		//addParam(createParam<sts_CKSS>(Vec(638, 302), module, Odyssey::SWITCH_PARAM_AUDIO + 3));   //, 0.0, 1.0, 1.0));

		addParam(createParam<sts_CKSS>(Vec(665, 302), module, Odyssey::SWITCH_PARAM_FILTER + 0));  //, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(692, 302), module, Odyssey::SWITCH_PARAM_FILTER + 1));  //, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(692, 334), module, Odyssey::SWITCH_PARAM_LFO_MOD_VCF)); //, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(719, 302), module, Odyssey::SWITCH_PARAM_FILTER + 2));  //, 0.0, 1.0, 1.0));

		addParam(createParam<sts_CKSS>(Vec(778, 302), module, Odyssey::SWITCH_PARAM_ADSR1_SW1)); //, 0.0, 1.0, 1.0));
		addParam(createParam<sts_CKSS>(Vec(778, 153), module, Odyssey::SWITCH_PARAM_ADSR2_SW1)); //, 0.0, 1.0, 1.0));
	}

	void step() override
	{
		if (module)
		{
			goldPanel->visible = ((((Odyssey *)module)->panelStyle) == 0);
			blackPanel->visible = ((((Odyssey *)module)->panelStyle) == 1);
			whitePanel->visible = ((((Odyssey *)module)->panelStyle) == 2);
		}
		Widget::step();
	}
};

Model *modelOdyssey = createModel<Odyssey, OdysseyWidget>("Odyssey");
