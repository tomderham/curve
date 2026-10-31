# Primeros pasos con Curve

Curve necesita macOS Sonoma 14.2 o posterior, en Apple silicon o Intel.

## Instalar Curve

1. Descarga Curve desde la [última versión publicada](https://github.com/tomderham/curve/releases).
2. Copia `Curve.app` a tu carpeta Aplicaciones y ábrela.

Curve se ejecuta en la barra de menús, así que por defecto su icono aparece allí y no en el Dock. La ventana del editor se abre la primera vez que lo usas. A partir de entonces, Curve se inicia sin ventanas abiertas.

Para acceder al menú de Curve, haz clic en el icono de la barra de menús o en el icono de menú de la parte superior izquierda de la ventana del editor.

## Buscar plug-ins

Omite este paso si solo quieres usar los plug-ins integrados de macOS, como AUNBandEQ, o [los plug-ins propios de Curve](curve-plugins.md).

1. Selecciona Settings > Plug-in Manager.
2. Haz clic en Options... abajo a la izquierda y selecciona 'Scan for new or updated AudioUnit plug-ins'. Esto puede tardar un rato, y es posible que macOS te pida permitir que Curve acceda a las carpetas que usan los plug-ins.
3. Si usas plug-ins VST3, repite el proceso con 'Scan for new or updated VST3 plug-ins'.
4. Cierra la ventana.

## Elegir tus dispositivos de audio

1. Selecciona Settings > Audio Settings.
2. Selecciona la interfaz de salida y, salvo que uses [loopback](loopback.md), la interfaz de entrada.
3. Selecciona los canales que quieras, además de la frecuencia de muestreo y el tamaño de búfer.
4. Si usas MIDI, selecciona tus interfaces MIDI.
5. Cierra la ventana.

## Crear tu primer preset

Un preset es una cadena de nodos conectados en el editor. Si el editor no está abierto, selecciona Show Editor en el menú de Curve.

1. Verás los nodos Audio Input, Interface Loopback (In) y Audio Output, con pines verdes para los canales que activaste.
2. Haz clic con el botón derecho en el fondo del editor y selecciona un plug-in. Su nodo aparece con las entradas arriba y las salidas abajo.
3. Arrastra desde un pin verde del nodo de entrada hasta un pin de entrada del plug-in.
4. Arrastra desde los pines de salida del plug-in hasta el nodo Audio Output.
5. Haz doble clic en el nodo del plug-in para abrir su ventana y ajustarlo.
6. Selecciona Save As Preset... en el menú de Curve y ponle un nombre al preset.

Si un preset no usa el nodo Interface Loopback (In), elimínalo o, al menos, deja sus pines sin conectar. De lo contrario, puede silenciar tu audio.

Ten cuidado con los enrutamientos poco habituales. Curve tiene cierta protección frente a bucles de realimentación, pero una mala configuración aún puede dañar el equipo o tu audición.

## Terminar la configuración

- Para iniciar Curve al iniciar sesión, selecciona Settings > Open at Login.
- La búsqueda automática de actualizaciones está activada por defecto. Puedes cambiarlo con Settings > Auto-Check for App Updates.

## Siguientes pasos

- [Presets](presets.md)
- [EQ para auriculares y AutoEQ](eq-and-autoeq.md)
- [Loopback y audio del sistema](loopback.md)
