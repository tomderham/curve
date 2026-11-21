# 設定リファレンス

CurveのメニューからSettingsを開きます。

| 項目 | 機能 |
|---|---|
| Audio Settings | オーディオデバイスとMIDIデバイスを選択します。[オーディオ設定とデバイス](audio-settings.md)を参照してください。 |
| Plug-in Manager | プラグインをスキャンして管理します。[Curveを使い始める](getting-started.md)を参照してください。 |
| Preferences... | Preferencesウインドウを開きます(下記を参照)。 |
| Curve Help | このガイドを開きます。 |
| About... | バージョンとライセンスの詳細を表示します。 |
| Quit | Curveを終了し、オーディオ処理を停止します。 |

## Preferences

Preferencesウインドウには3つのタブがあります。Curveを使っている間も開いたままにできます。

### General

| 項目 | 機能 | デフォルト |
|---|---|---|
| Open at Login | ログイン時にCurveを起動します。 | |
| Show in Dock | DockとCommand-TabスイッチャーにCurveを表示します。 | オフ |
| Prevent macOS from automatically sleeping | Curveのオーディオ動作中にMacがスリープしないようにします。 | オフ |
| Release audio interface when idle | 5秒間の無音の後にインターフェイスを解放し、AirPodsの自動切り替えを可能にします。[オーディオ設定](audio-settings.md)を参照してください。 | オフ |
| Auto-Check for App Updates | 新しいバージョンを自動で確認します。 | オン |
| Check for Updates... | 新しいバージョンを今すぐ確認します。 | |

### Keyboard

| 項目 | 機能 | デフォルト |
|---|---|---|
| Volume Keys | 独自の音量調整を持たないインターフェイス向けに、キーボードの音量キーでCurveの出力音量、またはInterface Loopback (In)の音量だけを調整できるようにします。[キーボードショートカット](keyboard-shortcuts.md)を参照してください。 | オフ |
| Show Volume Overlay | 音量キーでCurveの音量を変えたときに、メニューバーのサウンドアイコンの下に音量を表示します。 | オン |
| Global Menu Shortcut | どこからでもCurveのメニューを開くキーの組み合わせを設定します。[キーボードショートカット](keyboard-shortcuts.md)を参照してください。 | オフ |

### MIDI

| 項目 | 機能 | デフォルト |
|---|---|---|
| Program Change Selects Preset | コントローラからのプログラムチェンジでプリセットを読み込みます。[MIDIコントロール](midi-control.md)を参照してください。 | オフ |
| Next Preset Control, Previous Preset Control | プリセットを順に切り替えるコントローラのボタンです。 | |
| Clear Preset Controls | これら2つのボタンの割り当てを削除します。 | |

読み込まれているプリセットのMIDI割り当ては、エディタのメニューのMIDI Mappings for Current Preset...にあります。[MIDIコントロール](midi-control.md)を参照してください。
