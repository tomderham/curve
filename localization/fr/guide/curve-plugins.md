# Curve Plugins

Curve comprend les plug-ins suivants. Pour ajouter un plug-in à un preset, cliquez avec le bouton droit sur l’arrière-plan de l’éditeur et sélectionnez Curve Plugins.

## EQ

- **Parametric EQ :** un raccourci vers AUNBandEQ, l’EQ paramétrique multibande d’Apple, souvent utilisé pour l’étalonnage. Multicanal. Le même menu permet d’[importer un profil de casque ou un fichier d’EQ](eq-and-autoeq.md).

## Traitement pour casque

- **Headphone Crossfeed :** crossfeed pour une image stéréo plus naturelle au casque, avec une quantité réglable. Stéréo.
- **Headphone Speaker Emulation :** émule au casque des enceintes de studio de champ moyen, crossfeed compris. Stéréo.

## Niveau et sécurité

- **Gain :** réglage du volume de -60 dB à +60 dB, avec un bouton de sourdine. Multicanal.
- **Clipper :** écrête chaque canal à un plafond compris entre -50 et 0 dBFS, comme protection contre les accentuations d’EQ, avec un indicateur d’écrêtage. Il n’apporte une protection que si le plafond est inférieur à 0 dBFS. Aucune latence. Multicanal.
- **Meter :** affiche l'une de quatre vues de ce que vous y connectez. Levels affiche le RMS, moyenné sur environ 300 ms (une sinusoïde se lit 3 dB sous sa crête), le true peak en dBTP et la corrélation stéréo, où +1 correspond au mono et -1 à une opposition de phase. VU affiche des vumètres à aiguille avec un temps de montée de 300 ms, et vous pouvez choisir le niveau qui indique 0 VU : -20, -18, -14 ou -10 dBFS. Loudness affiche les LUFS momentané, court terme et intégré (ITU-R BS.1770 / EBU R128), avec l'historique de la dernière minute. Spectrum affiche la puissance combinée des deux canaux, et L/R les superpose séparément. Clip s'allume lorsqu'un échantillon dépasse 0 dBFS et reste allumé jusqu'à ce que vous cliquiez dessus. Il ne mesure que lorsque sa fenêtre est ouverte, et la sonie intégrée repart de zéro à chaque ouverture de la fenêtre. Par défaut, il n'a pas de sorties : connectez-le donc en dérivation depuis n'importe quelle broche. Pour l'insérer dans la chaîne, ajoutez des sorties avec Configure Audio I/O ; l'audio le traverse alors sans modification. Aucune latence. Mono ou stéréo.
- **Invert Phase :** inverse la polarité de chaque canal. Multicanal.

## Routage et stéréo

- **Pan :** place une entrée mono dans la sortie stéréo, avec une loi de panoramique de 0, -3, -4,5 ou -6 dB au centre. Avec une entrée stéréo, définie à l’aide de Configure Audio I/O, il agit comme une commande de balance.
- **Stereo Utility :** inversion gauche/droite, somme mono, gauche ou droite seule, et largeur stéréo de 0 à 200 %. Stéréo.
- **Mid/Side Encoder :** convertit la gauche et la droite en Mid sur la première broche et en Side sur la seconde. Stéréo.
- **Mid/Side Decoder :** reconvertit Mid et Side en gauche et droite. Placez un encodeur et un décodeur de part et d’autre du traitement séparé des deux broches. L’aller-retour est exact au bit près. Stéréo.
- **Delay :** retard par échantillons entiers allant jusqu’à 2 secondes, affiché en échantillons, millisecondes ou centimètres, pour aligner des enceintes ou synchroniser l’audio avec la vidéo. Le modifier pendant la lecture peut provoquer un bref clic. Multicanal.

## Enregistrement

- **Audio Recorder :** enregistre des fichiers WAV sur votre Bureau. Multicanal.

## Commandes assignables avec MIDI Learn

Consultez [Contrôle MIDI](midi-control.md) pour savoir ce que chaque plug-in expose.
