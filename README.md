Compile at your own risk, API is still in flux, even though it was supposed to be stable ;(  Wait until this line goes away to be safe.


Here is the V1 list so far.  Documentation to follow.....

Oddy :  Full polyphonic synth (formally Oddyssey, kept slug nale for compatibility. Has grown to the point that it does not really resemble an Oddysy any more.
Illiad:  V1 Compatible, no changes.
VU_Poly:  A polyphonic LED VU Meter.
Poly Sequencer 16:  4 channel polyphonic 16 step sequencer with 2 gates, 1 trigger per voice per step. CV controllable Channel on/off
Ring Modulator:  Orphaned module from JE ported to V1.  Will be POLY at a later date.
Wave Folder:  Orphaned module from JE ported to V1.  Will be POLY at a later date.


STS

New VCV Rack modules for V6.2.

Illiad ( I couldn't cal it the Oddysey) is a Synth Controller modeled after the ARP Odyssey. It is not a synth. Once you set up your synth any way you like it, you can zoom in on the Illiad, and use your synth to your hearts content without trying to remember where the controls are in the mass of modules and wires. I made this for personal use, and figured it would be great for others to use live, and for jamming.

The switch is a modified version of SouthPoles Manual Switch, Abr. It adds 3 more switches, so 2 will cover the 22 switch outputs on the Illiad, and makes them CV controllable.

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

2 -> 1 x 11 Switch:

The 2 -> 1 x 11 Switch in a VC controled version of Southpole's Manual Switch, 'Abr'. 11 switches so 2 of them covers the 22 switches on Illiad. Bottom position A, Top switch position a B.

Hope you have as much fun using these as I had writing them.


Suggestions are always welcome.
