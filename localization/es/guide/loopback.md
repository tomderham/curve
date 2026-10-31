# Loopback y audio del sistema

El loopback te permite enviar el audio de macOS a través de Curve antes de que llegue a tus auriculares o altavoces. Así es como se aplica EQ a todo lo que reproduce el Mac.

## Usar el nodo Interface Loopback de Curve

No se necesita software ni controladores adicionales.

1. En [Audio Settings](audio-settings.md), selecciona la interfaz de salida por la que quieres escuchar el audio.
2. En tu preset, usa el nodo Interface Loopback (In) como entrada en lugar de Audio Input.
3. Conéctalo a través de tus plug-ins al nodo Audio Output.

El nodo recibe, con exactitud de bit, el audio que de otro modo habría ido directamente a la interfaz de salida. Si no está, añádelo desde Audio I/O en el menú contextual del editor.

## Enviar todo el audio del sistema a Curve

**Force macOS System Audio to loopback** en el menú Settings está activado por defecto. Mantiene la salida del sistema de macOS en la interfaz seleccionada en Curve. Desactívalo si quieres elegir tú mismo la salida del sistema.

## Usar el loopback de tu interfaz de audio

Si tu interfaz tiene loopback por hardware, también puede ser una buena opción, ya que puede minimizar la latencia. Por ejemplo, con RME TotalMix:

1. Usa la función Loopback de TotalMix para enviar un par de canales de salida a un par de canales de entrada libres.
2. En Curve, selecciona esos canales de entrada y usa el nodo Audio Input.
3. Selecciona algunos de los canales de reproducción por software como salidas de Curve.
4. En TotalMix, asigna esos canales de reproducción a tus salidas de hardware.

No uses el nodo Interface Loopback (In) con esta configuración.

## Importante: pines de loopback sin usar

Si el nodo Interface Loopback (In) está conectado, el audio directo de esos canales deja de llegar al dispositivo de salida. Si está en un preset en el que no lo usas, elimínalo o deja sus pines sin conectar, o el audio podría silenciarse inesperadamente.

## Cómo funciona la redirección

Los canales de Stream 0, el flujo principal de la interfaz de salida seleccionada, se redirigen al nodo cuando sus pines están conectados. Los canales que el controlador de la interfaz asigna a otro flujo no se ven afectados.
