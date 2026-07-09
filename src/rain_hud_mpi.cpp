/*
 * SIMULASI HUJAN 2D DENGAN OPENMPI, SDL2, & HUD (FPS + JUDUL)
 * Faradila - Tugas Rancang Praktikum Pemrosesan Paralel
 */

#include <mpi.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>
#include <sstream>

// Konfigurasi Layar & Simulasi
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int NUM_PARTICLES = 1200;

// Struktur data partikel hujan
struct Particle {
    float x, y;
    float speed_y;
    float length;
};

// Fungsi untuk menginisialisasi partikel dengan nilai acak
void initParticles(std::vector<Particle>& particles) {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        particles[i].x = static_cast<float>(rand() % WINDOW_WIDTH);
        particles[i].y = static_cast<float>(rand() % WINDOW_HEIGHT);
        particles[i].speed_y = 6.0f + static_cast<float>(rand() % 10); // Kecepatan jatuh
        particles[i].length = 12.0f + static_cast<float>(rand() % 15);  // Panjang tetesan
    }
}

// Fungsi bantuan untuk merender teks ke layar SDL
void renderText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color) {
    if (!font || text.empty()) return;
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_Rect destRect = { x, y, surface->w, surface->h };
            SDL_RenderCopy(renderer, texture, nullptr, &destRect);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }
}

int main(int argc, char* argv[]) {
    // 1. Inisialisasi MPI
    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // 2. Perhitungan Pembagian Beban Kerja (Workload Distribution)
    int local_n = NUM_PARTICLES / world_size;
    int remainder = NUM_PARTICLES % world_size;
    
    int my_count = local_n + (world_rank < remainder ? 1 : 0);
    int my_offset = world_rank * local_n + (world_rank < remainder ? world_rank : remainder);

    // Persiapan array untuk MPI_Gatherv di Rank 0
    std::vector<int> recvcounts(world_size);
    std::vector<int> displs(world_size);
    for (int i = 0; i < world_size; i++) {
        int count = (NUM_PARTICLES / world_size) + (i < (NUM_PARTICLES % world_size) ? 1 : 0);
        recvcounts[i] = count * sizeof(Particle);
        displs[i] = (i * (NUM_PARTICLES / world_size) + (i < (NUM_PARTICLES % world_size) ? i : (NUM_PARTICLES % world_size))) * sizeof(Particle);
    }

    std::vector<Particle> particles(NUM_PARTICLES);

    if (world_rank == 0) {
        srand(static_cast<unsigned>(time(nullptr)));
        initParticles(particles);
    }

    // Broadcast data awal ke seluruh proses worker
    MPI_Bcast(particles.data(), NUM_PARTICLES * sizeof(Particle), MPI_BYTE, 0, MPI_COMM_WORLD);

    // 3. Inisialisasi SDL2 & SDL_ttf (Hanya Rank 0 / Master)
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* font = nullptr;

    if (world_rank == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0 || TTF_Init() < 0) {
            std::cerr << "SDL / TTF gagal inisialisasi: " << SDL_GetError() << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        window = SDL_CreateWindow("Simulasi Hujan 2D - OpenMPI", 
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                  WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

        // Memuat font sistem Ubuntu (DejaVuSans)
        font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 14);
        if (!font) {
            std::cerr << "Peringatan: Gagal memuat font, teks HUD tidak akan tampil." << std::endl;
        }
    }

    bool running = true;
    float wind_speed = 0.0f;

    // Variabel untuk menghitung FPS di Rank 0
    Uint32 last_fps_time = 0;
    int frame_count = 0;
    int current_fps = 60;

    // 4. Loop Utama Simulasi (Real-Time)
    while (running) {
        Uint32 frame_start_time = 0;
        if (world_rank == 0) {
            frame_start_time = SDL_GetTicks();
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                } else if (event.type == SDL_KEYDOWN) {
                    // Panah Kiri/Kanan untuk mengubah angin
                    if (event.key.keysym.sym == SDLK_LEFT) wind_speed -= 1.0f;
                    if (event.key.keysym.sym == SDLK_RIGHT) wind_speed += 1.0f;
                    if (event.key.keysym.sym == SDLK_DOWN) wind_speed = 0.0f; // Reset
                }
            }
        }

        // Broadcast status aplikasi & angin ke seluruh proses worker
        MPI_Bcast(&running, 1, MPI_CXX_BOOL, 0, MPI_COMM_WORLD);
        if (!running) break;
        MPI_Bcast(&wind_speed, 1, MPI_FLOAT, 0, MPI_COMM_WORLD);

        // --- PROSES PARALEL (Update Fisika Partikel di tiap Rank) ---
        for (int i = my_offset; i < my_offset + my_count; i++) {
            particles[i].y += particles[i].speed_y;
            particles[i].x += wind_speed; // Efek dorongan angin horizontal

            // Reset partikel jika keluar layar bawah/samping
            if (particles[i].y > WINDOW_HEIGHT) {
                particles[i].y = -particles[i].length;
                particles[i].x = static_cast<float>(rand() % WINDOW_WIDTH);
            }
            if (particles[i].x > WINDOW_WIDTH) particles[i].x = 0;
            if (particles[i].x < 0) particles[i].x = WINDOW_WIDTH;
        }

        // --- PENGUMPULAN DATA (GATHER) ---
        MPI_Gatherv(&particles[my_offset], my_count * sizeof(Particle), MPI_BYTE,
                    particles.data(), recvcounts.data(), displs.data(), MPI_BYTE,
                    0, MPI_COMM_WORLD);

        // --- RENDERING GRAFIS & HUD (Hanya Rank 0) ---
        if (world_rank == 0) {
            // Latar belakang langit malam gelap
            SDL_SetRenderDrawColor(renderer, 10, 15, 25, 255);
            SDL_RenderClear(renderer);

            // 1. Gambar Tetesan Hujan
            SDL_SetRenderDrawColor(renderer, 160, 200, 255, 200);
            for (const auto& p : particles) {
                float end_x = p.x + (wind_speed * 1.5f);
                float end_y = p.y + p.length;
                SDL_RenderDrawLine(renderer, static_cast<int>(p.x), static_cast<int>(p.y), 
                                             static_cast<int>(end_x), static_cast<int>(end_y));
            }

            // 2. Hitung FPS
            frame_count++;
            Uint32 current_time = SDL_GetTicks();
            if (current_time - last_fps_time >= 1000) {
                current_fps = frame_count;
                frame_count = 0;
                last_fps_time = current_time;
            }

            // 3. Gambar HUD (FPS & Judul ala Video)
            if (font) {
                // Tentukan warna FPS (Hijau jika lancar, Oranye jika sedang, Merah jika lag)
                SDL_Color fps_color = {0, 255, 0, 255}; // Hijau
                if (current_fps < 30) fps_color = {255, 165, 0, 255}; // Oranye
                if (current_fps < 15) fps_color = {255, 0, 0, 255};   // Merah

                std::stringstream fps_ss;
                fps_ss << current_fps << " FPS";
                
                // Render FPS di (x: 10, y: 10)
                renderText(renderer, font, fps_ss.str(), 10, 10, fps_color);

                // Render Judul di bawah FPS (x: 10, y: 30)
                SDL_Color title_color = {220, 220, 220, 255}; // Putih Terang / Abu-abu
                renderText(renderer, font, "OpenMPI Rain & Wind Particle Simulation", 10, 30, title_color);
            }

            SDL_RenderPresent(renderer);

            // Batasi hingga ~60 FPS agar stabil
            Uint32 frame_duration = SDL_GetTicks() - frame_start_time;
            if (frame_duration < 16) {
                SDL_Delay(16 - frame_duration);
            }
        }
    }

    // 5. Pembersihan Memori
    if (world_rank == 0) {
        if (font) TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
    }

    MPI_Finalize();
    return 0;
}
