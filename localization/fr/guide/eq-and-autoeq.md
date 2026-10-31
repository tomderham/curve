# EQ de casque et AutoEQ

Curve peut configurer un Parametric EQ à partir d’un profil de casque mesuré, ce qui vous permet de corriger la réponse en fréquence de votre casque sur tout l’audio de votre Mac.

## Utiliser un profil de casque AutoEQ

Cette opération nécessite une connexion Internet.

1. Cliquez avec le bouton droit sur l’arrière-plan de l’éditeur et sélectionnez Curve Plugins > Parametric EQ (import from AutoEQ...).
2. Dans le champ de recherche, saisissez le modèle de votre casque, par exemple HD 650 ou AirPods.
3. Sélectionnez un résultat pour voir sa courbe de réponse et ses filtres.
4. Cliquez sur Use this EQ. Curve crée un nœud Parametric EQ avec ces réglages.
5. [Connectez le nœud](editor.md) à votre chaîne.
6. Sélectionnez Save As Preset... pour le conserver.

Pour mettre à jour un nœud Parametric EQ existant, cliquez dessus avec le bouton droit et sélectionnez Import EQ from AutoEQ...

Si un modèle de casque est absent, cliquez sur Sync Index pour vérifier les ajouts récents à la base de données AutoEQ.

## Affiner ou trouver d’autres profils

- Pour ajuster les courbes cibles et la signature sonore, utilisez [autoeq.app](https://autoeq.app) et exportez la courbe d’EQ.
- Si votre casque n’est pas répertorié, vous pouvez aussi essayer [squig.link](https://squig.link), qui propose des mesures de casques et d’IEM issues de la communauté.

## Importer un fichier d’EQ

Curve importe les courbes d’EQ d’AutoEQ, de Squig.link et de REW, dans des fichiers `.txt`, `.csv`, `.tsv` ou `.eq`.

- Pour créer un nœud, cliquez avec le bouton droit sur l’arrière-plan de l’éditeur et sélectionnez Curve Plugins > Parametric EQ (import from file...), ou faites glisser le fichier sur la fenêtre de l’éditeur.
- Pour mettre à jour un nœud Parametric EQ existant, cliquez dessus avec le bouton droit et sélectionnez Import EQ from file...

## Ajuster l’EQ à la main

Double-cliquez sur le nœud Parametric EQ pour ouvrir sa fenêtre.
