# Presets

Ein Preset speichert einen ganzen Graphen: seine Plug-ins und deren Einstellungen, Verbindungen, das Audio-Kanalrouting und die MIDI-Zuweisungen. Beim Wechseln von Presets kannst du die Einstellungen derselben Plug-ins ändern, zu anderen Plug-ins wechseln oder das Kanalrouting ändern.

## Ein Preset speichern

1. Wähle Save As Preset... in Curves Menü.
2. Gib einen Namen ein und klicke auf Save.

Änderungen werden nicht automatisch gespeichert. Verwende Save As Preset..., wenn du sie behalten möchtest.

## Ein vorhandenes Preset ändern

1. Lade das Preset, indem du es in Curves Menü auswählst.
2. Nimm deine Änderungen vor.
3. Wähle Save As Preset..., wähle den vorhandenen Namen und bestätige, dass du ihn ersetzen möchtest.

## Presets wechseln

Deine Presets sind in Curves Menü alphabetisch aufgeführt, und das aktuelle trägt ein Häkchen. Wähle eines aus, um sofort dazu zu wechseln. Curve fragt vorher nicht nach, speichere also alle Änderungen, die du behalten möchtest, bevor du wechselst.

Du kannst Presets auch [über die Tastatur](keyboard-shortcuts.md) oder [über einen MIDI-Controller](midi-control.md) wechseln. Um eine Preset-Datei direkt zu laden, ziehe sie auf das Editorfenster.

## Ein neues Preset beginnen

Um mit einem leeren Graph neu zu beginnen, klicke auf das Menüsymbol oben links im Editorfenster und wähle New Preset. Alle Nodes außer Audio Input, Interface Loopback (In) und Audio Output werden entfernt. Diese sind nicht verbunden, du hörst also nichts, bis du sie verbindest.

Sofern der Graph nicht nur diese Nodes enthält, fragt Curve vorher nach, denn im Fenster eines Plug-ins geänderte Einstellungen werden nicht als Änderungen erfasst. Wähle Save As Preset..., um das aktuelle Setup zu behalten, oder Discard and Start New.

## Szenen

Wenn mehrere Presets Teile gemeinsam haben, kannst du sie zu Szenen machen. Szenen können zum Beispiel dieselbe Lautsprecherkorrektur nutzen, aber unterschiedliche Instrumente oder Signalwege, oder dieselben Instrumente mit unterschiedlicher Lautsprecherkorrektur. Szenen teilen sich ihre Plug-ins und Verbindungen, sodass du eine Änderung nur einmal vornehmen musst, um alle zu aktualisieren. Jede Szene steht wie jedes andere Preset in Curves Menü, und du wechselst auf dieselbe Weise zu ihr.

Szenen, die miteinander teilen, sind in Curves Menü und im Fenster Save Preset mit derselben Farbe markiert.

Der Wechsel zu einer anderen Szene geht schnell: Curve ändert nur, was diese Szene für sich hat, ohne ihre Plug-ins neu zu laden. Hast du seit dem Laden eine geteilte Einstellung geändert oder fehlt ein Plug-in, wird die Szene stattdessen vollständig geladen, und die Änderung geht verloren, wie beim Wechsel zu einem anderen Preset.

Was jede Szene für sich behält:

- Ihr Tempo. Siehe [Tempo und Transport](editor.md).
- Ob jeder Node umgangen oder stummgeschaltet ist. Ein umgangener Node gibt sein Eingangssignal unverändert weiter. Ein stummgeschalteter Node gibt nichts weiter und unterbricht so den Signalweg, auf dem er liegt. Siehe [Einen Node stummschalten](editor.md).
- Die Einstellungen von Nodes, die auf Vary by Scene gesetzt sind. Klicke mit der rechten Maustaste auf einen Node und wähle Vary by Scene; er zeigt dann ein kleines Symbol aus gestapelten Karten.
- Welche Plug-in-Fenster mit ihr geöffnet werden (Always Open Plug-in GUI with Preset) und wo sie liegen. Ein Node, den du einer Szene hinzufügst, wird mit den anderen nicht geöffnet.

Alles andere wird geteilt: die übrigen Nodes und ihre Einstellungen, Verbindungen, Kanal-Routing und MIDI-Zuordnungen. Änderst du einen geteilten Node in einer beliebigen Szene und speicherst sie, erhalten alle Szenen die Änderung. Ein auf Vary by Scene gesetzter Node teilt trotzdem seine Kanäle und Verbindungen.

So erstellst du eine Szene:

1. Lade das Preset, von dem die neue Szene ausgehen soll.
2. Klicke mit der rechten Maustaste auf jedes Plug-in, das in der neuen Szene anders eingestellt sein soll, und wähle Vary by Scene.
3. Nimm die Änderungen für die neue Szene vor. Um einen Signalweg wegzulassen, schalte einen Node darauf stumm.
4. Wähle Save As Preset..., gib einen neuen Namen ein, wähle die Option für eine Szene und klicke auf Save.

Vor dem Speichern zeigt das Fenster Save Preset, welche anderen Szenen aktualisiert werden, und nennt geteilte Nodes, deren Einstellungen sich darin ändern. Ist eine Änderung nur für diese Szene gedacht, brich ab, klicke mit der rechten Maustaste auf den Node, wähle Vary by Scene und speichere erneut. Jede Szene behält dann ihre eigenen Einstellungen für diesen Node. Sie bleibt eine Szene, und die Szenen teilen weiterhin alles andere.

Um eine Szene zu ändern, lade sie, nimm deine Änderungen vor und speichere sie unter demselben Namen. Speicherst du über eine andere Szene, mit der sie teilt, ersetzt das diese Szene, und sie bleibt eine Szene. Ein hinzugefügter Node erscheint auch in den anderen Szenen, dort umgangen, sodass sie gleich klingen, bis du ihn dort einschaltest. Ein umgangener Node gibt sein Eingangssignal trotzdem weiter. Damit ein neuer Signalweg in den anderen Szenen still bleibt, schalte dort einen seiner Nodes stumm.

Damit eine Szene nicht mehr mit den anderen teilt, klicke im Fenster Save Preset mit der rechten Maustaste darauf und wähle Convert to Standalone Preset. Sie behält ihre aktuellen Einstellungen.

## Ein Preset umbenennen oder löschen

Klicke im Fenster Save Preset mit der rechten Maustaste auf ein Preset, um es umzubenennen, in den Papierkorb zu legen oder im Finder anzuzeigen.

Presets sind `.filtergraph`-Dateien in `~/Library/Application Support/Curve/Presets/`, du kannst sie also auch im Finder verwalten.
