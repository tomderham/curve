# Solución de problemas

## No hay sonido o algunos canales están silenciados

- Comprueba que los nodos estén conectados hasta Audio Output.
- Si el nodo Interface Loopback (In) está en un preset pero no lo usas, elimínalo o deja sus pines sin conectar. Consulta [Loopback](loopback.md).
- Si has desconectado la interfaz seleccionada, Curve permanece en silencio hasta que vuelva, en lugar de usar otra salida. Para usar una distinta, selecciónala en Audio Settings.
- Comprueba que ningún nodo esté omitido o silenciado y que los canales correctos estén activados en [Audio Settings](audio-settings.md).

## Curve Help indica que el contenido no está disponible

Curve Help solo se abre cuando Curve está en tu carpeta Aplicaciones. Muévelo allí y vuelve a intentarlo.

## No encuentro Curve después de abrirlo

Curve se ejecuta en la barra de menús y normalmente no abre ninguna ventana. Haz clic en su icono de la barra de menús y selecciona Show Editor. Para que también aparezca en el Dock, activa Settings > Show in Dock.

## Mis cambios en un preset han desaparecido

Curve no guarda los cambios automáticamente. Usa Save As Preset... y sustituye el preset existente. Consulta [Presets](presets.md).

## Falta un plug-in en el menú

Selecciona Settings > Plug-in Manager y ejecuta la búsqueda para el formato de ese plug-in. Consulta [Primeros pasos](getting-started.md).

## Un plug-in tiene un número incorrecto de pines

Haz clic con el botón derecho en su nodo y selecciona Configure Audio I/O.

## La frecuencia de muestreo no deja de cambiar

Puede que otra app esté cambiando la frecuencia de muestreo del hardware. Para que Curve la fuerce de nuevo, activa Always enforce sample rate en [Audio Settings](audio-settings.md).

## Los AirPods no cambian automáticamente

Activa Settings > Release audio interface when idle. Consulta [Ajustes de audio y dispositivos](audio-settings.md).

## Curve no responde a mi controlador MIDI

Comprueba que el controlador esté activado en las entradas MIDI de Audio Settings y que la asignación se haya guardado con el preset. Consulta [Control MIDI](midi-control.md).
