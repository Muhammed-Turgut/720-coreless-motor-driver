# 720 Çekirdeksiz Motor Sürücü

ESP32 Dev Module, SI230DS N-kanallı MOSFET ile 720 çekirdeksiz motoru düşük taraftan sürer. Hız, telefon tarayıcısındaki kaydırıcıdan ayarlanır. MOSFET motoru açıp kapatır; hız, bu aç-kapa işinin 20 kHz’de ne kadar süre açık kaldığıdır.

Yazılım şu an **tek motoru GPIO 25** üzerinden sürer. Dört motorlu çizim aynı anahtarın dört kopyasıdır. Her kapı ayrı bir GPIO’ya gider, besleme ve şase ortaktır.

Motor **3,7 V** bataryadan beslenir. ESP32’nin 3,3 V pini motoru beslemez. Batarya eksi, MOSFET source ve ESP32 GND aynı düğümdedir.

## Malzemeler

| Parça | Görevi |
| --- | --- |
| ESP32 Dev Module | PWM üretir, web arayüzünü açar |
| SI230DS | Motor akımını GND’ye bağlayan anahtar |
| 10 kΩ | Gate’i açılışta GND’de tutar |
| 220 Ω | GPIO ile gate arasında seri direnç |
| Schottky diyot (1N5819) | Motor kapanınca bobin akımına yol verir |
| 720 çekirdeksiz motor | 3,7 V yük |
| 1S batarya | Motor beslemesi |

1N5819, 1 A / 40 V Schottky’dir. 20 kHz PWM’de normal diyottan hızlı toparlanır, 3,7 V hatta ileri gerilim düşümü küçüktür. Aynı işi gören SMD karşılığı SS14’tür. Her motorun kendi diyotu vardır.

## SI230DS

SOT-23. Çizimdeki özet:

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

ESP32 kapıya 3,3 V verir. SI230DS 2,5 V’ta da düşük dirençtedir, bu yüzden 3,3 V ile doyuma girer.

## Tek motor

![Tek motor ve SI230DS](https://raw.githubusercontent.com/Muhammed-Turgut/imageRaw/main/720-coreless-motor-driver-image-2.jpeg)

```
3,7 V ------+---------------- motor (+)
             |                    |
          [diyot]              [motor]
          katot üstte             |
             |                    |
             +-------------- drain (pin 3)
                                |
                             SI230DS
                                |
             +-------------- source (pin 2) ---- GND
             |
           10 kΩ
             |
GPIO 25 -- 220 Ω -- gate (pin 1)
```

| Uç | Nereye |
| --- | --- |
| GPIO 25 | 220 Ω, oradan gate |
| Gate | 10 kΩ ile source / GND |
| Source | ESP32 GND ve batarya eksi |
| Drain | Motor eksi |
| Motor artı | Batarya 3,7 V |
| Diyot katot (şerit) | 3,7 V / motor artı |
| Diyot anot | Drain / motor eksi |

10 kΩ, ESP32 pini henüz çıkış olmamışken gate’i aşağıda tutar. Bu direnç olmazsa açılışta motor bir an dönebilir. 220 Ω, kapı kapasitesini şarj ederken GPIO akımını sınırlar.

### Diyot nasıl bağlanır

Diyot motora paraleldir. Motor akımının önüne seri konmaz. 1N5819’un gövdesindeki **şerit katottur**. Şerit **3,7 V** tarafına, şeritsiz uç **drain** tarafına bakar. Çizimdeki “tek yön akış” bu diyodun iletim yönüdür.

MOSFET açıkken akım şöyle gider:

`3,7 V → motor → drain → source → GND`

Bu anda katot 3,7 V’ta, anot ise neredeyse GND’dedir. Diyot ters kutupludur ve iletmez. Motoru diyot döndürmez.

MOSFET her PWM periyodunda kapandığında motor bobini akımı kesmek istemez. Drain gerilimi 3,7 V’un üstüne fırlar. Diyot bu anda doğru kutuplanır ve bobin akımı şu halkada söner:

`motor → diyot → 3,7 V → motor`

Darbe MOSFET’in drain’ine binmez. Diyot olmazsa kapanma anındaki gerilim SI230DS’yi deler. Diyot ters takılırsa 3,7 V, diyot üzerinden drain’e akar; MOSFET açılınca batarya diyot ve MOSFET üzerinden kısa devre olur.

Dört motorda her motorun üzerine aynı yönde bir diyot konur. Ortak diyot yetmez, çünkü her sargı kendi drain düğümünde ayrı ayrı söner.

## Dört motor

![Dört motor](https://raw.githubusercontent.com/Muhammed-Turgut/imageRaw/main/720-coreless-motor-driver-image-1.jpeg)

Dört SI230DS aynı **+3,7 V** hattını ve aynı **GND**’yi paylaşır.

- Her motorun artı ucu ortak 3,7 V hattındadır.
- Her drain kendi motorunun eksi ucuna gider.
- Her motorun diyotu, katot 3,7 V’ta ve anot kendi drain’inde olacak şekilde bağlanır.
- Dört source birlikte GND’ye iner.
- Her gate, 220 Ω üzerinden ayrı bir GPIO’ya gider. Her gate’in kendi 10 kΩ pull-down’ı vardır.

Kanallar birbirinden bağımsız açılıp kapanır. Ortak olan yalnız besleme ve şasedir. Mevcut yazılım tek kanal üretir. Dört motor için her kapıya ayrı PWM gerekir.

## PWM ve arayüz

ESP32 açılınca `720-Motor` ağını yayınlar.

| | |
| --- | --- |
| Ağ | `720-Motor` |
| Şifre | `motor720` |
| Adres | `http://192.168.4.1` |
| PWM pini | GPIO 25 |
| Frekans | 20 kHz |
| Çözünürlük | 10 bit |

Kaydırıcı 0–100 arası görev döngüsüdür. 0 kapalı, 100 sürekli açıktır. **DUR** hedefi sıfırlar. Açılışta çıkış kapalıdır. Hedef değişince görev döngüsü her 8 ms’de yüzde 1 değişir; 0’dan 100’e yaklaşık 0,8 saniyede çıkar.

20 kHz, duyulabilir bandın üstündedir. Çekirdeksiz motorda düşük frekanslı PWM hem öter hem bobini gereksiz ısıtır.

Ev ağına bağlamak için `src/main.cpp` içindeki `WIFI_SSID` ve `WIFI_PASS` doldurulur. Ağ 12 saniye içinde bulunamazsa yine aynı erişim noktası açılır.

## Yükleme

```bash
pio run -t upload
```

Seri monitör 115200 baud. Bağlanan adres açılışta oraya yazılır.
