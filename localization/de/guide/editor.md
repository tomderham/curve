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

Ziehe von einem Pin eines Nodes zu einem Pin eines anderen. Audio-Pins sind grün, MIDI-Pins rot. Du kannst einen Ausgang mit mehreren Eingängen verbinden und beliebige Ketten aufbauen.

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
| Configure Audio I/O | Ändert die Anzahl der Eingangs- und Ausgangs-Pins eines Plug-ins. |
| Audio Settings... | Wird bei Audioeingangs- und -ausgangs-Nodes angezeigt. Öffnet Audio Settings. |
| Save Plug-in State | Speichert die Einstellungen des Plug-ins in einer Datei. |
| Load Plug-in State | Lädt die Einstellungen des Plug-ins aus einer Datei. |

## Die Kanalanzahl eines Plug-ins ändern

Wenn ein Plug-in nicht die gewünschte Anzahl an Pins hat, klicke mit der rechten Maustaste auf seinen Node und wähle Configure Audio I/O.
