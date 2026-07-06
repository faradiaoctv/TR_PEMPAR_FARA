# TR_PEMPAR_FARA
Tugas Rancang Matakuliah Pemprosesan Paralel (CE602) - Simulasi Hujan &amp; Angin 2D Real-Time menggunakan OpenMPI dan SDL2

## 1. Identitas Praktikan
* **Nama:** [Faradila Octavia Nabila]
* **NIM:** [622023016]
* **Mata Kuliah:** Praktikum Pemrosesan Paralel (CE602)

---

## 2. Setup Development Environment
Simulasi ini dikembangkan dan diuji pada sistem operasi **Ubuntu Linux** menggunakan bahasa pemrogramman C++ dengan library **OpenMPI** untuk pemrosesan paralel dan **SDL2 / SDL2_ttf** untuk visualisasi grafis berbasis window .

### A. Pemasangan Dependensi
Jalankan perintah berikut pada terminal Ubuntu untuk memasang kompilator C++, OpenMPI, library SDL2, serta font sistem:
```bash
sudo apt update
sudo apt install -y build-essential g++ openmpi-bin libopenmpi-dev libsdl2-dev libsdl2-ttf-dev fonts-dejavu-core

### B. Langkah Kompilasi
Kompilasi program menggunakan wrapper mpic++ dengan menambahkan flag library SDL2 dan SDL2_ttf
```bash
mpic++ -O2 src/rain_hud_mpi.cpp -o bin/rain_simulation -lSDL2 -lSDL2_ttf

---

##  3. Penjelasan Cara Kerja Program
Program yang saya buat melakukan render simulasi jatuh bebas dari 1.200 partikel air hujan pada layar grafis berukuran 800x600 piksel secara real-time.
* Aturan Perilaku Partikel : Setiap partikel tetesan hujan memiliki koordinat posisi (x, y), kecepatan jatuh vertikal (speed_y), dan panjang tetesan (length). Koordinat y mengatur pertambahan partikel sesuai dengan kecepatannya. Jika posisi partikel melewati batas bawah layar (y > 600), maka posisi akan kembali ke atas layar (y < 0) dengan koordinat x yang diacak kembali untuk menciptakan efek hujan yang kontinu dan alami.
* Fitur Kompleksitas Tambahan (Interaksi Angin) : Fitur fisika interaktif yang ditambahkan berupa dorongan angin secara horizontal (wind_speed) yang akan memiringkan arah jatuh air hujan dan dapat dikontrol oleh user.
* Heads-Up Display (HUD) : Menampilkan judul simulasi serta perhitungan FPS (Frames Per Second) secara dinamis yang memiliki 3 warna dan dapat berubah otomatis sesuai kondisinya, yaitu Hijau jika >= 30 FPS, Oranye jika 15-29 FPS, dan Merah jika < 15 FPS, hal ini berguna untuk memantau performa simulasi secara real-time.

 ## 4. Flowchart Program
```mermaid
flowchart TD
    Start([Mulai]) --> Init[MPI_Init & Inisialisasi SDL2 / SDL2_ttf<br><i>Hanya di Rank 0</i>]
    Init --> Workload[Hitung Pembagian Beban Kerja<br><i>local_n & my_offset di Tiap Rank</i>]
    Workload --> GenParticles[Rank 0: Inisialisasi & Acak 1.200 Partikel]
    GenParticles --> BcastStart[MPI_Bcast: Broadcast Data Partikel Awal ke Semua Worker]

    BcastStart --> LoopStart{Loop Frame Simulasi}

    LoopStart --> Input[Rank 0: Cek Event Keyboard<br><i>Kontrol Angin / Keluar</i>]
    Input --> BcastState[MPI_Bcast: Kirim Status 'running' & 'wind_speed']

    BcastState --> ParallelUpdate[style:ParallelUpdate fill:#e1f5fe,stroke:#0288d1,stroke-width:2px<br><b>PROSES PARALEL (Semua Rank)</b><br>Update Koordinat X, Y Partikel & Cek Batas Layar]
    ParallelUpdate --> GatherData[MPI_Gatherv: Kumpulkan Fragmen Partikel Kembali ke Rank 0]

    GatherData --> Render[Rank 0: Render Latar Belakang, Garis Hujan, & Teks HUD FPS]

    Render --> CheckRunning{Status 'running' Masih Aktif?}
    CheckRunning -- Ya --> LoopStart
    CheckRunning -- Tidak --> CleanSDL[Rank 0: Tutup Window & Cleanup Memory SDL/TTF]

    CleanSDL --> MPIFinal[MPI_Finalize]
    MPIFinal --> End([Selesai])

    style ParallelUpdate fill:#f9f9f1,stroke:#333,stroke-width:2px
