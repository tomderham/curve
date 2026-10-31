# Fehlerbehebung

## Kein Ton oder einige Kanäle sind stumm

- Prüfe, ob die Nodes durchgehend bis zu Audio Output verbunden sind.
- Wenn sich der Node Interface Loopback (In) in einem Preset befindet, du ihn aber nicht verwendest, entferne ihn oder lass seine Pins unverbunden. Siehe [Loopback](loopback.md).
- Wenn du dein ausgewähltes Interface getrennt hast, bleibt Curve stumm, bis es wieder da ist, statt einen anderen Ausgang zu verwenden. Um ein anderes zu verwenden, wähle es in Audio Settings aus.
- Prüfe, ob kein Node umgangen oder stummgeschaltet ist und ob die richtigen Kanäle in [Audio Settings](audio-settings.md) aktiviert sind.

## Curve Help meldet, dass der Inhalt nicht verfügbar ist

Curve Help öffnet sich nur, wenn sich Curve im Ordner „Programme“ befindet. Verschiebe die App dorthin und versuche es erneut.

## Ich finde Curve nach dem Öffnen nicht

Curve läuft in der Menüleiste und öffnet normalerweise keine Fenster. Klicke auf das Symbol in der Menüleiste und wähle dann Show Editor. Um Curve auch im Dock anzuzeigen, aktiviere Settings > Show in Dock.

## Meine Änderungen an einem Preset sind weg

Curve speichert Änderungen nicht automatisch. Verwende Save As Preset... und ersetze das vorhandene Preset. Siehe [Presets](presets.md).

## Ein Plug-in fehlt im Menü

Wähle Settings > Plug-in Manager und starte den Scan für das Format dieses Plug-ins. Siehe [Erste Schritte](getting-started.md).

## Ein Plug-in hat die falsche Anzahl an Pins

Klicke mit der rechten Maustaste auf seinen Node und wähle Configure Audio I/O.

## Die Samplerate ändert sich ständig

Möglicherweise ändert eine andere App die Samplerate der Hardware. Damit Curve sie zurücksetzt, aktiviere Always enforce sample rate in [Audio Settings](audio-settings.md).

## AirPods schalten nicht automatisch um

Aktiviere Settings > Release audio interface when idle. Siehe [Audioeinstellungen und Geräte](audio-settings.md).

## Curve reagiert nicht auf meinen MIDI-Controller

Prüfe, ob der Controller unter den MIDI-Eingängen in Audio Settings aktiviert ist und ob die Zuweisung mit dem Preset gespeichert wurde. Siehe [MIDI-Steuerung](midi-control.md).
