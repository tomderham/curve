# Tastaturkurzbefehle

## Curves Menü von überall öffnen

Der globale Kurzbefehl öffnet Curves Menü auch dann, wenn eine andere App im Vordergrund ist. Er ist deaktiviert, bis du ihn einrichtest.

1. Wähle Settings > Global Menu Shortcut.
2. Klicke auf Record Shortcut und drücke dann eine Tastenkombination mit Command, Option oder Control. Drücke Esc, um abzubrechen.
3. Klicke auf Save.

Klicke auf Reset to Default, um ⌥⇧C zu verwenden, oder auf Disable, um den Kurzbefehl auszuschalten.

## Curves Lautstärke mit den Lautstärketasten einstellen

Manche Audio-Interfaces haben keine Lautstärkeregelung, die macOS nutzen kann, daher bewirken die Lautstärketasten der Tastatur nichts. Curve kann die Tasten stattdessen für seine eigene Ausgangslautstärke verwenden.

1. Wähle Settings > Volume Keys > When macOS Can't Control the Interface's Volume.
2. Wenn macOS fragt, gib Curve unter Systemeinstellungen > Datenschutz & Sicherheit > Bedienungshilfen Zugriff. Curve braucht ihn, um die Tasten abzufangen.

Curve zeigt dann wie macOS seine eigene Lautstärkeanzeige unter dem Ton-Symbol in der Menüleiste.

- Die Lautstärketasten schalten wie in macOS durch 16 Stufen. Halte Shift und Option zusammen mit einer Lautstärketaste für feinere Stufen.
- Die Stummtaste schaltet Curves Ausgang aus und wieder ein.

Curve übernimmt die Tasten nur, solange der Ausgang von macOS Curves Ausgangs-Interface ist und macOS die Lautstärke der Kanäle, über die Curve spielt, nicht einstellen kann. Dazu gehören Interfaces ganz ohne Lautstärkeregelung und Interfaces, deren Lautstärkeregelung in macOS nur einige ihrer Kanäle umfasst, nicht die, über die Curve spielt. Andernfalls funktionieren die Tasten wie gewohnt in macOS. Damit die Tasten immer Curves Lautstärke einstellen, wähle stattdessen Always.

Curves Lautstärke gilt für alles, was Curve ausgibt, nach deinen Plug-ins, und nur, solange die Tasten sie einstellen. Curve merkt sie sich, wenn du es beendest. Bei voller Lautstärke bleibt das Audio unverändert.

Um die Anzeige auszublenden, schalte Show Volume Overlay im selben Menü aus.

## Presets über die Tastatur wechseln

Während Curves Menü geöffnet ist, drücke eine Taste, um ein Preset auszuwählen. Die Taste wird neben jedem Preset angezeigt.

| Presets | Tasten |
|---|---|
| 1 bis 10 | 1 bis 0 |
| 11 bis 20 | Option mit 1 bis 0 |
| 21 bis 30 | Shift mit 1 bis 0 |

Drücke zum Beispiel den globalen Kurzbefehl und dann 2, um aus jeder App zu deinem zweiten Preset zu wechseln.

## Im Editor während MIDI Learn

| Taste | Aktion |
|---|---|
| Esc | MIDI Learn beenden |
| Delete | Zuweisung für das ausgewählte Bedienelement entfernen |
