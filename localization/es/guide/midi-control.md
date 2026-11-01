# Control MIDI

Un controlador o teclado MIDI puede tocar plug-ins de instrumento, cambiar parámetros de plug-ins, omitir nodos y cambiar de preset. Actívalo en las entradas MIDI de [Audio Settings](audio-settings.md).

## Elegir cómo controlar un plug-in

Hay dos formas de controlar un plug-in desde tu controlador:

- **Conectar MIDI al plug-in.** Un nodo MIDI Input envía MIDI directamente al pin de entrada ámbar del plug-in, y el plug-in decide qué hacer con él. Úsala para tocar un instrumento y para aprovechar cualquier asignación MIDI integrada en el plug-in.
- **Usar MIDI Learn de Curve.** Curve lee el controlador directamente y cambia por ti los parámetros o la omisión del plug-in. No requiere un nodo MIDI Input ni conexiones. Úsala para plug-ins sin pin de entrada ámbar y para la omisión.

MIDI Learn solo llega a los parámetros que un plug-in pone a disposición de su host. Algunos plug-ins, sobre todo los instrumentos grandes, solo ponen a disposición parte de sus controles. Si mover un control en la ventana del plug-in durante MIDI Learn no tiene efecto, Curve no puede asignar ese control.

Si un plug-in tiene un pin de entrada ámbar y su propia función de MIDI Learn, suele ser la opción más sencilla, porque normalmente llega a todos los controles. Conecta un nodo MIDI Input al plug-in y configura la asignación en la ventana del plug-in. Su manual explica cómo hacerlo.

Usa un solo método para cada mando o botón. Un control asignado con MIDI Learn de Curve no llega a los plug-ins a través del nodo MIDI Input, así que el plug-in no responderá a él.

## Tocar un plug-in de instrumento

1. Haz clic con el botón derecho en el fondo del editor y añade tu plug-in de instrumento.
2. Haz clic con el botón derecho en el fondo del editor y selecciona MIDI I/O > MIDI Input.
3. Arrastra desde el pin ámbar del nodo MIDI Input hasta el pin ámbar de la parte superior del instrumento. Los pines y las conexiones MIDI son de color ámbar, y los de audio son grises.
4. Arrastra desde los pines de salida grises del instrumento hasta el nodo Audio Output.
5. Selecciona Save As Preset... para conservar la configuración.

Los plug-ins de efecto que responden a MIDI se conectan de la misma forma. Para enviar MIDI desde un plug-in a un dispositivo externo, activa el dispositivo como salida MIDI en Audio Settings y conecta el pin de salida ámbar del plug-in a un nodo MIDI Output.

## Asignar un control con MIDI Learn

1. Haz clic con el botón derecho en cualquier nodo y selecciona MIDI Learn. El editor muestra un borde resaltado y la barra de estado te guía.
2. Elige qué quieres controlar:
   - Para un parámetro de plug-in, abre la ventana del plug-in y mueve el control.
   - Para la omisión de un nodo, haz clic en su botón de encendido.
3. Mueve un mando, fader o botón de tu controlador. La asignación se hace de inmediato y puedes seguir asignando otros controles.
4. Pulsa Esc, o selecciona MIDI Learn de nuevo, para terminar.

Las asignaciones se guardan con el preset, así que usa Save As Preset... para conservarlas.

## Eliminar una asignación

- Para eliminar la asignación del parámetro u omisión seleccionados, pulsa Delete durante MIDI Learn.
- Para eliminar todas las asignaciones de un nodo, haz clic con el botón derecho en él y selecciona Clear MIDI Mappings.

## Cómo se comportan los controles asignados

- Los mandos y faders (CC) cubren todo el rango de un parámetro.
- Las notas y los cambios de programa alternan el destino.
- Para la omisión, Curve determina si un botón CC es momentáneo, donde cada pulsación alterna la omisión, o de enclavamiento, donde la omisión sigue el estado del botón.

Los nodos con asignaciones muestran un pequeño icono de conector MIDI. Pasa el puntero sobre el nodo para ver sus asignaciones en la barra de estado.

Los mensajes MIDI asignados no se transmiten a los plug-ins a través del nodo MIDI Input. El resto del MIDI sí.

## Cambiar de preset desde un controlador

Abre Settings > MIDI Control. Estos ajustes se aplican a todos los presets:

- **Program Change Selects Preset:** desactivado por defecto. El cambio de programa 0 carga el primer preset del menú, el 1 el segundo, y así sucesivamente, en el mismo orden que los [atajos de teclas numéricas](keyboard-shortcuts.md).
- **Next Preset Control** y **Previous Preset Control:** selecciona uno y pulsa el botón de tu controlador que quieras usar.
- **Clear Preset Controls:** elimina esas dos asignaciones de botones.

## Controles de los plug-ins de Curve

| Plug-in | Controles |
|---|---|
| Gain | Ganancia y silencio |
| Pan | Panorama |
| Clipper | Techo |
| Stereo Utility | Modo y amplitud |
| Headphone Crossfeed | Cantidad |
| Audio Recorder | Inicio y parada de grabación |

Headphone Speaker Emulation, Invert Phase, Mid/Side Encoder y Mid/Side Decoder no tienen ajustes, así que asigna su omisión. Delay no se puede asignar.
