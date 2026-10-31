# Curve Plugins

Curve enthält die folgenden Plug-ins. Um ein Plug-in zu einem Preset hinzuzufügen, klicke mit der rechten Maustaste auf den Hintergrund des Editors und wähle Curve Plugins.

## EQ

- **Parametric EQ:** eine Abkürzung zu Apples parametrischem Mehrband-EQ AUNBandEQ, der oft zur Kalibrierung verwendet wird. Mehrkanalig. Über dasselbe Menü kannst du [ein Kopfhörerprofil oder eine EQ-Datei importieren](eq-and-autoeq.md).

## Kopfhörerbearbeitung

- **Headphone Crossfeed:** Crossfeed für eine natürlichere Stereoabbildung auf Kopfhörern, mit einstellbarer Stärke. Stereo.
- **Headphone Speaker Emulation:** emuliert Mid-Field-Studiolautsprecher auf Kopfhörern, einschließlich Crossfeed. Stereo.

## Pegel und Sicherheit

- **Gain:** Lautstärkeanpassung von -60 dB bis +60 dB, mit Stummschalter. Mehrkanalig.
- **Clipper:** begrenzt jeden Kanal hart an einem Ceiling zwischen -50 und 0 dBFS, als Schutz vor EQ-Anhebungen, mit Clipping-Anzeige. Einen zusätzlichen Schutz bietet er nur, wenn das Ceiling unter 0 dBFS liegt. Keine Latenz. Mehrkanalig.
- **Invert Phase:** kehrt die Polarität jedes Kanals um. Mehrkanalig.

## Routing und Stereo

- **Pan:** platziert einen Mono-Eingang im Stereoausgang, mit einem Pan-Gesetz von 0, -3, -4,5 oder -6 dB in der Mitte. Mit einem Stereoeingang, eingestellt über Configure Audio I/O, wirkt es als Balance-Regler.
- **Stereo Utility:** Links und Rechts tauschen, Mono-Summe, nur links oder rechts, und Stereobreite von 0 bis 200 %. Stereo.
- **Mid/Side Encoder:** wandelt Links und Rechts in Mid auf dem ersten Pin und Side auf dem zweiten um. Stereo.
- **Mid/Side Decoder:** wandelt Mid und Side zurück in Links und Rechts. Setze Encoder und Decoder um die getrennte Bearbeitung der beiden Pins herum. Der Hin- und Rückweg ist bit-exakt. Stereo.
- **Delay:** samplegenaue Verzögerung von bis zu 2 Sekunden, angezeigt in Samples, Millisekunden oder Zentimetern, zur Lautsprecherausrichtung oder zum Synchronisieren von Ton und Video. Eine Änderung während der Wiedergabe kann ein kurzes Klicken verursachen. Mehrkanalig.

## Aufnahme

- **Audio Recorder:** nimmt WAV-Dateien auf deinem Schreibtisch auf. Mehrkanalig.

## Per MIDI Learn steuerbare Regler

Unter [MIDI-Steuerung](midi-control.md) findest du, was jedes Plug-in bereitstellt.
