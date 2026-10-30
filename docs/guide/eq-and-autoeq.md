# Headphone EQ and AutoEQ

Curve can set up a Parametric EQ from a measured headphone profile, so you can correct your headphones' frequency response across all your Mac's audio.

## Use an AutoEQ headphone profile

This needs an internet connection.

1. Right-click the editor background and select Curve Plugins > Parametric EQ (import from AutoEQ...).
2. In the search box, type your headphone model, such as HD 650 or AirPods.
3. Select a result to see its response curve and filters.
4. Click Use this EQ. Curve creates a Parametric EQ node with those settings.
5. [Connect the node](editor.md) into your chain.
6. Select Save As Preset... to keep it.

To update an existing Parametric EQ node instead, right-click it and select Import EQ from AutoEQ...

If a headphone model is missing, click Sync Index to check for recent additions to the AutoEQ database.

## Fine-tune or find other profiles

- To adjust target curves and sound signature, use [autoeq.app](https://autoeq.app) and export the EQ curve.
- If your headphones aren't listed, you can also try [squig.link](https://squig.link), which has crowd-sourced headphone and IEM measurements.

## Import an EQ file

Curve imports EQ curves from AutoEQ, Squig.link and REW, in `.txt`, `.csv`, `.tsv` or `.eq` files.

- To create a new node, right-click the editor background and select Curve Plugins > Parametric EQ (import from file...), or drag the file onto the editor window.
- To update an existing Parametric EQ node, right-click it and select Import EQ from file...

## Adjust the EQ by hand

Double-click the Parametric EQ node to open its window.
