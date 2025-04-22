# 開発日記

## 環境構築(25/4/18)

Raspberry Pi Pico の環境構築は、

* VS code の拡張機能 Raspberry Pi Pico をインストールして、
* そこから New Project From Example で Lチカ(blink)を起こして、
* USBの先に Pico2 をつなぎ、BOOTSEL を押しながら電源を入れて、
* 画面下の "RUN" をクリックすると、ツールチェーンをダウンロード・インストール・サンプルのビルド・ダウンロード・実行してくれる。

非常に簡単になった。とすると、Windows 環境で開発するのが自然となるので、Windows 環境で進めることにした。

## step by step

#### 1. Raspberry Pi Pico 拡張機能

拡張機能画面で Pico を入れて検索する。

<img height=300 src="img/001-pico-extension.png"/>

<div style="float: right; margin-left:1em;">
<img height=300 src="img/002-pico-extension-icon-anno2.png"/>
</div>

この拡張機能をインストールすると、アイコン列下側に Pico アイコンが追加される。

</section>


## 基板を作る。

PGA2350をB基板に載せてはんだ付けする。最初は Lチカを動かし、ダウンロードと PGA2350 の動作を確認した。

回路的には

* 電源は VBUS を PGA2350 の VBUS に直結。
* USB_DP, DM に USB コネクタ直結。
* BOOTSEL にスイッチにつなぎ GND に導通するようにする。抵抗不要。
* RUN にスイッチをつなぎ GND に導通するようにする。抵抗不要
* LED をGPIO25につなぐ。サンプル blink のデフォルトが GPIO25 なのだ。

これだけで動作した。PGA2350 よくできている。

## プロジェクトを作る

New C/C++ Project でプロジェクトを作る。

* hardware pwm
* pio
* uart

が欲しい。が、New C/C++ Project で指定できる機能は、"pio", "uart" のみなので、それを指定して入れた。

## プロジェクトを開く

<div style="float: right; margin-left: 1em;">
<img width=200 src="img/003-trust-this-folser.png"/>
</div>

VScode で「フォルダを開く」でプロジェクトが開けるようだ。

ここで、「はい、作成者を信頼します。」で了解を入力することで、プロジェクトペインが左側に開く。

## デバイスをファーム書き込み可能状態にする

PGA2350 に USB ケーブルをつなぐ。

BOOTSEL 押しながらリセットボタンを押して離す。それで、ファームウェアダウンロード可能状態になるが、最初はそうならなかった。「デバイス初期化エラー」とかななんとか。

Zadig をダウンロードして起動する。"RB2 Boot" を選択し、 WinUSB を選択する。"Install Driver" を叩く。

<img width=400 src="img/004-zadig-startup.png"/>

これで、「BOOTSELを押しながらRUNを押して離す」で Pico2 が USB Mass Storage デバイスとして認識され、ドライブが一つ増える。うちの環境では E ドライブが湧いた。

この状態にならないと、ファームを書き込んで実行することはできない。

## ビルドとダウンロード

VS code 右下の「RUN」と書かれた小さな△をクリックする。

<img width=400 src="img/005-vscode-run-command-anno.png"/>

ソースコード下側にペインが開き、そこでコンパイル・ダウンロード状況が表示される。

'Loading into Flush' が表示され、それが100%になればダウンロード開始成功で、デバイスはすでにプログラム実行開始している。　

<img width=400 src="img/006-download-success-anno.png"/>

## git for Windows を入れる。

インストーラをダウンロードして実行するだけである。

いろいろ聞かれる。

## 試作

プロジェクト生成した。
* 名前: `emuz80_pico2`
* Windows 環境で、Pico Exstension の "New C/C++ Project"
* uart, pio を使う、にチェックを入れた。

## ビルド、実機動作

* BOOTSEL押しながらRUN押して話すと書き込み可能モード
* 下右のRUNをクリックすると、ビルド&&ダウンロード&&リセット

* PIO Lチカは動作する(ピン25)
* シリアルが出てこない。
* uart1, pin42, 43 使用で出てこなかった。
* `gpio_set_function` で、 `UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN)` マクロを使うようにすると出るようになった。
* `uart_puts` での出力は出た。`printf` は出なかった。
* 調べてみると、標準入出力は uart0 がデフォルトらしい。
* ピン配置を変えて、pin46, 47 を使うようにすると printf も表示されるようになった。
* コード見てみると、`stdio_init_all();` でなく、`void stdio_uart_init_full(struct uart_inst *uart, uint baud_rate, int tx_pin, int rx_pin) ` 呼び出しで行けそうな気もする。今度試してみよう。

## pin41 に PIO Lチカが出力できない。

* `pio_set_gpio_base(pio, base);` を最初に呼び出すだけではだめらしい。ぱっと見に行けそうな気がしたのだが。
* pico-c-sdk のコードを見ると結構複雑なことをやっている。
+ pico-examples でも pio で 32ピン以上を使っている例が見つからない。
* 各関数終了時に、構造体メンバに何が設定されているかを printf で見ながら進めよう。

## gpio.h の説明

###  General Purpose Input/Output (GPIO) API

RP-series microcontrollers have two banks of General Purpose Input / Output (GPIO) pins, which are assigned as follows:

#### rp2040_specific

RP2040 has 30 user GPIO pins in bank 0, and 6 QSPI pins in the QSPI bank 1 (QSPI_SS, QSPI_SCLK and QSPI_SD0 to QSPI_SD3). The QSPI pins are used to execute code from an external flash device, leaving the User bank (GPIO0 to GPIO29) for the programmer to use. 

#### rp2350_specific

The number of GPIO pins available depends on the package. There are 30 user GPIOs in bank 0 in the QFN-60 package (RP2350A), or 48 user GPIOs in the QFN-80 package. Bank 1 contains the 6 QSPI pins and the USB DP/DM pins.

All GPIOs support digital input and output, but a subset can also be used as inputs to the chip’s Analogue to Digital Converter (ADC). The allocation of GPIO pins to the ADC depends on the packaging.

* RP2040 and RP2350 QFN-60 GPIO, ADC pins are 26-29.
* RP2350 QFN-80, ADC pins are 40-47.
 
Each GPIO can be controlled directly by software running on the processors, or by a number of other functional blocks.

The function allocated to each GPIO is selected by calling the `gpio_set_function` function. 

> Note: Not all functions are available on all pins.

* Each GPIO can have one function selected at a time. Likewise, each peripheral input (e.g. UART0 RX) should only be selected on
* one _GPIO_ at a time. If the same peripheral input is connected to multiple GPIOs, the peripheral sees the logical OR of these GPIO inputs. Please refer to the datasheet for more information on GPIO function select.

#### Function Select Table

##### rp2040_specific

On RP2040 the function selects are:

| GPIO   | F1       | F2        | F3       | F4     | F5  | F6   | F7   | F8            | F9            |
|--------|----------|-----------|----------|--------|-----|------|------|---------------|---------------|
| 0      | SPI0 RX  | UART0 TX  | I2C0 SDA | PWM0 A | SIO | PIO0 | PIO1 |               | USB OVCUR DET |
| 1      | SPI0 CSn | UART0 RX  | I2C0 SCL | PWM0 B | SIO | PIO0 | PIO1 |               | USB VBUS DET  |
| 2      | SPI0 SCK | UART0 CTS | I2C1 SDA | PWM1 A | SIO | PIO0 | PIO1 |               | USB VBUS EN   |
| 3      | SPI0 TX  | UART0 RTS | I2C1 SCL | PWM1 B | SIO | PIO0 | PIO1 |               | USB OVCUR DET |
| 4      | SPI0 RX  | UART1 TX  | I2C0 SDA | PWM2 A | SIO | PIO0 | PIO1 |               | USB VBUS DET  |
| 5      | SPI0 CSn | UART1 RX  | I2C0 SCL | PWM2 B | SIO | PIO0 | PIO1 |               | USB VBUS EN   |
| 6      | SPI0 SCK | UART1 CTS | I2C1 SDA | PWM3 A | SIO | PIO0 | PIO1 |               | USB OVCUR DET |
| 7      | SPI0 TX  | UART1 RTS | I2C1 SCL | PWM3 B | SIO | PIO0 | PIO1 |               | USB VBUS DET  |
| 8      | SPI1 RX  | UART1 TX  | I2C0 SDA | PWM4 A | SIO | PIO0 | PIO1 |               | USB VBUS EN   |
| 9      | SPI1 CSn | UART1 RX  | I2C0 SCL | PWM4 B | SIO | PIO0 | PIO1 |               | USB OVCUR DET |
| 10     | SPI1 SCK | UART1 CTS | I2C1 SDA | PWM5 A | SIO | PIO0 | PIO1 |               | USB VBUS DET  |
| 11     | SPI1 TX  | UART1 RTS | I2C1 SCL | PWM5 B | SIO | PIO0 | PIO1 |               | USB VBUS EN   |
| 12     | SPI1 RX  | UART0 TX  | I2C0 SDA | PWM6 A | SIO | PIO0 | PIO1 |               | USB OVCUR DET |
| 13     | SPI1 CSn | UART0 RX  | I2C0 SCL | PWM6 B | SIO | PIO0 | PIO1 |               | USB VBUS DET  |
| 14     | SPI1 SCK | UART0 CTS | I2C1 SDA | PWM7 A | SIO | PIO0 | PIO1 |               | USB VBUS EN   |
| 15     | SPI1 TX  | UART0 RTS | I2C1 SCL | PWM7 B | SIO | PIO0 | PIO1 |               | USB OVCUR DET |
| 16     | SPI0 RX  | UART0 TX  | I2C0 SDA | PWM0 A | SIO | PIO0 | PIO1 |               | USB VBUS DET  |
| 17     | SPI0 CSn | UART0 RX  | I2C0 SCL | PWM0 B | SIO | PIO0 | PIO1 |               | USB VBUS EN   |
| 18     | SPI0 SCK | UART0 CTS | I2C1 SDA | PWM1 A | SIO | PIO0 | PIO1 |               | USB OVCUR DET |
| 19     | SPI0 TX  | UART0 RTS | I2C1 SCL | PWM1 B | SIO | PIO0 | PIO1 |               | USB VBUS DET  |
| 20     | SPI0 RX  | UART1 TX  | I2C0 SDA | PWM2 A | SIO | PIO0 | PIO1 | CLOCK GPIN0   | USB VBUS EN   |
| 21     | SPI0 CSn | UART1 RX  | I2C0 SCL | PWM2 B | SIO | PIO0 | PIO1 | CLOCK GPOUT0  | USB OVCUR DET |
| 22     | SPI0 SCK | UART1 CTS | I2C1 SDA | PWM3 A | SIO | PIO0 | PIO1 | CLOCK GPIN1   | USB VBUS DET  |
| 23     | SPI0 TX  | UART1 RTS | I2C1 SCL | PWM3 B | SIO | PIO0 | PIO1 | CLOCK GPOUT1  | USB VBUS EN   |
| 24     | SPI1 RX  | UART1 TX  | I2C0 SDA | PWM4 A | SIO | PIO0 | PIO1 | CLOCK GPOUT2  | USB OVCUR DET |
| 25     | SPI1 CSn | UART1 RX  | I2C0 SCL | PWM4 B | SIO | PIO0 | PIO1 | CLOCK GPOUT3  | USB VBUS DET  |
| 26     | SPI1 SCK | UART1 CTS | I2C1 SDA | PWM5 A | SIO | PIO0 | PIO1 |               | USB VBUS EN   |
| 27     | SPI1 TX  | UART1 RTS | I2C1 SCL | PWM5 B | SIO | PIO0 | PIO1 |               | USB OVCUR DET |
| 28     | SPI1 RX  | UART0 TX  | I2C0 SDA | PWM6 A | SIO | PIO0 | PIO1 |               | USB VBUS DET  |
| 29     | SPI1 CSn | UART0 RX  | I2C0 SCL | PWM6 B | SIO | PIO0 | PIO1 |               | USB VBUS EN   |

##### rp2350_specific

On RP2350 the function selects are:

| GPIO  | F0   | F1       | F2        | F3       | F4     | F5  | F6   | F7   | F8   | F9           | F10           | F11      |
|-------|------|----------|-----------|----------|--------|-----|------|------|------|--------------|---------------|----------|
| 0     |      | SPI0 RX  | UART0 TX  | I2C0 SDA | PWM0 A | SIO | PIO0 | PIO1 | PIO2 | XIP_CS1n     | USB OVCUR DET |          |
| 1     |      | SPI0 CSn | UART0 RX  | I2C0 SCL | PWM0 B | SIO | PIO0 | PIO1 | PIO2 | TRACECLK     | USB VBUS DET  |          |
| 2     |      | SPI0 SCK | UART0 CTS | I2C1 SDA | PWM1 A | SIO | PIO0 | PIO1 | PIO2 | TRACEDATA0   | USB VBUS EN   | UART0 TX |
| 3     |      | SPI0 TX  | UART0 RTS | I2C1 SCL | PWM1 B | SIO | PIO0 | PIO1 | PIO2 | TRACEDATA1   | USB OVCUR DET | UART0 RX |
| 4     |      | SPI0 RX  | UART1 TX  | I2C0 SDA | PWM2 A | SIO | PIO0 | PIO1 | PIO2 | TRACEDATA2   | USB VBUS DET  |          |
| 5     |      | SPI0 CSn | UART1 RX  | I2C0 SCL | PWM2 B | SIO | PIO0 | PIO1 | PIO2 | TRACEDATA3   | USB VBUS EN   |          |
| 6     |      | SPI0 SCK | UART1 CTS | I2C1 SDA | PWM3 A | SIO | PIO0 | PIO1 | PIO2 |              | USB OVCUR DET | UART1 TX |
| 7     |      | SPI0 TX  | UART1 RTS | I2C1 SCL | PWM3 B | SIO | PIO0 | PIO1 | PIO2 |              | USB VBUS DET  | UART1 RX |
| 8     |      | SPI1 RX  | UART1 TX  | I2C0 SDA | PWM4 A | SIO | PIO0 | PIO1 | PIO2 | XIP_CS1n     | USB VBUS EN   |          |
| 9     |      | SPI1 CSn | UART1 RX  | I2C0 SCL | PWM4 B | SIO | PIO0 | PIO1 | PIO2 |              | USB OVCUR DET |          |
| 10    |      | SPI1 SCK | UART1 CTS | I2C1 SDA | PWM5 A | SIO | PIO0 | PIO1 | PIO2 |              | USB VBUS DET  | UART1 TX |
| 11    |      | SPI1 TX  | UART1 RTS | I2C1 SCL | PWM5 B | SIO | PIO0 | PIO1 | PIO2 |              | USB VBUS EN   | UART1 RX |
| 12    | HSTX | SPI1 RX  | UART0 TX  | I2C0 SDA | PWM6 A | SIO | PIO0 | PIO1 | PIO2 | CLOCK GPIN0  | USB OVCUR DET |          |
| 13    | HSTX | SPI1 CSn | UART0 RX  | I2C0 SCL | PWM6 B | SIO | PIO0 | PIO1 | PIO2 | CLOCK GPOUT0 | USB VBUS DET  |          |
| 14    | HSTX | SPI1 SCK | UART0 CTS | I2C1 SDA | PWM7 A | SIO | PIO0 | PIO1 | PIO2 | CLOCK GPIN1  | USB VBUS EN   | UART0 TX |
| 15    | HSTX | SPI1 TX  | UART0 RTS | I2C1 SCL | PWM7 B | SIO | PIO0 | PIO1 | PIO2 | CLOCK GPOUT1 | USB OVCUR DET | UART0 RX |
| 16    | HSTX | SPI0 RX  | UART0 TX  | I2C0 SDA | PWM0 A | SIO | PIO0 | PIO1 | PIO2 |              | USB VBUS DET  |          |
| 17    | HSTX | SPI0 CSn | UART0 RX  | I2C0 SCL | PWM0 B | SIO | PIO0 | PIO1 | PIO2 |              | USB VBUS EN   |          |
| 18    | HSTX | SPI0 SCK | UART0 CTS | I2C1 SDA | PWM1 A | SIO | PIO0 | PIO1 | PIO2 |              | USB OVCUR DET | UART0 TX |
| 19    | HSTX | SPI0 TX  | UART0 RTS | I2C1 SCL | PWM1 B | SIO | PIO0 | PIO1 | PIO2 | XIP_CS1n     | USB VBUS DET  | UART0 RX |
| 20    |      | SPI0 RX  | UART1 TX  | I2C0 SDA | PWM2 A | SIO | PIO0 | PIO1 | PIO2 | CLOCK GPIN0  | USB VBUS EN   |          |
| 21    |      | SPI0 CSn | UART1 RX  | I2C0 SCL | PWM2 B | SIO | PIO0 | PIO1 | PIO2 | CLOCK GPOUT0 | USB OVCUR DET |          |
| 22    |      | SPI0 SCK | UART1 CTS | I2C1 SDA | PWM3 A | SIO | PIO0 | PIO1 | PIO2 | CLOCK GPIN1  | USB VBUS DET  | UART1 TX |
| 23    |      | SPI0 TX  | UART1 RTS | I2C1 SCL | PWM3 B | SIO | PIO0 | PIO1 | PIO2 | CLOCK GPOUT1 | USB VBUS EN   | UART1 RX |
| 24    |      | SPI1 RX  | UART1 TX  | I2C0 SDA | PWM4 A | SIO | PIO0 | PIO1 | PIO2 | CLOCK GPOUT2 | USB OVCUR DET |          |
| 25    |      | SPI1 CSn | UART1 RX  | I2C0 SCL | PWM4 B | SIO | PIO0 | PIO1 | PIO2 | CLOCK GPOUT3 | USB VBUS DET  |          |
| 26    |      | SPI1 SCK | UART1 CTS | I2C1 SDA | PWM5 A | SIO | PIO0 | PIO1 | PIO2 |              | USB VBUS EN   | UART1 TX |
| 27    |      | SPI1 TX  | UART1 RTS | I2C1 SCL | PWM5 B | SIO | PIO0 | PIO1 | PIO2 |              | USB OVCUR DET | UART1 RX |
| 28    |      | SPI1 RX  | UART0 TX  | I2C0 SDA | PWM6 A | SIO | PIO0 | PIO1 | PIO2 |              | USB VBUS DET  |          |
| 29    |      | SPI1 CSn | UART0 RX  | I2C0 SCL | PWM6 B | SIO | PIO0 | PIO1 | PIO2 |              | USB VBUS EN   |          |

GPIOs 30 through 47 are QFN-80 only:

| GPIO | F0 | F1       | F2       | F3        | F4      | F5  | F6   | F7   | F8   | F9       | F10           | F11      |
|------|----|----------|----------|-----------|---------|-----|------|------|------|----------|---------------|----------|
| 30   |    | SPI1 SCK | UART0 CTS | I2C1 SDA | PWM7 A  | SIO | PIO0 | PIO1 | PIO2 |          | USB OVCUR DET | UART0 TX |
| 31   |    | SPI1 TX  | UART0 RTS | I2C1 SCL | PWM7 B  | SIO | PIO0 | PIO1 | PIO2 |          | USB VBUS DET  | UART0 RX |
| 32   |    | SPI0 RX  | UART0 TX  | I2C0 SDA | PWM8 A  | SIO | PIO0 | PIO1 | PIO2 |          | USB VBUS EN   |          |
| 33   |    | SPI0 CSn | UART0 RX  | I2C0 SCL | PWM8 B  | SIO | PIO0 | PIO1 | PIO2 |          | USB OVCUR DET |          |
| 34   |    | SPI0 SCK | UART0 CTS | I2C1 SDA | PWM9 A  | SIO | PIO0 | PIO1 | PIO2 |          | USB VBUS DET  | UART0 TX |
| 35   |    | SPI0 TX  | UART0 RTS | I2C1 SCL | PWM9 B  | SIO | PIO0 | PIO1 | PIO2 |          | USB VBUS EN   | UART0 RX |
| 36   |    | SPI0 RX  | UART1 TX  | I2C0 SDA | PWM10 A | SIO | PIO0 | PIO1 | PIO2 |          | USB OVCUR DET |          |
| 37   |    | SPI0 CSn | UART1 RX  | I2C0 SCL | PWM10 B | SIO | PIO0 | PIO1 | PIO2 |          | USB VBUS DET  |          |
| 38   |    | SPI0 SCK | UART1 CTS | I2C1 SDA | PWM11 A | SIO | PIO0 | PIO1 | PIO2 |          | USB VBUS EN   | UART1 TX |
| 39   |    | SPI0 TX  | UART1 RTS | I2C1 SCL | PWM11 B | SIO | PIO0 | PIO1 | PIO2 |          | USB OVCUR DET | UART1 RX |
| 40   |    | SPI1 RX  | UART1 TX  | I2C0 SDA | PWM8 A  | SIO | PIO0 | PIO1 | PIO2 |          | USB VBUS DET  |          |
| 41   |    | SPI1 CSn | UART1 RX  | I2C0 SCL | PWM8 B  | SIO | PIO0 | PIO1 | PIO2 |          | USB VBUS EN   |          |
| 42   |    | SPI1 SCK | UART1 CTS | I2C1 SDA | PWM9 A  | SIO | PIO0 | PIO1 | PIO2 |          | USB OVCUR DET | UART1 TX |
| 43   |    | SPI1 TX  | UART1 RTS | I2C1 SCL | PWM9 B  | SIO | PIO0 | PIO1 | PIO2 |          | USB VBUS DET  | UART1 RX |
| 44   |    | SPI1 RX  | UART0 TX  | I2C0 SDA | PWM10 A | SIO | PIO0 | PIO1 | PIO2 |          | USB VBUS EN   |          |
| 45   |    | SPI1 CSn | UART0 RX  | I2C0 SCL | PWM10 B | SIO | PIO0 | PIO1 | PIO2 |          | USB OVCUR DET |          |
| 46   |    | SPI1 SCK | UART0 CTS | I2C1 SDA | PWM11 A | SIO | PIO0 | PIO1 | PIO2 |          | USB VBUS DET  | UART0 TX |
| 47   |    | SPI1 TX  | UART0 RTS | I2C1 SCL | PWM11 B | SIO | PIO0 | PIO1 | PIO2 | XIP_CS1n | USB VBUS EN   | UART0 RX |

機能番号11 が `GPIO_FUNC_UART_AUX` になっており、通常の番号2と異なる。機能番号2で CTS/RTS を割り当てられているピンを、機能番号11(`GPIO_FUNC_UART_AUX`)とすることで、TX/RX として使うことができる。

## pio_gpio_init

```
static inline void pio_gpio_init(PIO pio, uint pin) {
    check_pio_param(pio);
    valid_params_if(HARDWARE_PIO, pin < NUM_BANK0_GPIOS);
    gpio_set_function(pin, PIO_FUNCSEL_NUM(pio, pin));
}
```

なので、普通に `gpio_set_function` を呼び出しているだけだ。 `PIO_FUNCSEL_NUM` マクロは普通に pio0, 1, 2 を切り替えているだけで特に問題はない。

## pio_set_gpio_base

これは本質的に、

    pio->gpiobase = gpio_base;

やっているだけだが、

```
#if PICO_PIO_VERSION > 0
```

で条件コンパイルしているので、ライブラリ選択がうまくいっていないと pio->gpiobase が正しく設定されていない可能性がある。これは printf で調べてみたい。

