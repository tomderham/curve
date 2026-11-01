# Contrôle MIDI

Un contrôleur ou un clavier MIDI peut jouer des plug-ins d’instrument, modifier des paramètres de plug-ins, contourner des nœuds et changer de preset. Activez-le sous les entrées MIDI dans [Audio Settings](audio-settings.md).

## Choisir comment contrôler un plug-in

Il existe deux façons de contrôler un plug-in depuis votre contrôleur :

- **Connecter le MIDI au plug-in.** Un nœud MIDI Input envoie le MIDI directement à la broche d’entrée ambre du plug-in, et le plug-in décide quoi en faire. Utilisez cette méthode pour jouer d’un instrument et pour profiter de toute assignation MIDI intégrée au plug-in.
- **Utiliser MIDI Learn de Curve.** Curve lit lui-même le contrôleur, puis modifie pour vous les paramètres ou le contournement du plug-in. Cela ne nécessite ni nœud MIDI Input ni connexion. Utilisez cette méthode pour les plug-ins sans broche d’entrée ambre et pour le contournement.

MIDI Learn n’atteint que les paramètres qu’un plug-in met à la disposition de son hôte. Certains plug-ins, surtout les gros instruments, ne rendent disponible qu’une partie de leurs commandes. Si le déplacement d’une commande dans la fenêtre du plug-in pendant MIDI Learn n’a aucun effet, Curve ne peut pas assigner cette commande.

Si un plug-in possède une broche d’entrée ambre et sa propre fonction MIDI Learn, c’est souvent le choix le plus simple, car elle atteint généralement toutes les commandes. Connectez un nœud MIDI Input au plug-in, puis configurez l’assignation dans la fenêtre du plug-in. Son manuel explique comment faire.

Utilisez une seule méthode pour chaque bouton. Une commande assignée avec MIDI Learn de Curve n’atteint pas les plug-ins par le nœud MIDI Input, le plug-in n’y réagira donc pas.

## Jouer d’un plug-in d’instrument

1. Cliquez avec le bouton droit sur l’arrière-plan de l’éditeur et ajoutez votre plug-in d’instrument.
2. Cliquez avec le bouton droit sur l’arrière-plan de l’éditeur et sélectionnez MIDI I/O > MIDI Input.
3. Faites glisser la broche ambre du nœud MIDI Input vers la broche ambre en haut de l’instrument. Les broches et connexions MIDI sont de couleur ambre, et les broches et connexions audio sont grises.
4. Faites glisser les broches de sortie grises de l’instrument vers le nœud Audio Output.
5. Sélectionnez Save As Preset... pour conserver la configuration.

Les plug-ins d’effet qui réagissent au MIDI se connectent de la même manière. Pour envoyer du MIDI d’un plug-in vers un appareil externe, activez l’appareil comme sortie MIDI dans Audio Settings, puis connectez la broche de sortie ambre du plug-in à un nœud MIDI Output.

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
