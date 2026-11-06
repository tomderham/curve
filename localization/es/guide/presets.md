# Presets

Un preset guarda un grafo completo: sus plug-ins y ajustes, conexiones, enrutamiento de canales de audio y asignaciones MIDI. Cambiar de preset te permite modificar los ajustes de los mismos plug-ins, pasar a otros plug-ins o cambiar el enrutamiento de canales.

## Guardar un preset

1. Selecciona Save As Preset... en el menú de Curve.
2. Escribe un nombre y haz clic en Save.

Los cambios no se guardan automáticamente. Usa Save As Preset... siempre que quieras conservarlos.

## Cambiar un preset existente

1. Carga el preset seleccionándolo en el menú de Curve.
2. Haz tus cambios.
3. Selecciona Save As Preset..., elige el nombre existente y confirma que quieres sustituirlo.

## Cambiar de preset

Tus presets aparecen en el menú de Curve, por orden alfabético, y el actual tiene una marca de verificación. Selecciona uno para cambiar a él de inmediato. Curve no pregunta antes, así que guarda los cambios que quieras conservar antes de cambiar.

También puedes cambiar de preset [con el teclado](keyboard-shortcuts.md) o [desde un controlador MIDI](midi-control.md). Para cargar directamente un archivo de preset, arrástralo a la ventana del editor.

## Empezar un preset nuevo

Para empezar de nuevo con un grafo vacío, haz clic en el icono de menú de la parte superior izquierda de la ventana del editor y selecciona New Preset. Se eliminan todos los nodos excepto Audio Input, Interface Loopback (In) y Audio Output, que quedan sin conectar, así que no hay sonido hasta que los conectes.

Salvo que el grafo solo tenga esos nodos, Curve pregunta antes, porque los ajustes cambiados en la ventana de un plug-in no se registran como cambios. Selecciona Save As Preset... para conservar la configuración actual, o Discard and Start New.

## Renombrar o eliminar un preset

En la ventana Save Preset, haz clic con el botón derecho en un preset para renombrarlo, moverlo a la Papelera o mostrarlo en el Finder.

Los presets son archivos `.filtergraph` en `~/Library/Application Support/Curve/Presets/`, por lo que también puedes gestionarlos en el Finder.
