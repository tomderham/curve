# Contrôle MIDI

Un contrôleur ou un clavier MIDI peut jouer des plug-ins d’instrument, modifier des paramètres de plug-ins, contourner des nœuds et changer de preset. Activez-le sous les entrées MIDI dans [Audio Settings](audio-settings.md).

Pour jouer d’un instrument ou envoyer du MIDI à tout autre plug-in, un nœud MIDI Input doit être connecté à ce plug-in. Le contrôle des paramètres, du contournement et des presets avec MIDI Learn ne nécessite ni nœud MIDI Input ni connexion dans le preset.

## Jouer d’un plug-in d’instrument

1. Cliquez avec le bouton droit sur l’arrière-plan de l’éditeur et ajoutez votre plug-in d’instrument.
2. Cliquez avec le bouton droit sur l’arrière-plan de l’éditeur et sélectionnez MIDI I/O > MIDI Input.
3. Faites glisser la broche rouge du nœud MIDI Input vers la broche rouge en haut de l’instrument. Les broches et connexions MIDI sont rouges, et les broches et connexions audio sont vertes.
4. Faites glisser les broches de sortie vertes de l’instrument vers le nœud Audio Output.
5. Sélectionnez Save As Preset... pour conserver la configuration.

Les plug-ins d’effet qui réagissent au MIDI se connectent de la même manière. Pour envoyer du MIDI d’un plug-in vers un appareil externe, activez l’appareil comme sortie MIDI dans Audio Settings, puis connectez la broche de sortie rouge du plug-in à un nœud MIDI Output.

## Assigner une commande avec MIDI Learn

1. Cliquez avec le bouton droit sur un nœud et sélectionnez MIDI Learn. L’éditeur reçoit une bordure en surbrillance, et la barre d’état vous guide.
2. Choisissez ce que vous voulez contrôler :
   - Pour un paramètre de plug-in, ouvrez la fenêtre du plug-in et déplacez la commande.
   - Pour le contournement d’un nœud, cliquez sur son bouton d’alimentation.
3. Déplacez un bouton rotatif, un fader ou un bouton de votre contrôleur. L’assignation est faite immédiatement, et vous pouvez continuer à assigner d’autres commandes.
4. Appuyez sur Esc, ou sélectionnez de nouveau MIDI Learn, pour terminer.

Les assignations sont enregistrées avec le preset : utilisez donc Save As Preset... pour les conserver.

## Supprimer une assignation

- Pour supprimer l’assignation du paramètre ou du contournement sélectionné, appuyez sur Delete pendant MIDI Learn.
- Pour supprimer toutes les assignations d’un nœud, cliquez dessus avec le bouton droit et sélectionnez Clear MIDI Mappings.

## Comportement des commandes assignées

- Les boutons rotatifs et les faders (CC) couvrent toute la plage d’un paramètre.
- Les notes et les changements de programme basculent la cible.
- Pour le contournement, Curve détermine si un bouton CC est momentané, chaque pression basculant le contournement, ou à verrouillage, le contournement suivant l’état du bouton.

Les nœuds comportant des assignations affichent une petite icône de prise MIDI. Survolez le nœud pour voir ses assignations dans la barre d’état.

Les messages MIDI assignés ne sont pas transmis aux plug-ins par le nœud MIDI Input. Tous les autres messages MIDI le sont.

## Changer de preset depuis un contrôleur

Ouvrez Settings > MIDI Control. Ces réglages s’appliquent à tous les presets :

- **Program Change Selects Preset :** désactivé par défaut. Le changement de programme 0 charge le premier preset du menu, 1 le deuxième, et ainsi de suite, dans le même ordre que les [raccourcis des touches numériques](keyboard-shortcuts.md).
- **Next Preset Control** et **Previous Preset Control :** sélectionnez l’une de ces options, puis appuyez sur le bouton de votre contrôleur que vous souhaitez utiliser.
- **Clear Preset Controls :** supprime ces deux assignations de boutons.

## Commandes des plug-ins de Curve

| Plug-in | Commandes |
|---|---|
| Gain | Gain et sourdine |
| Pan | Pan |
| Clipper | Plafond |
| Stereo Utility | Mode et largeur |
| Headphone Crossfeed | Quantité |
| Audio Recorder | Démarrage et arrêt de l’enregistrement |

Headphone Speaker Emulation, Invert Phase, Mid/Side Encoder et Mid/Side Decoder n’ont aucun réglage : assignez plutôt leur contournement. Delay n’est pas assignable.
