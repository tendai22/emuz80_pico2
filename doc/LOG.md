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

## gpio32 以上で blink が動いた。

結局、`sm_config_set_set_pins` の内部(正確には、 `sm_config_set_set_base` 内)で、gpiobase の補正を掛けていないことが問題だった。

```
   sm_config_set_set_pins(&c, pin - pio_get_gpio_base(pio), 1);
```

と pin を gpiobase だけ減らすことで、ピン番号 41 で Lチカが動くようになった。

> `pio_sm_set_consecutive_pindirs` 内部ではピン番号を補正しているので、こちらでも補正すべきと思うのだが。

## ステートマシン 2 個めを動かす

次は、ステートマシン2個を動かすことを試みる。1個を WAIT 生成器、2個目をクロックジェネレータとして使いたい。

6800, 6809 などの2相クロック生成に向いていそう。これらCPUはクロック伸ばしで WAIT を掛けるので、それとの連携にも向いている。PWM を読んで調べるのもめんどうくさい。

それも、同一 pio / ステートマシン2個で動かせるとよい。レトロCPU対応が PIO 1 個で収まるならありがたい。

## WAIT生成器のイメージ

1. メモリアクセス開始を待つ。Z80 なら MREQの立下り、 6809 なら Eの立ち上がり。
2. 条件をチェックする。Z80 の場合、RFSH == H であること。6809ならVMAビットを見る。また、アドレスデコードが必要ならここで行う。
3. ステータスビットを取り出して、RX FIFO に突っ込む。IN 命令。Z80 の場合、RD を入れる。
4. ソフト側は、RX FIFO をブロッキング待ちしている。そこから返ってくるので、返し値を解釈してメモリリード、ライトを呼び分ける。
5.  データバスの読み込み、書き出し(この場合GPIO0-7 のdirection も書き換える)を負えると、TX FIFO に「終わったよ」印でなにかデータを書き込む。
6.  PIO側は、TX FIFO待ち状態にある(OUT)、
7.  OUT命令から抜けてきたら、WAIT信号を解除する。6809ならEクロックを再開する。
8.  IN命令でメモリアクセス完了を待つ。Z80 なら MREQ の立ち上がり、6809 なら Eの立下り。
9.  メモリアクセス完了を受けて、GPIO0-7 の direction を IN に変える。
10. 最初に戻る。

これをステートマシン1個で実現する。

間違いなく 10命令以上、16命令ぐらい使いそうだ。PIO 1つで最大 32 命令のはずなので、なんとか入りそうだ。

## step by step

1. ステートマシン2つを動かす。blink 2個でいいだろう。
    + 1個めはクロックにする。
    + 2個めは将来の wait マシンにする。
2. 2個めのステートマシンを MREQ で起動する(wait mreq)
    + どこかのピンを立てて落とすだけ。
    + CLK を MREQ につなぎ、立下りエッジで動き出すことを確認する。
3. その前に、Z80 を NOP フリーランさせる。
    + RESET をトグル、BUSRQ, INT, NMI は High プルアップ。
    + CLK は1個目のステートマシンで発生させる。
    + D0-D7 は 10kでプルダウン(のちに 3.3V プルアップするので、抵抗まとめ先は切り替え考慮)。
    + WAIT はプルアップ
4. その前に、データバスの扱いを決める
    + PIOで IN/OUT を制御するのは PIODIRS レジスタか?ひょっとして、これ、PIO全体で1個しか使わない?
    + とすると、データバスの IN/OUT を制御するためには、D0-7 を GPIO16より上に割り当てる必要がある。
    + この方向でピンアサインを変えておくか。
    + アドレスバスは GPIO0-15 に割り当てる。
    + PIOで吸い出して RX FIFO 経由で CPU に食わせる？
    + RRXF0_PUTGET0 で FIFO をランダムアクセスして取り出す。
4. ソフト起動を確かめる。
    + RX FIFO に書き込み
    + ソフト側で TEST ピントグル

### とすると、

|||
|---|---|
|1. TESTピン|+ ソフト側で TEST ピントグル<br>+ GPIO45に割り当てた<br>+ 複数回数が波形に出てこない。
|2. ステートマシン2つを動かす。|+ blink 2個でいいだろう。<br>+ 1個めはクロックにする。<br>+ 2個めは将来の wait マシンにする。
|3. データバスの扱いを決める|+ PIOで IN/OUT を制御するのは PINDIRS レジスタか?<br>ひょっとして、これ、PIO全体で1個しか使わない?<br>+ とすると、データバスの IN/OUT を制御するためには、D0-7 を GPIO16より上に割り当てる必要がある。<br>+ この方向でピンアサインを変えておくか。<br>アドレスバスは GPIO0-15 に割り当てる。
|4. 2個めのステートマシンを<br> MREQ で起動する(wait mreq)|+ CLK を MREQ につなぎ、立下りエッジで動き出すことを確認する。
|5. Z80 を NOP フリーラン|+ RESET をトグル、BUSRQ, INT, NMI は High プルアップ。<br>+ CLK は1個目のステートマシンで発生させる。<br>+ D0-D7 は 10kでプルダウン(のちに 3.3V プルアップするので、抵抗まとめ先は切り替え考慮)。<br>+ WAIT はプルアップ
|6. ソフト起動を確かめる。|+ RX FIFO に書き込み
|7. おまけ|+ PIOで吸い出して RX FIFO 経由で CPU に食わせる？<br>+ RRXF0_PUTGET0 で FIFO をランダムアクセスして取り出す。

## クロックドライブのために 74HCゲートを入れる

EMUZ80 の 2 チップのエレガントさが失われるがやむを得ない。動き出すと LED を点灯させたくなるので、そのドライバも兼ねようと思う。

将来的には1チップゲート74AHC1G00W5, TC7S14F,LFとかで基板を起こすとすっきり行きそうだ。「1チップゲートはおやつに入りますか？」という質問はしない方向で進めたい。

## データバスの入出力

PIO を使う方向で考えている。

* GPIO16-23 に割り当てる。
* PINCTRL_OUT_BASE を16, PINCTRL_OUT_COUNT を 8 とすることで、out 命令で D0-D7 に書き出せる
* IN 命令のマッピング、PINCTRL_IN_BASE を 16 とすることで IN 命令で取り出せる。IN 命令のカウントを 8で指定することで1バイトのみ取り出しとできる。
* WAIT GPIO は PINCTRL_IN_BASE と別に絶対ピン番号で指定できるらしい。MREQ を待つこととする。

ステートマシンの PINCTRL レジスタ

|ビット|数|名前|説明|
|-----|---|----|----|
|31:29|3|SIDESET_COUNT|0-5の範囲で指定<br>0:すべて遅延扱い<br>5:すべてsideset扱い。<br>5ビット中上側がsidesetデータ、下側が遅延オペランド。|
|28:26|3|SET_COUNT|SET命令でアサートされるピンの数。0-5の範囲で指定。
|25:20|5|OUT_COUNT|OUT PINS, OUT PINDIRS, MOV PINS命令でアサートされるピンの数。0-32の範囲で指定する。
|19:15|5|IN_BASE|IN data bus命令のLSBにマッピングされるピン。
|14:10|5|SIDESET_BASE|sideset操作の影響を受けるLSBピン。
|9:5|5|SET_BASE|SET PINS, SET PINDIRS命令で影響を受けるLSBピン。
|4:0|5|OUT_BASE|OUT PINS, OUT PINDIRS命令で影響を受けるLSBピン。

CPUがデータバスアクセスの際に、読み込み用RX FIFOへの書き込みと、書き出し用TX FIFOからの読み出し、データバス展開(PINDIRSも変える)を行うPIOプログラムをステートマシン2に置いて使う感じかな。

あと、PINDIRSをall 0(IN)にするPIOプログラムもおいて、WAITマシンから叩く。

GPIOBASEレジスタはステートマシンごとではない、PIO ごとなので、やはり、GPIOBASE = 16 としている以上、D0-D7 はGPIO16以上に割り当てるしかない。

JMP PIN 命令のチェック先は EXECCTRL_JMP_PIN で指定できる。ステートマシンごとに1個、入力マッピングと別に使える。WAIT MREQ 後のチェックは、RD, RFSH と2本チェックが必要なので難しいが、どこかのピンは使えるかもしれない。

PULL 命令は (命令中のblkビットを立てておくと)、TX FIFOが空の場合はストールする。プログラムからのデータ待ちに使用できる。

Z80 からのライトサイクルで、データバスのデータを読み込むのにステートマシンが使える。WR 信号を使うとすると、それを WAIT して IN して PUSH する感じ。

使えなくても、WAITマシンが WR サイクル判定して delay 後に IN して PUSH したらいいか。

CPU側はWAITマシンの RX FIFO をブロック監視している。そこにステータスとデータこみで乗せる。

CPU側は単一イベント待ちにすべてのイベントを乗せたい。

* RDサイクルなら、RDステータスのみ、
* WRサイクルなら、WRステータスとデータ8ビット、
* INTAサイクルなら、INTAステータスのみ、
* シリアルポート受信もイベントとして乗せる。SERIAL_INステータスとデータ8ビット、

PIOで内蔵デバイスのステータスを見られないか。

ステートマシン間の同期には IRQ 命令を使える。IRQ0-3 ビットをセットした状態で待機に入り、クリアされると IRQ 命令から返る。

### 読み込み項目

1. まずはここから
  * WAIT 命令でMREQピン指定する
  * OUT命令でWAITピンを0にする
2. IRQ立ててソフト実行を再開
  * IRQ立てて、
  * TX FIFOデータ待ちに入る。

3. TX FIFOデータ待ち
  * OUT PINDIR ではOSRから8ビットシフトアウトする。ソフト側でTX FIFOに0xff or 0x0 を書き込んでデータバスの方向をソフトに指定させることにする。というか、この場所では常にPINDIRはin方向だ。

## データシート徒然

WRAP_TOP, WRAP_BOTTOM: あるステートマシンごとに機械語の最初と最後を示す。最後の JMP 命令が不要となる。ラベル .wrap_target, .wrap の値で取り出せるが、絶対アドレスなので、途中にプログラムをロードする際には、先頭アドレスからの調整が必要。

```
mm_pio->sm[0].execctrl =
    (auto_push_pull_wrap_target << PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB) |
    (auto_push_pull_wrap << PIO_SM0_EXECCTRL_WRAP_TOP_LSB);
```

> これはオフセット調整しているようには見えないが。

ステートマシンの EXECCTRLレジスタのビットマップ:

|ビット|数||シンボル|説明|
|---|---|---|---|---|
|31|1|EXEC_STALLED|RO|INSTRに書き込まれた命令はストールされ、ステートマシンにラッチされる。この命令が完了すると0にクリアされる。
|30|1|SIDE_EN|RW|Delay/SidesetフィールドのMSBをsideset enable として扱う。
|29|1|SIDE_PINDIR|RW|1のとき、サイドセットはPINDIRにアサートされる。
|28:24|5|JMP_PIN|RW|JMP PINとして使うGPIO番号。
|23:19|5|OUT_EN_SEL|RW|inline OUT enable に使用するデータビット(?!)
|18|1|INLINE_OUT_EN|RW|1のとき、OUT dataの1ビットを補助書き込みイネーブルとして使用する。<br>0のとき、OUT_STICKYと組み合わせて使用すると、最新のpin書き込みを出アサートする。<br>(ステートマシン間の書き込み優先順位と組み合わせて使うらしい)
|17|1|OUT_STICKY:1のとき、最新のOUT/SETをpinsにアサートし続ける。
|16:12|5|WRAP_TOP|機械語がこのアドレスに達すると、WRAP_BOTTOMに巻き戻る
|11:7|5|WRAP_BOTTOM|WRAP_TOPから巻き戻る先、おそらくループ先端。
|6:5|2|STATUS_SEL|

## PIO 2つ

問題なく動いた。ステートマシン番号を 0, 1 としていなかったので、2度目の初期化で 1番目のステートマシンがリセットされていた。

```
    uint offset1 = pio_add_program(pio, &clockgen_program);
    clockgen_pin_forever(pio, 0, offset1, clk_pin, 10);
    uint offset2 = pio_add_program(pio, &wait_control_program);
    wait_control_pin_forever(pio, 1, offset2, wait_pin, 200000);
```

## gpio

結局、

```
    gpio_set_dir(TEST_PIN, true);
    gpio_put(TEST_PIN, 0);
```

```
#define TOGGLE() do {    gpio_xor_mask64(((uint64_t)1)<<TEST_PIN); } while(0)
```

で佳かった。

ロジアナ(DSView v1.3.2)で、スレシホールド 0.8V だと TEST pin にノイズが乗るが、0.9Vなら大丈夫だった。

## 時間が一定しない。

```
    while (n-- > 0) TOGGLE();
    TOGGLE();
    TOGGLE();
    n = 100;
    while (n-- > 0) TOGGLE();
    TOGGLE();
    TOGGLE();

```

うーん、RAM 実行かなぁ。XIPで変動していることを疑っている。

## 4/25 wait gpio 25

クロック(clk_pin 41)をMREQ(pin 25)につないで、pin25 の 0, 1 を wait 命令でとらえてみる。

```
.wrap_target
    wait 0 gpio 25 [5]  ; pin 25
    set pins, 0     ; WAIT Low
    wait 1 gpio 25 [5]  ; pin 25
    set pins, 1    ; WAIT High
.wrap
```

これで CLK に 60ns 遅延する波形を WAIT(pin 31)に出力させることができた。

## 4/27 wait gpio 25

1. CMakeFile.txt の board 指定を pico2 から pimoroni_pga2350 にすると進んだ。

当初 pico2, これは PICO_PIO_USE_GPIA_BASE がゼロのまま。ライブラリの選択が 64ピン版だったらしく、
だめだった。

2. MREQ_Pin を `gpio_init(MREQ_Pin);` することが必要だった。これなしでは 無割り当て状態で、pin25が内部につながっていかなかった。

3. これで、wait 0 gpio 25 が動く。それどころか、wait 0 gpio 40 も動く。ボード指定を正しくすることが大事。

これで、CLK -> MREQ とつないでおいて、CLK に WAIT が追従するようになった。

* CLK のエッジに 40ns 遅れて wait が動く。以下のプログラムで。

```
.wrap_target
    wait 0 gpio 25  ; pin 25
    set pins, 0    ; WAIT Low
    wait 1 gpio 25  ; pin 25
    set pins, 1    ; WAIT High
.wrap
```

## TEST pin のノイズ

ノイズ(グリッジ)が乗っている。

* CLK, MREQ, WAIT が動くタイミングでひげがでる。
* ひげはLに貼り付けているときにでる。High に貼り付けているときはでない。
* VBUS - GND 間に 10uF 電解コンデンサを挟んだが改善しない。
* TEST Pin が 45 でも 10 でも同じように出る。
* ロジアナのスレシホールド 0.8V で出る。 1.3V で出なくなる。

## wait control

* MREQ Pin Low を待って起動
* WAIT Low
* ソフト側にイベント送付(push -> pio_sm_get_blocking(pio, 1))
* ソフト側は pio_sm_get_blocking から戻ってくる。
* アドレスバスを読み込み、READならデータバスにデータを載せる
  WRITEならデータバスからデータを読み込む(ここ未実装)
* 処理が終わると、pim_sm_put でメモリアクセス完了を通知する。
* PIO側では pull で終了イベント発生を知る。
* WAIT High にする。
* MREQ Pin High を待つ。本サイクル終了を待つ。

何もしないと、TX FIFO に１ワード何かが入っており、PIO側がうまくブロックしなかった。最初の pull noblock で読み出してフラッシュすることで、PIO側がうまくブロックするようになった。将来的にはC言語側で明示的にクリアするべき。

```
.program wait_control
    set pins, 1 [20]
    pull noblock    ; dummy pull for flush
.wrap_target
    wait 0 gpio 25  ; pin 25
    set pins, 0     ; WAIT Low
    push            ; notify an access occurs
    pull block      ; wait for cpu's process finished
    set pins, 1     ; WAIT High
    wait 1 gpio 25  ; sleep until this cycle ends
.wrap
```

```
    uint32_t dummy;
    while (true) {
        dummy = pio_sm_get_blocking(pio, 1);
        TOGGLE();
        sleep_us(1);
        pio_sm_put(pio, 1, dummy);
        TOGGLE();
    }
```

アドレスバスの読み込み、データバスの処理はまだ実装できていない。

## データバス処理

もう1つステートマシンを用意する。ステートマシン0, 1 はすでに OUT ピンを割り当てているため、D0-D7(GPIO16-GPIO23)をIN/OUTするには、別のステートマシンを用意してそこに割り当てる必要がある。

このステートマシンも MREQ ピンを待つ。

READ/WRITEを区別するには、RDピンを読んで分岐する。WRITEならデータバスを読み込み、RX FIFOにおく。ソフト側では pio_sm_get_blocking で２ワードを、１ワード目は all zero, 2ワードめで8bitデータを受け取る。READなら all one を返し、TX FIFO から8bitデータを取り出しデータバスに書き出す。PIODIRSにOUTをつけ、MREQ 1 を待つ。

MREQ 1 となると、PIODIRSをINにして、ループの最初に戻る。

sidesetをうまく使うとWAIT制御もできるかもしれない。

## ピン監視・操作命令の対象ピンの決まり方

> これらの各操作は GPIO の 4 つの連続した範囲の 1 つを使用し、各範囲のベースとカウントは各ステートマシンの PINCTRL レジスタで設定します。 OUT、SET、IN、およびサイドセット操作のそれぞれに範囲があります。 各範囲は、指定された PIO ブロック(RP2350 では 30 個のユーザ GPIO)にアクセス可能な GPIO のいずれかをカバーすることができ、範囲は重複することができます。

Index オペランドは5ビットあるので、連続4つまで・5ビット中2ビットしか使えないという制約は、GPIOマッパのハードウェア的都合かな。

WAIT, JMPのIndexは5ビット丸ごと有効です。PINCTRL_IN_BASE に0 or 16 を代入し、そこから連続32ピンしか使えない。

ちなみに、PINCTRL_IN_BASE に 16 を代入して、GPIO10に set pins 命令でクロック出力できたのはなんなんだろうか。謎。

* クロック生成でステートマシン1つ使う。set/sideset で最大連続2ビットを使う。現在は40, 41ピン。
* WAIT生成、バスRead/Write/OE制御で2つめのステートマシンを使う。使えると思う。
  + MREQ で wait する。もしくは MREQ/IORQ でビジーループ待ち。jmp pin で振り分ける。
  + RD,RFSHで jmp pin を使う。正規のメモリアクセス抽出と、Read/Write で処理を分ける。
  + IN/OUT は D0-D7 に割り当てる。8本の IN/OUT 命令と、OUT PINDIR でOE制御する。
  + メインCPUプログラムとのやりとりは、RX FIFO, TX FIFO を使う。Z80 WriteデータをRX FIFOで渡し、Z80 ReadデータをTX FIFOで受け付ける。
  + イベント発生もRX FIFOで渡す。RX FIFOの1つ目はRead/Write/MEM/IO/INTAフラグを渡す。Z80 Writeサイクルでは2つ目のデータを読み取る。Z80 ReadサイクルではRX FIFOを待ちに行かずにTX FIFOにデータを書き出し、ステートマシンがデータバスへの書き出しと、PINDIRの書き出しを行う。
  + 全て終わるとwait 1 MREQ し、MREQ が１に戻ったらPINDIRをall zeroにする。
* シリアルポートもステートマシン2つで実現すると、データ受信時のステータス改変ができるが、それは第２段階とする。最初は、メインCPU側でポーリングする。

ということで、PIO 各ベースの割り当て案。PINCTRLレジスタはステートマシンごとに存在する。

|type|SM|BASE|COUNT|description|
|---|---|---|
|SET|0|40|1 or 2|クロック発生をsidesetで行うように変更する。
|IN|0|25|1|クロックストレッチを行う場合、このピンを監視して0の間は止める。
|IN|1|16|8|データRead/Write/OE
|OUT|1|16|8|データRead/Write/OE
|SET|1|31|WAIT信号1ピンのみ(CPUによっては25,26,...と複数割り当ても想定している)
|PINCTRL_MOV_BASE|NA|とりあえずunused

## waitマシンの再構成

* MREQを待つ。
* RDで振り分ける。
* Read(Z80 Write)時には、RX FIFOにall zeroを書き込み、INしてpush
* Write(Z80 Read)時には、RX FIFOにall oneを書き込み、pullしてOUTする。OUT PINDIR alloneする。
* Read/Writeとも終わると、wait 1, MREQする。
* MREQが立ったら、OUT PINDIR allzero して先頭に戻る。

* RX FIFOにall zeroを書き込むには、
  + MOV ISR, NULL (or ~NULL)
  + PUSH

## pio 2個に分けなくても大丈夫

クロック生成とwait機械を同じPIOでできるか？

sm_config_set_in_pinsなどが smを引数に持たないので4sm共通ではないかと心配だったが、そうではなかった。

プログラムごとにset_set_pinsしているが、(41と31で)それなりに動いているようだ。少し謎。

## PIO設定後、runするまでの間のWAITを期待通りに設定できない

Low になってしまう。命令実行すると最初の命令でHighにしてあるが、事前にgpio_putでHighにしていてもLowになってしまう。

`pio_sm_set_pins64(pio, sm, pin);` を入れてみたが効果なし。

RESET Highまで waitマシンを止めておくことで無事 High に貼り付けることができた。

機械語先頭の `set pins 1` を抜くとゼロになった、失敗。

## メモリリードを組んでみた

動作しているかどうかは不明だが、まずコードを書いてみた。

```
.program wait_control
    set pins, 1     ; WAIT High
    pull noblock    ; dummy pull for flush
    wait 1 gpio 42  ; wait for RESET negate
    wait 1 gpio 25  ; assure MREQ High
.wrap_target
    wait 0 gpio 25  ; detect MREQ down edge
    set pins, 0     ; WAIT Low
    ; so far always READ
    mov x, ~NULL    ; set X to all one
    mov isr, X
    mov pindirs, x  ; pindir is (all one)out
    push            ; notify an access occurs
    pull block      ; wait for cpu's process finished
    out pins, 8
    set pins, 1     ; WAIT High
    wait 1 gpio 25  ; sleep until this cycle ends
    mov pindirs, NULL   ; D0-D7 reset to 3-state (input)
.wrap
```

これからデバッグ開始です。

