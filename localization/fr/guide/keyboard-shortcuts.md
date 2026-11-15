# Raccourcis clavier

## Ouvrir le menu de Curve depuis n’importe où

Le raccourci global ouvre le menu de Curve même lorsqu’une autre app est au premier plan. Il reste désactivé tant que vous ne l’avez pas configuré.

1. Sélectionnez Settings > Preferences..., ouvrez l’onglet Keyboard et cliquez sur Change... à côté de Global Menu Shortcut.
2. Cliquez sur Record Shortcut, puis appuyez sur une combinaison de touches comprenant Command, Option ou Control. Appuyez sur Esc pour annuler.
3. Cliquez sur Save.

Cliquez sur Reset to Default pour utiliser ⌥⇧C, ou sur Disable pour désactiver le raccourci.

## Régler le volume de Curve avec les touches de volume

Certaines interfaces audio n’ont pas de réglage de volume que macOS peut utiliser : les touches de volume du clavier ne font donc rien. Curve peut utiliser ces touches pour régler son propre volume de sortie.

1. Sélectionnez Settings > Preferences..., ouvrez l’onglet Keyboard et réglez Volume Keys sur When macOS Can't Control the Interface's Volume.
2. Quand macOS le demande, donnez l’accès à Curve dans Réglages Système > Confidentialité et sécurité > Accessibilité. Curve en a besoin pour intercepter les touches.

Curve affiche alors son propre indicateur de volume sous l’icône Son de la barre des menus, comme macOS.

- Les touches de volume parcourent 16 niveaux, comme dans macOS. Maintenez Shift et Option avec une touche de volume pour des pas plus fins.
- La touche de silence coupe et rétablit la sortie de Curve.

Curve ne prend les touches que tant que la sortie de macOS est l’interface de sortie de Curve et que macOS ne peut pas régler le volume des canaux par lesquels Curve joue. Cela comprend les interfaces sans aucun réglage de volume, et celles dont le réglage de volume dans macOS ne couvre que certains de leurs canaux, pas ceux par lesquels Curve joue. Sinon, les touches fonctionnent normalement dans macOS. Pour que les touches règlent toujours le volume de Curve, sélectionnez plutôt Always.

Par défaut, le volume de Curve s’applique à tout ce que Curve envoie en sortie, après vos plug-ins. Pour ne régler que le volume de l’audio de macOS et des autres apps, sélectionnez Control Interface Loopback Volume dans le menu situé sous Volume Keys. Les touches ne baissent alors que le nœud [Interface Loopback (In)](loopback.md), si bien que l’audio d’Audio Input et des plug-ins d’instrument dans Curve garde son niveau. Si le preset n’a aucun nœud Interface Loopback (In) connecté, les touches fonctionnent comme d’habitude dans macOS. Sélectionnez Control Curve's Output Volume pour revenir en arrière.

Le volume de Curve ne s’applique que tant que les touches le règlent. Curve le mémorise quand vous le quittez. À plein volume, l’audio n’est pas modifié.

Pour masquer l’indicateur, désactivez Show Volume Overlay dans le même menu.

## Changer de preset au clavier

Lorsque le menu de Curve est ouvert, appuyez sur une touche pour sélectionner un preset. La touche est indiquée à côté de chaque preset.

| Presets | Touches |
|---|---|
| 1 à 10 | 1 à 0 |
| 11 à 20 | Option avec 1 à 0 |
| 21 à 30 | Shift avec 1 à 0 |

Par exemple, appuyez sur le raccourci global puis sur 2 pour passer à votre deuxième preset depuis n’importe quelle app.

## Dans l’éditeur pendant MIDI Learn

| Touche | Action |
|---|---|
| Esc | Terminer MIDI Learn |
| Delete | Supprimer l’assignation de la commande sélectionnée |
