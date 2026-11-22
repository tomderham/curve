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

## Escenas

Si varios presets tienen partes en común, puedes convertirlos en escenas. Por ejemplo, las escenas pueden compartir la misma corrección de altavoces pero usar instrumentos o rutas de señal distintos, o usar los mismos instrumentos con distinta corrección de altavoces. Las escenas comparten sus plug-ins y conexiones, así que solo tienes que hacer un cambio una vez para actualizarlas todas. Cada escena sigue en el menú de Curve como cualquier otro preset, y cambias a ella de la misma forma.

Las escenas que comparten entre sí se marcan con el mismo color en el menú de Curve y en la ventana Save Preset.

Cambiar a otra escena es rápido: Curve solo cambia lo que esa escena tiene propio, sin volver a cargar sus plug-ins. Si has cambiado un ajuste compartido desde que la cargaste, o falta un plug-in, la escena se carga por completo y el cambio se pierde, igual que al cambiar de preset.

Lo que cada escena guarda para sí:

- Si cada nodo está omitido o silenciado. Un nodo omitido deja pasar su entrada sin cambios. Un nodo silenciado no deja pasar nada, así que corta la ruta en la que está. Consulta [Silenciar un nodo](editor.md).
- Los ajustes de los nodos configurados con Vary by Scene. Haz clic con el botón derecho en un nodo y selecciona Vary by Scene; mostrará una pequeña insignia de tarjetas apiladas.

Todo lo demás se comparte: los demás nodos y sus ajustes, las conexiones, el enrutamiento de canales y las asignaciones MIDI. Si cambias un nodo compartido en cualquier escena y la guardas, todas las escenas reciben el cambio. Un nodo con Vary by Scene sigue compartiendo sus canales y conexiones.

Para crear una escena:

1. Carga el preset del que debe partir la nueva escena.
2. Para cada plug-in que deba ajustarse de otra forma en la nueva escena, haz clic con el botón derecho en él y selecciona Vary by Scene.
3. Haz los cambios para la nueva escena. Para dejar fuera una ruta, silencia un nodo de esa ruta.
4. Selecciona Save As Preset..., escribe un nombre nuevo, elige la opción de escena y haz clic en Save.

Antes de guardar, la ventana Save Preset indica qué otras escenas se actualizan y nombra los nodos compartidos cuyos ajustes cambiarán en ellas. Si un cambio es solo para esta escena, cancela, haz clic con el botón derecho en el nodo, selecciona Vary by Scene y vuelve a guardar. Así cada escena conserva sus propios ajustes para ese nodo. Sigue siendo una escena, y las escenas siguen compartiendo todo lo demás.

Para cambiar una escena, cárgala, haz los cambios y guárdala con el mismo nombre. Guardar sobre otra escena con la que comparte reemplaza esa escena, y sigue siendo una escena. Un nodo que añadas también aparece en las demás escenas, omitido, para que suenen igual hasta que lo actives allí. Un nodo omitido sigue dejando pasar su entrada, así que para que una ruta nueva no suene en las demás escenas, silencia allí uno de sus nodos.

Para que una escena deje de compartir con las demás, haz clic con el botón derecho en ella en la ventana Save Preset y selecciona Convert to Standalone Preset. Conserva sus ajustes actuales.

## Renombrar o eliminar un preset

En la ventana Save Preset, haz clic con el botón derecho en un preset para renombrarlo, moverlo a la Papelera o mostrarlo en el Finder.

Los presets son archivos `.filtergraph` en `~/Library/Application Support/Curve/Presets/`, por lo que también puedes gestionarlos en el Finder.
