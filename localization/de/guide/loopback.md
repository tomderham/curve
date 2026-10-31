# Loopback und Systemaudio

Mit Loopback kannst du den Ton aus macOS durch Curve leiten, bevor er deine Kopfhörer oder Lautsprecher erreicht. So wendest du EQ auf alles an, was dein Mac wiedergibt.

## Curves Node Interface Loopback verwenden

Es wird keine zusätzliche Software und kein Treiber benötigt.

1. Wähle in [Audio Settings](audio-settings.md) das Ausgabe-Interface aus, über das du den Ton hören möchtest.
2. Verwende in deinem Preset den Node Interface Loopback (In) anstelle von Audio Input als Eingang.
3. Verbinde ihn über deine Plug-ins mit dem Node Audio Output.

Der Node empfängt bitgenauen Ton, der sonst direkt an das Ausgabe-Interface gegangen wäre. Wenn er fehlt, füge ihn über Audio I/O im Rechtsklick-Menü des Editors hinzu.

## Das gesamte Systemaudio an Curve senden

**Force macOS System Audio to loopback** im Menü Settings ist standardmäßig aktiviert. Die Option hält die Systemausgabe von macOS auf dem in Curve ausgewählten Interface. Deaktiviere sie, wenn du die Systemausgabe selbst wählen möchtest.

## Das Loopback deines Audio-Interfaces verwenden

Wenn dein Interface Hardware-Loopback bietet, kann das alternativ eine gute Wahl sein, da es die Latenz minimieren kann. Zum Beispiel mit RME TotalMix:

1. Verwende die Loopback-Funktion von TotalMix, um ein Paar Ausgangskanäle an ein freies Paar Eingangskanäle zu senden.
2. Wähle in Curve diese Eingangskanäle aus und verwende den Node Audio Input.
3. Wähle einige der Software-Playback-Kanäle als Ausgänge von Curve aus.
4. Weise diese Playback-Kanäle in TotalMix deinen Hardware-Ausgängen zu.

Verwende bei diesem Setup nicht den Node Interface Loopback (In).

## Wichtig: ungenutzte Loopback-Pins

Wenn der Node Interface Loopback (In) verbunden ist, erreicht der direkte Ton dieser Kanäle das Ausgabegerät nicht mehr. Befindet er sich in einem Preset, in dem du ihn nicht verwendest, entferne ihn oder lass seine Pins unverbunden, sonst kann es zu unerwarteter Stummschaltung kommen.

## So funktioniert die Umleitung

Die Kanäle von Stream 0, dem primären Stream des ausgewählten Ausgabe-Interfaces, werden an den Node umgeleitet, wenn seine Pins verbunden sind. Kanäle, die der Treiber des Interfaces einem anderen Stream zuweist, sind davon nicht betroffen.
