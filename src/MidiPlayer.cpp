//***********************************************************************************************
//MidiFile module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental, Core and AudibleInstruments plugins by Andrew Belt
//and graphics from the Component Library by Wes Milholen
//Also based on Midifile, a C++ MIDI file parsing library by Craig Stuart Sapp
//  https://github.com/craigsapp/midifile
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept by Marc Boulé @ Impromptu Modular
//Finished and converted to V1 and Polyphonic By Don Turnock @ STS
//***********************************************************************************************

//http://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html

#include "sts.hpp"
#include "midifile/MidiFile.h"
#include "osdialog.h"
#include <iostream>
#include <algorithm>
#include "string.hpp"
#include <set>

using namespace rack;
using namespace std;
using namespace smf;
using namespace string;

struct PlaybackTrack
{
	int track;
	int channel;
	int poly;
	std::string name;
};

struct MidiPlayer : Module
{
    enum ParamIds
    {
        LOADMIDI_PARAM,
        RESET_PARAM,
        RUN_PARAM,
        LOOP_PARAM,
        CHANNEL_PARAM, // 0 means all channels, 1 to 16 for filter channel
        NUM_PARAMS
    };
    enum InputIds
    {
        CLK_INPUT,
        RESET_INPUT,
        RUNCV_INPUT,
        NUM_INPUTS
    };
    enum OutputIds
    {
        //CV_OUTPUT,
        //GATE_OUTPUT,
        //VELOCITY_OUTPUT,
        //AFTERTOUCH_OUTPUT,
        PITCH_OUTPUT,
        MOD_OUTPUT,
        RETRIGGER_OUTPUT,
        CLOCK_OUTPUT,
        CLOCK_DIV_OUTPUT,
        START_OUTPUT,
        STOP_OUTPUT,
        CONTINUE_OUTPUT,
        ENUMS(CV_OUTPUT, 16),
        ENUMS(GATE_OUTPUT, 16),
        ENUMS(VELOCITY_OUTPUT, 16),
        ENUMS(AFTERTOUCH_OUTPUT, 16),
        NUM_OUTPUTS
    };
    enum LightIds
    {
        RESET_LIGHT,
        RUN_LIGHT,
        ENUMS(LOADMIDI_LIGHT, 2),
        NUM_LIGHTS
    };

    struct NoteData
    {
        uint8_t velocity = 0;
        uint8_t aftertouch = 0;
    };

    // Constants
    enum PolyMode
    {
        ROTATE_MODE,
        REUSE_MODE,
        RESET_MODE,
        REASSIGN_MODE,
        UNISON_MODE,
        MPE_MODE,
        NUM_MODES
    };

		// Midi track playback information
		std::vector<PlaybackTrack> playbackTracks;

    // Need to save
    int panelTheme = 0;
    PolyMode polyMode = RESET_MODE; // From QuadMIDIToCVInterface.cpp
    std::string lastPath = "";
    std::string lastFilename = "--";

    // No need to save
    bool running;
    double time;
    long event;
    //
    //--- START From QuadMIDIToCVInterface.cpp
    NoteData noteData[128];
    // cachedNotes : UNISON_MODE and REASSIGN_MODE cache all played notes. The other polyModes cache stolen notes (after the 4th one).
    std::vector<uint8_t> cachedNotes; //(128);
    uint8_t notes[16];
    bool gates[16];
    // gates set to TRUE by pedal and current gate. FALSE by pedal.
    bool pedalgates[16];
    bool pedal;
    int rotateIndex;
    int stealIndex;
    //--- END From QuadMIDIToCVInterface.cpp

    int tracks;
    unsigned int lightRefreshCounter = 0;
    float resetLight = 0.0f;
    bool fileLoaded = false;
    MidiFile midifile;
    dsp::SchmittTrigger runningTrigger;
    dsp::SchmittTrigger resetTrigger;
    dsp::SchmittTrigger btnLoadMidi;

    float CV_Out[MAX_POLY_CHANNELS];
    float GATE_Out[MAX_POLY_CHANNELS];
    float VELOCITY_Out[MAX_POLY_CHANNELS];

    inline int getChannelKnob() { return (int)(params[CHANNEL_PARAM].getValue() + 0.5f); }

    MidiPlayer()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS); //, cachedNotes(128)); //, cachedNotes(128);
        configParam(CHANNEL_PARAM, 0.0f, 16.0f, 0.0f, "Midi Channel ");
        cachedNotes.clear();
        onReset();
    }

    void onReset() override
    {
        running = false;
        resetPlayer();
    }

    void onRandomize() override
    {
    }

    void resetPlayer()
    {
        time = 0.0;
        event = 0;
        //
        cachedNotes.clear();
        for (int i = 0; i < 16; i++)
        {
            notes[i] = 60;
            gates[i] = false;
            pedalgates[i] = false;
        }
        pedal = false;
        rotateIndex = -1;
        stealIndex = 0;
    }

    json_t *dataToJson() override
    {
        json_t *rootJ = json_object();

        // panelTheme
        json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

        // polyMode
        json_object_set_new(rootJ, "polyMode", json_integer(polyMode));

        // lastPath
        json_object_set_new(rootJ, "lastPath", json_string(lastPath.c_str()));

        return rootJ;
    }

    void dataFromJson(json_t *rootJ) override
    {
        // panelTheme
        json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
        if (panelThemeJ)
            panelTheme = json_integer_value(panelThemeJ);

        // polyMode
        json_t *polyModeJ = json_object_get(rootJ, "polyMode");
        if (polyModeJ)
            polyMode = (PolyMode)json_integer_value(polyModeJ);

        // lastPath
        json_t *lastPathJ = json_object_get(rootJ, "lastPath");
        if (lastPathJ)
            lastPath = json_string_value(lastPathJ);
    }

    //------------ START QuadMIDIToCVInterface.cpp (with slight modifications) ------------------

    int getPolyIndex(int nowIndex)
    {
        for (int i = 0; i < 16; i++)
        {
            nowIndex++;
            if (nowIndex > 15) //if (nowIndex > 3)
                nowIndex = 0;
            if (!(gates[nowIndex] || pedalgates[nowIndex]))
            {
                stealIndex = nowIndex;
                return nowIndex;
            }
        }
        // All taken = steal (stealIndex always rotates)
        stealIndex++;
        if (stealIndex > 15) //if (stealIndex > 3)
            stealIndex = 0;
        if ((polyMode < REASSIGN_MODE) && (gates[stealIndex]))
            cachedNotes.push_back(notes[stealIndex]);
        return stealIndex;
    }

    void pressNote(uint8_t note)
    {
        // Set notes and gates
        switch (polyMode)
        {
        case ROTATE_MODE:
        {
            rotateIndex = getPolyIndex(rotateIndex);
        }
        break;

        case REUSE_MODE:
        {
            bool reuse = false;
            for (int i = 0; i < 16; i++)
            {
                if (notes[i] == note)
                {
                    rotateIndex = i;
                    reuse = true;
                    break;
                }
            }
            if (!reuse)
                rotateIndex = getPolyIndex(rotateIndex);
        }
        break;

        case RESET_MODE:
        {
            rotateIndex = getPolyIndex(-1);
        }
        break;

        case REASSIGN_MODE:
        {
            cachedNotes.push_back(note);
            rotateIndex = getPolyIndex(-1);
        }
        break;

        case UNISON_MODE:
        {
            cachedNotes.push_back(note);
            for (int i = 0; i < 16; i++)
            {
                notes[i] = note;
                gates[i] = true;
                pedalgates[i] = pedal;
                // reTrigger[i].trigger(1e-3);
            }
            return;
        }
        break;

        default:
            break;
        }
        // Set notes and gates
        // if (gates[rotateIndex] || pedalgates[rotateIndex])
        // 	reTrigger[rotateIndex].trigger(1e-3);
        notes[rotateIndex] = note;
        gates[rotateIndex] = true;
        pedalgates[rotateIndex] = pedal;
    }

    void releaseNote(uint8_t note)
    {
        // Remove the note
        auto it = find(cachedNotes.begin(), cachedNotes.end(), note);
        if (it != cachedNotes.end())
            cachedNotes.erase(it);

        switch (polyMode)
        {
        case REASSIGN_MODE:
        {
            for (int i = 0; i < 16; i++)
            {
                if (i < (int)cachedNotes.size())
                {
                    if (!pedalgates[i])
                        notes[i] = cachedNotes[i];
                    pedalgates[i] = pedal;
                }
                else
                {
                    gates[i] = false;
                }
            }
        }
        break;

        case UNISON_MODE:
        {
            if (!cachedNotes.empty())
            {
                uint8_t backnote = cachedNotes.back();
                for (int i = 0; i < 16; i++)
                {
                    notes[i] = backnote;
                    gates[i] = true;
                }
            }
            else
            {
                for (int i = 0; i < 16; i++)
                {
                    gates[i] = false;
                }
            }
        }
        break;

        // default ROTATE_MODE REUSE_MODE RESET_MODE
        default:
        {
            for (int i = 0; i < 16; i++)
            {
                if (notes[i] == note)
                {
                    if (pedalgates[i])
                    {
                        gates[i] = false;
                    }
                    else if (!cachedNotes.empty())
                    {
                        notes[i] = cachedNotes.back();
                        cachedNotes.pop_back();
                    }
                    else
                    {
                        gates[i] = false;
                    }
                }
            }
        }
        break;
        }
    }

    void pressPedal()
    {
        pedal = true;
        for (int i = 0; i < 16; i++)
        {
            pedalgates[i] = gates[i];
        }
    }

    void releasePedal()
    {
        pedal = false;
        // When pedal is off, recover notes for pressed keys (if any) after they were already being "cycled" out by pedal-sustained notes.
        for (int i = 0; i < 16; i++)
        {
            pedalgates[i] = false;
            if (!cachedNotes.empty())
            {
                if (polyMode < REASSIGN_MODE)
                {
                    notes[i] = cachedNotes.back();
                    cachedNotes.pop_back();
                    gates[i] = true;
                }
            }
        }
        if (polyMode == REASSIGN_MODE)
        {
            for (int i = 0; i < 16; i++)
            {
                if (i < (int)cachedNotes.size())
                {
                    notes[i] = cachedNotes[i];
                    gates[i] = true;
                }
                else
                {
                    gates[i] = false;
                }
            }
        }
    }

    void processMessage(MidiMessage *msg)
    {
        switch (msg->getCommandByte() >> 4)
        { //status()
        // note off
        case 0x8:
        {
            releaseNote(msg->getKeyNumber()); //note()
        }
        break;
        // note on
        case 0x9:
        {
            if (msg->getVelocity() > 0)
            {                                                                //value()
                noteData[msg->getKeyNumber()].velocity = msg->getVelocity(); //note(),  value()
                pressNote(msg->getKeyNumber());                              //note()
            }
            else
            {
                releaseNote(msg->getKeyNumber()); //note()
            }
        }
        break;
        // channel aftertouch
        case 0xa:
        {
            noteData[msg->getKeyNumber()].aftertouch = msg->getP2(); //note(),  value()
        }
        break;
        // cc
        case 0xb:
        {
            processCC(msg);
        }
        break;
        default:
            break;
        }
    }

    void processCC(MidiMessage *msg)
    {
        switch (msg->getControllerNumber())
        { //note()
        // sustain
        case 0x40:
        {
            if (msg->getControllerValue() >= 64) //value()
                pressPedal();
            else
                releasePedal();
        }
        break;
        default:
            break;
        }
    }

    //------------ END QuadMIDIToCVInterface.cpp ------------------

    void loadMidiFile()
    {
        osdialog_filters *filters = osdialog_filters_parse("Midi File (.mid):mid;Text File (.txt):txt");
        std::string dir = lastPath.empty() ? asset::user("") : directory(lastPath);
        char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, filters);
        if (path)
        {
            lastPath = path;
            lastFilename = filename(path);
            if (midifile.read(path))
            {
                fileLoaded = true;
                midifile.doTimeAnalysis();
                midifile.linkNotePairs();
                tracks = midifile.getTrackCount();

                cout << "TPQ: " << midifile.getTicksPerQuarterNote() << endl;
                //if (tracks > 1)
                cout << "TRACKS: " << tracks << endl;
								playbackTracks.clear();
                for (int ii = 0; ii < tracks; ii++)
                {
										std::set<int> channels;
										std::string name;
                    for (int i = 0; i < midifile[ii].getEventCount(); i++)
                    {
                        if (midifile[ii][i].isTrackName())
                        {
                            name = midifile[ii][i].getMetaContent();
                            // cout << "Track # " << ii << " " << name << endl;
                            //cout << content << endl;
                        } else if (midifile[ii][i].isNoteOn()) {
													channels.insert(midifile[ii][i].getChannelNibble());
												}
                    }

										for (int channel : channels) {
												int poly = 0;
												int maxpoly = 0;

												for (int i = 0; i < midifile[ii].getEventCount(); i++)
												{
														if (midifile[ii][i].isNoteOn() && midifile[ii][i].getChannelNibble() == channel) {
															poly += 1;
															if (poly > maxpoly) {
																maxpoly = poly;
															}
														} else if (midifile[ii][i].isNoteOff() && midifile[ii][i].getChannelNibble() == channel) {
															poly -= 1;
														}
												}

												PlaybackTrack playbackTrack;
												playbackTrack.name = name;
												playbackTrack.channel = channel;
												playbackTrack.poly = maxpoly;
												playbackTrack.track = ii;
												playbackTracks.push_back(playbackTrack);
										}

                }

								cout << "Playback tracks: " << playbackTracks.size() << endl;
								for (auto& playbackTrack : playbackTracks) {
									cout << "Playback track: " << playbackTrack.track << "/" << playbackTrack.channel << "@" << playbackTrack.poly << " " << playbackTrack.name << endl;
								}

                midifile.joinTracks();
                //midifile.splitTracks();
                //midifile.splitTracksByChannel();

                /*	
				for (int track = 0; track < tracks; track++)
				{
					if (tracks > 1)
						cout << "\nTrack " << track << endl;
						cout << "Tick\tSeconds\tDur\tMessage" << endl;
					for (int eventIndex = 0; eventIndex < midifile[track].size(); eventIndex++)
					{
						cout << dec << midifile[track][eventIndex].tick;
						cout << '\t' << dec << midifile[track][eventIndex].seconds;
						cout << '\t';
						if (midifile[track][eventIndex].isNoteOn())
							cout << midifile[track][eventIndex].getDurationInSeconds();
						cout << '\t' << hex;
						for (unsigned int i = 0; i < midifile[track][eventIndex].size(); i++)
							cout << (int)midifile[track][eventIndex][i] << ' ';
						cout << endl;
					}
				}
				cout << "event count: " << dec << midifile[0].size() << endl;
				*/
            }
            else
                fileLoaded = false;
            free(path);
            running = false;
            time = 0.0;
            event = 0;
        }
        osdialog_filters_free(filters);
    }

    // Advances the module by 1 audio frame with duration 1.0 / args.sampleRate
    void process(const ProcessArgs &args) override
    {
        int channels = 16;
        int Mchannel[100] = {};
        const int track = 0; // midifile was flattened when loaded
        double sampleTime = args.sampleTime;
        if (btnLoadMidi.process(params[LOADMIDI_PARAM].value))
        {

            loadMidiFile();
        }
        //********** Buttons, knobs, switches and inputs **********

        // Reset
        if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage()))
        {
            resetLight = 1.0f;
            resetPlayer();
        }

        // Run state button
        if (runningTrigger.process(params[RUN_PARAM].getValue() + inputs[RUNCV_INPUT].getVoltage()))
        {
            running = !running;
        }

        //********** Clock and reset **********
        ///////////////   tracks   =  # of tracks   .///////////////////////

        // "Clock"
        //int track = params[CHANNEL_PARAM].getValue() + 0.5f;
        double readTime = 0.0;

        if (running)
        {
            for (int ii = 0; ii < 100; ii++)
            { // assumes max N events at the same time
                if (event >= midifile[track].size())
                {
                    if (params[LOOP_PARAM].getValue() < 0.5f)
                        running = false;
                    resetPlayer();
                    break;
                }

                readTime = midifile[track][event].seconds;
                if (readTime > time)
                {
                    time += sampleTime;
                    break;
                }
                //int Mchannel = 0;
                //Mchannel = getChannelKnob() - 1; // getChannelKnob is 1 indexed
                //if (Mchannel < 0 || Mchannel == midifile[track][event].getChannelNibble()
                Mchannel[ii] = midifile[track][event].getChannelNibble();
                //{

                processMessage(&midifile[track][event]);
                //}
                event++;
            }
        }

        for (int i = 0; i < 16; i++)

        {

            uint8_t lastNote = notes[i];
            uint8_t lastGate = (gates[i] || pedalgates[i]);
            CV_Out[i] = ((lastNote - 60) / 12.f);
            GATE_Out[i] = ((lastGate && running) ? 10.f : 0.f);
            VELOCITY_Out[i] = (rescale(noteData[lastNote].velocity, 0, 127, 0.f, 10.f));
            //CV_Out[i] = ((lastNote - 60) / 12.f);
            //GATE_Out[i] = ((lastGate && running) ? 10.f : 0.f);
            //VELOCITY_Out[i] = (rescale(noteData[lastNote].velocity, 0, 127, 0.f, 10.f));
            //outputs[AFTERTOUCH_OUTPUTS + i].setVoltage(rescale(noteData[lastNote].aftertouch, 0, 127, 0.f, 10.f));
            switch (Mchannel[i])
            {
            case 1:
            {
                outputs[CV_OUTPUT].setChannels(channels);
                outputs[CV_OUTPUT].writeVoltages(CV_Out);
                outputs[GATE_OUTPUT].setChannels(channels);
                outputs[GATE_OUTPUT].writeVoltages(GATE_Out);
                outputs[VELOCITY_OUTPUT].setChannels(channels);
                outputs[VELOCITY_OUTPUT].writeVoltages(VELOCITY_Out);
            }
            break;
            case 2:
            {
                outputs[CV_OUTPUT + 1].setChannels(channels);
                outputs[CV_OUTPUT + 1].writeVoltages(CV_Out);
                outputs[GATE_OUTPUT + 1].setChannels(channels);
                outputs[GATE_OUTPUT + 1].writeVoltages(GATE_Out);
                outputs[VELOCITY_OUTPUT + 1].setChannels(channels);
                outputs[VELOCITY_OUTPUT + 1].writeVoltages(VELOCITY_Out);
            }
            break;
            case 9:
            {
                outputs[CV_OUTPUT + 8].setChannels(channels);
                outputs[CV_OUTPUT + 8].writeVoltages(CV_Out);
                outputs[GATE_OUTPUT + 8].setChannels(channels);
                outputs[GATE_OUTPUT + 8].writeVoltages(GATE_Out);
                outputs[VELOCITY_OUTPUT + 8].setChannels(channels);
                outputs[VELOCITY_OUTPUT + 8].writeVoltages(VELOCITY_Out);
            }
            break;
            case 10:
            {
                outputs[CV_OUTPUT + 9].setChannels(channels);
                outputs[CV_OUTPUT + 9].writeVoltages(CV_Out);
                outputs[GATE_OUTPUT + 9].setChannels(channels);
                outputs[GATE_OUTPUT + 9].writeVoltages(GATE_Out);
                outputs[VELOCITY_OUTPUT + 9].setChannels(channels);
                outputs[VELOCITY_OUTPUT + 9].writeVoltages(VELOCITY_Out);
            }
            break;
            default:
                break;
            }
        }

        lightRefreshCounter++;
        if (lightRefreshCounter >= displayRefreshStepSkips)
        {
            lightRefreshCounter = 0;

            // fileLoaded light
            lights[LOADMIDI_LIGHT + 0].value = fileLoaded ? 1.0f : 0.0f;
            lights[LOADMIDI_LIGHT + 1].value = !fileLoaded ? 1.0f : 0.0f;

            // Reset light
            lights[RESET_LIGHT].value = resetLight;
            resetLight -= (resetLight / lightLambda) * (float)sampleTime * displayRefreshStepSkips;

            // Run light
            lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;

        } // lightRefreshCounter
    }
};

struct MainDisplayWidget : TransparentWidget
{
    MidiPlayer *module;
    std::shared_ptr<Font> font;
    static const int displaySize = 12;
    char text[displaySize + 1];

    MainDisplayWidget()
    {
        font = APP->window->loadFont(asset::plugin(pluginInstance, "res/Segment14.ttf"));
    }
    std::string removeExtension(std::string filename)
    {
        size_t lastdot = filename.find_last_of(".");
        if (lastdot == std::string::npos)
            return filename;
        return filename.substr(0, lastdot);
    }

    void draw(const DrawArgs &args) override
    {
        NVGcolor backgroundColor = nvgRGB(0x38, 0x38, 0x38);
        NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.0, 0.0, 137, 23.5, 5.0);
        nvgFillColor(args.vg, backgroundColor);
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 1.0);
        nvgStrokeColor(args.vg, borderColor);
        nvgStroke(args.vg);
        nvgFontSize(args.vg, 12);

        //NVGcolor textColor = nvgRGB(0xaf, 0xd2, 0x2c);
        nvgFontFaceId(args.vg, font->handle);
        Vec textPos = Vec(5, 18);
        //nvgFillColor(args.vg, nvgRGBA(0xe1, 0x02, 0x78, 0xc0));
        //std::string empty = std::string(displaySize, '~');
        //nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
        nvgFillColor(args.vg, nvgRGBA(0x28, 0xb0, 0xf3, 0xc0));

        for (int i = 0; i <= displaySize; i++)
        {
            text[i] = ' ';
        }

        snprintf(text, displaySize + 1, "%s", (removeExtension(module->lastFilename)).c_str());
        //snprintf(text, displaySize + 1, "%s", (removeExtension(module->lastFilename)).c_str());

        nvgText(args.vg, textPos.x, textPos.y, text, NULL);
        //  to_string(t)
    }
};

// MidiPlayer : module

//struct MidiPlayerWidget : ModuleWidget {

struct MidiPlayerWidget : ModuleWidget
{

    /*
	struct LoadMidiPushButton : stsBigPushButton 
	{
		MidiPlayer *moduleL = nullptr;
		void onChange(Event &e) override   //void onChange(EventChange &e) override 
		{  
			if (value > 0.0 && moduleL != nullptr) 
			{
				moduleL->loadMidiFile();
			}
			stsBigPushButton::onChange(e);
		}
	}
	*/
    MidiPlayerWidget(MidiPlayer *module)
    {
        setModule(module);

        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/midiplayer.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(15, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
        addChild(createWidget<ScrewSilver>(Vec(15, 365)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

        static const int colRulerM0 = 32;
        static const int colRulerMSpacing = 55;
        static const int colRulerM1 = colRulerM0 + colRulerMSpacing;
        static const int colRulerM2 = colRulerM1 + colRulerMSpacing;
        static const int colRulerM3 = colRulerM2 + colRulerMSpacing;
        static const int colRulerM4 = colRulerM3 + colRulerMSpacing;
        static const int rowRulerT5 = 100; //Knob
        static const int rowRulerM0 = 100;

        if (module != NULL)
        {
            // Main display
            MainDisplayWidget *display = new MainDisplayWidget();
            display->module = module;
            display->box.pos = Vec(12, 46);
            //display->box.size = Vec(137, 23.5f); // x characters
            display->box.size = Vec(261, 23.5f); // x characters
            addChild(display);
        }

        // Channel knob
        addParam(createParam<sts_Davies_snap_35_Grey>(Vec(colRulerM1, rowRulerT5 - 5), module, MidiPlayer::CHANNEL_PARAM)); //, 0.0f, 16.0f, 0.0f));    /

        //Main load button
        //LoadMidiPushButton* midiButton = createDynamicParamCentered<LoadMidiPushButton>(Vec(colRulerM0, rowRulerM0), module, MidiPlayer::LOADMIDI_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme);
        addParam(createParam<stsBigPushButton>(Vec(colRulerM0 - 10, rowRulerM0 - 5), module, MidiPlayer::LOADMIDI_PARAM)); //, 0.0f, 1.0f, 0.0f, &module->panelTheme);
        //midiButton->moduleL = module;
        //addParam(midiButton);
        // Load light
        //addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(colRulerM0 + 20, rowRulerM0), module, MidiPlayer::LOADMIDI_LIGHT + 0));
        addChild(createLight<SmallLight<GreenRedLight>>(Vec(colRulerM0 + 20, rowRulerM0 + 5), module, MidiPlayer::LOADMIDI_LIGHT + 0));

        // Reset LED bezel and light
        addParam(createParam<stsLEDButton>(Vec(colRulerM2 + 5, rowRulerM0 + 2), module, MidiPlayer::RESET_PARAM)); //, 0.0f, 1.0f, 0.0f));
        addChild(createLight<MediumLight<BlueLight>>(Vec(colRulerM2 + 6.5, rowRulerM0 + 3.9f), module, MidiPlayer::RESET_LIGHT));

        // Run LED bezel and light
        addParam(createParam<stsLEDButton>(Vec(colRulerM3, rowRulerM0 + 2), module, MidiPlayer::RUN_PARAM)); //, 0.0f, 1.0f, 0.0f));
        addChild(createLight<MediumLight<GreenLight>>(Vec(colRulerM3 + 1.5, rowRulerM0 + 3.9f), module, MidiPlayer::RUN_LIGHT));

        // Loop
        addParam(createParamCentered<CKSS>(Vec(colRulerM4, rowRulerM0 + 8), module, MidiPlayer::LOOP_PARAM)); //, 0.0f, 1.0f, 1.0f));

        // channel outputs (CV, GATE, VELOCITY)
        static const int colRulerOuts0 = 43;
        static const int colRulerOutsSpacing = 29;
        static const int rowRulerOuts0 = 157;
        static const int rowRulerOutsSpacing = 30;
        //static const int colRulerOuts0 = 38;
        //static const int colRulerOutsSpacing = 30;
        static const int rowRulerOuts1 = 265;
        //static const int 2rowRulerOutsSpacing = 25;
        /*
        addOutput(createOutput<PJ301MPort>(mm2px(Vec(4.61505, 60.1445)), module, MidiPlayer::CV_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(16.214, 60.1445)), module, MidiPlayer::GATE_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(27.8143, 60.1445)), module, MidiPlayer::VELOCITY_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(4.61505, 76.1449)), module, MidiPlayer::AFTERTOUCH_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(16.214, 76.1449)), module, MidiPlayer::PITCH_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(27.8143, 76.1449)), module, MidiPlayer::MOD_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(4.61505, 92.1439)), module, MidiPlayer::CLOCK_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(16.214, 92.1439)), module, MidiPlayer::CLOCK_DIV_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(27.8143, 92.1439)), module, MidiPlayer::RETRIGGER_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(4.61505, 108.144)), module, MidiPlayer::START_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(16.214, 108.144)), module, MidiPlayer::STOP_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(27.8143, 108.144)), module, MidiPlayer::CONTINUE_OUTPUT));
        */
        for (int i = 0; i < 8; i++)
        {
            //addOutput(createDynamicPort<IMPort>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0), module, MidiPlayer::CV_OUTPUTS + i, &module->panelTheme));
            //addOutput(createDynamicPort<IMPort>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0 + rowRulerOutsSpacing), module, MidiPlayer::GATE_OUTPUTS + i, &module->panelTheme));
            //addOutput(createDynamicPort<IMPort>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0 + rowRulerOutsSpacing * 2), module, MidiPlayer::VELOCITY_OUTPUTS + i, &module->panelTheme));
            addOutput(createOutput<sts_Port>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0), module, MidiPlayer::CV_OUTPUT + i));                                 //, &module->panelTheme));
            addOutput(createOutput<sts_Port>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0 + rowRulerOutsSpacing), module, MidiPlayer::GATE_OUTPUT + i));         //, &module->panelTheme));
            addOutput(createOutput<sts_Port>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0 + rowRulerOutsSpacing * 2), module, MidiPlayer::VELOCITY_OUTPUT + i)); //, &module->panelTheme));
        }

        for (int i = 8; i < 16; i++)
        {
            //addOutput(createDynamicPort<IMPort>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0), module, MidiPlayer::CV_OUTPUTS + i, &module->panelTheme));
            //addOutput(createDynamicPort<IMPort>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0 + rowRulerOutsSpacing), module, MidiPlayer::GATE_OUTPUTS + i, &module->panelTheme));
            //addOutput(createDynamicPort<IMPort>(Vec(colRulerOuts0 + colRulerOutsSpacing * i, rowRulerOuts0 + rowRulerOutsSpacing * 2), module, MidiPlayer::VELOCITY_OUTPUTS + i, &module->panelTheme));
            addOutput(createOutput<sts_Port>(Vec(colRulerOuts0 + colRulerOutsSpacing * (i - 8), rowRulerOuts1), module, MidiPlayer::CV_OUTPUT + i));                                 //, &module->panelTheme));
            addOutput(createOutput<sts_Port>(Vec(colRulerOuts0 + colRulerOutsSpacing * (i - 8), rowRulerOuts1 + rowRulerOutsSpacing), module, MidiPlayer::GATE_OUTPUT + i));         //, &module->panelTheme));
            addOutput(createOutput<sts_Port>(Vec(colRulerOuts0 + colRulerOutsSpacing * (i - 8), rowRulerOuts1 + rowRulerOutsSpacing * 2), module, MidiPlayer::VELOCITY_OUTPUT + i)); //, &module->panelTheme));
        }
    }
};

Model *modelMidiPlayer = createModel<MidiPlayer, MidiPlayerWidget>("MidiPlayer");
