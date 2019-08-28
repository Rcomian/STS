V1.1.1

Fixed SVG in Wavefolder module.

And  ............

Introducing Midi Player, a collaboration between STS and RCM:
Midi Player allows you play standard Midi Files in VCV rack.  Unlimited tracks, with up to 16 VCV Channel Polyphony per track.

![pack](/res/midiplayer.png?raw=true "pack")


Load your file using the "Load File" button.  The light will show green when the file is successfully loaded.
Once you load a file, you will see the following in Green lettering:
1) File name of your file.
2) Time display for your file.  It will update as file is playing.
3) Track names as read from your file.  If the track names are blank, your file does not have track names defined. Midi Player does not support General Midi (GM) at this time. If you can try looking at your file in a standalone Midi Player such as "Midi Editor", and looking under the "Channels" tab to see if there are General Midi manes for the tracks.  Load VCV 'Notes" and write a list of the GM names for the tracks in your patch as a reference.
 
Under the time display, you will have a "Scrub" horizontal slider that works the same as the Scrub knob In DAWs.  It allows you to change position in your file by moving the slider back and forth. 

The 2 knobs, light, and 5 output ports name's are shortened to save on clutter. You will remember their function after your first use of Midi Player:
1) "Po" knob is "Max Poly Channels". It is preset for the number of VCV channels in the track. The tooltip will show its present setting.  It can be lowered, to wither save CPU use, or raised if you need more channels to play back properly in your present patch.  This will rarely need to be adjusted from default.
2) "Mm" knob is 'Midi Mode". It is defaulted to "Rotate", but can be changed is desired.  Using "Reuse" works better when a channel contains a number if drums.
3) The light is displayed Green if that track has content and Red if the track is blank.
The next five are standard outputs:
4) "Vo" is the "Volts per Octave" output for the track.
5) "Ga" is the 'Gate' output for the track.
6) "Ve" is "velocity" or volume of the notes in the track.
7) "At' is "After Touch. It sends any Aftertouch modulation that is in the file.
8) "Rt" is "ReTrigger". Sends a Re Trigger signal is needed.

The Loop Start and End Buttons can be set by clicking on either button. The will set you start point or end point for playing back you Midi File, instead of using actual beginning and end of your file.  If the "Loop" On/Off" button is lit, the file will playback between the selected loop points.  If neither are lit, it will repeat from the beginning when it gets to the end of the file. Clicking on either button clears the state.

Across the top of the module, you will see the following:
First Row:
1) "16+ Tracks" Light.  This will be lit if you file has more Tracks than will show in the module. Right clicking will bring up the context menu that allows you to select groups of tracks to display and play.  Using more than 1 instance of Midi Player will allow you to play more than 16 tracks.  Connect a output from a module,  AS "Triggers MK III" is perfect for this,  to both play and reset CV inputs to control multiple instances of Midi Player.
2) "Load File" brings up the standard file selector to load your midi file.
3) "Reset" button and CV input resets the stae to the same as when the file was newly loaded.
4) "Run/Pause" button and CV input starts and stops the playback of your Midi File.
5) "Loop On/Off" button and CV input, if button is lit, your file will start over when it gets to the endpoint.  See 'Loop start and end" button description for more info.
6) "Rtn to Start" CV input, when sent a high state from a trigger, returns your midi player to the beginning.
Second Row:
1) "Clock out" is a clock out trigger to sync other sequencers to the Midi Player.  Right click context menu can select clock out multiplier.
2, 3 4) CV inputs that go to the button above it.
5) "EOC" is END OF CYCLE".  It is a CV output that sends  a trigger at the end of the play cycle for external syncing.


V1.0.4

On PolySEQ16:
Fixed row 4 Octave control

on LFOPoly:
added variable phase control per LFO.
2 versions now:
1) LFOPoly has continuous control of each LFO's pahse, -180 degrees to +180 degrees
2) LFOPolySP has Knob 'snapped' to 30 degree increment on each LFO's phase, -180 degrees to +180 degrees.  Let me know if you nee 15 degree increments.
Will combine into 1 module is get enough requests.

If you need separate outputs per LFO, use VCV SPLIT.

Minor fixes on displays on all modules

V1.0.3

**************************************

on Oddy: (previously Oddyssey)

Lowered CPU ude on Oddy some more.

Finalized 4 waveforms available on VCOs and LFO.  Waveforms can be morphed using CV inputs.


On PolySEQ16:

  Misc Small fixes, including cleaned up and aligning interface layout,
  
  Added different step setting for each row.
  
  Tool tip for knobs now shows note letter.
  
  Added menu item for changing knobs from 'snapped to semitones' to 'continious'.
  
  Added separate 'index' lights per row.
  
  

V1.0.2

Added LFOPoly

Added label for channel on/off SEQ16 (ooops)

Lowered CPU use on Oddy Poly.

 


Here is the V1 list so far. Documentation to follow.....

Oddy : Full polyphonic synth (formally Oddyssey, kept slug name the same for compatibility. Has grown to the point that it does not really resemble an Oddysey any more.

Illiad: V1 Compatible, no changes.

VU_Poly: A polyphonic LED VU Meter.

Poly Sequencer 16: 4 channel polyphonic 16 step sequencer with 2 gates, 1 trigger per voice per step. CV controllable Channel on/off

Ring Modulator: Orphaned module from JE ported to V1. Will be POLY at a later date.

Wave Folder: Orphaned module from JE ported to V1. Will be POLY at a later date.

LFOPoly:  16 channel LFO for polyphonic modulation.   Independent Frequency, Waveshape, ans Pule Width per LFO. Polyphonic CV inputs for Frequency, Waveshape, and Pule Width, along with Reset trigger input



VCV Rack modules for V6.2.

Illiad ( I couldn't call it the Oddysey) is a Synth Controller modeled after the ARP Odyssey. It is not a synth. Once you set up your synth using any of the sliders or switches to control any module CV inputs any way you like it, you can zoom in on the Illiad, and use your synth to your hearts content without trying to remember where the controls are in the mass of modules and wires. I made this for personal use, and figured it would be great for others to use live, and for jamming.

The Illiad has 3 'Homage to the Odyssey' skins if you right click

I can't thank Jim Tupper enough for his help getting me going on this, and Omri Cohen's help along the way, plus a thanks to all the VCV Rack developers whose code sneaked into these modules, and to Andrew Belt for the wonderful VCV Rack!

Quick docs to get you started:

The Illiad:

The Idea behind the Illiad is to control a VCV Synthesizer patch. There are no limits to the patch at all, the names of the sliders and switches are simply suggestions, paying homage to the ARP Odyssey. The Idea is to get your patch set up the way you want it, and then zoom in to just the Illiad, and control your patch without having to dig through the mass of wires and modules to actually use you patch.

The Controls:

The 3 right hand Outputs scross the bottom are for the 3 switches in the top row. The rest of the outputs are below the switch that controls them. The switches are either 0 volts, switch down, or 1 volt, switch up.

The slider outputs are on the left of the panel, labeled to match the slider that controls them. These are voltage control sliders, NOT mixer sliders. Making the mixer sliders would be too resrtrictive for a lot of uses. The range is 0v to 10 volts.

Above each slider in an offset knob, centered (default, returned to by right clicking) the offset has no affect on the slider, turning the offset knob the the left lower the lowest voltage output of the slider down to -10V at max. Raising the offset knob obove center raises the output from the slider. Connect the ML Modules Volt Meter to se the exact voltage that are being output. With proper offset applied, you can easily get -5 volt to +5 volt range from the sliders.

The outputs are in order, going down each column, starting with the bottom row of sliders, and the top row of sliders. This is good to know, sice the labels had to be kind og cryptic sometime, due to lack of space.

As with all VCV controls, holding down the key allows for finer tuning.

The last 3 outputs are for:

The Octave switch, 2 octaves up, and 2 octave down from center.

The Portamento Slider,

The Pitch Bend knob, 0 to 2 Volts

Hope you have as much fun using these as I had writing them.

Suggestions are always welcome.
