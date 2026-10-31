# Curve Plugins

Curve includes these plug-ins. To add a plug-in to a preset, right-click the editor background and select Curve Plugins.

## EQ

- **Parametric EQ:** a shortcut to Apple's AUNBandEQ multi-band parametric EQ, often used for calibration. Multi-channel. The same menu can [import a headphone profile or EQ file](eq-and-autoeq.md).

## Headphone processing

- **Headphone Crossfeed:** crossfeed for more natural stereo imaging on headphones, with an adjustable amount. Stereo.
- **Headphone Speaker Emulation:** emulates mid-field studio speakers on headphones, including crossfeed. Stereo.

## Level and safety

- **Gain:** volume trim from -60 dB to +60 dB, with a mute toggle. Multi-channel.
- **Clipper:** hard clips each channel at a ceiling from -50 to 0 dBFS, as a safety guard against EQ boosts, with a clip indicator. It only adds protection when the ceiling is below 0 dBFS. No latency. Multi-channel.
- **Meter:** shows one of four views of whatever you connect to it. Levels shows RMS, averaged over about 300 ms (a sine reads 3 dB below its peak), true peak in dBTP, and stereo correlation, where +1 is mono and -1 is out of phase. VU shows needle meters with a 300 ms rise, and you can choose the level that reads 0 VU: -20, -18, -14 or -10 dBFS. Loudness shows momentary, short-term and integrated LUFS (ITU-R BS.1770 / EBU R128), with the last minute of history. Spectrum shows both channels' combined power, and L/R overlays them separately. Clip lights when a sample goes over 0 dBFS, and stays lit until clicked. It measures only while its window is open, and integrated loudness restarts each time the window opens. It has no outputs by default, so connect it as a branch from any pin. To put it inline, add outputs with Configure Audio I/O, and audio then passes through unchanged. No latency. Mono or stereo.
- **Invert Phase:** inverts the polarity of each channel. Multi-channel.

## Routing and stereo

- **Pan:** places a mono input in the stereo output, with a pan law of 0, -3, -4.5 or -6 dB at centre. With a stereo input, set using Configure Audio I/O, it acts as a balance control.
- **Stereo Utility:** swap left and right, mono sum, left or right only, and stereo width from 0 to 200%. Stereo.
- **Mid/Side Encoder:** converts left and right to Mid on the first pin and Side on the second. Stereo.
- **Mid/Side Decoder:** converts Mid and Side back to left and right. Put an encoder and decoder around separate processing of the two pins. The round trip is bit-exact. Stereo.
- **Delay:** whole-sample delay of up to 2 seconds, shown in samples, milliseconds or centimeters, for speaker alignment or syncing audio with video. Changing it during playback may cause a brief click. Multi-channel.

## Recording

- **Audio Recorder:** records to WAV files on your Desktop. Multi-channel.

## MIDI-learnable controls

See [MIDI control](midi-control.md) for what each plug-in exposes.
