# 720 Çekirdeksiz Motor Sürücü

ESP32 Dev Module, SI230DS N-kanallı MOSFET ile 720 çekirdeksiz motoru düşük taraftan PWM ile sürer. Hız, ESP32’nin açtığı web arayüzünden ayarlanır.

Yazılım şu an **tek motoru GPIO 25** üzerinden sürer. Dört motorlu çizim, aynı anahtarın dört kopyasıdır; her kapı ayrı bir GPIO’ya gider.

## SI230DS

SOT-23 kılıf. Çizimdeki özet:

| VDS | RDS(on) | ID |
| --- | --- | --- |
| 20 V | 0,085 Ω @ VGS = 4,5 V | 2,8 A |
| 20 V | 0,115 Ω @ VGS = 2,5 V | 2,4 A |

Pinler, gövdeye bakarken:

| Pin | Uç |
| --- | --- |
| 1 | Gate (G) |
| 2 | Source (S) |
| 3 | Drain (D) |

ESP32 3,3 V ile kapıyı sürer. Motor **3,7 V** bataryadan beslenir. ESP32’nin 3,3 V pini motoru beslemez; yalnızca GND ortaktır.

## Tek motor

![Tek motor ve SI230DS](https://raw.githubusercontent.com/Muhammed-Turgut/imageRaw/main/720-coreless-motor-driver-image-2.jpeg)

- **Gate:** GPIO. Gate ile source arasına **10 kΩ**; açılışta kapı boşta kalırsa motor dönmez.
- **Source:** GND.
- **Drain:** motorun eksi ucu. Motorun artı ucu **3,7 V**.
- Drain ile 3,7 V arasında diyot. Akım tek yönde gider; motor kapanınca endüktif darbe MOSFET’e binmez.

Firmware bu kanalı GPIO 25’te, 20 kHz PWM ile sürer.

## Dört motor

![Dört motor](https://raw.githubusercontent.com/Muhammed-Turgut/imageRaw/main/720-coreless-motor-driver-image-1.jpeg)

Dört SI230DS yan yana, aynı 3,7 V hattını ve aynı GND’yi paylaşır.

- Her motorun artı ucu ortak **+3,7 V** hattındadır.
- Her MOSFET’in drain’i kendi motorunun eksi ucuna gider.
- Dört source birlikte **GND**’ye iner.
- Her gate, kendi direnci üzerinden ayrı bir **GPIO**’ya gider.

Dört kanal birbirinden bağımsız açılıp kapanır. Ortak olan yalnız besleme ve şasedir.

## Web arayüzü

ESP32 açılınca `720-Motor` ağını yayınlar. Şifre `motor720`. Telefondan `http://192.168.4.1` adresini aç. Kaydırıcı 0–100 arası hız verir, **DUR** çıkışı kapatır. Açılışta motor kapalıdır; hız yumuşak rampa ile gelir.

Ev ağına bağlamak için `src/main.cpp` içindeki `WIFI_SSID` ve `WIFI_PASS` doldurulur. Ağ bulunamazsa yine aynı erişim noktası açılır.
