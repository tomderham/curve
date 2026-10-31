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
