# Curve Plugins

Curve incluye estos plug-ins. Para añadir un plug-in a un preset, haz clic con el botón derecho en el fondo del editor y selecciona Curve Plugins.

## EQ

- **Parametric EQ:** un acceso directo a AUNBandEQ, el EQ paramétrico multibanda de Apple, que suele usarse para calibrar. Multicanal. El mismo menú permite [importar un perfil de auriculares o un archivo de EQ](eq-and-autoeq.md).

## Procesamiento para auriculares

- **Headphone Crossfeed:** crossfeed para una imagen estéreo más natural con auriculares, con cantidad ajustable. Estéreo.
- **Headphone Speaker Emulation:** emula monitores de estudio de campo medio en auriculares, incluido el crossfeed. Estéreo.

## Nivel y seguridad

- **Gain:** ajuste de volumen de -60 dB a +60 dB, con botón de silencio. Multicanal.
- **Clipper:** recorta cada canal en un techo de -50 a 0 dBFS, como protección frente a los realces de EQ, con indicador de recorte. Solo añade protección cuando el techo está por debajo de 0 dBFS. Sin latencia. Multicanal.
- **Meter:** muestra una de cuatro vistas de lo que conectes a él. Levels muestra RMS, promediado durante unos 300 ms (una onda senoidal lee 3 dB por debajo de su pico), true peak en dBTP y correlación estéreo, donde +1 es mono y -1 está fuera de fase. VU muestra medidores de aguja con un tiempo de subida de 300 ms, y puedes elegir el nivel que lee 0 VU: -20, -18, -14 o -10 dBFS. Loudness muestra LUFS momentáneo, a corto plazo e integrado (ITU-R BS.1770 / EBU R128), con el historial del último minuto. Spectrum muestra la potencia combinada de ambos canales, y L/R los superpone por separado. Clip se enciende cuando una muestra supera 0 dBFS y sigue encendido hasta que haces clic en él. Solo mide mientras su ventana está abierta, y la sonoridad integrada se reinicia cada vez que se abre la ventana. Por defecto no tiene salidas, así que conéctalo como derivación desde cualquier pin. Para ponerlo en línea, añade salidas con Configure Audio I/O, y el audio pasará sin cambios. Sin latencia. Mono o estéreo.
- **Invert Phase:** invierte la polaridad de cada canal. Multicanal.

## Enrutamiento y estéreo

- **Pan:** coloca una entrada mono en la salida estéreo, con una ley de panorama de 0, -3, -4,5 o -6 dB en el centro. Con una entrada estéreo, definida mediante Configure Audio I/O, actúa como control de balance.
- **Stereo Utility:** intercambia izquierda y derecha, suma mono, solo izquierda o derecha, y amplitud estéreo de 0 a 200 %. Estéreo.
- **Mid/Side Encoder:** convierte izquierda y derecha en Mid en el primer pin y Side en el segundo. Estéreo.
- **Mid/Side Decoder:** convierte Mid y Side de nuevo en izquierda y derecha. Coloca un codificador y un decodificador alrededor del procesamiento independiente de los dos pines. El recorrido completo es exacto al bit. Estéreo.
- **Delay:** retardo por muestras enteras de hasta 2 segundos, mostrado en muestras, milisegundos o centímetros, para alinear altavoces o sincronizar audio con vídeo. Cambiarlo durante la reproducción puede causar un breve clic. Multicanal.

## Grabación

- **Audio Recorder:** graba archivos WAV en tu Escritorio. Multicanal.

## Controles asignables con MIDI Learn

Consulta [Control MIDI](midi-control.md) para ver lo que expone cada plug-in.
