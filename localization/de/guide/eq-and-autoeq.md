# Kopfhörer-EQ und AutoEQ

Curve kann einen Parametric EQ anhand eines gemessenen Kopfhörerprofils einrichten, sodass du den Frequenzgang deiner Kopfhörer für den gesamten Ton deines Mac korrigieren kannst.

## Ein AutoEQ-Kopfhörerprofil verwenden

Dafür ist eine Internetverbindung erforderlich.

1. Klicke mit der rechten Maustaste auf den Hintergrund des Editors und wähle Curve Plugins > Parametric EQ (import from AutoEQ...).
2. Gib im Suchfeld dein Kopfhörermodell ein, zum Beispiel HD 650 oder AirPods.
3. Wähle ein Ergebnis aus, um seine Frequenzkurve und Filter zu sehen.
4. Klicke auf Use this EQ. Curve erstellt einen Node Parametric EQ mit diesen Einstellungen.
5. [Verbinde den Node](editor.md) mit deiner Kette.
6. Wähle Save As Preset..., um ihn zu behalten.

Um stattdessen einen vorhandenen Node Parametric EQ zu aktualisieren, klicke mit der rechten Maustaste darauf und wähle Import EQ from AutoEQ...

Fehlt ein Kopfhörermodell, klicke auf Sync Index, um nach neuen Einträgen in der AutoEQ-Datenbank zu suchen.

## Feinabstimmung oder andere Profile finden

- Um Zielkurven und Klangcharakter anzupassen, verwende [autoeq.app](https://autoeq.app) und exportiere die EQ-Kurve.
- Wenn deine Kopfhörer nicht aufgeführt sind, kannst du es auch mit [squig.link](https://squig.link) versuchen, das Community-Messungen von Kopfhörern und IEMs bietet.

## Eine EQ-Datei importieren

Curve importiert EQ-Kurven aus AutoEQ, Squig.link und REW in `.txt`-, `.csv`-, `.tsv`- oder `.eq`-Dateien.

- Um einen neuen Node zu erstellen, klicke mit der rechten Maustaste auf den Hintergrund des Editors und wähle Curve Plugins > Parametric EQ (import from file...), oder ziehe die Datei auf das Editorfenster.
- Um einen vorhandenen Node Parametric EQ zu aktualisieren, klicke mit der rechten Maustaste darauf und wähle Import EQ from file...

## Den EQ von Hand anpassen

Doppelklicke auf den Node Parametric EQ, um sein Fenster zu öffnen.
