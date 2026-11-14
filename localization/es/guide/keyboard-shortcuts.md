# Atajos de teclado

## Abrir el menú de Curve desde cualquier lugar

El atajo global abre el menú de Curve incluso cuando otra app está en primer plano. Está desactivado hasta que lo configures.

1. Selecciona Settings > Global Menu Shortcut.
2. Haz clic en Record Shortcut y pulsa una combinación de teclas que incluya Command, Option o Control. Pulsa Esc para cancelar.
3. Haz clic en Save.

Haz clic en Reset to Default para usar ⌥⇧C, o en Disable para desactivar el atajo.

## Ajustar el volumen de Curve con las teclas de volumen

Algunas interfaces de audio no tienen un control de volumen que macOS pueda usar, así que las teclas de volumen del teclado no hacen nada. Curve puede usar las teclas para ajustar su propio volumen de salida.

1. Selecciona Settings > Volume Keys > When macOS Can't Control the Interface's Volume.
2. Cuando macOS lo pida, da acceso a Curve en Ajustes del Sistema > Privacidad y seguridad > Accesibilidad. Curve lo necesita para captar las teclas.

Curve muestra entonces su propio indicador de volumen bajo el icono de Sonido de la barra de menús, como hace macOS.

- Las teclas de volumen recorren 16 niveles, como en macOS. Mantén pulsadas Shift y Option con una tecla de volumen para pasos más finos.
- La tecla de silencio apaga y enciende la salida de Curve.

Curve solo toma las teclas mientras la salida de macOS sea la interfaz de salida de Curve y macOS no pueda ajustar el volumen de los canales por los que suena Curve. Esto incluye las interfaces sin ningún control de volumen y las interfaces cuyo control de volumen en macOS solo abarca algunos de sus canales, no los que usa Curve. En otro caso, las teclas funcionan como siempre en macOS. Para que las teclas ajusten siempre el volumen de Curve, selecciona Always.

El volumen de Curve se aplica a todo lo que Curve emite, después de tus plug-ins, y solo mientras las teclas lo ajustan. Curve lo recuerda al salir. Con el volumen al máximo, el audio no se modifica.

Para ocultar el indicador, desactiva Show Volume Overlay en el mismo menú.

## Cambiar de preset con el teclado

Mientras el menú de Curve está abierto, pulsa una tecla para seleccionar un preset. La tecla se muestra junto a cada preset.

| Presets | Teclas |
|---|---|
| 1 a 10 | 1 a 0 |
| 11 a 20 | Option con 1 a 0 |
| 21 a 30 | Shift con 1 a 0 |

Por ejemplo, pulsa el atajo global y luego 2 para cambiar a tu segundo preset desde cualquier app.

## En el editor durante MIDI Learn

| Tecla | Acción |
|---|---|
| Esc | Terminar MIDI Learn |
| Delete | Eliminar la asignación del control seleccionado |
