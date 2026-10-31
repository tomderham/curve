# Prise en main de Curve

Curve nécessite macOS Sonoma 14.2 ou version ultérieure, sur Apple silicon ou Intel.

## Installer Curve

1. Téléchargez Curve depuis la [dernière version](https://github.com/tomderham/curve/releases).
2. Copiez `Curve.app` dans votre dossier Applications et ouvrez-la.

Curve s’exécute dans la barre des menus : par défaut, son icône y apparaît plutôt que dans le Dock. La fenêtre de l’éditeur s’ouvre à la première utilisation. Ensuite, Curve démarre sans aucune fenêtre ouverte.

Pour accéder au menu de Curve, cliquez sur son icône dans la barre des menus, ou sur l’icône de menu en haut à gauche de la fenêtre de l’éditeur.

## Rechercher des plug-ins

Ignorez cette étape si vous souhaitez uniquement utiliser les plug-ins intégrés de macOS, comme AUNBandEQ, ou [les plug-ins de Curve](curve-plugins.md).

1. Sélectionnez Settings > Plug-in Manager.
2. Cliquez sur Options... en bas à gauche, puis sélectionnez 'Scan for new or updated AudioUnit plug-ins'. Cela peut prendre un certain temps, et macOS peut vous demander d’autoriser Curve à accéder aux dossiers utilisés par les plug-ins.
3. Si vous utilisez des plug-ins VST3, répétez l’opération avec 'Scan for new or updated VST3 plug-ins'.
4. Fermez la fenêtre.

## Choisir vos appareils audio

1. Sélectionnez Settings > Audio Settings.
2. Sélectionnez l’interface de sortie et, sauf si vous utilisez le [loopback](loopback.md), l’interface d’entrée.
3. Sélectionnez les canaux souhaités, ainsi que la fréquence d’échantillonnage et la taille de la mémoire tampon.
4. Si vous utilisez le MIDI, sélectionnez vos interfaces MIDI.
5. Fermez la fenêtre.

## Créer votre premier preset

Un preset est une chaîne de nœuds connectés dans l’éditeur. Si l’éditeur n’est pas ouvert, sélectionnez Show Editor dans le menu de Curve.

1. Vous voyez les nœuds Audio Input, Interface Loopback (In) et Audio Output, avec des broches vertes pour les canaux activés.
2. Cliquez avec le bouton droit sur l’arrière-plan de l’éditeur et sélectionnez un plug-in. Son nœud apparaît avec les entrées en haut et les sorties en bas.
3. Faites glisser une broche verte du nœud d’entrée vers une broche d’entrée du plug-in.
4. Faites glisser les broches de sortie du plug-in vers le nœud Audio Output.
5. Double-cliquez sur le nœud du plug-in pour ouvrir sa fenêtre et le régler.
6. Sélectionnez Save As Preset... dans le menu de Curve et donnez un nom au preset.

Si un preset n’utilise pas le nœud Interface Loopback (In), supprimez-le ou laissez au moins ses broches non connectées. Sinon, il peut couper votre audio.

Soyez prudent avec les routages inhabituels. Curve offre une certaine protection contre les boucles de rétroaction, mais une mauvaise configuration peut tout de même endommager votre matériel ou votre audition.

## Terminer la configuration

- Pour lancer Curve à l’ouverture de session, sélectionnez Settings > Open at Login.
- La recherche automatique de mises à jour est activée par défaut. Vous pouvez la modifier avec Settings > Auto-Check for App Updates.

## Étapes suivantes

- [Presets](presets.md)
- [EQ de casque et AutoEQ](eq-and-autoeq.md)
- [Loopback et audio du système](loopback.md)
