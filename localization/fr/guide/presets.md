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

## Renommer ou supprimer un preset

Dans la fenêtre Save Preset, cliquez avec le bouton droit sur un preset pour le renommer, le placer dans la Corbeille ou l’afficher dans le Finder.

Les presets sont des fichiers `.filtergraph` situés dans `~/Library/Application Support/Curve/Presets/`, vous pouvez donc aussi les gérer dans le Finder.
