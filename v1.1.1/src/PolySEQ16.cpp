#include "sts.hpp"

using namespace std;
using namespace rack;

struct PolySEQ16 : Module
{
    enum ParamIds
    {
        //CLOCK_PARAM,
        RUN_PARAM,
        RESET_PARAM,
        ENUMS(STEPS_PARAM, 4),
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
    enum InputIds
    {
        INT_CLOCK_CV,
        EXT_CLOCK_INPUT,
        RESET_INPUT,
        RUN_ON_OFF,
        ENUMS(STEPS_INPUT, 4),
        OCT4_CV_INPUT,
        ENUMS(ROW_ON_CV, 4),
        NUM_INPUTS
    };
    enum OutputIds
    {
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
    enum LightIds
    {
        RUNNING_LIGHT,
        RESET_LIGHT,
        GATES_LIGHT,
        ENUMS(ROW_LIGHTS, 4),
        ENUMS(ROW_ON_LIGHTS, 4),
        ENUMS(GATE_LIGHTS, 16),
        ENUMS(GATE_LIGHTS2, 16),
        ENUMS(GATE_LIGHTS3, 16),
        ENUMS(GATE_LIGHTS4, 16),
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
    int numSteps2;
    int numSteps3;
    int numSteps4;

    int panelStyle = 0;

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
    int index2 = 0;
    int index3 = 0;
    int index4 = 0;

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

    bool snap = true;

    const int chromaticScale[26] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};

    PolySEQ16()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        //configParam(CLOCK_PARAM, -2.f, 6.f, 2.f, "Clock Speed  ");
        configParam(RUN_PARAM, 0.f, 1.f, 0.f);
        configParam(RESET_PARAM, 0.f, 1.f, 0.f);
        configParam(STEPS_PARAM + 0, 1.f, 16.f, 16.f, "Steps Row 1");
        configParam(STEPS_PARAM + 1, 1.f, 16.f, 16.f, "Steps Row 2");
        configParam(STEPS_PARAM + 2, 1.f, 16.f, 16.f, "Steps Row 3");
        configParam(STEPS_PARAM + 3, 1.f, 16.f, 16.f, "Steps Row 4");
        configParam(OCT4_PARAM, -2.0f, 0.0f, 0.0f, "Row 4 Octave ");

        configParam(ROW_ON_PARAM + 0, 0.f, 1.f, 1.f);
        configParam(ROW_ON_PARAM + 1, 0.f, 1.f, 1.f);
        configParam(ROW_ON_PARAM + 2, 0.f, 1.f, 1.f);
        configParam(ROW_ON_PARAM + 3, 0.f, 1.f, 1.f);

        static std::string noteNames[25] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C"};

        struct RowParamQuantity : engine::ParamQuantity
        {
            std::string getDisplayValueString() override
            {
                int v = getValue();

                std::string note = noteNames[v];

                return note;
            }
        };

        //configParam<WaveParamQuantity>(WAVE_PARAM, 0.0f, 4.0f, 1.5f, "Waveform");
        //paramQuantities[WAVE_PARAM]->description = "Continuous: Sine - Triangle - Saw - Square - Sine";

        for (int i = 0; i < 16; i++)
        {
            configParam<RowParamQuantity>(ROW1_PARAM + i, 0.f, 24.f, 12.f, " ");
            configParam<RowParamQuantity>(ROW2_PARAM + i, 0.f, 24.f, 12.f, " ");
            configParam<RowParamQuantity>(ROW3_PARAM + i, 0.f, 24.f, 12.f, " ");
            configParam<RowParamQuantity>(ROW4_PARAM + i, 0.f, 24.f, 12.f, " ");
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
        snap = true;
    }

    void onReset() override
    {
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
        for (int i = 0; i < 4; i++)
        {
            row_on[i] = true;
        }
        snap = true;
        index = 0;
        index2 = 0;
        index3 = 0;
        index4 = 0;

    }

    
    void onRandomize() override
    {
        snap = (random::uniform() < 0.5);

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

        json_object_set_new(rootJ, "running", json_boolean(running));

        json_object_set_new(rootJ, "panelStyle", json_integer(panelStyle));

        json_object_set_new(rootJ, "Snap", json_boolean(snap));

        
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
            json_array_insert_new(gates1J, i, json_integer((int)gates_R1[i]));
        }
        json_object_set_new(rootJ, "gates1", gates1J);
        // row2
        json_t *gates2J = json_array();
        for (int i = 0; i < 16; i++)
        {
            json_array_insert_new(gates2J, i, json_integer((int)gates_R2[i]));
        }
        json_object_set_new(rootJ, "gates2", gates2J);
        // row3
        json_t *gates3J = json_array();
        for (int i = 0; i < 16; i++)
        {
            json_array_insert_new(gates3J, i, json_integer((int)gates_R3[i]));
        }
        json_object_set_new(rootJ, "gates3", gates3J);
        // row4
        json_t *gates4J = json_array();
        for (int i = 0; i < 16; i++)
        {
            json_array_insert_new(gates4J, i, json_integer((int)gates_R4[i]));
        }
        json_object_set_new(rootJ, "gates4", gates4J);
        //////////////// gate 2
        //row1
        json_t *gates12J = json_array();
        for (int i = 0; i < 16; i++)
        {
            json_array_insert_new(gates12J, i, json_integer((int)gates2_R1[i]));
        }
        json_object_set_new(rootJ, "gates12", gates12J);
        // row2
        json_t *gates22J = json_array();
        for (int i = 0; i < 16; i++)
        {
            json_array_insert_new(gates22J, i, json_integer((int)gates2_R2[i]));
        }
        json_object_set_new(rootJ, "gates22", gates22J);
        // row3
        json_t *gates32J = json_array();
        for (int i = 0; i < 16; i++)
        {
            json_array_insert_new(gates32J, i, json_integer((int)gates2_R3[i]));
        }
        json_object_set_new(rootJ, "gates32", gates32J);
        // row4
        json_t *gates42J = json_array();
        for (int i = 0; i < 16; i++)
        {
            json_array_insert_new(gates42J, i, json_integer((int)gates2_R4[i]));
        }
        json_object_set_new(rootJ, "gates42", gates42J);

        ///////////////////////// triggers
        //row1
        json_t *trigs1J = json_array();
        for (int i = 0; i < 16; i++)
        {
            json_array_insert_new(trigs1J, i, json_integer((int)trigs_R1[i]));
        }
        json_object_set_new(rootJ, "triggers1", trigs1J);
        //row2
        json_t *trigs2J = json_array();
        for (int i = 0; i < 16; i++)
        {
            json_array_insert_new(trigs2J, i, json_integer((int)trigs_R2[i]));
        }
        json_object_set_new(rootJ, "triggers2", trigs2J);
        //row3
        json_t *trigs3J = json_array();
        for (int i = 0; i < 16; i++)
        {
            json_array_insert_new(trigs3J, i, json_integer((int)trigs_R3[i]));
        }
        json_object_set_new(rootJ, "triggers3", trigs3J);
        //row4
        json_t *trigs4J = json_array();
        for (int i = 0; i < 16; i++)
        {
            json_array_insert_new(trigs4J, i, json_integer((int)trigs_R4[i]));
        }
        json_object_set_new(rootJ, "triggers4", trigs4J);

        // TODO ADD SNAP

        return rootJ;
    }

    void dataFromJson(json_t *rootJ) override
    {
        // running
        json_t *runningJ = json_object_get(rootJ, "running");
        if (runningJ)
            running = json_is_true(runningJ);

        json_t *panelStyleJ = json_object_get(rootJ, "panelStyle");
        if (panelStyleJ)
            panelStyle = json_integer_value(panelStyleJ);

        
        json_t *SnapJ = json_object_get(rootJ, "Snap");
        if (SnapJ)
        {
            snap = json_is_true(SnapJ);
            //setSnap(snap);
            
        }
        
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

        // TODO ADD SNAP
    }

    void incIndex()
    {
        numSteps = (int)clamp(std::round(params[STEPS_PARAM].getValue() + inputs[STEPS_INPUT].getVoltage()), 1.f, 16.f);
        numSteps2 = (int)clamp(std::round(params[STEPS_PARAM + 1].getValue() + inputs[STEPS_INPUT + 1].getVoltage()), 1.f, 16.f);
        numSteps3 = (int)clamp(std::round(params[STEPS_PARAM + 2].getValue() + inputs[STEPS_INPUT + 2].getVoltage()), 1.f, 16.f);
        numSteps4 = (int)clamp(std::round(params[STEPS_PARAM + 3].getValue() + inputs[STEPS_INPUT + 3].getVoltage()), 1.f, 16.f);
        phase = 0.f;

        index += 1;
        if (index >= numSteps)
        {
            index = 0;
        }

        index2 += 1;
        if (index2 >= numSteps2)
        {
            index2 = 0;
        }

        index3 += 1;
        if (index3 >= numSteps3)
        {
            index3 = 0;
        }

        index4 += 1;
        if (index4 >= numSteps4)
        {
            index4 = 0;
        }
    }

    void process(const ProcessArgs &args) override
    {

        int channels = 4;
        int rowSetting;

        //setSnap(snap);

        // Run
        if (runningTrigger.process(params[RUN_PARAM].getValue() || inputs[RUN_ON_OFF].getVoltage())) //  RUN_ON_OFF
        {
            running = !running;
        }

        bool gateIn = false;
        if (running)
        {
            if (inputs[EXT_CLOCK_INPUT].isConnected())
            {
                // External clock
                if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].getVoltage()))
                {
                    //setIndex(index + 1);
                    incIndex();
                    gatePulse.trigger(1e-3);
                }
                gateIn = clockTrigger.isHigh();
            }
        }

        // Reset
        if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage()))
        {
            index = 0;
            index2 = 0;
            index3 = 0;
            index4 = 0;
        }

        for (int i = 0; i < 16; i++)
        {
            lights[GATE_LIGHTS + i].setSmoothBrightness(0.f, args.sampleTime);
            lights[GATE_LIGHTS2 + i].setSmoothBrightness(0.f, args.sampleTime);
            lights[GATE_LIGHTS3 + i].setSmoothBrightness(0.f, args.sampleTime);
            lights[GATE_LIGHTS4 + i].setSmoothBrightness(0.f, args.sampleTime);
            outVoct[i] = 0.f;
        }

        // Row On Off Buttons

        for (int i = 0; i < 4; i++)
        {
            lights[ROW_ON_LIGHTS + i].setSmoothBrightness((params[ROW_ON_PARAM + i].getValue() + inputs[ROW_ON_CV + i].getVoltage()) - 0.7f, args.sampleTime);
            //lights[GATE_LIGHTS_R1 + i].setSmoothBrightness((gates_R1[i] ? 0.66 : 0.0), args.sampleTime);
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

            lights[GATE_LIGHTS_R1 + i].setSmoothBrightness((gates_R1[i] ? 0.66 : 0.0), args.sampleTime);
            lights[GATE2_LIGHTS_R1 + i].setSmoothBrightness((gates2_R1[i] ? 0.66 : 0.0), args.sampleTime);
            lights[TRIG_LIGHTS_R1 + i].setSmoothBrightness((trigs_R1[i] ? 0.66 : 0.0), args.sampleTime);

            lights[GATE_LIGHTS_R2 + i].setSmoothBrightness((gates_R2[i] ? 0.66 : 0.0), args.sampleTime);
            lights[GATE2_LIGHTS_R2 + i].setSmoothBrightness((gates2_R2[i] ? 0.66 : 0.0), args.sampleTime);
            lights[TRIG_LIGHTS_R2 + i].setSmoothBrightness((trigs_R2[i] ? 0.66 : 0.0), args.sampleTime);

            lights[GATE_LIGHTS_R3 + i].setSmoothBrightness((gates_R3[i] ? 0.66 : 0.0), args.sampleTime);
            lights[GATE2_LIGHTS_R3 + i].setSmoothBrightness((gates2_R3[i] ? 0.66 : 0.0), args.sampleTime);
            lights[TRIG_LIGHTS_R3 + i].setSmoothBrightness((trigs_R3[i] ? 0.66 : 0.0), args.sampleTime);

            lights[GATE_LIGHTS_R4 + i].setSmoothBrightness((gates_R4[i] ? 0.66 : 0.0), args.sampleTime);
            lights[GATE2_LIGHTS_R4 + i].setSmoothBrightness((gates2_R4[i] ? 0.66 : 0.0), args.sampleTime);
            lights[TRIG_LIGHTS_R4 + i].setSmoothBrightness((trigs_R4[i] ? 0.66 : 0.0), args.sampleTime);

            outputs[GATE_OUTPUT + i].setVoltage((running && gateIn && (i == index || i == index2 || i == index3 || i == index4)) ? 10.f : 0.f);
            //outputs[CH_GATE_OUTPUT + i].setVoltage((running && gateIn && i == index && (gates_R1[i] || gates_R2[i]))  ? 10.f : 0.f);
        }
        // Row Outputs

        // Row1
        if (params[ROW_ON_PARAM + 0].getValue())
        {
            if (!snap)
            {
                outputs[ROW1_OUTPUT].setVoltage((params[ROW1_PARAM + index].getValue() + 0.1) / 12);
                outVoct[0] = (params[ROW1_PARAM + index].getValue() + 0.1) / 12;
            }
            else
            {
                rowSetting = round(params[ROW1_PARAM + index].getValue() + 0.1);
                outputs[ROW1_OUTPUT].setVoltage(chromaticScale[rowSetting] / 12.0f);
                outVoct[0] = chromaticScale[rowSetting] / 12.0f;
            }
        }
        outGate[0] = (gateIn && gates_R1[index] && params[ROW_ON_PARAM + 0].getValue() + inputs[ROW_ON_CV + 0].getVoltage()) ? 10.f : 0.f;
        outGate2[0] = (gateIn && gates2_R1[index] && params[ROW_ON_PARAM + 0].getValue() + inputs[ROW_ON_CV + 0].getVoltage()) ? 10.f : 0.f;
        outTrig[0] = (gateIn && trigs_R1[index] && params[ROW_ON_PARAM + 0].getValue() + inputs[ROW_ON_CV + 0].getVoltage() && pulse) ? 10.f : 0.f;

        // Row2
        if (params[ROW_ON_PARAM + 1].getValue())
        {
            if (!snap)
            {
                outputs[ROW2_OUTPUT].setVoltage((params[ROW2_PARAM + index2].getValue() + 0.1) / 12);
                outVoct[1] = (params[ROW2_PARAM + index2].getValue() + 0.1) / 12;
            }
            else
            {
                rowSetting = round(params[ROW2_PARAM + index2].getValue() + 0.1);
                outputs[ROW2_OUTPUT].setVoltage(chromaticScale[rowSetting] / 12.0f);
                outVoct[1] = chromaticScale[rowSetting] / 12.0f;
            }
        }
        outGate[1] = (gateIn && gates_R2[index2] && params[ROW_ON_PARAM + 1].getValue() + inputs[ROW_ON_CV + 1].getVoltage()) ? 10.f : 0.f;
        outGate2[1] = (gateIn && gates2_R2[index2] && params[ROW_ON_PARAM + 1].getValue() + inputs[ROW_ON_CV + 1].getVoltage()) ? 10.f : 0.f;
        outTrig[1] = (gateIn && trigs_R2[index2] && params[ROW_ON_PARAM + 1].getValue() + inputs[ROW_ON_CV + 1].getVoltage() && pulse) ? 10.f : 0.f;

        // Row3

        if (params[ROW_ON_PARAM + 2].getValue())
        {
            if (!snap)
            {
                outputs[ROW3_OUTPUT].setVoltage((params[ROW3_PARAM + index2].getValue() + 0.1) / 12);
                outVoct[2] = (params[ROW3_PARAM + index3].getValue() + 0.1) / 12;
            }
            else
            {
                rowSetting = round(params[ROW3_PARAM + index3].getValue() + 0.1);
                outputs[ROW3_OUTPUT].setVoltage(chromaticScale[rowSetting] / 12.0f);
                outVoct[2] = chromaticScale[rowSetting] / 12.0f;
            }
        }
        outGate[2] = (gateIn && gates_R3[index3] && params[ROW_ON_PARAM + 2].getValue() + inputs[ROW_ON_CV + 2].getVoltage()) ? 10.f : 0.f;
        outGate2[2] = (gateIn && gates2_R3[index3] && params[ROW_ON_PARAM + 2].getValue() + inputs[ROW_ON_CV + 2].getVoltage()) ? 10.f : 0.f;
        outTrig[2] = (gateIn && trigs_R3[index3] && params[ROW_ON_PARAM + 2].getValue() + inputs[ROW_ON_CV + 2].getVoltage() && pulse) ? 10.f : 0.f;

        // Row4

        if (params[ROW_ON_PARAM + 3].getValue())
        {
            
            if (!snap)
            {
                float temp1;
                temp1 = ((params[ROW4_PARAM + index3].getValue()) + (params[OCT4_PARAM].getValue() * 12));
                outputs[ROW4_OUTPUT].setVoltage(temp1 / 12);
                outVoct[3] = temp1 / 12;
            }
            else
            {
                //rowSetting = round((params[ROW4_PARAM + index3].getValue()));
                rowSetting = params[ROW4_PARAM + index3].getValue();
                outputs[ROW4_OUTPUT].setVoltage(((chromaticScale[rowSetting]) + (params[OCT4_PARAM].getValue() * 12))/ 12.0f);
                outVoct[3] = ((chromaticScale[rowSetting]) + (params[OCT4_PARAM].getValue() * 12)) / 12.0f;
            }
        }
        outGate2[3] = (gateIn && gates2_R4[index4] && params[ROW_ON_PARAM + 3].getValue() + inputs[ROW_ON_CV + 3].getVoltage()) ? 10.f : 0.f;
        outTrig[3] = (gateIn && trigs_R4[index4] && params[ROW_ON_PARAM + 3].getValue() + inputs[ROW_ON_CV + 3].getVoltage() && pulse) ? 10.f : 0.f;

        outputs[GATES_OUTPUT].setVoltage(running && gateIn && (gates_R1[index] || gates2_R1[index] || gates_R2[index2] || gates2_R2[index2] || gates_R3[index3] || gates2_R3[index3] || gates_R4[index4] || gates2_R4[index4]) ? 10.f : 0.f);

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
        lights[GATE_LIGHTS + index].setSmoothBrightness(.5f, args.sampleTime);
        lights[GATE_LIGHTS2 + index2].setSmoothBrightness(.5f, args.sampleTime);
        lights[GATE_LIGHTS3 + index3].setSmoothBrightness(.5f, args.sampleTime);
        lights[GATE_LIGHTS4 + index4].setSmoothBrightness(.5f, args.sampleTime);
        lights[ROW_LIGHTS + 0].value = outputs[ROW1_OUTPUT].value / 10.f;
        lights[ROW_LIGHTS + 1].value = outputs[ROW2_OUTPUT].value / 10.f;
        lights[ROW_LIGHTS + 2].value = outputs[ROW3_OUTPUT].value / 10.f;
        lights[ROW_LIGHTS + 3].value = outputs[ROW3_OUTPUT].value / 10.f;
    }
};

struct PolySEQ16Widget;

struct SetSnapItem : MenuItem
{
    PolySEQ16Widget *widget;
    PolySEQ16 *module;
    void onAction(const event::Action &e) override;
};

struct PolySEQ16Widget : ModuleWidget
{
    
    Knob *knobs[16 * 4];
    
    void setSnap(bool snap)
    {
        for (auto i = 0; i < 16 * 4; i++)
        {
            knobs[i]->snap = snap;
            knobs[i]->smooth = !snap;
        }
    }
    
    SvgPanel *goldPanel;
    SvgPanel *blackPanel;
    SvgPanel *whitePanel;
    
    struct panelStyleItem : MenuItem
    {
        PolySEQ16 *module;
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
        PolySEQ16 *module = dynamic_cast<PolySEQ16 *>(this->module);

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

        SetSnapItem *setSnapItem = new SetSnapItem;
        setSnapItem->text = "Snap to Semitones?";
        setSnapItem->rightText = module->snap ? "✓" : "";
        setSnapItem->widget = this;
        setSnapItem->module = module;
        menu->addChild(setSnapItem);
    }

    PolySEQ16Widget(PolySEQ16 *module)
    {
        setModule(module);

        goldPanel = new SvgPanel();
        goldPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/polyseq16_gold.svg")));
        box.size = goldPanel->box.size;
        addChild(goldPanel);
        blackPanel = new SvgPanel();
        blackPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/polyseq16_black.svg")));
        blackPanel->visible = false;
        addChild(blackPanel);
        whitePanel = new SvgPanel();
        whitePanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/polyseq16_white.svg")));
        whitePanel->visible = false;
        addChild(whitePanel);

        addChild(createWidget<ScrewSilver>(Vec(15, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
        addChild(createWidget<ScrewSilver>(Vec(15, 365)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

        static const float locX[17] = {31, 68, 105, 142, 179, 216, 253, 290, 327, 364, 401, 438, 475, 512, 549, 586, 623};
        static const float locY[20] = {46, 73, 120, 136, 144, 156, 181, 197, 205, 217, 242, 258, 266, 278, 303, 319, 327, 339, 355};

        addParam(createParamCentered<LEDButton>(Vec(locX[2], locY[0]), module, PolySEQ16::RUN_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(locX[2], locY[0]), module, PolySEQ16::RUNNING_LIGHT));
        addParam(createParamCentered<LEDButton>(Vec(locX[1], locY[0]), module, PolySEQ16::RESET_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(locX[1], locY[0]), module, PolySEQ16::RESET_LIGHT));
        addParam(createParamCentered<sts_Davies_snap_25_Teal>(Vec(locX[7], locY[0]), module, PolySEQ16::OCT4_PARAM));

        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(locX[8], locY[0]), module, PolySEQ16::GATES_LIGHT));
        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(locX[9], locY[0]), module, PolySEQ16::ROW_LIGHTS));
        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(locX[10], locY[0]), module, PolySEQ16::ROW_LIGHTS + 1));
        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(locX[11], locY[0]), module, PolySEQ16::ROW_LIGHTS + 2));
        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(locX[12], locY[0]), module, PolySEQ16::ROW_LIGHTS + 3));

        addParam(createParamCentered<sts_Davies_snap_25_Yellow>(Vec(locX[3], locY[0]), module, PolySEQ16::STEPS_PARAM + 0));
        addInput(createInputCentered<sts_Port>(Vec(locX[3], locY[1]), module, PolySEQ16::STEPS_INPUT + 0));
        addParam(createParamCentered<sts_Davies_snap_25_Red>(Vec(locX[4], locY[0]), module, PolySEQ16::STEPS_PARAM + 1));
        addInput(createInputCentered<sts_Port>(Vec(locX[4], locY[1]), module, PolySEQ16::STEPS_INPUT + 1));
        addParam(createParamCentered<sts_Davies_snap_25_Blue>(Vec(locX[5], locY[0]), module, PolySEQ16::STEPS_PARAM + 2));
        addInput(createInputCentered<sts_Port>(Vec(locX[5], locY[1]), module, PolySEQ16::STEPS_INPUT + 2));
        addParam(createParamCentered<sts_Davies_snap_25_Teal>(Vec(locX[6], locY[0]), module, PolySEQ16::STEPS_PARAM + 3));
        addInput(createInputCentered<sts_Port>(Vec(locX[6], locY[1]), module, PolySEQ16::STEPS_INPUT + 3));

        //addInput(createInput<sts_Port>(Vec(portX[0]+2, 64), module, PolySEQ16::INT_CLOCK_CV));
        addInput(createInputCentered<sts_Port>(Vec(locX[0], locY[1]), module, PolySEQ16::EXT_CLOCK_INPUT));
        addInput(createInputCentered<sts_Port>(Vec(locX[1], locY[1]), module, PolySEQ16::RESET_INPUT));
        addInput(createInputCentered<sts_Port>(Vec(locX[2], locY[1]), module, PolySEQ16::RUN_ON_OFF));

        addInput(createInputCentered<sts_Port>(Vec(locX[7], locY[1]), module, PolySEQ16::OCT4_CV_INPUT));
        addOutput(createOutputCentered<sts_Port>(Vec(locX[8], locY[1]), module, PolySEQ16::GATES_OUTPUT));
        addOutput(createOutputCentered<sts_Port>(Vec(locX[9], locY[1]), module, PolySEQ16::ROW1_OUTPUT));
        addOutput(createOutputCentered<sts_Port>(Vec(locX[10], locY[1]), module, PolySEQ16::ROW2_OUTPUT));
        addOutput(createOutputCentered<sts_Port>(Vec(locX[11], locY[1]), module, PolySEQ16::ROW3_OUTPUT));
        addOutput(createOutputCentered<sts_Port>(Vec(locX[12], locY[1]), module, PolySEQ16::ROW4_OUTPUT));

        addOutput(createOutputCentered<sts_Port>(Vec(locX[13], locY[1]), module, PolySEQ16::P_GATE_OUTPUT));
        addOutput(createOutputCentered<sts_Port>(Vec(locX[14], locY[1]), module, PolySEQ16::P_GATE2_OUTPUT));
        addOutput(createOutputCentered<sts_Port>(Vec(locX[15], locY[1]), module, PolySEQ16::P_TRIG_OUTPUT));
        addOutput(createOutputCentered<sts_Port>(Vec(locX[16], locY[1]), module, PolySEQ16::P_VOCT_OUTPUT));

        ////  Right side channel stuff
        //////////////////////  Row 1
        addParam(createParamCentered<stsLEDButton1>(Vec(locX[16], locY[2]), module, PolySEQ16::ROW_ON_PARAM + 0));
        addChild(createLightCentered<MediumLight<YellowLight>>(Vec(locX[16], locY[2]), module, PolySEQ16::ROW_ON_LIGHTS + 0));
        addInput(createInputCentered<sts_Port>(Vec(locX[16], locY[4]), module, PolySEQ16::ROW_ON_CV + 0));

        //////////////////////  Row 2
        addParam(createParamCentered<stsLEDButton1>(Vec(locX[16], locY[6]), module, PolySEQ16::ROW_ON_PARAM + 1));
        addChild(createLightCentered<MediumLight<YellowLight>>(Vec(locX[16], locY[6]), module, PolySEQ16::ROW_ON_LIGHTS + 1));
        addInput(createInputCentered<sts_Port>(Vec(locX[16], locY[8]), module, PolySEQ16::ROW_ON_CV + 1));

        //////////////////////  Row 3
        addParam(createParamCentered<stsLEDButton1>(Vec(locX[16], locY[10]), module, PolySEQ16::ROW_ON_PARAM + 2));
        addChild(createLightCentered<MediumLight<YellowLight>>(Vec(locX[16], locY[10]), module, PolySEQ16::ROW_ON_LIGHTS + 2));
        addInput(createInputCentered<sts_Port>(Vec(locX[16], locY[12]), module, PolySEQ16::ROW_ON_CV + 2));

        //////////////////////  Row 4
        addParam(createParamCentered<stsLEDButton1>(Vec(locX[16], locY[14]), module, PolySEQ16::ROW_ON_PARAM + 3));
        addChild(createLightCentered<MediumLight<YellowLight>>(Vec(locX[16], locY[14]), module, PolySEQ16::ROW_ON_LIGHTS + 3));
        addInput(createInputCentered<sts_Port>(Vec(locX[16], locY[16]), module, PolySEQ16::ROW_ON_CV + 3));

        for (int i = 0; i < 16; i++)
        {
            //////////////////////  Row 1 Yellow
            addChild(createLightCentered<SmallLight<YellowLight>>(Vec(locX[i], locY[3]), module, PolySEQ16::GATE_LIGHTS + i));
            //addParam(createParamCentered<sts_Davies_snap_25_Yellow>(Vec(locX[i], locY[2]), module, PolySEQ16::ROW1_PARAM + i));
            knobs[16 * 0 + i] = createParamCentered<sts_Davies_25_Yellow>(Vec(locX[i], locY[2]), module, PolySEQ16::ROW1_PARAM + i);
            addParam(knobs[16 * 0 + i]);
            //Gate 1 Green
            addParam(createParamCentered<stsLEDButton>(Vec(locX[i] - 8, locY[4]), module, PolySEQ16::GATE_PARAM_R1 + i));
            addChild(createLightCentered<MediumLight<GreenLight>>(Vec(locX[i] - 8, locY[4]), module, PolySEQ16::GATE_LIGHTS_R1 + i));
            //Gate 2 Blue
            addParam(createParamCentered<stsLEDButton>(Vec(locX[i] + 8, locY[4]), module, PolySEQ16::GATE2_PARAM_R1 + i));
            addChild(createLightCentered<MediumLight<BlueLight>>(Vec(locX[i] + 8, locY[4]), module, PolySEQ16::GATE2_LIGHTS_R1 + i));
            //Trigger 1 Red
            addParam(createParamCentered<stsLEDButton>(Vec(locX[i], locY[5]), module, PolySEQ16::TRIG_PARAM_R1 + i));
            addChild(createLightCentered<MediumLight<RedLight>>(Vec(locX[i], locY[5]), module, PolySEQ16::TRIG_LIGHTS_R1 + i));

            //////////////////////  Row 2
            addChild(createLightCentered<SmallLight<YellowLight>>(Vec(locX[i], locY[7]), module, PolySEQ16::GATE_LIGHTS2 + i));
            knobs[16 * 1 + i] = createParamCentered<sts_Davies_snap_25_Red>(Vec(locX[i], locY[6]), module, PolySEQ16::ROW2_PARAM + i);
            addParam(knobs[16 * 1 + i]);

            addParam(createParamCentered<stsLEDButton>(Vec(locX[i] - 8, locY[8]), module, PolySEQ16::GATE_PARAM_R2 + i));
            addChild(createLightCentered<MediumLight<GreenLight>>(Vec(locX[i] - 8, locY[8]), module, PolySEQ16::GATE_LIGHTS_R2 + i));

            addParam(createParamCentered<stsLEDButton>(Vec(locX[i] + 8, locY[8]), module, PolySEQ16::GATE2_PARAM_R2 + i));
            addChild(createLightCentered<MediumLight<BlueLight>>(Vec(locX[i] + 8, locY[8]), module, PolySEQ16::GATE2_LIGHTS_R2 + i));

            addParam(createParamCentered<stsLEDButton>(Vec(locX[i], locY[9]), module, PolySEQ16::TRIG_PARAM_R2 + i));
            addChild(createLightCentered<MediumLight<RedLight>>(Vec(locX[i], locY[9]), module, PolySEQ16::TRIG_LIGHTS_R2 + i));

            //////////////////////  Row 3
            addChild(createLightCentered<SmallLight<YellowLight>>(Vec(locX[i], locY[11]), module, PolySEQ16::GATE_LIGHTS3 + i));
            knobs[16 * 2 + i] = createParamCentered<sts_Davies_snap_25_Blue>(Vec(locX[i], locY[10]), module, PolySEQ16::ROW3_PARAM + i);
            addParam(knobs[16 * 2 + i]);

            addParam(createParamCentered<stsLEDButton>(Vec(locX[i] - 8, locY[12]), module, PolySEQ16::GATE_PARAM_R3 + i));
            addChild(createLightCentered<MediumLight<GreenLight>>(Vec(locX[i] - 8, locY[12]), module, PolySEQ16::GATE_LIGHTS_R3 + i));

            addParam(createParamCentered<stsLEDButton>(Vec(locX[i] + 8, locY[12]), module, PolySEQ16::GATE2_PARAM_R3 + i));
            addChild(createLightCentered<MediumLight<BlueLight>>(Vec(locX[i] + 8, locY[12]), module, PolySEQ16::GATE2_LIGHTS_R3 + i));

            addParam(createParamCentered<stsLEDButton>(Vec(locX[i], locY[13]), module, PolySEQ16::TRIG_PARAM_R3 + i));
            addChild(createLightCentered<MediumLight<RedLight>>(Vec(locX[i], locY[13]), module, PolySEQ16::TRIG_LIGHTS_R3 + i));

            //////////////////////  Row 4
            addChild(createLightCentered<SmallLight<YellowLight>>(Vec(locX[i], locY[15]), module, PolySEQ16::GATE_LIGHTS4 + i));
            knobs[16 * 3 + i] = createParamCentered<sts_Davies_snap_25_Teal>(Vec(locX[i], locY[14]), module, PolySEQ16::ROW4_PARAM + i);
            addParam(knobs[16 * 3 + i]);

            addParam(createParamCentered<stsLEDButton>(Vec(locX[i] - 8, locY[16]), module, PolySEQ16::GATE_PARAM_R4 + i));
            addChild(createLightCentered<MediumLight<GreenLight>>(Vec(locX[i] - 8, locY[16]), module, PolySEQ16::GATE_LIGHTS_R4 + i));

            addParam(createParamCentered<stsLEDButton>(Vec(locX[i] + 8, locY[16]), module, PolySEQ16::GATE2_PARAM_R4 + i));
            addChild(createLightCentered<MediumLight<BlueLight>>(Vec(locX[i] + 8, locY[16]), module, PolySEQ16::GATE2_LIGHTS_R4 + i));

            addParam(createParamCentered<stsLEDButton>(Vec(locX[i], locY[17]), module, PolySEQ16::TRIG_PARAM_R4 + i));
            addChild(createLightCentered<MediumLight<RedLight>>(Vec(locX[i], locY[17]), module, PolySEQ16::TRIG_LIGHTS_R4 + i));

            //addParam(createParam<LEDButton>(Vec(portX[i]+2, 278-1), module, PolySEQ16::GATE_PARAM + i));
            addOutput(createOutputCentered<sts_Port>(Vec(locX[i], locY[18]), module, PolySEQ16::GATE_OUTPUT + i));
        }
    }
    /*
    json_t *toJson() override
    {
        json_t *rootJ = ModuleWidget::toJson();

        // snap
        json_object_set_new(rootJ, "snap", json_boolean(snap));

        return rootJ;
    }

    void fromJson(json_t *rootJ) override
    {
        ModuleWidget::fromJson(rootJ);

        // snap
        json_t *snapJ = json_object_get(rootJ, "snap");
        if (snapJ)
            snap = json_is_true(snapJ);
    }
    */
    void step() override
    {
        if (module)
        {
            goldPanel->visible = ((((PolySEQ16 *)module)->panelStyle) == 0);
            blackPanel->visible = ((((PolySEQ16 *)module)->panelStyle) == 1);
            whitePanel->visible = ((((PolySEQ16 *)module)->panelStyle) == 2);

            if(((PolySEQ16 *)module) -> snap != knobs[0]->snap)
            {
                setSnap(((PolySEQ16 *)module)->snap);
            }
        }
        Widget::step();
    }
};
void SetSnapItem::onAction(const event::Action &e)
{
    module->snap = !module->snap;
    //widget->setSnap(module->snap);
}

Model *modelPolySEQ16 = createModel<PolySEQ16, PolySEQ16Widget>("PolySEQ16");
