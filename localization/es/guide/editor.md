# El editor

El editor muestra tu preset como un grafo de nodos. El audio fluye desde las salidas de cada nodo, en la parte inferior del nodo, hasta las entradas del nodo siguiente, en la parte superior. Para abrirlo, selecciona Show Editor en el menú de Curve. Para cerrarlo, haz clic en su botón de cierre o selecciona Hide Editor. El audio sigue funcionando en ambos casos.

La barra de estado de la parte inferior del editor ofrece indicaciones y describe lo que haya bajo el puntero.

## Añadir un nodo

Haz clic con el botón derecho en el fondo del editor y elige entre:

- **Audio I/O:** nodos de entrada de audio, salida de audio e Interface Loopback (In).
- **MIDI I/O:** nodos de entrada y salida MIDI.
- **Curve Plugins:** [los plug-ins integrados de Curve](curve-plugins.md).
- **Tus plug-ins escaneados:** aparecen debajo de esos menús, organizados por fabricante.

También puedes arrastrar un archivo de plug-in, o un [archivo de EQ](eq-and-autoeq.md), a la ventana del editor.

## Conectar nodos

Arrastra desde un pin de un nodo hasta un pin de otro. Los pines de audio son verdes y los pines MIDI son rojos. Puedes conectar una salida a varias entradas y crear la cadena que quieras.

## Omitir un nodo

Haz clic en el botón de encendido del nodo. Vuelve a hacer clic para reactivarlo. También puedes hacer clic con el botón derecho en el nodo y seleccionar Toggle Bypass.

## Abrir la ventana de un plug-in

Haz doble clic en el nodo, o haz clic con el botón derecho y selecciona Show Plug-in GUI.

## Menú del nodo

Haz clic con el botón derecho en un nodo para ver estas opciones:

| Elemento | Función |
|---|---|
| Delete This Node | Elimina el nodo. |
| Disconnect All Pins | Elimina todas las conexiones del nodo. |
| Toggle Bypass | Activa o desactiva la omisión. |
| Rename This Node | Cambia el nombre que se muestra en el nodo. |
| MIDI Learn | Inicia [MIDI Learn](midi-control.md). |
| Clear MIDI Mappings | Elimina las asignaciones MIDI del nodo. Solo se muestra si tiene alguna. |
| Show Plug-in GUI | Abre la ventana del plug-in. |
| Import EQ from AutoEQ... | Se muestra en los nodos Parametric EQ. Consulta [EQ para auriculares y AutoEQ](eq-and-autoeq.md). |
| Import EQ from file... | Se muestra en los nodos Parametric EQ. Consulta [EQ para auriculares y AutoEQ](eq-and-autoeq.md). |
| Configure Audio I/O | Cambia el número de pines de entrada y salida de un plug-in. |
| Audio Settings... | Se muestra en los nodos de entrada y salida de audio. Abre Audio Settings. |
| Save Plug-in State | Guarda los ajustes del plug-in en un archivo. |
| Load Plug-in State | Carga los ajustes del plug-in desde un archivo. |

## Cambiar el número de canales de un plug-in

Si un plug-in no tiene el número de pines que quieres, haz clic con el botón derecho en su nodo y selecciona Configure Audio I/O.
