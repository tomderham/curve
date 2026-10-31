# トラブルシューティング

## 音が出ない、または一部のチャンネルがミュートされる

- ノードがAudio Outputまで接続されていることを確認します。
- Interface Loopback (In)ノードがプリセットにあるのに使用していない場合は、削除するかピンを未接続のままにします。[ループバック](loopback.md)を参照してください。
- 選択したインターフェイスを取り外した場合、Curveは他の出力を使わず、戻るまで無音のままです。別のインターフェイスを使用するには、Audio Settingsで選択します。
- バイパスまたはミュートされているノードがないこと、および[Audio Settings](audio-settings.md)で正しいチャンネルが有効になっていることを確認します。

## Curve Helpにコンテンツを利用できないと表示される

Curve Helpは、Curveがアプリケーションフォルダにある場合にのみ開きます。Curveをそこへ移動してから、もう一度試してください。

## 開いた後にCurveが見つからない

Curveはメニューバーで動作し、通常はウインドウを開きません。メニューバーのアイコンをクリックし、Show Editorを選択します。Dockにも表示するには、Settings > Show in Dockをオンにします。

## プリセットへの変更が消えた

Curveは変更を自動で保存しません。Save As Preset...を使用して、既存のプリセットを置き換えてください。[プリセット](presets.md)を参照してください。

## プラグインがメニューにない

Settings > Plug-in Managerを選択し、そのプラグインのフォーマットのスキャンを実行します。[Curveを使い始める](getting-started.md)を参照してください。

## プラグインのピン数が正しくない

ノードを右クリックしてConfigure Audio I/Oを選択します。

## サンプルレートが変わり続ける

他のアプリがハードウェアのサンプルレートを変更している可能性があります。Curveで強制的に戻すには、[Audio Settings](audio-settings.md)のAlways enforce sample rateをオンにします。

## AirPodsが自動で切り替わらない

Settings > Release audio interface when idleをオンにします。[オーディオ設定とデバイス](audio-settings.md)を参照してください。

## CurveがMIDIコントローラに反応しない

Audio SettingsのMIDI入力でコントローラが有効になっていること、およびマッピングがプリセットとともに保存されていることを確認します。[MIDIコントロール](midi-control.md)を参照してください。
