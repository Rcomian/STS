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
//Finished and converted to V1 and Polyphonic By Jim Tupper @ RCM and Don Turnock @ STS 
//***********************************************************************************************

//http://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html

#include "sts.hpp"
#include "midifile/MidiFile.h"
#include "osdialog.h"
#include <iostream>
#include <algorithm>
#include "string.hpp"
#include <ui/ScrollBar.hpp>
#include <ui/ScrollWidget.hpp>
#include <set>
#include <limits>

using namespace rack;
using namespace std;
using namespace smf;
using namespace string;


struct MIDI_CV {
	enum OutputIds {
		CV_OUTPUT,
		GATE_OUTPUT,
		VELOCITY_OUTPUT,
        AFTERTOUCH_OUTPUT,
        RETRIGGER_OUTPUT
	};

	int channels;
	enum PolyMode {
		ROTATE_MODE,
		REUSE_MODE,
		RESET_MODE,
		MPE_MODE,
		NUM_POLY_MODES
	};
	PolyMode polyMode;

	uint32_t clock = 0;
	int clockDivision;

	bool pedal;
	// Indexed by channel
	uint8_t notes[16];
	bool gates[16];
	uint8_t velocities[16];
	uint8_t aftertouches[16];
	std::vector<uint8_t> heldNotes;

	int rotateIndex;

	// 16 channels for MPE. When MPE is disabled, only the first channel is used.
	uint16_t pitches[16];
	uint8_t mods[16];
	dsp::ExponentialFilter pitchFilters[16];
	dsp::ExponentialFilter modFilters[16];

	dsp::PulseGenerator clockPulse;
	dsp::PulseGenerator clockDividerPulse;
	dsp::PulseGenerator retriggerPulses[16];
	dsp::PulseGenerator startPulse;
	dsp::PulseGenerator stopPulse;
	dsp::PulseGenerator continuePulse;

	MIDI_CV() {
		heldNotes.reserve(128);
		for (int c = 0; c < 16; c++) {
			pitchFilters[c].lambda = 1 / 0.01f;
			modFilters[c].lambda = 1 / 0.01f;
		}
		onReset();
	}

	void onReset() {
		channels = 1;
		polyMode = ROTATE_MODE;
		clockDivision = 24;
		panic();
	}

	/** Resets performance state */
	void panic() {
		pedal = false;
		for (int c = 0; c < 16; c++) {
			notes[c] = 60;
			gates[c] = false;
			velocities[c] = 0;
			aftertouches[c] = 0;
			pitches[c] = 8192;
			mods[c] = 0;
			pitchFilters[c].reset();
			modFilters[c].reset();
		}
		pedal = false;
		rotateIndex = -1;
		heldNotes.clear();
	}

	void process(const Module::ProcessArgs &args, Output **outputs) {

		outputs[CV_OUTPUT]->setChannels(channels);
		outputs[GATE_OUTPUT]->setChannels(channels);
		outputs[VELOCITY_OUTPUT]->setChannels(channels);
		outputs[AFTERTOUCH_OUTPUT]->setChannels(channels);
		outputs[RETRIGGER_OUTPUT]->setChannels(channels);
		for (int c = 0; c < channels; c++) {
			bool retrigger = outputs[RETRIGGER_OUTPUT]->isConnected() ? false : retriggerPulses[c].process(args.sampleTime);

			outputs[CV_OUTPUT]->setVoltage((notes[c] - 60.f) / 12.f, c);
			outputs[GATE_OUTPUT]->setVoltage(gates[c] && !retrigger ? 10.f : 0.f, c);
			outputs[VELOCITY_OUTPUT]->setVoltage(rescale(velocities[c], 0, 127, 0.f, 10.f), c);
			outputs[AFTERTOUCH_OUTPUT]->setVoltage(rescale(aftertouches[c], 0, 127, 0.f, 10.f), c);
			outputs[RETRIGGER_OUTPUT]->setVoltage(retriggerPulses[c].process(args.sampleTime) ? 10.f : 0.f, c);
		}

	}

	void processMessage(midi::Message msg) {
		// DEBUG("MIDI: %01x %01x %02x %02x", msg.getStatus(), msg.getChannel(), msg.getNote(), msg.getValue());

		switch (msg.getStatus()) {
			// note off
			case 0x8: {
				releaseNote(msg.getNote());
			} break;
			// note on
			case 0x9: {
				if (msg.getValue() > 0) {
					int c = (polyMode == MPE_MODE) ? msg.getChannel() : assignChannel(msg.getNote());
					velocities[c] = msg.getValue();
					pressNote(msg.getNote(), c);
				}
				else {
					// For some reason, some keyboards send a "note on" event with a velocity of 0 to signal that the key has been released.
					releaseNote(msg.getNote());
				}
			} break;
			// key pressure
			case 0xa: {
				// Set the aftertouches with the same note
				// TODO Should we handle the MPE case differently?
				for (int c = 0; c < 16; c++) {
					if (notes[c] == msg.getNote())
						aftertouches[c] = msg.getValue();
				}
			} break;
			// cc
			case 0xb: {
				processCC(msg);
			} break;
			// channel pressure
			case 0xd: {
				if (polyMode == MPE_MODE) {
					// Set the channel aftertouch
					aftertouches[msg.getChannel()] = msg.getNote();
				}
				else {
					// Set all aftertouches
					for (int c = 0; c < 16; c++) {
						aftertouches[c] = msg.getNote();
					}
				}
			} break;
			// pitch wheel
			case 0xe: {
				int c = (polyMode == MPE_MODE) ? msg.getChannel() : 0;
				pitches[c] = ((uint16_t) msg.getValue() << 7) | msg.getNote();
			} break;
			case 0xf: {
				processSystem(msg);
			} break;
			default: break;
		}
	}

	void processCC(midi::Message msg) {
		switch (msg.getNote()) {
			// mod
			case 0x01: {
				int c = (polyMode == MPE_MODE) ? msg.getChannel() : 0;
				mods[c] = msg.getValue();
			} break;
			// sustain
			case 0x40: {
				if (msg.getValue() >= 64)
					pressPedal();
				else
					releasePedal();
			} break;
			default: break;
		}
	}

	void processSystem(midi::Message msg) {
		switch (msg.getChannel()) {
			// Timing
			case 0x8: {
				clockPulse.trigger(1e-3);
				if (clock % clockDivision == 0) {
					clockDividerPulse.trigger(1e-3);
				}
				clock++;
			} break;
			// Start
			case 0xa: {
				startPulse.trigger(1e-3);
				clock = 0;
			} break;
			// Continue
			case 0xb: {
				continuePulse.trigger(1e-3);
			} break;
			// Stop
			case 0xc: {
				stopPulse.trigger(1e-3);
				clock = 0;
			} break;
			default: break;
		}
	}

	int assignChannel(uint8_t note) {
		if (channels == 1)
			return 0;

		switch (polyMode) {
			case REUSE_MODE: {
				// Find channel with the same note
				for (int c = 0; c < channels; c++) {
					if (notes[c] == note)
						return c;
				}
			} // fallthrough

			case ROTATE_MODE: {
				// Find next available channel
				for (int i = 0; i < channels; i++) {
					rotateIndex++;
					if (rotateIndex >= channels)
						rotateIndex = 0;
					if (!gates[rotateIndex])
						return rotateIndex;
				}
				// No notes are available. Advance rotateIndex once more.
				rotateIndex++;
				if (rotateIndex >= channels)
					rotateIndex = 0;
				return rotateIndex;
			} break;

			case RESET_MODE: {
				for (int c = 0; c < channels; c++) {
					if (!gates[c])
						return c;
				}
				return channels - 1;
			} break;

			case MPE_MODE: {
				// This case is handled by querying the MIDI message channel.
				return 0;
			} break;

			default: return 0;
		}
	}

	void pressNote(uint8_t note, int channel) {
		// Remove existing similar note
		auto it = std::find(heldNotes.begin(), heldNotes.end(), note);
		if (it != heldNotes.end())
			heldNotes.erase(it);
		// Push note
		heldNotes.push_back(note);
		// Set note
		notes[channel] = note;
		gates[channel] = true;
		retriggerPulses[channel].trigger(1e-3);
	}

	void releaseNote(uint8_t note) {
		// Remove the note
		auto it = std::find(heldNotes.begin(), heldNotes.end(), note);
		if (it != heldNotes.end())
			heldNotes.erase(it);
		// Hold note if pedal is pressed
		if (pedal)
			return;
		// Turn off gate of all channels with note
		for (int c = 0; c < channels; c++) {
			if (notes[c] == note) {
				gates[c] = false;
			}
		}
		// Set last note if monophonic
		if (channels == 1) {
			if (note == notes[0] && !heldNotes.empty()) {
				uint8_t lastNote = heldNotes.back();
				notes[0] = lastNote;
				gates[0] = true;
				return;
			}
		}
	}

    void releaseHeldNotes() {
        for(auto& heldNote : heldNotes) {
            releaseNote(heldNote);
        }
    }

	void pressPedal() {
		pedal = true;
	}

	void releasePedal() {
		pedal = false;
		// Clear all gates
		for (int c = 0; c < 16; c++) {
			gates[c] = false;
		}
		// Add back only the gates from heldNotes
		for (uint8_t note : heldNotes) {
			// Find note's channels
			for (int c = 0; c < channels; c++) {
				if (notes[c] == note) {
					gates[c] = true;
				}
			}
		}
		// Set last note if monophonic
		if (channels == 1) {
			if (!heldNotes.empty()) {
				uint8_t lastNote = heldNotes.back();
				notes[0] = lastNote;
			}
		}
	}

	void setChannels(int channels) {
		if (channels == this->channels)
			return;
		this->channels = channels;
		panic();
	}

	void setPolyMode(PolyMode polyMode) {
		if (polyMode == this->polyMode)
			return;
		this->polyMode = polyMode;
		panic();
	}

	json_t *dataToJson() {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "channels", json_integer(channels));
		json_object_set_new(rootJ, "polyMode", json_integer(polyMode));
		json_object_set_new(rootJ, "clockDivision", json_integer(clockDivision));
		// Saving/restoring pitch and mod doesn't make much sense for MPE.
		if (polyMode != MPE_MODE) {
			json_object_set_new(rootJ, "lastPitch", json_integer(pitches[0]));
			json_object_set_new(rootJ, "lastMod", json_integer(mods[0]));
		}
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) {
		json_t *channelsJ = json_object_get(rootJ, "channels");
		if (channelsJ)
			setChannels(json_integer_value(channelsJ));

		json_t *polyModeJ = json_object_get(rootJ, "polyMode");
		if (polyModeJ)
			polyMode = (PolyMode) json_integer_value(polyModeJ);

		json_t *clockDivisionJ = json_object_get(rootJ, "clockDivision");
		if (clockDivisionJ)
			clockDivision = json_integer_value(clockDivisionJ);

		json_t *lastPitchJ = json_object_get(rootJ, "lastPitch");
		if (lastPitchJ)
			pitches[0] = json_integer_value(lastPitchJ);

		json_t *lastModJ = json_object_get(rootJ, "lastMod");
		if (lastModJ)
			mods[0] = json_integer_value(lastModJ);

	}
};

struct PlaybackTrack
{
	int track;
	int channel;
	int poly;
	std::string name;
	MIDI_CV midiCV;
};

struct TempoMap {
    struct TempoEntry {
        double startTime;
        double quarterNoteDurationInSeconds;
        double beatOffset; // Logical time when the first quarter note for the section started
    };

    std::vector<TempoEntry> tempoEntries;
    int currentTempoEntry = -1;
    double timeMarker = 0.0;
    double lastTick = 0.0;
    double clockMultiplier = 4.0;
    double clockPeriod = 0.5;
    double sectionStartedAt = 0.f;
    double nextSectionAt = numeric_limits<double>::max();

    void reset() {
        tempoEntries.clear();
        timeMarker = 0.0;
        currentTempoEntry = -1;
    }

    void addTempo(double startTime, double quarterNoteDurationInSeconds) {
        double beatOffset = 0.0;

        if (!tempoEntries.empty()) {
            TempoEntry& lastEntry = tempoEntries[tempoEntries.size()-1];
            double lastEntryDuration = (startTime - (lastEntry.startTime-lastEntry.beatOffset));
            double quarterNotes = lastEntryDuration / lastEntry.quarterNoteDurationInSeconds;
            double unusedQuarterNoteFraction = quarterNotes - floor(quarterNotes);
            beatOffset = unusedQuarterNoteFraction * quarterNoteDurationInSeconds;
        }
        DEBUG("ADD TEMP MAP %f, %f, %f", startTime, quarterNoteDurationInSeconds, beatOffset);
        tempoEntries.push_back({startTime, quarterNoteDurationInSeconds, beatOffset});
    }

    void setMarkerTo(double timePoint) {
        timeMarker = timePoint;
        currentTempoEntry = -1;

        for (size_t i = 0; i < tempoEntries.size(); i++) {
            if (tempoEntries[i].startTime < timePoint) {
                currentTempoEntry = i;
            } else {
                break;
            }
        }

        sectionStartedAt = currentTempoEntry >= 0 ? (tempoEntries[currentTempoEntry].startTime - tempoEntries[currentTempoEntry].beatOffset) : 0.f;
        clockPeriod = (currentTempoEntry >= 0 ? tempoEntries[currentTempoEntry].quarterNoteDurationInSeconds : 0.5f) / clockMultiplier;

        double timeIntoCurrentSection = timeMarker - sectionStartedAt;
        int numberOfClocksIntoSection = (int)floor(timeIntoCurrentSection / clockPeriod);

        lastTick = sectionStartedAt + (numberOfClocksIntoSection * clockPeriod);
        nextSectionAt = currentTempoEntry < ((int)tempoEntries.size()-1) ? tempoEntries[currentTempoEntry + 1].startTime : numeric_limits<double>::max();
    }

    bool process(double stepTime) {
        timeMarker += stepTime;

        if (timeMarker >= nextSectionAt) {
            currentTempoEntry += 1;

            sectionStartedAt = currentTempoEntry >= 0 ? (tempoEntries[currentTempoEntry].startTime - tempoEntries[currentTempoEntry].beatOffset) : 0.f;
            clockPeriod = (currentTempoEntry >= 0 ? tempoEntries[currentTempoEntry].quarterNoteDurationInSeconds : 0.5f) / clockMultiplier;
            lastTick = (currentTempoEntry >= 0 ? tempoEntries[currentTempoEntry].startTime - tempoEntries[currentTempoEntry].beatOffset : 0.f);
            nextSectionAt = currentTempoEntry < ((int)tempoEntries.size()-1) ? tempoEntries[currentTempoEntry + 1].startTime : numeric_limits<double>::max();

            return (timeMarker - lastTick) <= stepTime;
        } else if ( (timeMarker - lastTick) > clockPeriod ) {
            lastTick += clockPeriod;
            return true;
        }

        return false;
    }
};

struct MidiPlayer : Module
{
    enum ParamIds
    {
        LOADMIDI_PARAM,
        RESET_PARAM,
        RUN_PARAM,
        LOOP_ON_OFF,
        SCRUB_PARAM,
        LOOP_START,
        LOOP_END,
        
        ENUMS(MAX_CHANNELS, 16),
        ENUMS(MIDI_MODE, 16),
        
        NUM_PARAMS
    };
    enum InputIds
    {
        CLK_INPUT,
        CV_RESET_INPUT,
        CV_LOOP_ON_OFF,
        CV_RUN_INPUT,
        BACK_TO_START_INPUT,
        NUM_INPUTS
    };
    enum OutputIds
    {
        //PITCH_OUTPUT,
        //MOD_OUTPUT,
        CLOCK_OUTPUT,
        //CLOCK_DIV_OUTPUT,
        //START_OUTPUT,
        //STOP_OUTPUT,
        //CONTINUE_OUTPUT,
        ENUMS(CV_OUTPUT, 16),
        ENUMS(GATE_OUTPUT, 16),
        ENUMS(VELOCITY_OUTPUT, 16),
        ENUMS(AFTERTOUCH_OUTPUT, 16),
        ENUMS(RETRIGGER_OUTPUT, 16),
        END_OF_SECTION_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds
    {
        RESET_LIGHT,
        RUN_LIGHT,
        LOOP_START_LIGHT,
        LOOP_END_LIGHT,
        LOOP_ON_OFF_LIGHT,
        EXTRA_TRACKS_LIGHT,
        ENUMS(LOADMIDI_LIGHT, 2),
        ENUMS(PLAYABLE_TRACKS_LIGHT, 16 * 2),
        NUM_LIGHTS
    };

    struct NoteData
    {
        uint8_t velocity = 0;
        uint8_t aftertouch = 0;
    };

    // Midi track playback information
    std::vector<PlaybackTrack> playbackTracks;
    TempoMap tempoMap;

    // Need to save
    int panelTheme = 0;
    std::string lastPath = "";
    std::string lastFilename = "--";
    int trackOffset = 0;
    int nextTrackOffset = -1; // track offset requested from the UI

    // No need to save
    bool running = false;
    double duration = 0.f;
    long event = 0;
    float applyParamsTimer = 0.f;

    double loopStartPos = -1.f;
    double loopEndPos = -1.f;
    bool looping = false;

    //

    int tracks;
    unsigned int lightRefreshCounter = 0;
    float resetLight = 0.0f;
    bool doMidiFileOpen = false;
    bool doMidiFileLoad = false;
    bool fileLoaded = false;

    //std::string trackName[16] = {};
    std::string path;
    MidiFile midifile;
    dsp::SchmittTrigger runningTrigger;
    dsp::SchmittTrigger resetTrigger;
    dsp::SchmittTrigger btnLoadMidi;
    dsp::SchmittTrigger loopStart;
    dsp::SchmittTrigger loopEnd;
    dsp::SchmittTrigger backToStartTrigger;

    dsp::PulseGenerator endOfSectionPulse;
    dsp::PulseGenerator clockPulse;


    MidiPlayer()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS); //, cachedNotes(128)); //, cachedNotes(128);

        configParam(LOOP_ON_OFF, 0.0f, 1.0f, 0.0f, "Loop Active = ");

        configParam(SCRUB_PARAM, 0.0f, 100.0f, 0.0f, "Playback Position = ");
        configParam(LOOP_START, 0.0f, 1.0f, 0.0f, "Loop Start = ");
        configParam(LOOP_END, 0.0f, 1.0f, 0.0f, "Loop End = ");
                
        // config MAX midi channels
        for (int i = 0; i < 16; ++i)
        {
        configParam(MAX_CHANNELS + i, 1.0f, 16.0f, 1.0f, "Max Midi Channels = ");
        }
        
        // Config MIDI Mode
        static std::string midiMode[4] = {"ROTATE_MODE","REUSE_MODE","RESET_MODE","MPE_MODE",};
        
        struct midiModeQuantity : engine::ParamQuantity
        {
            std::string getDisplayValueString() override
            {
                int v = getValue();

                std::string midiVal = midiMode[v];
                
                return midiVal;   
            }
        };
        
        for (int i = 0; i < 16; ++i)
        {
        configParam<midiModeQuantity>(MIDI_MODE +i, 0.0f, 2.0f, 0.0f, "Midi Mode ");
        //configParam(MIDI_MODE + i, 0.0f, 3.0f, 0.0f, "Midi Mode ");
        }
        
        onReset();
    }

    void setTrackOffset(int newTrackOffset) {
        nextTrackOffset = newTrackOffset;
    }

    void setTrackOffsetInternal(int newTrackOffset) {
		trackOffset = newTrackOffset;
        panic();

        for (int i = trackOffset; i < (int)playbackTracks.size() && i < trackOffset + 16; i++) {
            paramQuantities[MAX_CHANNELS + i - trackOffset]->defaultValue = playbackTracks[i].poly;
            params[MAX_CHANNELS + i - trackOffset].setValue(playbackTracks[i].poly);
        }

    }

    void onReset() override
    {
        running = false;
        resetPlayer();
    }

    void onRandomize() override
    {
    }

    void releaseHeldNotes() {
        for (auto& playbackTrack : playbackTracks) {
            playbackTrack.midiCV.releaseHeldNotes();
        }
    }

    void resetPlayer()
    {
        event = 0;
        params[SCRUB_PARAM].setValue(0.0);
        tempoMap.setMarkerTo(0.0);

        for (auto& playbackTrack : playbackTracks) {
            playbackTrack.midiCV.onReset();
        }
    }

    void panic() {
        for (auto& playbackTrack : playbackTracks) {
            playbackTrack.midiCV.panic();
        }
        for (int i = 0; i < 16; i++) {
            for (int ii = 0; ii < outputs[GATE_OUTPUT + i].getChannels(); ii++) {
                outputs[GATE_OUTPUT + i].setVoltage(0, ii);
            }
        }
    }

    json_t *dataToJson() override
    {
        json_t *rootJ = json_object();

        // panelTheme
        json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

        // lastPath
        json_object_set_new(rootJ, "lastPath", json_string(lastPath.c_str()));

        // loopStartPos
        json_object_set_new(rootJ, "loopStartPos", json_real(loopStartPos));

        // loopEndPos
        json_object_set_new(rootJ, "loopEndPos", json_real(loopEndPos));

        // track Offset
        json_object_set_new(rootJ, "trackOffset", json_integer(trackOffset));

        // Clock multiplier
        json_object_set_new(rootJ, "clockMultiplier", json_real(tempoMap.clockMultiplier));

        // Looping
        json_object_set_new(rootJ, "looping", json_boolean(looping));
    
        // Running
        json_object_set_new(rootJ, "running", json_boolean(running));

        // time
        json_object_set_new(rootJ, "time", json_real(tempoMap.timeMarker));

        // event
        json_object_set_new(rootJ, "event", json_integer(event));

        return rootJ;
    }

    void dataFromJson(json_t *rootJ) override
    {
        // panelTheme
        json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
        if (panelThemeJ)
            panelTheme = json_integer_value(panelThemeJ);

        // lastPath
        json_t *lastPathJ = json_object_get(rootJ, "lastPath");
        if (lastPathJ)
            lastPath = json_string_value(lastPathJ);

        // loopStartPos
        json_t *loopStartPosJ = json_object_get(rootJ, "loopStartPos");
        if (loopStartPosJ)
            loopStartPos = json_number_value(loopStartPosJ);

        // loopEndPos
        json_t *loopEndPosJ = json_object_get(rootJ, "loopEndPos");
        if (loopEndPosJ)
            loopEndPos = json_number_value(loopEndPosJ);

        // trackOffset
        json_t *trackOffsetJ = json_object_get(rootJ, "trackOffset");
        if (trackOffsetJ)
            trackOffset = json_integer_value(trackOffsetJ);

        if (lastPath != "") {
            // Save data to re-apply after loading the file (it's normally cleared when loading a new file)
            float polyParam[16] {};
            for (int i = 0; i < 16; i++) {
                polyParam[i] = params[MAX_CHANNELS + i].getValue();
            }
            double savedStartPos = loopStartPos;
            double savedEndPos = loopEndPos;


            loadMidiFile();

            if (fileLoaded) {
                lastFilename = filename(lastPath);

                // Reapply saved data
                for (int i = 0; i < 16; i++) {
                    params[MAX_CHANNELS + i].setValue(polyParam[i]);
                }

                loopStartPos = savedStartPos;
                loopEndPos = savedEndPos;

                if (loopStartPos >= 0) {
                    params[SCRUB_PARAM].setValue(loopStartPos);
                }
            }
        }

        // Clock multiplier
        json_t *clockMultiplierJ = json_object_get(rootJ, "clockMultiplier");
        if (clockMultiplierJ)
            tempoMap.clockMultiplier = json_number_value(clockMultiplierJ);

        // Looping
        json_t *loopingJ = json_object_get(rootJ, "looping");
        if (loopingJ)
            looping = json_boolean_value(loopingJ);
    
        // Running
        json_t *runningJ = json_object_get(rootJ, "running");
        if (runningJ)
            running = json_boolean_value(runningJ);

        // time
        json_t *timeJ = json_object_get(rootJ, "time");
        if (timeJ) {
            params[SCRUB_PARAM].setValue(json_number_value(timeJ));
        }

        // event
        json_t *eventJ = json_object_get(rootJ, "event");
        if (eventJ)
            event = json_integer_value(eventJ);

    }

    void loadMidiFile()
    {
        releaseHeldNotes();

        if (midifile.read(lastPath))
        {
            fileLoaded = true;

            midifile.doTimeAnalysis();
            midifile.linkNotePairs();
            tracks = midifile.getTrackCount();
            duration = midifile.getFileDurationInSeconds();

            cout << "TPQ: " << midifile.getTicksPerQuarterNote() << endl;
            //if (tracks > 1)
            cout << "TRACKS: " << tracks << endl;
            playbackTracks.clear();

            for (int ii = 0; ii < tracks; ii++)
            {
                std::set<int> channels;
                //std::string trackName[16];
                std::string name;

                
                for (int i = 0; i < midifile[ii].getEventCount(); i++)
                {
                    if (i == midifile[ii].getEventCount() - 1) {
                        double timepos = midifile.getTimeInSeconds(ii, i);
                        if (timepos > duration) {
                            duration = timepos;
                        }
                    }
                    if (midifile[ii][i].isTrackName())
                    {
                        name = midifile[ii][i].getMetaContent();
                        // cout << "Track # " << ii << " " << name << endl;
                        //cout << content << endl;
                        //trackName[ii] = name;
                        

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

                    maxpoly = clamp(maxpoly, 1, 16);

                    PlaybackTrack playbackTrack;
                    playbackTrack.name = name;
                    playbackTrack.channel = channel;
                    playbackTrack.poly = maxpoly;
                    playbackTrack.track = ii;
                    playbackTrack.midiCV.onReset();
                    playbackTrack.midiCV.setChannels(maxpoly);
                    playbackTrack.midiCV.setPolyMode(MIDI_CV::RESET_MODE);
                    playbackTracks.push_back(playbackTrack);
                }

            }

            cout << "Playback tracks: " << playbackTracks.size() << endl;
            for (int i = 0; i < (int)playbackTracks.size(); i++) {
                auto& playbackTrack = playbackTracks[i];
                cout << "Playback track: " << playbackTrack.track << "/" << playbackTrack.channel << "@" << playbackTrack.poly << " " << playbackTrack.name << endl;
            }

            paramQuantities[SCRUB_PARAM]->minValue = 0.f;
            paramQuantities[SCRUB_PARAM]->maxValue = duration;
            params[SCRUB_PARAM].setValue(tempoMap.timeMarker);

            midifile.joinTracks();

            tempoMap.reset();
            for (int i = 0; i < midifile[0].getEventCount(); i++) {
                // Handle tempo set / change
                if (midifile[0][i].isTempo()) {
                    tempoMap.addTempo(midifile.getTimeInSeconds(0, i), midifile[0][i].getTempoSeconds());
                }
            }

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

        setTrackOffsetInternal(0);
        running = false;
        event = 0;
        loopStartPos = -1;
        loopEndPos = -1;
        tempoMap.setMarkerTo(0.0);
        params[SCRUB_PARAM].setValue(0.0);
    }

    // Advances the module by 1 audio frame with duration 1.0 / args.sampleRate
    void process(const ProcessArgs &args) override
    {
        const int track = 0; // midifile was flattened when loaded
        double sampleTime = args.sampleTime;
        if (btnLoadMidi.process(params[LOADMIDI_PARAM].value))
        {
            doMidiFileOpen = true;
        }

        if (doMidiFileLoad) {
            doMidiFileLoad = false;
            loadMidiFile();
        }

        if (nextTrackOffset >= 0) {
            setTrackOffsetInternal(nextTrackOffset);
            nextTrackOffset = -1;
        }

        //********** Buttons, knobs, switches and inputs **********

        // Reset play position to start pos, do before scrubbing and scrubbing will handle the actual movement
        if (backToStartTrigger.process(inputs[BACK_TO_START_INPUT].getVoltage())) {
            params[SCRUB_PARAM].setValue(loopStartPos >= 0 ? loopStartPos : 0);
        }

        // Reset player
        if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[CV_RESET_INPUT].getVoltage()))
        {
            resetLight = 1.0f;
            resetPlayer();
        }

        // Scrub
        if (fileLoaded && (float)tempoMap.timeMarker != params[SCRUB_PARAM].getValue())
        {
            double time = tempoMap.timeMarker;
            while (time > params[SCRUB_PARAM].getValue() && event > 0) {
                event--;
                time = midifile[track][event].seconds;
            }
            while (time < params[SCRUB_PARAM].getValue() && event < (int)midifile[track].size() -1) {
                event++;
                time = midifile[track][event].seconds;
            }
            tempoMap.setMarkerTo(time);
            releaseHeldNotes();
        }


        // Run state button
        if (runningTrigger.process(params[RUN_PARAM].getValue() + inputs[CV_RUN_INPUT].getVoltage()))
        {
            running = !running;

            if (!running) {
                releaseHeldNotes();
            }
        }

        // Set loop start
        if (loopStart.process(params[LOOP_START].getValue())) {
            if (loopStartPos >= 0) {
                loopStartPos = -1;
            } else {
                loopStartPos = tempoMap.timeMarker;
            }
        }

        // Set loop end
        if (loopEnd.process(params[LOOP_END].getValue())) {
            if (loopEndPos >= 0) {
                loopEndPos = -1;
            } else {
                loopEndPos = tempoMap.timeMarker;
            }
        }

        // Set looping
        if (loopEndPos >= 0 && tempoMap.timeMarker > loopEndPos) {
            endOfSectionPulse.trigger();
        }

        if (params[LOOP_ON_OFF].getValue() > 0.f) {
            if (loopStartPos > loopEndPos && loopStartPos >= 0 && loopEndPos >= 0) {
                swap(loopStartPos, loopEndPos);
            }

            if (running && loopEndPos >= 0 && tempoMap.timeMarker >= loopEndPos) {
                double time = tempoMap.timeMarker;
                while (time > loopStartPos && event > 0) {
                    event--;
                    time = midifile[track][event].seconds;
                }
                tempoMap.setMarkerTo(time);
                releaseHeldNotes();
            }
        }

        applyParamsTimer -= args.sampleTime;
        if (applyParamsTimer <= 0.f) {
            applyParamsTimer = 0.2f;
            for (int i = trackOffset; i < (int)playbackTracks.size() && i < trackOffset + 16; i++) {
                playbackTracks[i].midiCV.setChannels(clamp((int)params[MAX_CHANNELS + i - trackOffset].getValue(), 1, 16));
                playbackTracks[i].midiCV.setPolyMode((MIDI_CV::PolyMode)(clamp((int)round(params[MIDI_MODE + i - trackOffset].getValue()), 0, 2)));
            }
        }

        if (running && fileLoaded) {
            if (tempoMap.process(args.sampleTime)) {
                clockPulse.trigger();
            }

            while (running && event < midifile[track].size() && tempoMap.timeMarker > midifile[track][event].seconds) {

                auto playbackTrack = std::find_if(playbackTracks.begin(), playbackTracks.end(), [this, track](const PlaybackTrack& playbackTrack) -> bool {
                        return playbackTrack.track == midifile[track][event].track && playbackTrack.channel == midifile[track][event].getChannelNibble();
                });

                if (playbackTrack != playbackTracks.end()) {
                    midi::Message msg;
                    msg.bytes[0] = midifile[track][event][0];
                    msg.bytes[1] = midifile[track][event][1];
                    msg.bytes[2] = midifile[track][event][2];
                    //DEBUG("MIDI: %01x %01x %02x %02x", msg.getStatus(), msg.getChannel(), msg.getNote(), msg.getValue());
                    playbackTrack->midiCV.processMessage(msg);
                }

                event++;

                if (event >= midifile[track].size())
                {
                    double time = tempoMap.timeMarker;
                    if (params[LOOP_ON_OFF].getValue() < 0.5f) {
                        running = false;
                        event = 0;
                        time = 0;
                    } else if (loopStartPos >= 0) {
                        while (time > loopStartPos && event > 0) {
                            event--;
                            time = midifile[track][event].seconds;
                        }
                    } else {
                        event = 0;
                        time = 0;
                    }
                    
                    endOfSectionPulse.trigger();
                    releaseHeldNotes();
                    tempoMap.setMarkerTo(time);
                    clockPulse.reset();
                    break;
                }
            }
        }

        params[SCRUB_PARAM].setValue(tempoMap.timeMarker);

        for (int i = trackOffset; i < (int)playbackTracks.size() && i < trackOffset + 16; i++) 
        {
            Output* outs[5] {};
            outs[0] = &outputs[CV_OUTPUT + i - trackOffset];
            outs[1] = &outputs[GATE_OUTPUT + i - trackOffset];
            outs[2] = &outputs[VELOCITY_OUTPUT + i - trackOffset];
            outs[3] = &outputs[AFTERTOUCH_OUTPUT + i - trackOffset];
            outs[4] = &outputs[RETRIGGER_OUTPUT + i - trackOffset];
            playbackTracks[i].midiCV.process(args, outs);
        }

        if (!running) {
            for (int i = 0; i < 16; i++) {
                for (int ii = 0; ii < 16; ii++) {
                    outputs[GATE_OUTPUT + i].setVoltage(0, ii);
                }
            }
        }

        outputs[END_OF_SECTION_OUTPUT].setVoltage(endOfSectionPulse.process(args.sampleTime) ? 10.f : 0.f);
        outputs[CLOCK_OUTPUT].setVoltage(clockPulse.process(args.sampleTime) ? 10.f : 0.f);

        lightRefreshCounter++;
        if (lightRefreshCounter >= displayRefreshStepSkips)
        {
            lightRefreshCounter = 0;

            // fileLoaded light
            lights[LOADMIDI_LIGHT + 0].value = fileLoaded ? 1.0f : 0.0f;
            lights[LOADMIDI_LIGHT + 1].value = !fileLoaded ? 1.0f : 0.0f;

            for (int i = 0; i < 16; i++)
            {
                lights[PLAYABLE_TRACKS_LIGHT + 0 + (i *2)].value = trackOffset + i < (int)playbackTracks.size() ? 1.0f : 0.0f;
                lights[PLAYABLE_TRACKS_LIGHT + 1 + (i *2)].value = trackOffset + i < (int)playbackTracks.size() ? 0.0f : 1.0f;
            }

            // Reset light
            lights[RESET_LIGHT].value = resetLight;
            resetLight -= (resetLight / lightLambda) * (float)sampleTime * displayRefreshStepSkips;

            // Run light
            lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;

            // Loop lights
            lights[LOOP_START_LIGHT].value = loopStartPos < 0 ? 0.f : 1.f;
            lights[LOOP_END_LIGHT].value = loopEndPos < 0 ? 0.f : 1.f;
            lights[LOOP_ON_OFF_LIGHT].value = params[LOOP_ON_OFF].getValue() > 0 ? 1.f : 0.f;

            // Extra tacks light
            lights[EXTRA_TRACKS_LIGHT].value = playbackTracks.size() > 16 ? 10.f : 0.f;

        } // lightRefreshCounter
    }
};


struct MainDisplayWidget : TransparentWidget
{
    struct Drawtname 
    { 
        Drawtname()
        {}
        void step(const Widget::DrawArgs &args, Vec pos,std::string tName)
        {
            nvgFontSize(args.vg, 10);
            // nvgFontFaceId(args.vg, font->handle);
            //nvgTextLetterSpacing(args.vg, -2);

            nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
            char text[128];
            snprintf(text, sizeof(text), "%s", tName.c_str());
            nvgText(args.vg, pos.x, pos.y, text, NULL);
        }
    };

    MidiPlayer *module;
    std::shared_ptr<Font> font;
    static const int displaySize = 10;
    char text[displaySize + 1];
    Drawtname T_Name[16];

    MainDisplayWidget()
    {
        font = APP->window->loadFont(asset::plugin(pluginInstance, "res/DejaVuSansMono.ttf"));
    }
    
    void drawTitle(const DrawArgs &args, Vec pos)  //, std::string title) 
    { 
        nvgFontSize(args.vg, 12);
        nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s", (removeExtension(module->lastFilename)).c_str());
        nvgText(args.vg, pos.x, pos.y, text, NULL);
	}

    std::string formatTime(float time) {
        int minutes = floor(time / 60);
        float seconds = time - (minutes * 60);
        return rack::string::f("%02d:%05.2f", minutes, seconds);
    }

    void drawTimer(const DrawArgs &args, Vec pos)  //, std::string title) 
    { 
        nvgFontSize(args.vg, 12);
        nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0);

		nvgFillColor(args.vg, nvgRGBA(0x00, 0xff, 0x00, 0xff));
		char text[128];
		snprintf(text, sizeof(text), "%s / %s", formatTime(module->tempoMap.timeMarker).c_str(), formatTime(module->duration).c_str());
        nvgText(args.vg, pos.x, pos.y, text, NULL);
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
        if (!module)
			return;
		
		drawTitle(args, Vec(19,45));    //, module->lastFilename);

        drawTimer(args, Vec(19,65));

        //cout << "Num Track " << module->playbackTracks.size() << endl;   
        for (int i = 0 + module->trackOffset; i < module->trackOffset + 8 && i < (int)module->playbackTracks.size(); i++)
        {
            T_Name[i - module->trackOffset].step(args, Vec(19,128  + ( (i - module->trackOffset) * 30)),module->playbackTracks[i].name);
        }

        for (int i = 8 + module->trackOffset; i < 16 + module->trackOffset && i < (int)module->playbackTracks.size(); i++)
        {
            T_Name[i - module->trackOffset].step(args, Vec(269,128  + (( (i - module->trackOffset) - 8) * 30)),module->playbackTracks[i].name);
        }

        
    }
        
};

struct ClockMultiplierValueItem : MenuItem {
	MidiPlayer* module;
	double clockMultiplier;
	void onAction(const event::Action& e) override {
		module->tempoMap.clockMultiplier = clockMultiplier;
        module->tempoMap.setMarkerTo(module->tempoMap.timeMarker);
	}
};


struct ClockMultiplierItem : MenuItem {
	MidiPlayer* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		std::vector<int> multiples = {1, 2, 4, 8, 16, 24, 32, 64};
		std::vector<std::string> multipleNames = {"1x", "2x", "4x", "8x", "16x", "24x", "32x", "64x"};
		for (size_t i = 0; i < multiples.size(); i++) {
			ClockMultiplierValueItem* item = new ClockMultiplierValueItem;
			item->text = multipleNames[i];
			item->rightText = CHECKMARK(module->tempoMap.clockMultiplier == multiples[i]);
			item->module = module;
			item->clockMultiplier = multiples[i];
			menu->addChild(item);
		}
		return menu;
	}
};

struct TrackOffsetValueItem : MenuItem {
	MidiPlayer* module;
	int trackOffset;
	void onAction(const event::Action& e) override {
        module->setTrackOffset(trackOffset);
	}
};


struct TrackOffsetItem : MenuItem {
	MidiPlayer* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		for (size_t i = 0; i < module->playbackTracks.size(); i += 16) {
			TrackOffsetValueItem* item = new TrackOffsetValueItem;
            int firstTrack = i+1;
            int lastTrack = min(module->playbackTracks.size(), i+16);
			item->text = firstTrack != lastTrack ? rack::string::f("%d-%d", firstTrack, lastTrack) : rack::string::f("%d", firstTrack);
			item->rightText = CHECKMARK(module->trackOffset == (int)i);
			item->module = module;
			item->trackOffset = i;
			menu->addChild(item);
		}
		return menu;
	}
};


struct MIDI_CVPanicItem : MenuItem {
	MidiPlayer* module;
	void onAction(const event::Action& e) override {
		module->panic();
	}
};


struct MidiPlayerWidget : ModuleWidget
{
    MidiPlayerWidget(MidiPlayer *module)
    {
        setModule(module);

        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/midiplayer.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(15, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
        addChild(createWidget<ScrewSilver>(Vec(15, 365)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

        //if (module != NULL)
        {
            // Main display
            MainDisplayWidget *display = new MainDisplayWidget;
            display->module = module;
            //display->box.pos = Vec(12, 46);
            //display->box.size = Vec(261, 23.f); // x characters
            display->box.pos = Vec(0, 0);
            display->box.size = Vec(box.size.x, 600); // x characters
            addChild(display);

        }

        static const int colRulerM0 = 331;
        static const int colRulerMSpacing = 40;
        static const int colRulerM1 = colRulerM0 + colRulerMSpacing;
        static const int colRulerM2 = colRulerM1 + colRulerMSpacing;
        static const int colRulerM3 = colRulerM2 + colRulerMSpacing;
        static const int colRulerM4 = colRulerM3 + colRulerMSpacing;
        static const int rowRulerM0 = 50;
        static const int rowRulerM1 = 82;

        
        
        // Main File load button
        addParam(createParamCentered<stsPushButton>(Vec(colRulerM0, rowRulerM0), module, MidiPlayer::LOADMIDI_PARAM)); 
        addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(colRulerM0, rowRulerM0), module, MidiPlayer::LOADMIDI_LIGHT + 0));

        // Reset LED bezel and light
        addParam(createParamCentered<stsLEDButton>(Vec(colRulerM1, rowRulerM0), module, MidiPlayer::RESET_PARAM)); 
        addChild(createLightCentered<MediumLight<BlueLight>>(Vec(colRulerM1, rowRulerM0), module, MidiPlayer::RESET_LIGHT));
        addInput(createInputCentered<sts_Port>(Vec(colRulerM1, rowRulerM1), module, MidiPlayer::CV_RESET_INPUT));

        // Run LED bezel and light
        addParam(createParamCentered<stsLEDButton>(Vec(colRulerM2, rowRulerM0), module, MidiPlayer::RUN_PARAM)); 
        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(colRulerM2, rowRulerM0), module, MidiPlayer::RUN_LIGHT));
        addInput(createInputCentered<sts_Port>(Vec(colRulerM2, rowRulerM1), module, MidiPlayer::CV_RUN_INPUT));
        
        // Loop On / Off
        addParam(createParamCentered<stsLEDButton1>(Vec(colRulerM3, rowRulerM0), module, MidiPlayer::LOOP_ON_OFF));
        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(colRulerM3, rowRulerM0), module, MidiPlayer::LOOP_ON_OFF_LIGHT));
        addInput(createInputCentered<sts_Port>(Vec(colRulerM3, rowRulerM1), module, MidiPlayer::CV_LOOP_ON_OFF));

        // Return to Start Input
        addInput(createInputCentered<sts_Port>(Vec(colRulerM4, rowRulerM0), module, MidiPlayer::BACK_TO_START_INPUT));

        // EOC output
        addOutput(createOutputCentered<sts_Port>(Vec(colRulerM4, rowRulerM1), module, MidiPlayer::END_OF_SECTION_OUTPUT));

        // Clock Output
        addOutput(createOutputCentered<sts_Port>(Vec(colRulerM0, rowRulerM1), module, MidiPlayer::CLOCK_OUTPUT));

        // Scrub Slider
        // sts_SlidePotGrayHoriz
        auto scrubParam = createParam<sts_SlidePotGrayHoriz>(Vec(19, 77), module, MidiPlayer::SCRUB_PARAM);
        scrubParam->smooth = false; 
        addParam(scrubParam);

        // set End of loop 
        auto loopEndParam = createParamCentered<stsLEDButton1>(Vec(200, rowRulerM1), module, MidiPlayer::LOOP_END);
        loopEndParam->momentary = true;
        addParam(loopEndParam);
        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(200, rowRulerM1), module, MidiPlayer::LOOP_END_LIGHT));

        // set Start of loop 
        auto loopStartParam = createParamCentered<stsLEDButton1>(Vec(170, rowRulerM1), module, MidiPlayer::LOOP_START);
        loopStartParam->momentary = true;
        addParam(loopStartParam);
        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(170, rowRulerM1), module, MidiPlayer::LOOP_START_LIGHT));

        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(colRulerM0-colRulerMSpacing, rowRulerM0), module, MidiPlayer::EXTRA_TRACKS_LIGHT));

    

        // channel outputs ( CV, GATE, VELOCITY, AFTERTOUCH, RE-TRIGGER)  Max Poly Knob,Mode Knob
        static const int colRulerOuts0 = 98;
        static const int colRulerOutsSpacing = 22;
        static const int rowRulerOuts0 = 125;    //115
        static const int rowRulerOutsSpacing = 30;
        //static const int colRulerOuts0 = 38;
        //static const int colRulerOutsSpacing = 30;
        static const int colRulerOuts1 = 336;
        //static const int 2rowRulerOutsSpacing = 25;
        
        for (int i = 0; i < 8; i++)
        {
            addParam(createParamCentered<sts_Davies15_snap_Grey>(Vec(colRulerOuts0, rowRulerOuts0 + rowRulerOutsSpacing * i), module, MidiPlayer::MAX_CHANNELS + i));
            addParam(createParamCentered<sts_Davies15_snap_Grey>(Vec(colRulerOuts0 + colRulerOutsSpacing * 1, rowRulerOuts0 + rowRulerOutsSpacing * i), module, MidiPlayer::MIDI_MODE + i));
            addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(colRulerOuts0 + colRulerOutsSpacing * 2, rowRulerOuts0 + rowRulerOutsSpacing * i), module, MidiPlayer::PLAYABLE_TRACKS_LIGHT + (i * 2)));
            addOutput(createOutputCentered<sts_Port>(Vec(colRulerOuts0 + colRulerOutsSpacing * 3, rowRulerOuts0 + rowRulerOutsSpacing * i), module, MidiPlayer::CV_OUTPUT + i));                                 
            addOutput(createOutputCentered<sts_Port>(Vec(colRulerOuts0 + colRulerOutsSpacing * 4, rowRulerOuts0 + rowRulerOutsSpacing * i), module, MidiPlayer::GATE_OUTPUT + i));         
            addOutput(createOutputCentered<sts_Port>(Vec(colRulerOuts0 + colRulerOutsSpacing * 5, rowRulerOuts0 + rowRulerOutsSpacing * i), module, MidiPlayer::VELOCITY_OUTPUT + i));
            addOutput(createOutputCentered<sts_Port>(Vec(colRulerOuts0 + colRulerOutsSpacing * 6, rowRulerOuts0 + rowRulerOutsSpacing * i), module, MidiPlayer::AFTERTOUCH_OUTPUT + i));
            addOutput(createOutputCentered<sts_Port>(Vec(colRulerOuts0 + colRulerOutsSpacing * 7, rowRulerOuts0 + rowRulerOutsSpacing * i), module, MidiPlayer::RETRIGGER_OUTPUT + i));
        }
        
        for (int i = 8; i < 16; i++)
        {
            addParam(createParamCentered<sts_Davies15_snap_Grey>(Vec(colRulerOuts1, rowRulerOuts0 + rowRulerOutsSpacing * (i - 8)), module, MidiPlayer::MAX_CHANNELS + i));
            addParam(createParamCentered<sts_Davies15_snap_Grey>(Vec(colRulerOuts1 + colRulerOutsSpacing * 1, rowRulerOuts0 + rowRulerOutsSpacing * (i - 8)), module, MidiPlayer::MIDI_MODE + i));
            addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(colRulerOuts1 + colRulerOutsSpacing * 2, rowRulerOuts0 + rowRulerOutsSpacing * (i - 8)), module, MidiPlayer::PLAYABLE_TRACKS_LIGHT + (i * 2)));
            addOutput(createOutputCentered<sts_Port>(Vec(colRulerOuts1 + colRulerOutsSpacing * 3, rowRulerOuts0 + rowRulerOutsSpacing * (i - 8)), module, MidiPlayer::CV_OUTPUT + i));                                 
            addOutput(createOutputCentered<sts_Port>(Vec(colRulerOuts1 + colRulerOutsSpacing * 4, rowRulerOuts0 + rowRulerOutsSpacing * (i - 8)), module, MidiPlayer::GATE_OUTPUT + i));         
            addOutput(createOutputCentered<sts_Port>(Vec(colRulerOuts1 + colRulerOutsSpacing * 5, rowRulerOuts0 + rowRulerOutsSpacing * (i - 8)), module, MidiPlayer::VELOCITY_OUTPUT + i));
            addOutput(createOutputCentered<sts_Port>(Vec(colRulerOuts1 + colRulerOutsSpacing * 6, rowRulerOuts0 + rowRulerOutsSpacing * (i - 8)), module, MidiPlayer::AFTERTOUCH_OUTPUT + i));
            addOutput(createOutputCentered<sts_Port>(Vec(colRulerOuts1 + colRulerOutsSpacing * 7, rowRulerOuts0 + rowRulerOutsSpacing * (i - 8)), module, MidiPlayer::RETRIGGER_OUTPUT + i));
        }
        
    }

    void step() override {
        ModuleWidget::step();
        MidiPlayer* midiPlayer = (MidiPlayer*)this->module;
        if (midiPlayer && midiPlayer->doMidiFileOpen) {
            midiPlayer->doMidiFileOpen = false;

            osdialog_filters *filters = osdialog_filters_parse("Midi File (.mid):mid;Text File (.txt):txt");
            std::string dir = midiPlayer->lastPath.empty() ? asset::user("") : directory(midiPlayer->lastPath);
            char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, filters);
            if (path) {
                midiPlayer->lastPath = path;
                midiPlayer->lastFilename = filename(path);
                midiPlayer->doMidiFileLoad = true;
            }
            free(path);
            osdialog_filters_free(filters);
        }
    }

	void appendContextMenu(Menu* menu) override {
		MidiPlayer* module = dynamic_cast<MidiPlayer*>(this->module);

		menu->addChild(new MenuEntry);

        TrackOffsetItem* trackOffsetItem = new TrackOffsetItem();
        trackOffsetItem->text = "Tracks";
        trackOffsetItem->rightText = RIGHT_ARROW;
        trackOffsetItem->module = module;
        menu->addChild(trackOffsetItem);

		ClockMultiplierItem* clockMultiplierItem = new ClockMultiplierItem;
		clockMultiplierItem->text = "Clock multiplier";
		clockMultiplierItem->rightText = RIGHT_ARROW;
		clockMultiplierItem->module = module;
		menu->addChild(clockMultiplierItem);

		MIDI_CVPanicItem* panicItem = new MIDI_CVPanicItem;
		panicItem->text = "Panic";
		panicItem->module = module;
		menu->addChild(panicItem);
	}
};

Model *modelMidiPlayer = createModel<MidiPlayer, MidiPlayerWidget>("MidiPlayer");