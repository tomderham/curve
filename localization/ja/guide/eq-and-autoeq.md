# ヘッドフォンEQとAutoEQ

Curveは、測定済みのヘッドフォンプロファイルからParametric EQを設定できます。これにより、Macのすべてのオーディオに対してヘッドフォンの周波数特性を補正できます。

## AutoEQのヘッドフォンプロファイルを使用する

インターネット接続が必要です。

1. エディタの背景を右クリックし、Curve Plugins > Parametric EQ (import from AutoEQ...)を選択します。
2. 検索フィールドに、HD 650やAirPodsなどのヘッドフォンのモデル名を入力します。
3. 結果を選択すると、周波数特性カーブとフィルタが表示されます。
4. Use this EQをクリックします。Curveはその設定でParametric EQノードを作成します。
5. [ノードを接続](editor.md)してチェーンに組み込みます。
6. 保持するには、Save As Preset...を選択します。

代わりに既存のParametric EQノードを更新するには、ノードを右クリックしてImport EQ from AutoEQ...を選択します。

ヘッドフォンのモデルが見つからない場合は、Sync IndexをクリックしてAutoEQデータベースへの最近の追加を確認します。

## 微調整と他のプロファイルの検索

- ターゲットカーブやサウンドの傾向を調整するには、[autoeq.app](https://autoeq.app)を使用してEQカーブを書き出します。
- お使いのヘッドフォンが見つからない場合は、コミュニティによるヘッドフォンとIEMの測定データがある[squig.link](https://squig.link)も試してみてください。

## EQファイルを読み込む

Curveは、AutoEQ、Squig.link、REWのEQカーブを、`.txt`、`.csv`、`.tsv`、`.eq`ファイルから読み込めます。

- 新しいノードを作成するには、エディタの背景を右クリックしてCurve Plugins > Parametric EQ (import from file...)を選択するか、ファイルをエディタウインドウにドラッグします。
- 既存のParametric EQノードを更新するには、ノードを右クリックしてImport EQ from file...を選択します。

## EQを手動で調整する

Parametric EQノードをダブルクリックしてウインドウを開きます。
