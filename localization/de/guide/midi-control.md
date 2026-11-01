# MIDI-Steuerung

Ein MIDI-Controller oder -Keyboard kann Instrument-Plug-ins spielen, Plug-in-Parameter ändern, Nodes umgehen und Presets wechseln. Aktiviere ihn unter den MIDI-Eingängen in [Audio Settings](audio-settings.md).

## Wählen, wie du ein Plug-in steuerst

Es gibt zwei Wege, ein Plug-in mit deinem Controller zu steuern:

- **MIDI mit dem Plug-in verbinden.** Ein Node MIDI Input sendet MIDI direkt an den bernsteinfarbenen Eingangs-Pin des Plug-ins, und das Plug-in entscheidet, was damit geschieht. Nutze das, um ein Instrument zu spielen und um eine im Plug-in eingebaute MIDI-Zuweisung zu verwenden.
- **Curves MIDI Learn verwenden.** Curve liest den Controller selbst und ändert dann für dich die Parameter oder den Bypass des Plug-ins. Dafür sind weder ein Node MIDI Input noch Verbindungen nötig. Nutze das für Plug-ins ohne bernsteinfarbenen Eingangs-Pin und für den Bypass.

MIDI Learn erreicht nur die Parameter, die ein Plug-in seinem Host zur Verfügung stellt. Manche Plug-ins, vor allem große Instrumente, stellen nur einen Teil ihrer Regler bereit. Wenn das Bewegen eines Reglers im Plug-in-Fenster während MIDI Learn nichts bewirkt, kann Curve diesen Regler nicht zuweisen.

Hat ein Plug-in einen bernsteinfarbenen Eingangs-Pin und eine eigene MIDI-Learn-Funktion, ist das oft die einfachere Wahl, weil sie meist jeden Regler erreicht. Verbinde einen Node MIDI Input mit dem Plug-in und richte die Zuweisung dann im Fenster des Plug-ins ein. Wie das geht, steht in seinem Handbuch.

Verwende für jeden Regler oder Taster nur eine Methode. Ein mit Curves MIDI Learn zugewiesenes Bedienelement erreicht Plug-ins nicht über den Node MIDI Input, das Plug-in reagiert also nicht darauf.

## Ein Instrument-Plug-in spielen

1. Klicke mit der rechten Maustaste auf den Hintergrund des Editors und füge dein Instrument-Plug-in hinzu.
2. Klicke mit der rechten Maustaste auf den Hintergrund des Editors und wähle MIDI I/O > MIDI Input.
3. Ziehe vom bernsteinfarbenen Pin des Nodes MIDI Input zum bernsteinfarbenen Pin oben am Instrument. MIDI-Pins und -Verbindungen sind bernsteinfarben, Audio-Pins und -Verbindungen grau.
4. Ziehe von den grauen Ausgangs-Pins des Instruments zum Node Audio Output.
5. Wähle Save As Preset..., um das Setup zu behalten.

Effekt-Plug-ins, die auf MIDI reagieren, werden genauso verbunden. Um MIDI von einem Plug-in an ein externes Gerät zu senden, aktiviere das Gerät in Audio Settings als MIDI-Ausgang und verbinde dann den bernsteinfarbenen Ausgangs-Pin des Plug-ins mit einem Node MIDI Output.

## Ein Bedienelement mit MIDI Learn zuweisen

1. Klicke mit der rechten Maustaste auf einen beliebigen Node und wähle MIDI Learn. Der Editor erhält einen hervorgehobenen Rahmen, und die Statusleiste führt dich durch die Schritte.
2. Wähle aus, was gesteuert werden soll:
   - Für einen Plug-in-Parameter öffne das Fenster des Plug-ins und bewege das Bedienelement.
   - Für den Bypass eines Nodes klicke auf dessen Ein/Aus-Taste.
3. Bewege einen Regler, Fader oder Taster an deinem Controller. Die Zuweisung erfolgt sofort, und du kannst weitere Bedienelemente zuweisen.
4. Drücke Esc oder wähle erneut MIDI Learn, um den Vorgang zu beenden.

Zuweisungen werden mit dem Preset gespeichert. Verwende also Save As Preset..., um sie zu behalten.

## Eine Zuweisung entfernen

- Um die Zuweisung für den ausgewählten Parameter oder Bypass zu entfernen, drücke während MIDI Learn die Taste Delete.
- Um alle Zuweisungen eines Nodes zu entfernen, klicke mit der rechten Maustaste darauf und wähle Clear MIDI Mappings.

## So verhalten sich zugewiesene Bedienelemente

- Regler und Fader (CC) decken den gesamten Bereich eines Parameters ab.
- Noten und Program Changes schalten das Ziel um.
- Beim Bypass erkennt Curve, ob ein CC-Taster als Taster arbeitet, bei dem jeder Druck den Bypass umschaltet, oder als Schalter, bei dem der Bypass dem Zustand des Tasters folgt.

Nodes mit Zuweisungen zeigen ein kleines MIDI-Buchsensymbol. Bewege den Mauszeiger über den Node, um seine Zuweisungen in der Statusleiste zu sehen.

Zugewiesene MIDI-Nachrichten werden nicht über den Node MIDI Input an Plug-ins weitergegeben. Alle anderen MIDI-Daten schon.

## Presets über einen Controller wechseln

Öffne Settings > MIDI Control. Diese Einstellungen gelten für alle Presets:

- **Program Change Selects Preset:** standardmäßig deaktiviert. Program Change 0 lädt das erste Preset im Menü, 1 das zweite und so weiter, in derselben Reihenfolge wie bei den [Zifferntasten-Kurzbefehlen](keyboard-shortcuts.md).
- **Next Preset Control** und **Previous Preset Control:** wähle eine der Optionen und drücke dann die gewünschte Taste an deinem Controller.
- **Clear Preset Controls:** entfernt diese beiden Tastenzuweisungen.

## Bedienelemente von Curves eigenen Plug-ins

| Plug-in | Bedienelemente |
|---|---|
| Gain | Gain und Stummschaltung |
| Pan | Pan |
| Clipper | Ceiling |
| Stereo Utility | Modus und Breite |
| Headphone Crossfeed | Stärke |
| Audio Recorder | Aufnahme starten und stoppen |

Headphone Speaker Emulation, Invert Phase sowie Mid/Side Encoder und Mid/Side Decoder haben keine Einstellungen, weise daher stattdessen ihren Bypass zu. Delay lässt sich nicht zuweisen.
