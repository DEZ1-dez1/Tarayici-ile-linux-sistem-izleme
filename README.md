# 🟢 Linux Sistem İzleme Sunucusu

Harici hiçbir kütüphane (framework) kullanmaksızın, tamamen **saf C diliyle** sıfırdan yazılmış; Linux çekirdeğinin sanal dosya sistemini (`/proc` ve `/sys`) doğrudan hedefleyen yüksek performanslı ve hafif bir gömülü sistem izleme web sunucusudur.

Özellikle **Termux / Android** gibi mobil Linux ortamlarında, düşük kaynak tüketimiyle tam performans çalışacak şekilde optimize edilmiştir.

## ✨ Öne Çıkan Özellikler

* **Sıfır Bağımlılık (Zero-Dependency):** Apache, Nginx veya ağır HTTP kütüphaneleri yok; doğrudan ham TCP soketleri (`socket`, `bind`, `listen`, `accept`).
* **Asenkron AJAX / Fetch Mimarisi:** Sayfa zıplamalarına (flicker) son! Arayüz arka planda `/data` uç noktasından verileri günceller, tarayıcı kaydırma (scroll) pozisyonu bozulmaz.
* **Genişletilmiş Veri Havuzu:** İşlemci yükü, bellek durumu, disk G/Ç sayaçları, ağ trafiği ve donanım sıcaklıkları dahil **42 farklı sistem kaynağı**.
* **Master Dashboard:** Kritik sistem metriklerini tek bir ekranda, şık bir grid (ızgara) yapısında eşzamanlı izleme imkanı.
* **Bellek Güvenliği:** Dinamik tampon yönetimi (`realloc`), taşma koruması (`buffer overflow checks`) ve `SIGPIPE` hata toleransı.
* **Manuel / Otomatik Kontrol:** Tarayıcı üzerinden otomatik yenilemeyi durdurup başlatma (`localStorage` desteği ile kalıcı tercih).

## 🚀 Derleme ve Çalıştırma

Depoyu klonladıktan sonra aşadaki kodu düzenledikten sonra konsola yapıştıştıra bilirsiniz.

```
bash
gcc dosya_adi.c -o dosya.adi

./dosya_adi

