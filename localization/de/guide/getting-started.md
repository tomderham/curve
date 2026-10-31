# Erste Schritte mit Curve

Curve benötigt macOS Sonoma 14.2 oder neuer auf Apple Silicon oder Intel.

## Curve installieren

1. Lade Curve von der [neuesten Version](https://github.com/tomderham/curve/releases) herunter.
2. Kopiere `Curve.app` in deinen Ordner „Programme“ und öffne die App.

Curve läuft in der Menüleiste, sein Symbol erscheint daher standardmäßig dort und nicht im Dock. Das Editorfenster öffnet sich beim ersten Start. Danach startet Curve ohne geöffnete Fenster.

Um Curves Menü zu öffnen, klicke auf das Symbol in der Menüleiste oder auf das Menüsymbol oben links im Editorfenster.

## Nach Plug-ins suchen

Überspringe diesen Schritt, wenn du nur die integrierten Plug-ins von macOS wie AUNBandEQ oder [Curves eigene Plug-ins](curve-plugins.md) verwenden möchtest.

1. Wähle Settings > Plug-in Manager.
2. Klicke unten links auf Options... und wähle dann 'Scan for new or updated AudioUnit plug-ins'. Das kann eine Weile dauern, und macOS fragt möglicherweise, ob Curve auf Ordner zugreifen darf, die von Plug-ins verwendet werden.
3. Wenn du VST3-Plug-ins verwendest, wiederhole das mit 'Scan for new or updated VST3 plug-ins'.
4. Schließe das Fenster.

## Audiogeräte auswählen

1. Wähle Settings > Audio Settings.
2. Wähle das Ausgabe-Interface und, sofern du kein [Loopback](loopback.md) verwendest, das Eingabe-Interface.
3. Wähle die gewünschten Kanäle sowie Samplerate und Puffergröße.
4. Wenn du MIDI verwendest, wähle deine MIDI-Interfaces aus.
5. Schließe das Fenster.

## Dein erstes Preset erstellen

Ein Preset ist eine Kette verbundener Nodes im Editor. Wenn der Editor nicht geöffnet ist, wähle Show Editor in Curves Menü.

1. Du siehst die Nodes Audio Input, Interface Loopback (In) und Audio Output mit grünen Pins für die aktivierten Kanäle.
2. Klicke mit der rechten Maustaste auf den Hintergrund des Editors und wähle ein Plug-in aus. Sein Node erscheint mit Eingängen oben und Ausgängen unten.
3. Ziehe von einem grünen Pin des Eingangs-Nodes zu einem Eingangs-Pin des Plug-ins.
4. Ziehe von den Ausgangs-Pins des Plug-ins zum Node Audio Output.
5. Doppelklicke auf den Plug-in-Node, um sein Fenster zu öffnen und es anzupassen.
6. Wähle Save As Preset... in Curves Menü und gib dem Preset einen Namen.

Wenn ein Preset den Node Interface Loopback (In) nicht verwendet, entferne ihn oder lass zumindest seine Pins unverbunden. Sonst kann er deinen Ton stummschalten.

Sei vorsichtig mit ungewöhnlichem Routing. Curve bietet einen gewissen Schutz vor Rückkopplungen, aber eine falsche Konfiguration kann trotzdem Geräte oder dein Gehör schädigen.

## Einrichtung abschließen

- Um Curve bei der Anmeldung zu starten, wähle Settings > Open at Login.
- Die automatische Suche nach Updates ist standardmäßig aktiviert. Du kannst das unter Settings > Auto-Check for App Updates ändern.

## Nächste Schritte

- [Presets](presets.md)
- [Kopfhörer-EQ und AutoEQ](eq-and-autoeq.md)
- [Loopback und Systemaudio](loopback.md)
