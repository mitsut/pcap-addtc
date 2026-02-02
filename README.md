# pcap-addtc

SpaceWire TimeCodeをPCAPファイルに挿入するツール

## 概要

このツールは、既存のPCAPファイルを読み込み、指定された時間範囲で指定周波数のSpaceWire TimeCodeパケットを生成し、元のパケットと時系列でマージした新しいPCAPファイルを作成します。

## 機能

- PCAPファイルの統計情報表示（フレーム数、タイムスタンプ、継続時間）
- SpaceWire TimeCodeパケットの周期的生成（デフォルト64Hz、--freqオプションで変更可能）
- TimeCode値の自動インクリメント（0-255の8ビットサイクル）
- 既存パケットとTimeCodeパケットのタイムスタンプによるマージ
- DLT_USER2（149）フォーマットでの出力

## TimeCode仕様

SpaceWire TimeCodeは以下の形式で生成されます：

```
ESC (0xFC) + データキャラクタ (1バイト)
```

データキャラクタの構成：
- T0-T5: 6ビットの時刻情報
- T6-T7: 2ビットの制御フラグ

TimeCode値は0から255まで8ビット全体を使用し、各TimeCodeパケット生成時に自動的にインクリメントされます。

## ビルド方法

### 必要な依存関係

- CMake 3.16以上
- C++17対応コンパイラ
- libpcap開発ライブラリ

### macOSの場合

```bash
# libpcapのインストール
brew install libpcap

# ビルド
mkdir build
cd build
cmake ..
make
```

### Linuxの場合

```bash
# libpcapのインストール（Ubuntu/Debian）
sudo apt-get install libpcap-dev

# ビルド
mkdir build
cd build
cmake ..
make
```

## 使用方法

### 基本的な統計情報の表示

```bash
./pcap_summary --pcap <input-pcap-file>
```

### TimeCodeパケットの生成と挿入

```bash
./pcap_addtc --pcap <input-pcap> --start <epoch_us> --end <epoch_us> --file <output-pcap> [--freq <frequency_hz>]
```

#### パラメータ

- `--pcap <input-pcap>`: 入力PCAPファイルのパス（必須）
- `--start <epoch_us>`: TimeCode挿入開始時刻（エポックマイクロ秒）
- `--end <epoch_us>`: TimeCode挿入終了時刻（エポックマイクロ秒）
- `--file <output-pcap>`: 出力PCAPファイルのパス
- `--freq <frequency_hz>`: TimeCode周波数（Hz）。デフォルト: 64

## 使用例

### 例1: 統計情報の表示

```bash
./pcap_addtc --pcap rmap_write.pcap
```

出力例：
```
File: rmap_write.pcap
Frame count: 5
First frame time:
  JST: 2023-03-15 11:08:53.001048+09:00
  epoch_us: 1678846133001048
Last frame time:
  JST: 2023-03-15 11:08:57.001048+09:00
  epoch_us: 1678846137001048
Duration: 4.000000 s
```

### 例2: TimeCodeパケットの生成（デフォルト64Hz）

```bash
./pcap_addtc --pcap rmap_write.pcap --start 0 --end 1000000 --file output_with_timecode.pcap
```

この例では：
- 開始時刻: 0マイクロ秒（エポック）
- 終了時刻: 1,000,000マイクロ秒（1秒）
- 周波数: 64Hz（デフォルト）
- 生成されるTimeCodeパケット数: 約65個（64Hz × 1秒 + 1）

### 例3: TimeCodeパケットの生成（カスタム周波数）

```bash
./pcap_addtc --pcap rmap_write.pcap --start 0 --end 1000000 --file output_with_timecode.pcap --freq 100
```

この例では：
- 開始時刻: 0マイクロ秒（エポック）
- 終了時刻: 1,000,000マイクロ秒（1秒）
- 周波数: 100Hz（カスタム指定）
- 生成されるTimeCodeパケット数: 約101個（100Hz × 1秒 + 1）

出力例：
```
File: rmap_write.pcap
Frame count: 5
...
Generating TimeCode packets...
Total packets (original + TimeCode): 70
Output PCAP file created: output_with_timecode.pcap
```

## 出力フォーマット

- データリンクタイプ: DLT_USER2 (149)
- TimeCodeパケットペイロード: 2バイト（ESC + データキャラクタ）
- 既存のパケットは変更されません
- すべてのパケットはタイムスタンプ順にソートされます

## テスト

```bash
cd build
ctest
```

## 参考リンク
- [scdha-wireshark](https://github.com/matsuzaki-keiichi/scdha-wireshark)
- [pcap-nc](https://github.com/matsuzaki-keiichi/pcap-nc)

## ライセンス
