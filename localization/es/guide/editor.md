# El editor

El editor muestra tu preset como un grafo de nodos. El audio fluye desde las salidas de cada nodo, en la parte inferior del nodo, hasta las entradas del nodo siguiente, en la parte superior. Para abrirlo, selecciona Show Editor en el menú de Curve. Para cerrarlo, haz clic en su botón de cierre o selecciona Hide Editor. El audio sigue funcionando en ambos casos.

La barra de estado de la parte inferior del editor ofrece indicaciones y describe lo que haya bajo el puntero. En un plug-in, también muestra el fabricante y el formato, por ejemplo AudioUnit o VST3.

## Añadir un nodo

Haz clic con el botón derecho en el fondo del editor y elige entre:

- **Audio I/O:** nodos de entrada de audio, salida de audio e Interface Loopback (In).
- **MIDI I/O:** nodos de entrada y salida MIDI.
- **Curve Plugins:** [los plug-ins integrados de Curve](curve-plugins.md).
- **Tus plug-ins escaneados:** aparecen debajo de esos menús, organizados por fabricante.

También puedes arrastrar un archivo de plug-in, o un [archivo de EQ](eq-and-autoeq.md), a la ventana del editor.

## Conectar nodos

Arrastra desde un pin de un nodo hasta un pin de otro. Los pines de audio son grises y los pines MIDI son de color ámbar. Puedes conectar una salida a varias entradas y crear la cadena que quieras.

Los canales consecutivos que van entre los mismos dos nodos, como un par estéreo, se dibujan como una sola línea más gruesa que se bifurca hacia cada pin. Los canales siguen separados y no se mezcla nada. Arrastra la línea para reconectar un canal cada vez.

## Organizar los nodos

Arrastra un nodo para moverlo. Los nodos se ajustan a una cuadrícula, de modo que los nodos alineados tienen conexiones rectas. Mantén pulsada la tecla Option mientras arrastras para colocar un nodo libremente. Si vuelves a arrastrarlo sin Option, regresa a la cuadrícula.

Para organizar todo el grafo según el flujo de la señal, haz clic derecho en el fondo y selecciona Tidy Up Layout. Si el grafo necesita más espacio, la ventana crece para que quepa, hasta el tamaño de tu pantalla. Undo Tidy Up restaura después tu disposición y el tamaño de la ventana, hasta que vuelvas a mover un nodo.

## Omitir un nodo

Haz clic en el botón de encendido del nodo. Vuelve a hacer clic para reactivarlo. También puedes hacer clic con el botón derecho en el nodo y seleccionar Toggle Bypass.

## Abrir la ventana de un plug-in

Haz doble clic en el nodo.

Para que la ventana se abra cada vez que se cargue el preset, aunque el editor esté cerrado, haz clic con el botón derecho en el nodo, selecciona Always Open Plug-in GUI with Preset y usa Save As Preset... La ventana se abre en la posición que tenía al guardar.

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
| Always Open Plug-in GUI with Preset | Abre la ventana del plug-in cada vez que se carga el preset. |
| Import EQ from AutoEQ... | Se muestra en los nodos Parametric EQ. Consulta [EQ para auriculares y AutoEQ](eq-and-autoeq.md). |
| Import EQ from file... | Se muestra en los nodos Parametric EQ. Consulta [EQ para auriculares y AutoEQ](eq-and-autoeq.md). |
| Configure Audio I/O | Cambia las disposiciones de canales de un plug-in y desactiva u oculta sus pines. Consulta más abajo. |
| Audio Settings... | Se muestra en los nodos de entrada y salida de audio. Abre Audio Settings. |
| Save Plug-in State | Guarda los ajustes del plug-in en un archivo. |
| Load Plug-in State | Carga los ajustes del plug-in desde un archivo. |

## Configurar las entradas y salidas de un plug-in

Haz clic con el botón derecho en el nodo de un plug-in y selecciona Configure Audio I/O. Los pines de un plug-in se agrupan en buses, como una entrada estéreo principal y una entrada de sidechain. Selecciona un bus por su número y usa:

- **Channel Layout:** define cuántos canales tiene el bus. Las disposiciones de más de dos canales se agrupan por número de canales. Un plug-in de sonido envolvente o ambisónico puede tratar de forma distinta dos disposiciones con el mismo número de canales, así que elige la que corresponda a tu audio.
- **Enabled:** desactiva el bus, lo que elimina sus pines. Aparece atenuado si el plug-in no puede funcionar sin el bus.
- **Shown:** oculta los pines del bus en el grafo, pero el bus sigue funcionando. Úsalo para quitar de en medio un bus que no usas y que no se puede desactivar. Al ocultar un bus se eliminan sus conexiones, y una entrada oculta recibe silencio. Shown aparece atenuado mientras el bus está desactivado.

Si el plug-in tiene un pin MIDI, MIDI Port tiene su propio ajuste Shown. Un lado que solo tiene MIDI, como la entrada de un instrumento, solo tiene ese ajuste.

Cambiar una disposición o activar o desactivar un bus desconecta todos los pines del nodo. Los ajustes se guardan con el preset. Al señalar un nodo con pines ocultos, la barra de estado lo indica.
