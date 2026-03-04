[🇬🇧 English](README.md) | [🇹🇷 Türkçe](README.tr.md)

# Flood Tuber - OBS Eklentisi

**Flood Tuber**, yayınlarınıza hayat katan, hafif ve güçlü bir tepkisel PNGTuber avatar eklentisidir. Mikrofonunuzun ses seviyesini algılayarak avatarınızı konuşma, göz kırpma ve rastgele aksiyon durumlarıyla canlandırır, yayınlarınıza karmaşık kurulumlar olmadan dinamizm katar.

## Özellikler

*   **🎙️ Sese Duyarlı:** Mikrofon girişine göre otomatik olarak "Bekleme" (Idle) ve "Konuşma" (Talking) modları arasında geçiş yapar.
*   **🖼️ 3-Kare Konuşma Animasyonu:** Daha akıcı ve doğal konuşma efektleri için `Talk A`, `Talk B` ve `Talk C` karelerini destekler.
*   **📚 Avatar Kütüphanesi:** Hazır avatar setlerini (görseller + ayarlar) tek tıkla yükleyip değiştirebileceğiniz dahili kütüphane sistemi.
*   **👀 Otomatik Göz Kırpma:** Avatarınıza canlılık katmak için ayarlanabilir aralıklarla göz kırpma animasyonu.
*   **✨ Rastgele Aksiyonlar:** Belirli aralıklarla rastgele aksiyon animasyonları (emoji, özel poz vb.) oynatır.
*   **🌊 Hareket Efektleri (Motion):**
    *   **Shake (Titreme):** Konuşurken avatar titrer.
    *   **Bounce (Zıplama):** Konuşurken avatar yukarı aşağı zıplar.
    *   **None (Yok):** Sabit durur.
*   **🎞️ Gelişmiş Animasyon Desteği:** **APNG**, **WebP** ve **GIF** formatları için tam destek.
    *   **Akıllı Algılama:** Dosya uzantısı yanlış olsa bile (.png olarak kaydedilmiş APNG gibi) animasyonları otomatik algılar.
*   **⚡ Konuşma Animasyon Hızı:** Avatarınızın konuşma hızını (ağız hareketlerini) özel bir kaydırıcı ile ayarlayın.
*   **↔️ Aynalama (Mirror):** Tek bir kutucuk ile avatarınızı yatay olarak çevirebilirsiniz.
*   **🛠️ Avatara Özel Ayarlar:** Kütüphanedeki her avatarın hassasiyet, hız ve zamanlama ayarlarını içeren kendi `settings.ini` dosyası olabilir.

## Kurulum

Flood Tuber'ın en son sürümünü şuradan indirebilirsiniz:

*   **[Resmi OBS Kaynaklar Sayfası](https://obsproject.com/forum/resources/flood-tuber-native-pngtuber-plugin.2336/)**
*   **[GitHub Sürümler (Releases)](https://github.com/justflood/flood-tuber/releases/latest)** (Önerilen)

İki kurulum yöntemi vardır:

### Seçenek 1: Yükleyici (Önerilen)
1.  `FloodTuber-Installer-x.x.x.exe` dosyasını indirin.
2.  Yükleyiciyi çalıştırın.
    > **⚠️ "Windows kişisel bilgisayarınızı korudu" uyarısı hakkında not:**
    > Bu açık kaynaklı bir proje olduğundan, pahalı bir kod imzalama sertifikam bulunmamaktadır. Yükleyiciyi çalıştırdığınızda, Windows SmartScreen "Bilinmeyen Yayıncı" uyarısı gösterebilir.
    > Bu, açık kaynaklı yazılımlar için tamamen normaldir. Devam etmek için:
    > *   **"Ek Bilgi" (More Info)** seçeneğine tıklayın.
    > *   **"Yine de Çalıştır" (Run Anyway)** butonuna basın.
3.  Kurulum sihirbazındaki adımları takip edin.

### Seçenek 2: Taşınabilir / Zip (Manuel)
1.  `FloodTuber-Portable-vx.x.x.zip` dosyasını indirin.
2.  Dosyaları bir klasöre çıkarın.
3.  `flood-tuber.dll` dosyasını OBS eklenti klasörünüze kopyalayın (genellikle `C:\Program Files\obs-studio\obs-plugins\64bit`).
4.  `data/obs-plugins` klasörü içindeki `flood-tuber` klasörünü OBS veri dizinine kopyalayın (genellikle `C:\Program Files\obs-studio\data\obs-plugins\flood-tuber`).
5.  OBS Studio'yu yeniden başlatın.

## Kullanım

1.  Sahnenize yeni bir **"Flood Tuber Avatar"** kaynağı ekleyin.
2.  **Ses Kaynağını Seçin:** Özellikler listesinden mikrofonunuzu seçin.
3.  **Avatar Yükleyin:**
    *   "Avatar Library" (Avatar Kütüphanesi) altından bir hazır ayar seçin (örn: "Flood Tuber Avatar") ve **"Load & Apply Avatar"** (Yükle ve Uygula) butonuna basın.
    *   Veya Idle, Talk, Blink gibi görselleri manuel olarak seçin.
4.  **Özelleştirin:**
    *   **Threshold** (Eşik) ayarını ses seviyenize göre düzenleyin.
    *   Gerekirse **Mirror** (Aynala) ile avatarı çevirin.
    *   Bir **Motion Type** (Hareket Tipi) seçin ve şiddetini ayarlayın.

## Kendi Avatarınızı Oluşturma

1.  Özelliklerdeki **"Open Library Folder"** (Kütüphane Klasörünü Aç) butonuna tıklayın.
2.  Avatarınızın ismiyle yeni bir klasör oluşturun (örn: `Benim Havali Avatarim`).
3.  Görsellerinizi içine atın:
    *   `idle.png`
    *   `talk_a.png`, `talk_b.png`, `talk_c.png`
    *   `blink.png`
    *   `action.png`
    *   *(İsteğe bağlı)* Konuşurken göz kırpmak için `talk_a_blink.png` vb.
4.  *(İsteğe bağlı)* Varsayılan ayarları değiştirmek isterseniz başka bir avatardan `settings.ini` dosyasını kopyalayıp düzenleyin.
5.  OBS'de listeden yeni klasörünüzü seçin ve Yükle butonuna basın!

## Lisans

Bu proje GNU General Public License v2.0 (GPLv2) Lisansı altında lisanslanmıştır.
