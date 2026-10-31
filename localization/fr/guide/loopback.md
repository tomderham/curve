# Loopback et audio du système

Le loopback vous permet d’envoyer l’audio de macOS dans Curve avant qu’il n’atteigne votre casque ou vos haut-parleurs. C’est ainsi que vous appliquez un EQ à tout ce que votre Mac lit.

## Utiliser le nœud Interface Loopback de Curve

Aucun logiciel ni pilote supplémentaire n’est nécessaire.

1. Dans [Audio Settings](audio-settings.md), sélectionnez l’interface de sortie sur laquelle vous souhaitez écouter l’audio.
2. Dans votre preset, utilisez le nœud Interface Loopback (In) comme entrée à la place d’Audio Input.
3. Connectez-le au nœud Audio Output en passant par vos plug-ins.

Le nœud reçoit, au bit près, l’audio qui serait sinon allé directement à l’interface de sortie. S’il n’est pas présent, ajoutez-le depuis Audio I/O dans le menu contextuel de l’éditeur.

## Envoyer tout l’audio du système vers Curve

**Force macOS System Audio to loopback** dans le menu Settings est activé par défaut. Cette option maintient la sortie système de macOS sur l’interface sélectionnée dans Curve. Désactivez-la si vous voulez choisir vous-même la sortie système.

## Utiliser le loopback de votre interface audio

Si votre interface dispose d’un loopback matériel, c’est aussi un bon choix, car il peut réduire la latence au minimum. Par exemple, avec RME TotalMix :

1. Utilisez la fonction Loopback de TotalMix pour envoyer une paire de canaux de sortie vers une paire de canaux d’entrée libres.
2. Dans Curve, sélectionnez ces canaux d’entrée et utilisez le nœud Audio Input.
3. Sélectionnez certains des canaux de lecture logiciels comme sorties de Curve.
4. Dans TotalMix, assignez ces canaux de lecture à vos sorties matérielles.

N’utilisez pas le nœud Interface Loopback (In) avec cette configuration.

## Important : broches de loopback inutilisées

Si le nœud Interface Loopback (In) est connecté, l’audio direct de ces canaux n’atteint plus l’appareil de sortie. S’il se trouve dans un preset où vous ne l’utilisez pas, supprimez-le ou laissez ses broches non connectées, sinon le son pourrait être coupé de manière inattendue.

## Fonctionnement de la redirection

Les canaux de Stream 0, le flux principal de l’interface de sortie sélectionnée, sont redirigés vers le nœud lorsque ses broches sont connectées. Les canaux que le pilote de l’interface attribue à un autre flux ne sont pas concernés.
