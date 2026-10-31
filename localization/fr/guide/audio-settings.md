# Réglages audio et appareils

Sélectionnez Settings > Audio Settings pour choisir vos appareils audio et MIDI.

## Choisir les interfaces et les canaux

Sélectionnez une interface de sortie, ainsi qu’une interface d’entrée si votre preset en utilise une. Activez ensuite les canaux souhaités. Ils apparaissent sous forme de broches vertes sur les nœuds Audio Input et Audio Output.

Si vous utilisez le [nœud Interface Loopback (In)](loopback.md) de Curve, vous n’avez pas besoin d’interface d’entrée.

## Fréquence d’échantillonnage et taille de la mémoire tampon

C’est ici que vous sélectionnez la fréquence d’échantillonnage et la taille de la mémoire tampon. Une mémoire tampon plus petite réduit la latence, mais sollicite davantage votre Mac et peut provoquer des coupures.

**Always enforce sample rate** est désactivé par défaut. Lorsque cette option est désactivée, Curve s’adapte si l’interface audio ou une autre app modifie la fréquence d’échantillonnage du matériel. Lorsqu’elle est activée, Curve rétablit la fréquence que vous avez choisie.

## Appareils MIDI

Activez les entrées et la sortie MIDI que vous souhaitez utiliser. Utilisez un nœud MIDI Input ou MIDI Output pour les connecter aux plug-ins. Un contrôleur utilisé pour le [contrôle MIDI](midi-control.md) doit être activé ici.

## Lorsque les appareils changent

Curve gère pour vous les changements d’appareils courants :

- Il reste sur l’interface que vous avez sélectionnée. Si cette interface est déconnectée, Curve devient silencieux au lieu de basculer vers une autre sortie, comme les haut-parleurs de votre Mac.
- Il se reconnecte et rétablit l’audio lorsque l’interface revient.
- Il relance l’audio lorsque votre Mac sort de veille.

## AirPods et bascule automatique

macOS peut ne pas basculer automatiquement les AirPods tant que Curve garde une interface audio ouverte. Pour l’autoriser, activez Settings > Release audio interface when idle.

Curve libère alors l’interface après 5 secondes de silence et la rouvre lorsqu’une autre app commence à l’utiliser. Cela ne se produit que si Curve peut se réveiller de façon fiable. L’interface n’est donc pas libérée dans les cas suivants :

- Votre entrée est un microphone physique ou une entrée ligne sur un appareil distinct. Les appareils virtuels comme BlackHole ne posent pas de problème. Il en va de même pour une entrée de votre interface de sortie si le nœud Audio Input est connecté dans votre preset.
- Une autre app utilise l’appareil.
- Curve est occupé, par exemple par un enregistrement.
