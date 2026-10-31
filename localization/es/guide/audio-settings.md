# Ajustes de audio y dispositivos

Selecciona Settings > Audio Settings para elegir tus dispositivos de audio y MIDI.

## Elegir interfaces y canales

Selecciona una interfaz de salida y, si tu preset la usa, una interfaz de entrada. Después activa los canales que quieras. Aparecen como pines verdes en los nodos Audio Input y Audio Output.

Si usas el [nodo Interface Loopback (In)](loopback.md) de Curve, no necesitas una interfaz de entrada.

## Frecuencia de muestreo y tamaño de búfer

Aquí se seleccionan la frecuencia de muestreo y el tamaño de búfer. Un búfer más pequeño significa menos latencia, pero supone más carga para el Mac y puede provocar cortes.

**Always enforce sample rate** está desactivado por defecto. Si está desactivado, Curve se adapta cuando la interfaz de audio u otra app cambia la frecuencia de muestreo del hardware. Si está activado, Curve fuerza de nuevo la frecuencia que elegiste.

## Dispositivos MIDI

Activa las entradas y la salida MIDI que quieras usar. Usa un nodo MIDI Input o MIDI Output para conectarlas a los plug-ins. Un controlador usado para el [control MIDI](midi-control.md) debe estar activado aquí.

## Cuando cambian los dispositivos

Curve se encarga de los cambios de dispositivo más habituales:

- Se mantiene en la interfaz que seleccionaste. Si esa interfaz se desconecta, Curve queda en silencio en lugar de pasar a otra salida, como los altavoces del Mac.
- Se reconecta y restablece el audio cuando la interfaz vuelve.
- Reinicia el audio cuando el Mac sale del reposo.

## AirPods y cambio automático

Es posible que macOS no cambie los AirPods automáticamente mientras Curve tenga abierta una interfaz de audio. Para permitirlo, activa Settings > Release audio interface when idle.

Curve libera entonces la interfaz tras 5 segundos de silencio y la vuelve a abrir cuando otra app empieza a usarla. Esto solo ocurre si Curve puede reactivarse de forma fiable, por lo que la interfaz no se libera en estos casos:

- Tu entrada es un micrófono físico o una entrada de línea en un dispositivo independiente. Los dispositivos virtuales como BlackHole no presentan problemas. Lo mismo ocurre con una entrada de tu interfaz de salida si el nodo Audio Input está conectado en tu preset.
- Otra app está usando el dispositivo.
- Curve está ocupado, por ejemplo, grabando.
