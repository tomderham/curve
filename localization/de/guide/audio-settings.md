# Audioeinstellungen und Geräte

Wähle Settings > Audio Settings, um deine Audio- und MIDI-Geräte auszuwählen.

## Interfaces und Kanäle wählen

Wähle ein Ausgabe-Interface und, falls dein Preset eines verwendet, ein Eingabe-Interface. Aktiviere dann die gewünschten Kanäle. Sie erscheinen als graue Pins auf den Nodes Audio Input und Audio Output.

Wenn du Curves [Node Interface Loopback (In)](loopback.md) verwendest, brauchst du kein Eingabe-Interface.

Curve merkt sich für jedes Ausgabe-Interface die Samplerate, die Puffergröße und die aktivierten Ausgangskanäle und stellt sie wieder her, wenn du zu diesem Interface zurückwechselst.

## Dem Ausgang von macOS folgen

Aktiviere **Use output interface selected in macOS**, damit macOS den Ausgang von Curve bestimmt. Wenn du in den Systemeinstellungen, im Kontrollzentrum oder in der Menüleiste einen Ausgang wählst, verlagert Curve seine Verarbeitung innerhalb von etwa einer Sekunde dorthin. Wird dieses Interface getrennt, wechselt Curve zu dem Ausgang, den macOS als Nächstes verwendet. Solange die Option aktiv ist, ist Curves Menü Output ausgegraut.

Verbindungen deines Presets zu Ausgängen, die das neue Interface nicht hat, bleiben erhalten, sind aber stumm, bis du zu einem Interface zurückwechselst, das sie hat. Eine Korrektur für ein bestimmtes Interface, etwa EQ für bestimmte Kopfhörer, wird auf jeden Ausgang angewendet, zu dem macOS wechselt.

## macOS auf Curves Interface halten

**Force macOS system output to this interface** ist standardmäßig aktiviert. Die Option hält die Systemausgabe von macOS auf Curves Ausgabe-Interface, damit der [Node Interface Loopback (In)](loopback.md) den gesamten Systemton erhält. Deaktiviere sie, wenn du die Systemausgabe selbst wählen möchtest. Solange Use output interface selected in macOS aktiv ist, ist sie ausgegraut, weil der Ausgang von macOS dann immer der von Curve ist.

## Samplerate und Puffergröße

Hier wählst du Samplerate und Puffergröße aus. Ein kleinerer Puffer bedeutet geringere Latenz, belastet deinen Mac aber stärker und kann zu Aussetzern führen.

**Always enforce sample rate** ist standardmäßig deaktiviert. Ist die Option deaktiviert, passt sich Curve an, wenn das Audio-Interface oder eine andere App die Samplerate der Hardware ändert. Ist sie aktiviert, setzt Curve die Rate wieder auf den von dir gewählten Wert zurück.

## MIDI-Geräte

Aktiviere die MIDI-Eingänge und den MIDI-Ausgang, die du verwenden möchtest. Mit einem Node MIDI Input oder MIDI Output verbindest du sie mit Plug-ins. Ein Controller für die [MIDI-Steuerung](midi-control.md) muss hier aktiviert sein.

## Wenn sich Geräte ändern

Curve kümmert sich selbst um häufige Geräteänderungen:

- Curve bleibt bei dem Interface, das du ausgewählt hast, außer wenn Use output interface selected in macOS aktiv ist. Wird es getrennt, verstummt Curve, statt auf einen anderen Ausgang wie die Lautsprecher deines Mac auszuweichen.
- Curve verbindet sich neu und stellt den Ton wieder her, sobald das Interface zurück ist.
- Curve startet die Audioverarbeitung neu, nachdem dein Mac aus dem Ruhezustand aufgewacht ist.

## AirPods und automatisches Umschalten

macOS schaltet AirPods möglicherweise nicht automatisch um, solange Curve ein Audio-Interface geöffnet hält. Um das zu ermöglichen, aktiviere Settings > Release audio interface when idle.

Curve gibt das Interface dann nach 5 Sekunden Stille frei und öffnet es wieder, sobald eine andere App es verwendet. Das geschieht nur, wenn Curve zuverlässig wieder aufwachen kann. In diesen Fällen wird das Interface daher nicht freigegeben:

- Dein Eingang ist ein physisches Mikrofon oder ein Line-Eingang an einem separaten Gerät. Virtuelle Geräte wie BlackHole sind kein Problem. Das Gleiche gilt für einen Eingang deines Ausgabe-Interfaces, wenn der Node Audio Input in deinem Preset verbunden ist.
- Eine andere App verwendet das Gerät.
- Curve ist beschäftigt und nimmt zum Beispiel gerade auf.
