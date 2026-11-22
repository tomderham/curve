# Presets

Un preset stocke un graphe entier : ses plug-ins et leurs réglages, ses connexions, le routage des canaux audio et les assignations MIDI. Changer de preset vous permet de modifier les réglages des mêmes plug-ins, de passer à d’autres plug-ins ou de changer le routage des canaux.

## Enregistrer un preset

1. Sélectionnez Save As Preset... dans le menu de Curve.
2. Saisissez un nom et cliquez sur Save.

Les modifications ne sont pas enregistrées automatiquement. Utilisez Save As Preset... chaque fois que vous souhaitez les conserver.

## Modifier un preset existant

1. Chargez le preset en le sélectionnant dans le menu de Curve.
2. Effectuez vos modifications.
3. Sélectionnez Save As Preset..., choisissez le nom existant, puis confirmez que vous souhaitez le remplacer.

## Changer de preset

Vos presets sont répertoriés dans le menu de Curve, par ordre alphabétique, et le preset actuel est coché. Sélectionnez-en un pour y passer immédiatement. Curve ne demande pas de confirmation : enregistrez les modifications que vous voulez garder avant de changer.

Vous pouvez aussi changer de preset [au clavier](keyboard-shortcuts.md) ou [depuis un contrôleur MIDI](midi-control.md). Pour charger directement un fichier de preset, faites-le glisser sur la fenêtre de l’éditeur.

## Commencer un nouveau preset

Pour repartir d’un graphe vide, cliquez sur l’icône de menu en haut à gauche de la fenêtre de l’éditeur et sélectionnez New Preset. Tous les nœuds sont supprimés sauf Audio Input, Interface Loopback (In) et Audio Output, qui ne sont pas connectés : il n’y a donc aucun son tant que vous ne les connectez pas.

Sauf si le graphe ne contient que ces nœuds, Curve demande d’abord confirmation, car les réglages modifiés dans la fenêtre d’un plug-in ne sont pas suivis comme des modifications. Sélectionnez Save As Preset... pour garder la configuration actuelle, ou Discard and Start New.

## Scènes

Si plusieurs presets ont des parties en commun, vous pouvez en faire des scènes. Par exemple, des scènes peuvent partager la même correction d’enceintes mais utiliser des instruments ou des chemins de signal différents, ou utiliser les mêmes instruments avec une correction d’enceintes différente. Les scènes partagent leurs plug-ins et leurs connexions : il suffit de faire une modification une fois pour toutes les mettre à jour. Chaque scène reste dans le menu de Curve comme n’importe quel autre preset, et vous y passez de la même façon.

Les scènes qui partagent entre elles sont marquées de la même couleur dans le menu de Curve et dans la fenêtre Save Preset.

Passer à une autre scène est rapide : Curve ne change que ce que cette scène a en propre, sans recharger ses plug-ins. Si vous avez modifié un réglage partagé depuis le chargement, ou si un plug-in manque, la scène est chargée entièrement, et la modification est perdue, comme lorsque vous changez de preset.

Ce que chaque scène conserve pour elle-même :

- Si chaque nœud est contourné ou coupé. Un nœud contourné laisse passer son entrée telle quelle. Un nœud coupé ne laisse rien passer : il coupe le chemin sur lequel il se trouve. Voir [Couper le son d’un nœud](editor.md).
- Les réglages des nœuds réglés sur Vary by Scene. Cliquez sur un nœud avec le bouton droit et sélectionnez Vary by Scene : il affiche un petit badge de cartes empilées.
- Les fenêtres de plug-ins qui s’ouvrent avec elle (Always Open Plug-in GUI with Preset), et leur position. Un nœud ajouté à une scène ne s’ouvre pas avec les autres.

Tout le reste est partagé : les autres nœuds et leurs réglages, les connexions, le routage des canaux et les assignations MIDI. Si vous modifiez un nœud partagé dans n’importe quelle scène et l’enregistrez, toutes les scènes reçoivent la modification. Un nœud réglé sur Vary by Scene partage toujours ses canaux et ses connexions.

Pour créer une scène :

1. Chargez le preset dont la nouvelle scène doit partir.
2. Pour chaque plug-in qui doit être réglé différemment dans la nouvelle scène, cliquez dessus avec le bouton droit et sélectionnez Vary by Scene.
3. Faites les modifications pour la nouvelle scène. Pour écarter un chemin, coupez le son d’un nœud de ce chemin.
4. Sélectionnez Save As Preset..., saisissez un nouveau nom, choisissez l’option de scène et cliquez sur Save.

Avant l’enregistrement, la fenêtre Save Preset indique quelles autres scènes l’enregistrement met à jour et nomme les nœuds partagés dont les réglages y changeront. Si une modification ne concerne que cette scène, annulez, cliquez sur le nœud avec le bouton droit, sélectionnez Vary by Scene, puis enregistrez de nouveau. Chaque scène garde alors ses propres réglages pour ce nœud. Elle reste une scène, et les scènes partagent toujours tout le reste.

Pour modifier une scène, chargez-la, faites vos modifications, puis enregistrez-la sous le même nom. Enregistrer par-dessus une autre scène avec laquelle elle partage remplace cette scène, qui reste une scène. Un nœud que vous ajoutez apparaît aussi dans les autres scènes, contourné, pour qu’elles sonnent de la même façon jusqu’à ce que vous l’y activiez. Un nœud contourné laisse tout de même passer son entrée : pour qu’un nouveau chemin reste silencieux dans les autres scènes, coupez-y le son de l’un de ses nœuds.

Pour qu’une scène cesse de partager avec les autres, cliquez dessus avec le bouton droit dans la fenêtre Save Preset et sélectionnez Convert to Standalone Preset. Elle conserve ses réglages actuels.

## Renommer ou supprimer un preset

Dans la fenêtre Save Preset, cliquez avec le bouton droit sur un preset pour le renommer, le placer dans la Corbeille ou l’afficher dans le Finder.

Les presets sont des fichiers `.filtergraph` situés dans `~/Library/Application Support/Curve/Presets/`, vous pouvez donc aussi les gérer dans le Finder.
