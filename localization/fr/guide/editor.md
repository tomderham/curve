# L’éditeur

L’éditeur affiche votre preset sous forme de graphe de nœuds. L’audio circule des sorties de chaque nœud, en bas du nœud, vers les entrées du nœud suivant, en haut du nœud. Pour l’ouvrir, sélectionnez Show Editor dans le menu de Curve. Pour le fermer, cliquez sur son bouton de fermeture ou sélectionnez Hide Editor. L’audio continue de fonctionner dans les deux cas.

La barre d’état en bas de l’éditeur donne des conseils et décrit ce qui se trouve sous le pointeur. Pour un plug-in, elle indique aussi le fabricant et le format, par exemple AudioUnit ou VST3.

## Ajouter un nœud

Cliquez avec le bouton droit sur l’arrière-plan de l’éditeur, puis choisissez parmi :

- **Audio I/O :** les nœuds d’entrée audio, de sortie audio et Interface Loopback (In).
- **MIDI I/O :** les nœuds d’entrée et de sortie MIDI.
- **Curve Plugins :** [les plug-ins intégrés de Curve](curve-plugins.md).
- **Vos plug-ins analysés :** répertoriés sous ces menus, classés par fabricant.

Vous pouvez aussi faire glisser un fichier de plug-in, ou un [fichier d’EQ](eq-and-autoeq.md), sur la fenêtre de l’éditeur.

## Connecter des nœuds

Faites glisser une broche d’un nœud vers une broche d’un autre nœud. Les broches audio sont grises et les broches MIDI sont de couleur ambre. Vous pouvez connecter une sortie à plusieurs entrées et créer la chaîne de votre choix.

Les canaux consécutifs reliant les deux mêmes nœuds, comme une paire stéréo, sont représentés par une seule ligne plus épaisse qui se divise vers chaque broche. Les canaux restent séparés et rien n’est mixé. Faites glisser la ligne pour reconnecter un canal à la fois.

## Organiser les nœuds

Faites glisser un nœud pour le déplacer. Les nœuds s’alignent sur une grille, de sorte que les nœuds alignés ont des connexions droites. Maintenez la touche Option enfoncée pendant le déplacement pour placer un nœud librement. Si vous le déplacez à nouveau sans Option, il revient sur la grille.

Pour organiser tout le graphe selon le flux du signal, faites un clic droit sur l’arrière-plan et sélectionnez Tidy Up Layout. Si le graphe a besoin de plus de place, la fenêtre s’agrandit pour le contenir, jusqu’à la taille de votre écran. Undo Tidy Up restaure ensuite votre disposition et la taille de la fenêtre, jusqu’à ce que vous déplaciez de nouveau un nœud.

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
| Configure Audio I/O | Modifie les dispositions de canaux d’un plug-in, et désactive ou masque ses broches. Voir ci-dessous. |
| Audio Settings... | Affiché sur les nœuds d’entrée et de sortie audio. Ouvre Audio Settings. |
| Save Plug-in State | Enregistre les réglages du plug-in dans un fichier. |
| Load Plug-in State | Charge les réglages du plug-in depuis un fichier. |

## Configurer les entrées et sorties d’un plug-in

Cliquez avec le bouton droit sur le nœud d’un plug-in et sélectionnez Configure Audio I/O. Les broches d’un plug-in sont regroupées en bus, comme une entrée stéréo principale et une entrée sidechain. Sélectionnez un bus par son numéro, puis utilisez :

- **Channel Layout:** définit le nombre de canaux du bus. Les dispositions de plus de deux canaux sont regroupées par nombre de canaux. Un plug-in surround ou ambisonique peut traiter différemment deux dispositions ayant le même nombre de canaux : choisissez celle qui correspond à votre audio.
- **Enabled:** désactive le bus, ce qui retire ses broches. L’option est grisée si le plug-in ne peut pas fonctionner sans ce bus.
- **Shown:** masque les broches du bus dans le graphe, mais le bus continue de fonctionner. Servez-vous-en pour ranger un bus que vous n’utilisez pas et qui ne peut pas être désactivé. Masquer un bus supprime ses connexions, et une entrée masquée reçoit du silence. Shown est grisé tant que le bus est désactivé.

Si le plug-in a une broche MIDI, MIDI Port a son propre réglage Shown. Un côté qui n’a que du MIDI, comme l’entrée d’un instrument, n’a que ce réglage.

Changer une disposition, ou activer ou désactiver un bus, déconnecte toutes les broches du nœud. Les réglages sont enregistrés avec le preset. Quand vous pointez un nœud dont des broches sont masquées, la barre d’état l’indique.
