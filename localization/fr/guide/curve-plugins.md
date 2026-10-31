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
