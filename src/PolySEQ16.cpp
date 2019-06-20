#include "sts.hpp"

using namespace std;
using namespace rack;

struct PolySEQ16 : Module {
	enum ParamIds {
		CLOCK_PARAM,
		RUN_PARAM,
		RESET_PARAM,
		STEPS_PARAM,
		OCT4_PARAM,
		ENUMS(ROW1_PARAM, 16),
		ENUMS(ROW2_PARAM, 16),
		ENUMS(ROW3_PARAM, 16),
		ENUMS(ROW4_PARAM, 16),
		ENUMS(GATE_PARAM_R1, 16),
		ENUMS(GATE_PARAM_R2, 16),
		ENUMS(GATE_PARAM_R3, 16),
		ENUMS(GATE_PARAM_R4, 16),
		ENUMS(GATE2_PARAM_R1, 16),
		ENUMS(GATE2_PARAM_R2, 16),
		ENUMS(GATE2_PARAM_R3, 16),
		ENUMS(GATE2_PARAM_R4, 16),
        ENUMS(TRIG_PARAM_R1, 16),
        ENUMS(TRIG_PARAM_R2, 16),
        ENUMS(TRIG_PARAM_R3, 16),
        ENUMS(TRIG_PARAM_R4, 16),        
		ENUMS(ROW_ON_PARAM, 4),
		NUM_PARAMS
	};
	enum InputIds {
		INT_CLOCK_CV,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		RUN_ON_OFF,
		STEPS_INPUT,
		OCT4_CV_INPUT,
		ENUMS(ROW_ON_CV, 4),
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		ROW1_OUTPUT,
		ROW2_OUTPUT,
		ROW3_OUTPUT,
		ROW4_OUTPUT,
		P_GATE_OUTPUT,
		P_GATE2_OUTPUT,
        P_TRIG_OUTPUT,
        P_VOCT_OUTPUT,
		ENUMS(GATE_OUTPUT, 16),
		NUM_OUTPUTS
	};
	enum LightIds {
		RUNNING_LIGHT,
		RESET_LIGHT,
		GATES_LIGHT,
		ENUMS(ROW_LIGHTS, 4),
		ENUMS(ROW_ON_LIGHTS, 4),
		ENUMS(GATE_LIGHTS, 16),
		ENUMS(GATE_LIGHTS_R1, 16),
		ENUMS(GATE_LIGHTS_R2, 16),
		ENUMS(GATE_LIGHTS_R3, 16),
		ENUMS(GATE_LIGHTS_R4, 16),
		ENUMS(GATE2_LIGHTS_R1, 16),
		ENUMS(GATE2_LIGHTS_R2, 16),
		ENUMS(GATE2_LIGHTS_R3, 16),
		ENUMS(GATE2_LIGHTS_R4, 16),
        ENUMS(TRIG_LIGHTS_R1, 16),
        ENUMS(TRIG_LIGHTS_R2, 16),
        ENUMS(TRIG_LIGHTS_R3, 16),
        ENUMS(TRIG_LIGHTS_R4, 16),
		NUM_LIGHTS
	};

	bool running = true;
	int numSteps;
	float clockTime;
	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger runningTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::PulseGenerator gatePulse;

	dsp::SchmittTrigger gateTriggers_R1[16];
	dsp::SchmittTrigger gateTriggers_R2[16];
	dsp::SchmittTrigger gateTriggers_R3[16];
	dsp::SchmittTrigger gateTriggers_R4[16];
	dsp::SchmittTrigger gate2Triggers_R1[16];
	dsp::SchmittTrigger gate2Triggers_R2[16];
	dsp::SchmittTrigger gate2Triggers_R3[16];
	dsp::SchmittTrigger gate2Triggers_R4[16];
    dsp::SchmittTrigger trigTriggers_R1[16];
	dsp::SchmittTrigger trigTriggers_R2[16];
	dsp::SchmittTrigger trigTriggers_R3[16];
	dsp::SchmittTrigger trigTriggers_R4[16];
	dsp::SchmittTrigger row_onTriggers[4];
	
	/** Phase of internal LFO */
	float phase = 0.f;

	int index = 0;
	bool gates_R1[16] = {};
	bool gates_R2[16] = {};
	bool gates_R3[16] = {};
	bool gates_R4[16] = {};
	bool gates2_R1[16] = {};
	bool gates2_R2[16] = {};
	bool gates2_R3[16] = {};
	bool gates2_R4[16] = {};
    bool trigs_R1[16] = {};
	bool trigs_R2[16] = {};
	bool trigs_R3[16] = {};
	bool trigs_R4[16] = {};
	bool row_on[4] = {};
	float outGate[MAX_POLY_CHANNELS];
	float outGate2[MAX_POLY_CHANNELS];
	float outTrig[MAX_POLY_CHANNELS];
    float outVoct[MAX_POLY_CHANNELS];

	const int chromaticScale[26] = {0, 1, 2, 3, 4, 5,  6,  7,  8,  9, 10, 11, 12,   13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};
		
	PolySEQ16() 
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(CLOCK_PARAM, -2.f, 6.f, 2.f, "Clock Speed  ");
		configParam(RUN_PARAM, 0.f, 1.f, 0.f);
		configParam(RESET_PARAM, 0.f, 1.f, 0.f);
		configParam(STEPS_PARAM, 1.f, 16.f, 16.f, "Steps ");
		configParam(OCT4_PARAM, -2.f, 2.f, 0.f, "Row 4 Octave ");

		configParam(ROW_ON_PARAM + 0, 0.f, 1.f, 0.f);
		configParam(ROW_ON_PARAM + 1, 0.f, 1.f, 0.f);
		configParam(ROW_ON_PARAM + 2, 0.f, 1.f, 0.f);
		configParam(ROW_ON_PARAM + 3, 0.f, 1.f, 0.f);

		for (int i = 0; i < 16; i++) 
		{
			configParam(ROW1_PARAM + i, 0.f, 24.f, 1.f, "semitones ");
			configParam(ROW2_PARAM + i, 0.f, 24.f, 1.f, "semitones ");
			configParam(ROW3_PARAM + i, 0.f, 24.f, 1.f, "semitones ");
			configParam(ROW4_PARAM + i, 0.f, 24.f, 1.f, "semitones ");
			configParam(GATE_PARAM_R1 + i, 0.f, 1.f, 0.f);
			configParam(GATE_PARAM_R2 + i, 0.f, 1.f, 0.f);
			configParam(GATE_PARAM_R3 + i, 0.f, 1.f, 0.f);
			configParam(GATE_PARAM_R4 + i, 0.f, 1.f, 0.f);
			configParam(GATE2_PARAM_R1 + i, 0.f, 1.f, 0.f);
			configParam(GATE2_PARAM_R2 + i, 0.f, 1.f, 0.f);
			configParam(GATE2_PARAM_R3 + i, 0.f, 1.f, 0.f);
			configParam(GATE2_PARAM_R4 + i, 0.f, 1.f, 0.f);
            configParam(TRIG_PARAM_R1 + i, 0.f, 1.f, 0.f);
			configParam(TRIG_PARAM_R2 + i, 0.f, 1.f, 0.f);
			configParam(TRIG_PARAM_R3 + i, 0.f, 1.f, 0.f);
			configParam(TRIG_PARAM_R4 + i, 0.f, 1.f, 0.f);
			
		}

		onReset();
	}

	void onReset() override {
		for (int i = 0; i < 16; i++) 
		{
			gates_R1[i] = true;
			gates_R2[i] = true;
			gates_R3[i] = true;
			gates_R4[i] = true;
			gates2_R1[i] = true;
			gates2_R2[i] = true;
			gates2_R3[i] = true;
			gates2_R4[i] = true;
			trigs_R1[i] = true;
			trigs_R2[i] = true;
			trigs_R3[i] = true;
			trigs_R4[i] = true;
		}
	}

	void onRandomize() override {
		for (int i = 0; i < 16; i++) 
		{
			gates_R1[i] = (random::uniform() > 0.5f);
			gates_R2[i] = (random::uniform() > 0.5f);
			gates_R3[i] = (random::uniform() > 0.5f);
			gates_R4[i] = (random::uniform() > 0.5f);
			gates2_R1[i] = (random::uniform() > 0.5f);
			gates2_R2[i] = (random::uniform() > 0.5f);
			gates2_R3[i] = (random::uniform() > 0.5f);
			gates2_R4[i] = (random::uniform() > 0.5f);
			trigs_R1[i] = (random::uniform() > 0.5f);
			trigs_R2[i] = (random::uniform() > 0.5f);
			trigs_R3[i] = (random::uniform() > 0.5f);
			trigs_R4[i] = (random::uniform() > 0.5f);
		}
		for (int i = 0; i < 4; i++) 
		{
		row_on[i] = (random::uniform() > 0.5f);
		}
	}
	
	json_t *dataToJson() override 
	{
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		//row on lights
		json_t *row_onJ = json_array();
		for (int i = 0; i < 4; i++) 
		{
			json_array_insert_new(row_onJ, i, json_boolean(row_on[i]));
		}
		json_object_set_new(rootJ, "row_on", row_onJ);

		// ////////////// gate 1
        //row1
		json_t *gates1J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(gates1J, i, json_integer((int) gates_R1[i]));
		}
		json_object_set_new(rootJ, "gates1", gates1J);
		// row2
		json_t *gates2J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(gates2J, i, json_integer((int) gates_R2[i]));
		}
		json_object_set_new(rootJ, "gates2", gates2J);
		// row3
		json_t *gates3J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(gates3J, i, json_integer((int) gates_R3[i]));
		}
		json_object_set_new(rootJ, "gates3", gates3J);
		// row4
		json_t *gates4J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(gates4J, i, json_integer((int) gates_R4[i]));
		}
		json_object_set_new(rootJ, "gates4", gates4J);
		//////////////// gate 2
        //row1
		json_t *gates12J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(gates12J, i, json_integer((int) gates2_R1[i]));
		}
		json_object_set_new(rootJ, "gates12", gates12J);
		// row2
		json_t *gates22J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(gates22J, i, json_integer((int) gates2_R2[i]));
		}
		json_object_set_new(rootJ, "gates22", gates22J);
		// row3
		json_t *gates32J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(gates32J, i, json_integer((int) gates2_R3[i]));
		}
		json_object_set_new(rootJ, "gates32", gates32J);
		// row4
		json_t *gates42J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(gates42J, i, json_integer((int) gates2_R4[i]));
		}
		json_object_set_new(rootJ, "gates42", gates42J);

        ///////////////////////// triggers
        //row1
		json_t *trigs1J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(trigs1J, i, json_integer((int) trigs_R1[i]));
		}
		json_object_set_new(rootJ, "triggers1", trigs1J);
        //row2
		json_t *trigs2J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(trigs2J, i, json_integer((int) trigs_R2[i]));
		}
		json_object_set_new(rootJ, "triggers2", trigs2J);
        //row3
		json_t *trigs3J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(trigs3J, i, json_integer((int) trigs_R3[i]));
		}
		json_object_set_new(rootJ, "triggers3", trigs3J);
        //row4
		json_t *trigs4J = json_array();
		for (int i = 0; i < 16; i++) 
		{
			json_array_insert_new(trigs4J, i, json_integer((int) trigs_R4[i]));
		}
		json_object_set_new(rootJ, "triggers4", trigs4J);

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override 
	{
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		//row on lights
		json_t *row_onJ = json_object_get(rootJ, "row_on");
		if (row_onJ) 
		{
			for (int i = 0; i < 4; i++) 
			{
				json_t *rowonJ = json_array_get(row_onJ, i);
				if (rowonJ)
					row_on[i] = json_is_true(rowonJ);
			}
		}
		
		// gates //
		//row 1
		json_t *gates1J = json_object_get(rootJ, "gates1");
		if (gates1J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *gate1J = json_array_get(gates1J, i);
				if (gate1J)
					gates_R1[i] = !!json_integer_value(gate1J);
			}
		}
		//row2
		json_t *gates2J = json_object_get(rootJ, "gates2");
		if (gates2J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *gate2J = json_array_get(gates2J, i);
				if (gate2J)
					gates_R2[i] = !!json_integer_value(gate2J);
			}
		}
		//row3
		json_t *gates3J = json_object_get(rootJ, "gates3");
		if (gates3J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *gate3J = json_array_get(gates3J, i);
				if (gate3J)
					gates_R3[i] = !!json_integer_value(gate3J);
			}
		}
		//row4
		json_t *gates4J = json_object_get(rootJ, "gates4");
		if (gates4J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *gate4J = json_array_get(gates4J, i);
				if (gate4J)
					gates_R4[i] = !!json_integer_value(gate4J);
			}
		}

		// gates 2 //
		//row 1
		json_t *gates12J = json_object_get(rootJ, "gates12");
		if (gates12J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *gate12J = json_array_get(gates12J, i);
				if (gate12J)
					gates2_R1[i] = !!json_integer_value(gate12J);
			}
		}
		//row2
		json_t *gates22J = json_object_get(rootJ, "gates22");
		if (gates22J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *gate22J = json_array_get(gates22J, i);
				if (gate22J)
					gates2_R2[i] = !!json_integer_value(gate22J);
			}
		}
		//row3
		json_t *gates32J = json_object_get(rootJ, "gates32");
		if (gates32J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *gate32J = json_array_get(gates32J, i);
				if (gate32J)
					gates2_R3[i] = !!json_integer_value(gate32J);
			}
		}
		//row4
		json_t *gates42J = json_object_get(rootJ, "gates42");
		if (gates42J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *gate42J = json_array_get(gates42J, i);
				if (gate42J)
					gates2_R4[i] = !!json_integer_value(gate42J);
			}
		}

        // triggers //
		//row 1
		json_t *trigs1J = json_object_get(rootJ, "triggers1");
		if (trigs1J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *trig1J = json_array_get(trigs1J, i);
				if (trig1J)
					trigs_R1[i] = !!json_integer_value(trig1J);
			}
		}
        //row 2
		json_t *trigs2J = json_object_get(rootJ, "triggers2");
		if (trigs2J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *trig2J = json_array_get(trigs2J, i);
				if (trig2J)
					trigs_R2[i] = !!json_integer_value(trig2J);
			}
		}
        //row 3
		json_t *trigs3J = json_object_get(rootJ, "triggers3");
		if (trigs3J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *trig3J = json_array_get(trigs3J, i);
				if (trig3J)
					trigs_R3[i] = !!json_integer_value(trig3J);
			}
		}
        //row 4
		json_t *trigs4J = json_object_get(rootJ, "triggers4");
		if (trigs4J) 
		{
			for (int i = 0; i < 16; i++) 
			{
				json_t *trig4J = json_array_get(trigs4J, i);
				if (trig4J)
					trigs_R4[i] = !!json_integer_value(trig4J);
			}
		}
	}
	
	void setIndex(int index) 
	{
		numSteps = (int) clamp(std::round(params[STEPS_PARAM].getValue() + inputs[STEPS_INPUT].getVoltage()), 1.f, 16.f);
		phase = 0.f;
		this->index = index;
		if (this->index >= numSteps)
			this->index = 0;
	}

	void process(const ProcessArgs &args) override 
	{
		int rowSetting;
		int channels = 4;
		// Run
		if (runningTrigger.process(params[RUN_PARAM].getValue() || inputs[RUN_ON_OFF].getVoltage()))     //  RUN_ON_OFF
		{     
			running = !running;
		}
		
		bool gateIn = false;
		if (running) {
			if (inputs[EXT_CLOCK_INPUT].isConnected()) 
			{
				// External clock
				if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].getVoltage())) 
				{
					setIndex(index + 1);
					gatePulse.trigger(1e-3);
				}
				gateIn = clockTrigger.isHigh();
			}
			
		}

		// Reset
		if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) 
		{
			setIndex(0);
		}

		for (int i = 0; i < 16; i++) 
			{
				lights[GATE_LIGHTS + i].setSmoothBrightness(0.f, args.sampleTime);
			}

		// Row On Off Buttons
		
		for (int i = 0; i < 4; i++) 
			{
				lights[ROW_ON_LIGHTS + i].setSmoothBrightness(params[ROW_ON_PARAM + i].getValue() + inputs[ROW_ON_CV + i].getVoltage(), args.sampleTime);

			}
		
		bool pulse = gatePulse.process(args.sampleTime);

		// Gate buttons
		for (int i = 0; i < 16; i++) 
			{
				if (gateTriggers_R1[i].process(params[GATE_PARAM_R1 + i].getValue())) 
				{
					gates_R1[i] = !gates_R1[i];
				}
				if (gateTriggers_R2[i].process(params[GATE_PARAM_R2 + i].getValue())) 
				{
					gates_R2[i] = !gates_R2[i];
				}
				if (gateTriggers_R3[i].process(params[GATE_PARAM_R3 + i].getValue())) 
				{
					gates_R3[i] = !gates_R3[i];
				}
				if (gateTriggers_R4[i].process(params[GATE_PARAM_R4 + i].getValue())) 
				{
					gates_R4[i] = !gates_R4[i];
				}
				if (gate2Triggers_R1[i].process(params[GATE2_PARAM_R1 + i].getValue())) 
				{
					gates2_R1[i] = !gates2_R1[i];
				}
				if (gate2Triggers_R2[i].process(params[GATE2_PARAM_R2 + i].getValue())) 
				{
					gates2_R2[i] = !gates2_R2[i];
				}
				if (gate2Triggers_R3[i].process(params[GATE2_PARAM_R3 + i].getValue())) 
				{
					gates2_R3[i] = !gates2_R3[i];
				}
				if (gate2Triggers_R4[i].process(params[GATE2_PARAM_R4 + i].getValue())) 
				{
					gates2_R4[i] = !gates2_R4[i];
				}
                if (trigTriggers_R1[i].process(params[TRIG_PARAM_R1 + i].getValue())) 
				{
					trigs_R1[i] = !trigs_R1[i];
				}
                if (trigTriggers_R2[i].process(params[TRIG_PARAM_R2 + i].getValue())) 
				{
					trigs_R2[i] = !trigs_R2[i];
				}
                if (trigTriggers_R3[i].process(params[TRIG_PARAM_R3 + i].getValue())) 
				{
					trigs_R3[i] = !trigs_R3[i];
				}
                if (trigTriggers_R4[i].process(params[TRIG_PARAM_R4 + i].getValue())) 
				{
					trigs_R4[i] = !trigs_R4[i];
				}
								
				
				lights[GATE_LIGHTS_R1 + i].setSmoothBrightness((gateIn && i == index) ? (gates_R1[i] ? 1.f : 0.33) : (gates_R1[i] ? 0.66 : 0.0), args.sampleTime);
				lights[GATE2_LIGHTS_R1 + i].setSmoothBrightness((gateIn && i == index) ? (gates2_R1[i] ? 1.f : 0.33) : (gates2_R1[i] ? 0.66 : 0.0), args.sampleTime);
                lights[TRIG_LIGHTS_R1 + i].setSmoothBrightness((gateIn && i == index) ? (trigs_R1[i] ? 1.f : 0.33) : (trigs_R1[i] ? 0.66 : 0.0), args.sampleTime);

				//outputs[GATE_OUTPUT + i].setVoltage((running && i == index && gateIn && gates_R2[i]) ? 10.f : 0.f);
				lights[GATE_LIGHTS_R2 + i].setSmoothBrightness((gateIn && i == index) ? (gates_R2[i] ? 1.f : 0.33) : (gates_R2[i] ? 0.66 : 0.0), args.sampleTime);
				lights[GATE2_LIGHTS_R2 + i].setSmoothBrightness((gateIn && i == index) ? (gates2_R2[i] ? 1.f : 0.33) : (gates2_R2[i] ? 0.66 : 0.0), args.sampleTime);
                lights[TRIG_LIGHTS_R2 + i].setSmoothBrightness((gateIn && i == index) ? (trigs_R2[i] ? 1.f : 0.33) : (trigs_R2[i] ? 0.66 : 0.0), args.sampleTime);

				//outputs[GATE_OUTPUT + i].setVoltage((running && gateIn && i == index && gates_R3[i]) ? 10.f : 0.f);
				lights[GATE_LIGHTS_R3 + i].setSmoothBrightness((gateIn && i == index) ? (gates_R3[i] ? 1.f : 0.33) : (gates_R3[i] ? 0.66 : 0.0), args.sampleTime);
                lights[GATE2_LIGHTS_R3 + i].setSmoothBrightness((gateIn && i == index) ? (gates2_R3[i] ? 1.f : 0.33) : (gates2_R3[i] ? 0.66 : 0.0), args.sampleTime);
				lights[TRIG_LIGHTS_R3 + i].setSmoothBrightness((gateIn && i == index) ? (trigs_R3[i] ? 1.f : 0.33) : (trigs_R3[i] ? 0.66 : 0.0), args.sampleTime);

				//outputs[GATE_OUTPUT + i].setVoltage((running && gateIn && i == index && gates_R4[i]) ? 10.f : 0.f);
				lights[GATE_LIGHTS_R4 + i].setSmoothBrightness((gateIn && i == index) ? (gates_R4[i] ? 1.f : 0.33) : (gates_R4[i] ? 0.66 : 0.0), args.sampleTime);
                lights[GATE2_LIGHTS_R4 + i].setSmoothBrightness((gateIn && i == index) ? (gates2_R4[i] ? 1.f : 0.33) : (gates2_R4[i] ? 0.66 : 0.0), args.sampleTime);
				lights[TRIG_LIGHTS_R4 + i].setSmoothBrightness((gateIn && i == index) ? (trigs_R4[i] ? 1.f : 0.33) : (trigs_R4[i] ? 0.66 : 0.0), args.sampleTime);
			
				outputs[GATE_OUTPUT + i].setVoltage((running && gateIn && i == index ) ? 10.f : 0.f);
				//outputs[CH_GATE_OUTPUT + i].setVoltage((running && gateIn && i == index && (gates_R1[i] || gates_R2[i]) )  ? 10.f : 0.f);			
			}
		//  Row Outputs
		// Row1
		//if (params[ROW_ON_PARAM + 0].getValue())
		//{
			rowSetting = round(params[ROW1_PARAM + index].getValue() + 0.1);
			outputs[ROW1_OUTPUT].setVoltage(chromaticScale[rowSetting] / 12.0f);
			outVoct[0] = chromaticScale[rowSetting] / 12.0f;
		
			outGate[0] = (gateIn && gates_R1[index]  && params[ROW_ON_PARAM + 0].getValue() + inputs[ROW_ON_CV + 0].getVoltage()) ? 10.f : 0.f;
			outGate2[0] = (gateIn && gates2_R1[index] && params[ROW_ON_PARAM + 0].getValue() + inputs[ROW_ON_CV + 0].getVoltage()) ? 10.f : 0.f;
			outTrig[0] = (gateIn && trigs_R1[index] && params[ROW_ON_PARAM + 0].getValue() + inputs[ROW_ON_CV + 0].getVoltage() && pulse)  ? 10.f : 0.f;
		
		// Row2 
		//if (params[ROW_ON_PARAM + 1].getValue())
		//{
			rowSetting = round(params[ROW2_PARAM + index].getValue() + 0.1);
			outputs[ROW2_OUTPUT].setVoltage(chromaticScale[rowSetting] / 12.0f);
			outVoct[1] = chromaticScale[rowSetting] / 12.0f;

			outGate[1] = (gateIn && gates_R2[index]  && params[ROW_ON_PARAM + 1].getValue() + inputs[ROW_ON_CV + 1].getVoltage()) ? 10.f : 0.f;
			outGate2[1] = (gateIn && gates2_R2[index]  && params[ROW_ON_PARAM + 1].getValue() + inputs[ROW_ON_CV + 1].getVoltage()) ? 10.f : 0.f;
			outTrig[1] = (gateIn && trigs_R2[index]  && params[ROW_ON_PARAM + 1].getValue() + inputs[ROW_ON_CV + 1].getVoltage() && pulse) ? 10.f : 0.f;
		//}
		// Row3
		//if (params[ROW_ON_PARAM + 2].getValue())
		//{
			rowSetting = round(params[ROW3_PARAM + index].getValue() + 0.1);
		    outputs[ROW3_OUTPUT].setVoltage(chromaticScale[rowSetting] / 12.0f);
			outVoct[2] = chromaticScale[rowSetting] / 12.0f;

			outGate[2] = (gateIn && gates_R3[index]  && params[ROW_ON_PARAM + 2].getValue() + inputs[ROW_ON_CV + 2].getVoltage()) ? 10.f : 0.f;
			outGate2[2] = (gateIn && gates2_R3[index]  && params[ROW_ON_PARAM + 2].getValue() + inputs[ROW_ON_CV + 2].getVoltage()) ? 10.f : 0.f;
			outTrig[2] = (gateIn && trigs_R3[index]  && params[ROW_ON_PARAM + 2].getValue() + inputs[ROW_ON_CV + 2].getVoltage() && pulse) ? 10.f : 0.f;
		//}
        // Row4
		//if (params[ROW_ON_PARAM + 3].getValue())
		//{
			rowSetting = round(params[ROW4_PARAM + index].getValue() + 0.1);
			outputs[ROW4_OUTPUT].setVoltage((chromaticScale[rowSetting] / 12.0) + params[OCT4_PARAM].getValue());
			outVoct[3] = ((chromaticScale[rowSetting] / 12.0f) + (params[OCT4_PARAM].getValue() + inputs[OCT4_PARAM].getVoltage()));

			outGate[3] = (gateIn && gates_R4[index]  && params[ROW_ON_PARAM + 3].getValue() + inputs[ROW_ON_CV + 3].getVoltage()) ? 10.f : 0.f;
			outGate2[3] = (gateIn && gates2_R4[index]  && params[ROW_ON_PARAM + 3].getValue() + inputs[ROW_ON_CV + 3].getVoltage()) ? 10.f : 0.f;
			outTrig[3] = (gateIn && trigs_R4[index]  && params[ROW_ON_PARAM + 3].getValue() + inputs[ROW_ON_CV + 3].getVoltage() && pulse) ? 10.f : 0.f;
		//}

		outputs[GATES_OUTPUT].setVoltage(running && gateIn && (gates_R1[index] || gates2_R1[index]
																|| gates_R2[index] || gates2_R2[index]
																|| gates_R3[index] || gates2_R3[index]
																|| gates_R4[index] || gates2_R4[index]) ? 10.f : 0.f);
		
		//    Poly Outs
		outputs[P_GATE_OUTPUT].setChannels(channels);
        outputs[P_GATE_OUTPUT].writeVoltages(outGate);
		outputs[P_GATE2_OUTPUT].setChannels(channels);
        outputs[P_GATE2_OUTPUT].writeVoltages(outGate2);
		outputs[P_TRIG_OUTPUT].setChannels(channels);
        outputs[P_TRIG_OUTPUT].writeVoltages(outTrig);
        outputs[P_VOCT_OUTPUT].setChannels(channels);
        outputs[P_VOCT_OUTPUT].writeVoltages(outVoct);

		lights[RUNNING_LIGHT].value = (running);
		lights[RESET_LIGHT].setSmoothBrightness(resetTrigger.isHigh(), args.sampleTime);
		lights[GATES_LIGHT].setSmoothBrightness(gateIn, args.sampleTime);
		lights[GATE_LIGHTS + index].setSmoothBrightness(1.f, args.sampleTime);
		lights[ROW_LIGHTS + 0].value = outputs[ROW1_OUTPUT].value / 10.f;
		lights[ROW_LIGHTS + 1].value = outputs[ROW2_OUTPUT].value / 10.f;
		lights[ROW_LIGHTS + 2].value = outputs[ROW3_OUTPUT].value / 10.f;
		lights[ROW_LIGHTS + 3].value = outputs[ROW3_OUTPUT].value / 10.f;
	}
};

struct PolySEQ16Widget : ModuleWidget {
	PolySEQ16Widget(PolySEQ16 *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/polyseq16_gold.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		//static const float portX[16] = {20, 58, 96, 135, 173, 212, 250, 289, 327, 365, 404, 442, 481, 519, 558, 597};
		static const float portX[16] = {20, 57, 94, 132, 169, 207, 244, 282, 319, 356, 394, 431, 469, 506, 544, 582};

		//addParam(createParam<sts_Davies_37_Grey>(Vec(portX[0]-1, 30), module, PolySEQ16::CLOCK_PARAM));
		addParam(createParam<LEDButton>(Vec(portX[3]+2.5, 33), module, PolySEQ16::RUN_PARAM));
		addChild(createLight<MediumLight<GreenLight>>(Vec(portX[3]+6.9, 37), module, PolySEQ16::RUNNING_LIGHT));
		addParam(createParam<LEDButton>(Vec(96, 33), module, PolySEQ16::RESET_PARAM));
		addChild(createLight<MediumLight<GreenLight>>(Vec(100.4f, 37), module, PolySEQ16::RESET_LIGHT));
		addParam(createParam<sts_Davies_snap_25_Grey>(Vec(portX[4]-1, 30), module, PolySEQ16::STEPS_PARAM));
		addParam(createParam<sts_Davies_snap_25_Grey>(Vec(portX[11]-1, 30), module, PolySEQ16::OCT4_PARAM));


		addChild(createLight<MediumLight<GreenLight>>(Vec(portX[5]+7, 37), module, PolySEQ16::GATES_LIGHT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(portX[6]+7, 37), module, PolySEQ16::ROW_LIGHTS));
		addChild(createLight<MediumLight<GreenLight>>(Vec(portX[7]+7, 37), module, PolySEQ16::ROW_LIGHTS + 1));
		addChild(createLight<MediumLight<GreenLight>>(Vec(portX[8]+7, 37), module, PolySEQ16::ROW_LIGHTS + 2));
		addChild(createLight<MediumLight<GreenLight>>(Vec(portX[9]+7, 37), module, PolySEQ16::ROW_LIGHTS + 3));
				
		//addInput(createInput<sts_Port>(Vec(portX[0]+2, 64), module, PolySEQ16::INT_CLOCK_CV));
		addInput(createInput<sts_Port>(Vec(portX[0]+2, 64), module, PolySEQ16::EXT_CLOCK_INPUT));
		addInput(createInput<sts_Port>(Vec(portX[2]+2, 64), module, PolySEQ16::RESET_INPUT));
		addInput(createInput<sts_Port>(Vec(portX[3]+3, 64), module, PolySEQ16::RUN_ON_OFF));
		addInput(createInput<sts_Port>(Vec(portX[4]+2, 64), module, PolySEQ16::STEPS_INPUT));
		addInput(createInput<sts_Port>(Vec(portX[11]+2, 64), module, PolySEQ16::OCT4_CV_INPUT));

		addOutput(createOutput<sts_Port>(Vec(portX[5]+2, 64), module, PolySEQ16::GATES_OUTPUT));
		addOutput(createOutput<sts_Port>(Vec(portX[6]+2, 64), module, PolySEQ16::ROW1_OUTPUT));
		addOutput(createOutput<sts_Port>(Vec(portX[7]+2, 64), module, PolySEQ16::ROW2_OUTPUT));
		addOutput(createOutput<sts_Port>(Vec(portX[8]+2, 64), module, PolySEQ16::ROW3_OUTPUT));
		addOutput(createOutput<sts_Port>(Vec(portX[9]+2, 64), module, PolySEQ16::ROW4_OUTPUT));

		addOutput(createOutput<sts_Port>(Vec(portX[12]+2, 64), module, PolySEQ16::P_GATE_OUTPUT));
		addOutput(createOutput<sts_Port>(Vec(portX[13]+2, 64), module, PolySEQ16::P_GATE2_OUTPUT));
        addOutput(createOutput<sts_Port>(Vec(portX[14]+2, 64), module, PolySEQ16::P_TRIG_OUTPUT));
		addOutput(createOutput<sts_Port>(Vec(portX[15]+2, 64), module, PolySEQ16::P_VOCT_OUTPUT));

		////  Right side channel stuff
		//////////////////////  Row 1
		addParam(createParam<stsLEDButton1>(Vec(618, 129.9), module, PolySEQ16::ROW_ON_PARAM + 0));     
		addChild(createLight<MediumLight<YellowLight>>(Vec(620, 132), module, PolySEQ16::ROW_ON_LIGHTS + 0));
		addInput(createInputCentered<sts_Port>(Vec(624, 155), module, PolySEQ16::ROW_ON_CV + 0));

		//////////////////////  Row 2
		addParam(createParam<stsLEDButton1>(Vec(618, 189.9), module, PolySEQ16::ROW_ON_PARAM + 1));
		addChild(createLight<MediumLight<YellowLight>>(Vec(620, 192), module, PolySEQ16::ROW_ON_LIGHTS + 1));
		addInput(createInputCentered<sts_Port>(Vec(624, 215), module, PolySEQ16::ROW_ON_CV + 1));
		
		//////////////////////  Row 3
		addParam(createParam<stsLEDButton1>(Vec(618, 249.9), module, PolySEQ16::ROW_ON_PARAM + 2));
		addChild(createLight<MediumLight<YellowLight>>(Vec(620, 252), module, PolySEQ16::ROW_ON_LIGHTS + 2));
		addInput(createInputCentered<sts_Port>(Vec(624,275), module, PolySEQ16::ROW_ON_CV + 2));
		
		//////////////////////  Row 4
		addParam(createParam<stsLEDButton1>(Vec(618, 309.9), module, PolySEQ16::ROW_ON_PARAM + 3));
		addChild(createLight<MediumLight<YellowLight>>(Vec(620, 312), module, PolySEQ16::ROW_ON_LIGHTS + 3));
		addInput(createInputCentered<sts_Port>(Vec(624, 335), module, PolySEQ16::ROW_ON_CV + 3));

		for (int i = 0; i < 16; i++) 
		{
			addChild(createLight<SmallLight<GreenLight>>(Vec(portX[i]+8.f, 89), module, PolySEQ16::GATE_LIGHTS + i));

			//////////////////////  Row 1
			addParam(createParam<sts_Davies_snap_25_Grey>(Vec(portX[i]-2, 109), module, PolySEQ16::ROW1_PARAM + i));
			//Gate 1 Green
			addParam(createParam<stsLEDButton>(Vec(portX[i]-3.5, 137.9), module, PolySEQ16::GATE_PARAM_R1 + i));
			addChild(createLight<MediumLight<GreenLight>>(Vec(portX[i]-1.4f, 140), module, PolySEQ16::GATE_LIGHTS_R1 + i));
			//Gate 2 Blue
            addParam(createParam<stsLEDButton>(Vec(portX[i]+13.3, 137.9), module, PolySEQ16::GATE2_PARAM_R1 + i));
            addChild(createLight<MediumLight<BlueLight>>(Vec(portX[i]+15.4f, 140), module, PolySEQ16::GATE2_LIGHTS_R1 + i));
			//Trigger 1 Red
			addParam(createParam<stsLEDButton>(Vec(portX[i]+5.25, 148.9), module, PolySEQ16::TRIG_PARAM_R1 + i));
            addChild(createLight<MediumLight<RedLight>>(Vec(portX[i]+7.4f, 151), module, PolySEQ16::TRIG_LIGHTS_R1 + i));

			//////////////////////  Row 2
			addParam(createParam<sts_Davies_snap_25_Red>(Vec(portX[i]-2, 169), module, PolySEQ16::ROW2_PARAM + i));

			addParam(createParam<stsLEDButton>(Vec(portX[i]-3.5, 197.9), module, PolySEQ16::GATE_PARAM_R2 + i));
			addChild(createLight<MediumLight<GreenLight>>(Vec(portX[i]-1.4f, 200), module, PolySEQ16::GATE_LIGHTS_R2 + i));

            addParam(createParam<stsLEDButton>(Vec(portX[i]+13.3, 197.9), module, PolySEQ16::GATE2_PARAM_R2 + i));
            addChild(createLight<MediumLight<BlueLight>>(Vec(portX[i]+15.4f, 200), module, PolySEQ16::GATE2_LIGHTS_R2 + i));

			addParam(createParam<stsLEDButton>(Vec(portX[i]+5.25, 208.9), module, PolySEQ16::TRIG_PARAM_R2 + i));
            addChild(createLight<MediumLight<RedLight>>(Vec(portX[i]+7.4f, 211), module, PolySEQ16::TRIG_LIGHTS_R2 + i));

			//////////////////////  Row 3
			addParam(createParam<sts_Davies_snap_25_Blue>(Vec(portX[i]-2, 229), module, PolySEQ16::ROW3_PARAM + i));

			addParam(createParam<stsLEDButton>(Vec(portX[i]-3.5, 257.9), module, PolySEQ16::GATE_PARAM_R3 + i));
			addChild(createLight<MediumLight<GreenLight>>(Vec(portX[i]-1.4f, 260), module, PolySEQ16::GATE_LIGHTS_R3 + i));

            addParam(createParam<stsLEDButton>(Vec(portX[i]+13.3, 257.9), module, PolySEQ16::GATE2_PARAM_R3 + i));
            addChild(createLight<MediumLight<BlueLight>>(Vec(portX[i]+15.4f, 260), module, PolySEQ16::GATE2_LIGHTS_R3 + i));

			addParam(createParam<stsLEDButton>(Vec(portX[i]+5.25, 268.9), module, PolySEQ16::TRIG_PARAM_R3 + i));
            addChild(createLight<MediumLight<RedLight>>(Vec(portX[i]+7.4f, 271), module, PolySEQ16::TRIG_LIGHTS_R3 + i));

			//////////////////////  Row 4
			addParam(createParam<sts_Davies_snap_25_Teal>(Vec(portX[i]-2, 289), module, PolySEQ16::ROW4_PARAM + i));

			addParam(createParam<stsLEDButton>(Vec(portX[i]-3.5, 317.9), module, PolySEQ16::GATE_PARAM_R4 + i));
			addChild(createLight<MediumLight<GreenLight>>(Vec(portX[i]-1.4f, 320), module, PolySEQ16::GATE_LIGHTS_R4 + i));

            addParam(createParam<stsLEDButton>(Vec(portX[i]+13.3, 317.9), module, PolySEQ16::GATE2_PARAM_R4 + i));
            addChild(createLight<MediumLight<BlueLight>>(Vec(portX[i]+15.4f, 320), module, PolySEQ16::GATE2_LIGHTS_R4 + i));

			addParam(createParam<stsLEDButton>(Vec(portX[i]+5.25, 328.9), module, PolySEQ16::TRIG_PARAM_R4 + i));
            addChild(createLight<MediumLight<RedLight>>(Vec(portX[i]+7.4f, 331), module, PolySEQ16::TRIG_LIGHTS_R4 + i));

			//addParam(createParam<LEDButton>(Vec(portX[i]+2, 278-1), module, PolySEQ16::GATE_PARAM + i));
			addOutput(createOutput<sts_Port>(Vec(portX[i]+2, 346), module, PolySEQ16::GATE_OUTPUT + i));
		}
	}
};



Model *modelPolySEQ16 = createModel<PolySEQ16, PolySEQ16Widget>("PolySEQ16");
