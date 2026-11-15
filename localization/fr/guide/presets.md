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

Si plusieurs presets sont identiques à quelques réglages près, comme un nœud contourné ou un plug-in réglé autrement, vous pouvez en faire des scènes. Les scènes partagent leurs plug-ins et leurs connexions : il suffit d’apporter une modification une fois pour les mettre toutes à jour. Chaque scène figure dans le menu de Curve comme n’importe quel autre preset, et vous y passez de la même façon. Dans la fenêtre Save Preset, les scènes qui partagent entre elles ont la même couleur, et en sélectionner une met les autres en évidence.

Ce que chaque scène garde pour elle :

- Si chaque nœud est contourné.
- Les réglages des nœuds réglés sur Vary by Scene. Faites un clic droit sur un nœud et sélectionnez Vary by Scene : il affiche un petit badge de cartes empilées.

Tout le reste est partagé : les autres nœuds et leurs réglages, les connexions, le routage des canaux et les assignations MIDI. Modifiez un nœud partagé dans n’importe quelle scène, enregistrez-la, et toutes les scènes reçoivent la modification. Un nœud réglé sur Vary by Scene partage tout de même ses canaux et ses connexions.

Pour créer une scène :

1. Chargez le preset dont la nouvelle scène doit partir.
2. Pour chaque plug-in qui doit être réglé autrement dans la nouvelle scène, faites un clic droit dessus et sélectionnez Vary by Scene.
3. Faites les modifications pour la nouvelle scène.
4. Sélectionnez Save As Preset..., saisissez un nouveau nom, choisissez l’option de scène et cliquez sur Save.

Avant l’enregistrement, la fenêtre Save Preset indique quelles autres scènes l’enregistrement met à jour et nomme les nœuds partagés dont les réglages y changeront. Si un réglage ne concerne que cette scène, annulez, réglez ce nœud sur Vary by Scene et enregistrez à nouveau.

Pour modifier une scène, chargez-la, faites vos modifications, puis enregistrez-la sous le même nom. Enregistrer par-dessus une autre scène avec laquelle elle partage remplace cette scène, qui reste une scène. Un nœud que vous ajoutez apparaît aussi dans les autres scènes, contourné, pour qu’elles sonnent pareil jusqu’à ce que vous l’y activiez.

Pour qu’une scène cesse de partager avec les autres, faites un clic droit dessus dans la fenêtre Save Preset et sélectionnez Convert to Standalone Preset. Elle garde ses réglages actuels.

## Renommer ou supprimer un preset

Dans la fenêtre Save Preset, cliquez avec le bouton droit sur un preset pour le renommer, le placer dans la Corbeille ou l’afficher dans le Finder.

Les presets sont des fichiers `.filtergraph` situés dans `~/Library/Application Support/Curve/Presets/`, vous pouvez donc aussi les gérer dans le Finder.
