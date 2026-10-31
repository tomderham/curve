# エディタ

エディタには、プリセットがノードのグラフとして表示されます。オーディオは、各ノードの下部にある出力から、次のノードの上部にある入力へと流れます。エディタを開くには、CurveのメニューからShow Editorを選択します。閉じるには、閉じるボタンをクリックするか、Hide Editorを選択します。どちらの場合もオーディオは動作し続けます。

エディタ下部のステータスバーには、ヒントとポインタの下にある項目の説明が表示されます。

## ノードを追加する

エディタの背景を右クリックし、次から選択します。

- **Audio I/O:** オーディオ入力、オーディオ出力、Interface Loopback (In)の各ノード。
- **MIDI I/O:** MIDI入力とMIDI出力のノード。
- **Curve Plugins:** [Curveの内蔵プラグイン](curve-plugins.md)。
- **スキャン済みのプラグイン:** これらのメニューの下に、メーカー別に表示されます。

プラグインファイルや[EQファイル](eq-and-autoeq.md)をエディタウインドウにドラッグすることもできます。

## ノードを接続する

ノードのピンから別のノードのピンへドラッグします。オーディオピンは緑色、MIDIピンは赤色です。1つの出力を複数の入力に接続でき、自由にチェーンを組むことができます。

## ノードをバイパスする

ノードの電源ボタンをクリックします。もう一度クリックすると、ノードが再び有効になります。ノードを右クリックしてToggle Bypassを選択することもできます。

## プラグインのウインドウを開く

ノードをダブルクリックします。

エディタが閉じていてもプリセットの読み込み時に毎回ウインドウを開くには、ノードを右クリックしてAlways Open Plug-in GUI with Presetを選択し、Save As Preset...を使用します。ウインドウは保存時の位置に開きます。

## ノードメニュー

ノードを右クリックすると、次の項目が表示されます。

| 項目 | 機能 |
|---|---|
| Delete This Node | ノードを削除します。 |
| Disconnect All Pins | ノードの接続をすべて解除します。 |
| Toggle Bypass | バイパスのオンとオフを切り替えます。 |
| Rename This Node | ノードに表示される名前を変更します。 |
| MIDI Learn | [MIDI Learn](midi-control.md)を開始します。 |
| Clear MIDI Mappings | ノードのMIDIマッピングを削除します。マッピングがある場合にのみ表示されます。 |
| Always Open Plug-in GUI with Preset | プリセットを読み込むたびにプラグインのウインドウを開きます。 |
| Import EQ from AutoEQ... | Parametric EQノードに表示されます。[ヘッドフォンEQとAutoEQ](eq-and-autoeq.md)を参照してください。 |
| Import EQ from file... | Parametric EQノードに表示されます。[ヘッドフォンEQとAutoEQ](eq-and-autoeq.md)を参照してください。 |
| Configure Audio I/O | プラグインの入力ピンと出力ピンの数を変更します。 |
| Audio Settings... | オーディオ入出力ノードに表示されます。Audio Settingsを開きます。 |
| Save Plug-in State | プラグインの設定をファイルに保存します。 |
| Load Plug-in State | プラグインの設定をファイルから読み込みます。 |

## プラグインのチャンネル数を変更する

プラグインのピン数が希望どおりでない場合は、ノードを右クリックしてConfigure Audio I/Oを選択します。
