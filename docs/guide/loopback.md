# Loopback and system audio

Loopback lets you send the audio from macOS through Curve before it reaches your headphones or speakers. This is how you apply EQ to everything your Mac plays.

## Use Curve's Interface Loopback node

No extra software or drivers are needed.

1. In [Audio Settings](audio-settings.md), select the output interface that you want to hear audio from.
2. In your preset, use the Interface Loopback (In) node as the input instead of Audio Input.
3. Connect it through your plug-ins to the Audio Output node.

The node receives bit-perfect audio that would otherwise have gone straight to the output interface. Add it from Audio I/O in the editor's right-click menu if it isn't there.

## Send all system audio to Curve

**Force macOS System Audio to loopback** in the Settings menu is on by default. It keeps macOS's system output set to the interface selected in Curve. Turn it off if you want to choose the system output yourself.

## Use your audio interface's own loopback

Alternatively, if your interface has hardware loopback, this can be a good choice as it can minimize latency. For example, with RME TotalMix:

1. Use TotalMix's Loopback function to send a pair of output channels to a spare pair of input channels.
2. In Curve, select those input channels and use the Audio Input node.
3. Select some of the software playback channels as Curve's outputs.
4. In TotalMix, assign those playback channels to your hardware outputs.

Don't use the Interface Loopback (In) node with this setup.

## Important: unused loopback pins

If the Interface Loopback (In) node is connected, the direct audio for those channels no longer reaches the output device. If it's in a preset you aren't using it in, remove it or leave its pins unconnected, or you may get unexpected muting.

## How the redirect works

The channels of Stream 0, the primary stream of the selected output interface, are redirected to the node when its pins are connected. Channels that the interface's driver assigns to a different stream are unaffected.
