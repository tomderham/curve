# L’éditeur

L’éditeur affiche votre preset sous forme de graphe de nœuds. L’audio circule des sorties de chaque nœud, en bas du nœud, vers les entrées du nœud suivant, en haut du nœud. Pour l’ouvrir, sélectionnez Show Editor dans le menu de Curve. Pour le fermer, cliquez sur son bouton de fermeture ou sélectionnez Hide Editor. L’audio continue de fonctionner dans les deux cas.

La barre d’état en bas de l’éditeur donne des conseils et décrit ce qui se trouve sous le pointeur.

## Ajouter un nœud

Cliquez avec le bouton droit sur l’arrière-plan de l’éditeur, puis choisissez parmi :

- **Audio I/O :** les nœuds d’entrée audio, de sortie audio et Interface Loopback (In).
- **MIDI I/O :** les nœuds d’entrée et de sortie MIDI.
- **Curve Plugins :** [les plug-ins intégrés de Curve](curve-plugins.md).
- **Vos plug-ins analysés :** répertoriés sous ces menus, classés par fabricant.

Vous pouvez aussi faire glisser un fichier de plug-in, ou un [fichier d’EQ](eq-and-autoeq.md), sur la fenêtre de l’éditeur.

## Connecter des nœuds

Faites glisser une broche d’un nœud vers une broche d’un autre nœud. Les broches audio sont vertes et les broches MIDI sont rouges. Vous pouvez connecter une sortie à plusieurs entrées et créer la chaîne de votre choix.

## Contourner un nœud

Cliquez sur le bouton d’alimentation du nœud. Cliquez de nouveau pour le réactiver. Vous pouvez aussi cliquer avec le bouton droit sur le nœud et sélectionner Toggle Bypass.

## Ouvrir la fenêtre d’un plug-in

Double-cliquez sur le nœud.

Pour que la fenêtre s’ouvre à chaque chargement du preset, même si l’éditeur est fermé, cliquez avec le bouton droit sur le nœud, sélectionnez Always Open Plug-in GUI with Preset, puis utilisez Save As Preset... La fenêtre s’ouvre à l’emplacement qu’elle occupait lors de l’enregistrement.

## Menu du nœud

Cliquez avec le bouton droit sur un nœud pour accéder aux options suivantes :

| Élément | Fonction |
|---|---|
| Delete This Node | Supprime le nœud. |
| Disconnect All Pins | Supprime toutes les connexions du nœud. |
| Toggle Bypass | Active ou désactive le contournement. |
| Rename This Node | Modifie le nom affiché sur le nœud. |
| MIDI Learn | Démarre [MIDI Learn](midi-control.md). |
| Clear MIDI Mappings | Supprime les assignations MIDI du nœud. Affiché uniquement s’il en a. |
| Always Open Plug-in GUI with Preset | Ouvre la fenêtre du plug-in à chaque chargement du preset. |
| Import EQ from AutoEQ... | Affiché sur les nœuds Parametric EQ. Voir [EQ de casque et AutoEQ](eq-and-autoeq.md). |
| Import EQ from file... | Affiché sur les nœuds Parametric EQ. Voir [EQ de casque et AutoEQ](eq-and-autoeq.md). |
| Configure Audio I/O | Modifie le nombre de broches d’entrée et de sortie d’un plug-in. |
| Audio Settings... | Affiché sur les nœuds d’entrée et de sortie audio. Ouvre Audio Settings. |
| Save Plug-in State | Enregistre les réglages du plug-in dans un fichier. |
| Load Plug-in State | Charge les réglages du plug-in depuis un fichier. |

## Modifier le nombre de canaux d’un plug-in

Si un plug-in n’a pas le nombre de broches souhaité, cliquez avec le bouton droit sur son nœud et sélectionnez Configure Audio I/O.
