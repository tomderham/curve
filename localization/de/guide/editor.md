# Der Editor

Der Editor zeigt dein Preset als Graph aus Nodes. Der Ton fließt von den Ausgängen eines Nodes, unten am Node, zu den Eingängen des nächsten Nodes, oben am Node. Um ihn zu öffnen, wähle Show Editor in Curves Menü. Um ihn zu schließen, klicke auf die Schließen-Taste oder wähle Hide Editor. Die Audioverarbeitung läuft in beiden Fällen weiter.

Die Statusleiste unten im Editor gibt Hinweise und beschreibt, was sich gerade unter dem Mauszeiger befindet. Bei einem Plug-in zeigt sie auch Hersteller und Format, zum Beispiel AudioUnit oder VST3.

## Einen Node hinzufügen

Klicke mit der rechten Maustaste auf den Hintergrund des Editors und wähle dann aus:

- **Audio I/O:** die Nodes für Audioeingang, Audioausgang und Interface Loopback (In).
- **MIDI I/O:** die Nodes für MIDI-Eingang und -Ausgang.
- **Curve Plugins:** [Curves integrierte Plug-ins](curve-plugins.md).
- **Deine gescannten Plug-ins:** unter diesen Menüs aufgeführt, nach Hersteller geordnet.

Du kannst auch eine Plug-in-Datei oder eine [EQ-Datei](eq-and-autoeq.md) auf das Editorfenster ziehen.

## Nodes verbinden

Ziehe von einem Pin eines Nodes zu einem Pin eines anderen. Audio-Pins sind grau, MIDI-Pins bernsteinfarben. Du kannst einen Ausgang mit mehreren Eingängen verbinden und beliebige Ketten aufbauen.

Aufeinanderfolgende Kanäle zwischen denselben zwei Nodes, etwa ein Stereopaar, werden als eine dickere Linie dargestellt, die sich zu jedem Pin hin verzweigt. Die Kanäle bleiben getrennt, es wird nichts heruntergemischt. Ziehe die Linie, um jeweils einen Kanal neu zu verbinden.

## Nodes anordnen

Ziehe einen Node, um ihn zu verschieben. Nodes rasten in einem Raster ein, sodass ausgerichtete Nodes gerade Verbindungen erhalten. Halte beim Ziehen die Option-Taste gedrückt, um einen Node frei zu platzieren. Ziehst du ihn danach ohne Option erneut, rastet er wieder im Raster ein.

Um den gesamten Graphen nach Signalfluss anzuordnen, klicke mit der rechten Maustaste auf den Hintergrund und wähle Tidy Up Layout. Braucht der Graph mehr Platz, wird das Fenster passend vergrößert, bis zur Größe deines Bildschirms. Undo Tidy Up stellt dann dein Layout und die Fenstergröße wieder her, bis du das nächste Mal einen Node verschiebst.

## Einen Node umgehen

Klicke auf die Ein/Aus-Taste am Node. Klicke erneut darauf, um den Node wieder zu aktivieren. Du kannst auch mit der rechten Maustaste auf den Node klicken und Toggle Bypass wählen.

## Das Fenster eines Plug-ins öffnen

Doppelklicke auf den Node.

Damit sich das Fenster bei jedem Laden des Presets öffnet, auch wenn der Editor geschlossen ist, klicke mit der rechten Maustaste auf den Node, wähle Always Open Plug-in GUI with Preset und verwende dann Save As Preset... Das Fenster öffnet sich an der Position, die es beim Speichern hatte.

## Node-Menü

Klicke mit der rechten Maustaste auf einen Node, um diese Optionen zu sehen:

| Eintrag | Funktion |
|---|---|
| Delete This Node | Entfernt den Node. |
| Disconnect All Pins | Entfernt alle Verbindungen des Nodes. |
| Toggle Bypass | Schaltet den Bypass ein oder aus. |
| Rename This Node | Ändert den auf dem Node angezeigten Namen. |
| MIDI Learn | Startet [MIDI Learn](midi-control.md). |
| Clear MIDI Mappings | Entfernt die MIDI-Zuweisungen des Nodes. Wird nur angezeigt, wenn es welche gibt. |
| Always Open Plug-in GUI with Preset | Öffnet das Fenster des Plug-ins bei jedem Laden des Presets. |
| Import EQ from AutoEQ... | Wird bei Nodes vom Typ Parametric EQ angezeigt. Siehe [Kopfhörer-EQ und AutoEQ](eq-and-autoeq.md). |
| Import EQ from file... | Wird bei Nodes vom Typ Parametric EQ angezeigt. Siehe [Kopfhörer-EQ und AutoEQ](eq-and-autoeq.md). |
| Configure Audio I/O | Ändert die Kanal-Layouts eines Plug-ins und schaltet seine Pins aus oder blendet sie aus. Siehe unten. |
| Audio Settings... | Wird bei Audioeingangs- und -ausgangs-Nodes angezeigt. Öffnet Audio Settings. |
| Save Plug-in State | Speichert die Einstellungen des Plug-ins in einer Datei. |
| Load Plug-in State | Lädt die Einstellungen des Plug-ins aus einer Datei. |

## Die Ein- und Ausgänge eines Plug-ins konfigurieren

Klicke mit der rechten Maustaste auf den Node eines Plug-ins und wähle Configure Audio I/O. Die Pins eines Plug-ins sind in Busse gruppiert, etwa einen Stereo-Haupteingang und einen Sidechain-Eingang. Wähle einen Bus über seine Nummer und verwende dann:

- **Channel Layout:** legt fest, wie viele Kanäle der Bus hat. Layouts mit mehr als zwei Kanälen sind nach Kanalanzahl gruppiert. Ein Surround- oder Ambisonics-Plug-in kann zwei Layouts mit derselben Kanalanzahl unterschiedlich behandeln, wähle also das, das zu deinem Audio passt.
- **Enabled:** schaltet den Bus aus, wodurch seine Pins verschwinden. Die Option ist ausgegraut, wenn das Plug-in ohne den Bus nicht laufen kann.
- **Shown:** blendet die Pins des Busses im Graph aus, der Bus läuft aber weiter. Damit räumst du einen Bus weg, den du nicht verwendest und der sich nicht ausschalten lässt. Beim Ausblenden werden die Verbindungen des Busses entfernt, und ein ausgeblendeter Eingang erhält Stille. Shown ist ausgegraut, solange der Bus aus ist.

Wenn das Plug-in einen MIDI-Pin hat, hat MIDI Port eine eigene Shown-Einstellung. Eine Seite, die nur MIDI hat, etwa der Eingang eines Instruments, hat nur diese Einstellung.

Wenn du ein Layout änderst oder einen Bus ein- oder ausschaltest, werden alle Pins des Nodes getrennt. Die Einstellungen werden mit dem Preset gespeichert. Wenn du auf einen Node mit ausgeblendeten Pins zeigst, weist die Statusleiste darauf hin.
