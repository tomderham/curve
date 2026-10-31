# EQ para auriculares y AutoEQ

Curve puede configurar un Parametric EQ a partir de un perfil de auriculares medido, para que puedas corregir la respuesta en frecuencia de tus auriculares en todo el audio del Mac.

## Usar un perfil de auriculares de AutoEQ

Esto requiere conexión a Internet.

1. Haz clic con el botón derecho en el fondo del editor y selecciona Curve Plugins > Parametric EQ (import from AutoEQ...).
2. En el cuadro de búsqueda, escribe el modelo de tus auriculares, como HD 650 o AirPods.
3. Selecciona un resultado para ver su curva de respuesta y sus filtros.
4. Haz clic en Use this EQ. Curve crea un nodo Parametric EQ con esos ajustes.
5. [Conecta el nodo](editor.md) a tu cadena.
6. Selecciona Save As Preset... para conservarlo.

Para actualizar un nodo Parametric EQ existente, haz clic con el botón derecho en él y selecciona Import EQ from AutoEQ...

Si falta un modelo de auriculares, haz clic en Sync Index para comprobar las novedades de la base de datos de AutoEQ.

## Ajustar con precisión o buscar otros perfiles

- Para ajustar las curvas objetivo y el carácter sonoro, usa [autoeq.app](https://autoeq.app) y exporta la curva de EQ.
- Si tus auriculares no aparecen, también puedes probar [squig.link](https://squig.link), que ofrece mediciones de auriculares e IEM aportadas por la comunidad.

## Importar un archivo de EQ

Curve importa curvas de EQ de AutoEQ, Squig.link y REW, en archivos `.txt`, `.csv`, `.tsv` o `.eq`.

- Para crear un nodo nuevo, haz clic con el botón derecho en el fondo del editor y selecciona Curve Plugins > Parametric EQ (import from file...), o arrastra el archivo a la ventana del editor.
- Para actualizar un nodo Parametric EQ existente, haz clic con el botón derecho en él y selecciona Import EQ from file...

## Ajustar el EQ a mano

Haz doble clic en el nodo Parametric EQ para abrir su ventana.
