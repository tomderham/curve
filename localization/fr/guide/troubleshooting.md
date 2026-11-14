# Dépannage

## Il n’y a aucun son, ou certains canaux sont muets

- Vérifiez que les nœuds sont connectés jusqu’à Audio Output.
- Si le nœud Interface Loopback (In) se trouve dans un preset mais que vous ne l’utilisez pas, supprimez-le ou laissez ses broches non connectées. Voir [Loopback](loopback.md).
- Si vous avez débranché l’interface sélectionnée, Curve reste silencieux jusqu’à son retour, au lieu d’utiliser une autre sortie. Pour en utiliser une autre, sélectionnez-la dans Audio Settings.
- Vérifiez qu’aucun nœud n’est contourné ou en sourdine, et que les bons canaux sont activés dans [Audio Settings](audio-settings.md).

## Curve Help indique que le contenu n’est pas disponible

Curve Help ne s’ouvre que si Curve se trouve dans votre dossier Applications. Déplacez-le à cet endroit et réessayez.

## Je ne trouve plus Curve après l’avoir ouvert

Curve s’exécute dans la barre des menus et n’ouvre normalement aucune fenêtre. Cliquez sur son icône dans la barre des menus, puis sélectionnez Show Editor. Pour l’afficher aussi dans le Dock, activez Settings > Show in Dock.

## Mes modifications d’un preset ont disparu

Curve n’enregistre pas les modifications automatiquement. Utilisez Save As Preset... et remplacez le preset existant. Voir [Presets](presets.md).

## Un plug-in est absent du menu

Sélectionnez Settings > Plug-in Manager et lancez la recherche pour le format de ce plug-in. Voir [Prise en main](getting-started.md).

S’il apparaît en rouge en bas de la liste du Plug-in Manager, avec Scan failed comme catégorie, le plug-in a planté ou a mis plus de 30 secondes à se charger, et les recherches l’ignorent désormais. Cliquez dessus avec le bouton droit et sélectionnez Retry Scan, ou sélectionnez Remove from List pour le retirer.

## Un plug-in n’a pas le bon nombre de broches

Cliquez avec le bouton droit sur son nœud et sélectionnez Configure Audio I/O.

## La fréquence d’échantillonnage change sans cesse

Une autre app modifie peut-être la fréquence d’échantillonnage du matériel. Pour que Curve la rétablisse, activez Always enforce sample rate dans [Audio Settings](audio-settings.md).

## Les AirPods ne basculent pas automatiquement

Activez Settings > Release audio interface when idle. Voir [Réglages audio et appareils](audio-settings.md).

## Les touches de volume ne changent pas le volume de Curve

- Vérifiez que Settings > Volume Keys n’est pas réglé sur Off.
- Si le menu affiche Allow Accessibility Access..., sélectionnez-le et activez Curve dans Accessibilité. Si Curve y est déjà activé, désactivez-le puis réactivez-le.
- Avec When macOS Can't Control the Interface's Volume, macOS garde les touches s’il peut régler le volume des canaux par lesquels Curve joue, ou si la sortie de macOS n’est pas l’interface de sortie de Curve. Activez Force macOS System Audio to loopback ou sélectionnez Always. Voir [Raccourcis clavier](keyboard-shortcuts.md).

## Curve ne réagit pas à mon contrôleur MIDI

Vérifiez que le contrôleur est activé sous les entrées MIDI dans Audio Settings et que l’assignation a bien été enregistrée avec le preset. Voir [Contrôle MIDI](midi-control.md).
