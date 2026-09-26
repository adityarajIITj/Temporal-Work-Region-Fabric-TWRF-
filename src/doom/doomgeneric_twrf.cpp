#include "doomgeneric.h"
#include "doomkeys.h"
#include "twrf/core/types.hpp"
#include "twrf/core/state_store.hpp"
#include "twrf/core/scheduler.hpp"
#include "twrf/core/change_tracker.hpp"
#include "twrf/core/cost_model.hpp"
#include "twrf/timing/timing_params.hpp"
#include "twrf/timing/cycle_accounting.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <cmath>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

namespace {

// TWRF-DOOM Configuration
constexpr uint32_t DOOM_W = DOOMGENERIC_RESX; // Typically 640 or 320
constexpr uint32_t DOOM_H = DOOMGENERIC_RESY; // Typically 400 or 200
constexpr uint32_t TILE_SIZE = 16;            // 16x16 pixel TWR units

const uint32_t TILES_X = (DOOM_W + TILE_SIZE - 1) / TILE_SIZE;
const uint32_t TILES_Y = (DOOM_H + TILE_SIZE - 1) / TILE_SIZE;
const uint32_t TOTAL_TILES = TILES_X * TILES_Y;

// HUD scanlines (bottom 16% of screen)
const uint32_t HUD_START_Y = DOOM_H - (DOOM_H * 32 / 200);

struct TileStats {
    uint32_t executed_count = 0;
    uint32_t skipped_count = 0;
};

struct TWRFDoomContext {
    bool initialized = false;
    std::chrono::steady_clock::time_point start_time;
    uint64_t frame_index = 0;

    // TWRF Core Components
    twrf::LogicalStateStore state_store;
    twrf::timing::TimingParameters timing_params;

    // Per-tile shadow buffers (for change detection against previous frame)
    std::vector<std::vector<uint32_t>> cached_tiles;
    std::vector<TileStats> tile_stats;

    // Benchmark Execution Limits
    uint64_t max_benchmark_frames = 150;
    bool headless_benchmark = false;
    bool export_ppms = false;

    // Cumulative Accounting
    uint64_t cum_tiles_executed = 0;
    uint64_t cum_tiles_skipped = 0;
    uint64_t cum_hud_executed = 0;
    uint64_t cum_hud_skipped = 0;
    double cum_twrf_cycles = 0.0;
    double cum_baseline_cycles = 0.0;

    // Frame-by-frame history for JSON export
    struct FrameRecord {
        uint64_t frame;
        uint32_t executed;
        uint32_t skipped;
        double skip_ratio;
        double twrf_cycles;
        double baseline_cycles;
        bool hud_static;
    };
    std::vector<FrameRecord> history;
} g_ctx;

#if defined(_WIN32) || defined(_WIN64)
static BITMAPINFO s_Bmi = { { sizeof(BITMAPINFOHEADER), (LONG)DOOMGENERIC_RESX, -(LONG)DOOMGENERIC_RESY, 1, 32, BI_RGB, 0, 0, 0, 0, 0 }, { { 0, 0, 0, 0 } } };
static HWND s_Hwnd = nullptr;
static HDC s_Hdc = nullptr;

#define KEYQUEUE_SIZE 32
static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(unsigned char key) {
    switch (key) {
    case VK_RETURN: return KEY_ENTER;
    case VK_ESCAPE: return KEY_ESCAPE;
    case VK_LEFT:   return KEY_LEFTARROW;
    case VK_RIGHT:  return KEY_RIGHTARROW;
    case VK_UP:     return KEY_UPARROW;
    case VK_DOWN:   return KEY_DOWNARROW;
    case VK_CONTROL:return KEY_FIRE;
    case VK_SPACE:  return KEY_USE;
    case VK_SHIFT:  return KEY_RSHIFT;
    case 'W': case 'w': return KEY_UPARROW;
    case 'S': case 's': return KEY_DOWNARROW;
    case 'A': case 'a': return KEY_STRAFE_L;
    case 'D': case 'd': return KEY_STRAFE_R;
    default:
        return static_cast<unsigned char>(tolower(key));
    }
}

static void addKeyToQueue(int pressed, unsigned char keyCode) {
    unsigned char key = convertToDoomKey(keyCode);
    unsigned short keyData = static_cast<unsigned short>((pressed << 8) | key);
    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex = (s_KeyQueueWriteIndex + 1) % KEYQUEUE_SIZE;
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        ExitProcess(0);
        return 0;
    case WM_KEYDOWN:
        addKeyToQueue(1, static_cast<unsigned char>(wParam));
        return 0;
    case WM_KEYUP:
        addKeyToQueue(0, static_cast<unsigned char>(wParam));
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}
#endif

void save_ppm(const std::string& filename, const uint32_t* buffer, uint32_t width, uint32_t height) {
    std::filesystem::create_directories("results");
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) return;
    ofs << "P6\n" << width << " " << height << "\n255\n";
    std::vector<uint8_t> rgb(width * height * 3);
    for (size_t i = 0; i < width * height; ++i) {
        uint32_t pixel = buffer[i];
        rgb[i * 3 + 0] = static_cast<uint8_t>((pixel >> 16) & 0xFF);
        rgb[i * 3 + 1] = static_cast<uint8_t>((pixel >> 8) & 0xFF);
        rgb[i * 3 + 2] = static_cast<uint8_t>(pixel & 0xFF);
    }
    ofs.write(reinterpret_cast<const char*>(rgb.data()), rgb.size());
}

} // anonymous namespace

extern "C" {

void DG_Init() {
    g_ctx.start_time = std::chrono::steady_clock::now();
    g_ctx.frame_index = 0;

    // Initialize State Store capacity: 16x16 tiles * 4 bytes per pixel * total_tiles
    size_t tile_bytes = TILE_SIZE * TILE_SIZE * sizeof(uint32_t);
    g_ctx.state_store = twrf::LogicalStateStore(TOTAL_TILES * tile_bytes * 2);

    g_ctx.cached_tiles.resize(TOTAL_TILES, std::vector<uint32_t>(TILE_SIZE * TILE_SIZE, 0));
    g_ctx.tile_stats.resize(TOTAL_TILES);

    for (uint32_t i = 0; i < TOTAL_TILES; ++i) {
        g_ctx.state_store.allocate_slot(i, tile_bytes);
    }

    g_ctx.initialized = true;

    if (!g_ctx.headless_benchmark) {
#if defined(_WIN32) || defined(_WIN64)
        const char windowClassName[] = "TWRFDoomWindowClass";
        const char windowTitle[] = "DOOM (1993) on TWRF Virtual GPU";
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = wndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = windowClassName;
        RegisterClassExA(&wc);

        int scale = 2; // 1280 x 800 window
        int client_w = DOOM_W * scale;
        int client_h = DOOM_H * scale;
        RECT rect = { 0, 0, client_w, client_h };
        AdjustWindowRect(&rect, (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX), FALSE);

        s_Hwnd = CreateWindowExA(0, windowClassName, windowTitle, 
                                 (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX) | WS_VISIBLE, 
                                 CW_USEDEFAULT, CW_USEDEFAULT, 
                                 rect.right - rect.left, rect.bottom - rect.top, 
                                 nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        if (s_Hwnd) {
            s_Hdc = GetDC(s_Hwnd);
            ShowWindow(s_Hwnd, SW_SHOW);
            UpdateWindow(s_Hwnd);
        }
#endif
    }

    std::cout << "========================================================================\n"
              << "           TWRF-DOOM: TEMPORAL WORK REGION FABRIC DOOM PORT             \n"
              << "========================================================================\n"
              << "  Mode:             " << (g_ctx.headless_benchmark ? "Automated Headless Benchmark" : "Interactive Windowed Gameplay") << "\n"
              << "  Resolution:       " << DOOM_W << " x " << DOOM_H << "\n"
              << "  Tile Grid:        " << TILES_X << " x " << TILES_Y << " (" << TOTAL_TILES << " total TWR tiles)\n"
              << "  Tile Dimensions:  " << TILE_SIZE << " x " << TILE_SIZE << " pixels\n"
              << "  State Store Size: " << (TOTAL_TILES * tile_bytes / 1024) << " KB persistent on-chip SRAM\n"
              << "  Status Bar (HUD): Rows " << (HUD_START_Y / TILE_SIZE) << "-" << (TILES_Y - 1) 
              << " (Persistently Cached)\n"
              << "------------------------------------------------------------------------\n";
    if (!g_ctx.headless_benchmark) {
        std::cout << "  Controls:\n"
                  << "    * Move:         Arrow Keys or W/A/S/D\n"
                  << "    * Fire:         Ctrl\n"
                  << "    * Use / Open:   Space\n"
                  << "    * Strafe:       A / D\n"
                  << "    * Run:          Shift\n"
                  << "    * Menu / Exit:  Esc\n"
                  << "------------------------------------------------------------------------\n\n";
    }
}

void DG_DrawFrame() {
    if (!g_ctx.initialized || !DG_ScreenBuffer) return;

    ++g_ctx.frame_index;
    uint32_t frame_executed = 0;
    uint32_t frame_skipped = 0;
    uint32_t hud_executed = 0;
    uint32_t hud_skipped = 0;

    // Check each tile in the grid against the TWRF Persistent State Store
    for (uint32_t ty = 0; ty < TILES_Y; ++ty) {
        for (uint32_t tx = 0; tx < TILES_X; ++tx) {
            uint32_t tile_id = ty * TILES_X + tx;
            uint32_t start_x = tx * TILE_SIZE;
            uint32_t start_y = ty * TILE_SIZE;
            uint32_t end_x = std::min(start_x + TILE_SIZE, DOOM_W);
            uint32_t end_y = std::min(start_y + TILE_SIZE, DOOM_H);

            bool is_hud = (start_y >= HUD_START_Y);
            bool is_dirty = false;

            auto& cached = g_ctx.cached_tiles[tile_id];
            size_t pixel_idx = 0;

            for (uint32_t y = start_y; y < end_y; ++y) {
                for (uint32_t x = start_x; x < end_x; ++x) {
                    uint32_t current_pixel = DG_ScreenBuffer[y * DOOM_W + x];
                    if (current_pixel != cached[pixel_idx]) {
                        is_dirty = true;
                        cached[pixel_idx] = current_pixel;
                    }
                    ++pixel_idx;
                }
            }

            if (g_ctx.frame_index == 1) {
                is_dirty = true;
            }

            if (is_dirty) {
                ++frame_executed;
                g_ctx.tile_stats[tile_id].executed_count++;
                if (is_hud) ++hud_executed;

                g_ctx.state_store.write_output(tile_id, cached.data(), 
                                               pixel_idx * sizeof(uint32_t), 
                                               g_ctx.frame_index);
            } else {
                ++frame_skipped;
                g_ctx.tile_stats[tile_id].skipped_count++;
                if (is_hud) ++hud_skipped;
            }
        }
    }

    double skip_ratio = static_cast<double>(frame_skipped) / TOTAL_TILES;
    const double C_r_tile = 250.0;
    const double baseline_frame_cycles = TOTAL_TILES * C_r_tile;
    const double C_t_tile = g_ctx.timing_params.cycles_version_check * 2.0 
                          + (TILE_SIZE * TILE_SIZE * sizeof(uint32_t) * g_ctx.timing_params.cycles_state_store_read_byte * 0.1);
    double twrf_frame_cycles = (TOTAL_TILES * C_t_tile) + (frame_executed * C_r_tile);

    g_ctx.cum_tiles_executed += frame_executed;
    g_ctx.cum_tiles_skipped += frame_skipped;
    g_ctx.cum_hud_executed += hud_executed;
    g_ctx.cum_hud_skipped += hud_skipped;
    g_ctx.cum_twrf_cycles += twrf_frame_cycles;
    g_ctx.cum_baseline_cycles += baseline_frame_cycles;

    bool hud_static = (hud_executed == 0);

    g_ctx.history.push_back({
        g_ctx.frame_index,
        frame_executed,
        frame_skipped,
        skip_ratio,
        twrf_frame_cycles,
        baseline_frame_cycles,
        hud_static
    });

    // Interactive window update
    if (!g_ctx.headless_benchmark) {
#if defined(_WIN32) || defined(_WIN64)
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE) > 0) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (s_Hdc && DG_ScreenBuffer) {
            StretchDIBits(s_Hdc, 0, 0, DOOM_W * 2, DOOM_H * 2, 
                          0, 0, DOOM_W, DOOM_H, 
                          DG_ScreenBuffer, &s_Bmi, DIB_RGB_COLORS, SRCCOPY);
        }

        if (g_ctx.frame_index % 10 == 0 && s_Hwnd) {
            char title_buf[256];
            snprintf(title_buf, sizeof(title_buf), 
                     "DOOM [TWRF Virtual GPU] - Frame: %llu | Skipped Tiles: %.1f%% | HUD: %s",
                     (unsigned long long)g_ctx.frame_index, 
                     skip_ratio * 100.0, 
                     hud_static ? "100% CACHED" : "UPDATED");
            SetWindowTextA(s_Hwnd, title_buf);
        }
#endif
    } else {
        // Logging milestones in benchmark mode
        if (g_ctx.frame_index == 1 || g_ctx.frame_index % 25 == 0 || g_ctx.frame_index == g_ctx.max_benchmark_frames) {
            std::cout << "  [FRAME " << std::setw(3) << std::setfill('0') << g_ctx.frame_index << "] "
                      << "Executed: " << std::setw(3) << std::setfill(' ') << frame_executed << " / " << TOTAL_TILES
                      << " | Skipped: " << std::setw(3) << frame_skipped 
                      << " (" << std::fixed << std::setprecision(1) << (skip_ratio * 100.0) << "% reuse)"
                      << " | HUD: " << (hud_static ? "STATIC (100%)" : "UPDATED")
                      << std::endl;
        }

        if (g_ctx.export_ppms) {
            if (g_ctx.frame_index == 1) {
                save_ppm("results/doom_frame0_cold.ppm", DG_ScreenBuffer, DOOM_W, DOOM_H);
            } else if (g_ctx.frame_index == 25) {
                save_ppm("results/doom_frame25_gameplay.ppm", DG_ScreenBuffer, DOOM_W, DOOM_H);
            } else if (g_ctx.frame_index == 100) {
                save_ppm("results/doom_frame100_action.ppm", DG_ScreenBuffer, DOOM_W, DOOM_H);
            }
        }

        if (g_ctx.frame_index >= g_ctx.max_benchmark_frames) {
            std::cout << "\n========================================================================\n"
                      << "                 TWRF-DOOM BENCHMARK SWEEP COMPLETE                     \n"
                      << "========================================================================\n";

            double total_tiles_all = static_cast<double>(g_ctx.cum_tiles_executed + g_ctx.cum_tiles_skipped);
            double avg_skip_ratio = (g_ctx.cum_tiles_skipped / total_tiles_all) * 100.0;
            double hud_skip_ratio = (g_ctx.cum_hud_skipped / static_cast<double>(g_ctx.cum_hud_executed + g_ctx.cum_hud_skipped)) * 100.0;
            double overall_speedup = g_ctx.cum_baseline_cycles / g_ctx.cum_twrf_cycles;

            std::cout << "  Total Simulated Frames:   " << g_ctx.frame_index << "\n"
                      << "  Total Tiles Evaluated:    " << (g_ctx.cum_tiles_executed + g_ctx.cum_tiles_skipped) << "\n"
                      << "  Total Tiles Skipped:      " << g_ctx.cum_tiles_skipped << " (" << std::fixed << std::setprecision(2) << avg_skip_ratio << "% reuse)\n"
                      << "  Total Tiles Executed:     " << g_ctx.cum_tiles_executed << "\n"
                      << "  Status Bar (HUD) Reuse:   " << std::fixed << std::setprecision(1) << hud_skip_ratio << "% temporal persistence\n"
                      << "  TWRF Simulated Cycles:    " << std::fixed << std::setprecision(0) << g_ctx.cum_twrf_cycles << "\n"
                      << "  Baseline Full Recompute:  " << std::fixed << std::setprecision(0) << g_ctx.cum_baseline_cycles << "\n"
                      << "  Measured Speedup:         " << std::fixed << std::setprecision(2) << overall_speedup << "x\n"
                      << "------------------------------------------------------------------------\n";

            std::filesystem::create_directories("results");
            std::ofstream json_out("results/doom_twrf_benchmark.json");
            if (json_out) {
                json_out << "{\n"
                         << "  \"total_frames\": " << g_ctx.frame_index << ",\n"
                         << "  \"resolution\": {\"width\": " << DOOM_W << ", \"height\": " << DOOM_H << "},\n"
                         << "  \"tile_grid\": {\"tiles_x\": " << TILES_X << ", \"tiles_y\": " << TILES_Y << ", \"total\": " << TOTAL_TILES << "},\n"
                         << "  \"cum_tiles_executed\": " << g_ctx.cum_tiles_executed << ",\n"
                         << "  \"cum_tiles_skipped\": " << g_ctx.cum_tiles_skipped << ",\n"
                         << "  \"avg_skip_ratio\": " << (avg_skip_ratio / 100.0) << ",\n"
                         << "  \"hud_reuse_ratio\": " << (hud_skip_ratio / 100.0) << ",\n"
                         << "  \"cum_twrf_cycles\": " << g_ctx.cum_twrf_cycles << ",\n"
                         << "  \"cum_baseline_cycles\": " << g_ctx.cum_baseline_cycles << ",\n"
                         << "  \"speedup\": " << overall_speedup << ",\n"
                         << "  \"frames\": [\n";
                for (size_t i = 0; i < g_ctx.history.size(); ++i) {
                    const auto& rec = g_ctx.history[i];
                    json_out << "    {\"frame\": " << rec.frame 
                             << ", \"executed\": " << rec.executed 
                             << ", \"skipped\": " << rec.skipped 
                             << ", \"skip_ratio\": " << rec.skip_ratio 
                             << ", \"hud_static\": " << (rec.hud_static ? "true" : "false") 
                             << "}" << (i + 1 < g_ctx.history.size() ? "," : "") << "\n";
                }
                json_out << "  ]\n}\n";
                json_out.flush();
                json_out.close();
                std::cout << "  [INFO] Benchmark JSON exported to: results/doom_twrf_benchmark.json\n";
            }
            std::cout << "========================================================================\n\n";

            std::exit(0);
        }
    }
}

void DG_SleepMs(uint32_t ms) {
    if (!g_ctx.headless_benchmark && ms > 0) {
#if defined(_WIN32) || defined(_WIN64)
        Sleep(ms);
#endif
    }
}

uint32_t DG_GetTicksMs() {
#if defined(_WIN32) || defined(_WIN64)
    if (!g_ctx.headless_benchmark) {
        return static_cast<uint32_t>(GetTickCount());
    }
#endif
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - g_ctx.start_time).count()
    );
}

int DG_GetKey(int* pressed, unsigned char* key) {
    if (g_ctx.headless_benchmark) {
        *pressed = 0;
        *key = 0;
        return 0;
    }
#if defined(_WIN32) || defined(_WIN64)
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) {
        return 0;
    }
    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex = (s_KeyQueueReadIndex + 1) % KEYQUEUE_SIZE;
    *pressed = keyData >> 8;
    *key = static_cast<unsigned char>(keyData & 0xFF);
    return 1;
#else
    *pressed = 0;
    *key = 0;
    return 0;
#endif
}

void DG_SetWindowTitle(const char* title) {
    (void)title;
}

} // extern "C"

int main(int argc, char** argv) {
    std::vector<std::string> args_vec;
    for (int i = 0; i < argc; ++i) {
        args_vec.push_back(argv[i]);
    }

    bool is_bench = false;
    std::vector<std::string> filtered_args;
    for (const auto& a : args_vec) {
        if (a == "--bench" || a == "-bench" || a == "--benchmark") {
            is_bench = true;
        } else {
            filtered_args.push_back(a);
        }
    }

    g_ctx.headless_benchmark = is_bench;
    g_ctx.export_ppms = is_bench;

    // Ensure WAD file is provided if not passed explicitly
    bool has_iwad = false;
    for (size_t i = 0; i < filtered_args.size(); ++i) {
        if (filtered_args[i] == "-iwad") {
            has_iwad = true;
            break;
        }
    }
    if (!has_iwad) {
        filtered_args.push_back("-iwad");
        filtered_args.push_back("doom1.wad");
    }

    // In benchmark mode, default to timedemo demo1
    if (g_ctx.headless_benchmark) {
        bool has_demo = false;
        for (size_t i = 0; i < filtered_args.size(); ++i) {
            if (filtered_args[i] == "-timedemo" || filtered_args[i] == "-playdemo" || filtered_args[i] == "-warp") {
                has_demo = true;
                break;
            }
        }
        if (!has_demo) {
            filtered_args.push_back("-timedemo");
            filtered_args.push_back("demo1");
        }
    }

    // Convert back to char* argv array
    std::vector<char*> c_argv;
    for (auto& s : filtered_args) {
        c_argv.push_back(const_cast<char*>(s.c_str()));
    }
    int c_argc = static_cast<int>(c_argv.size());

    doomgeneric_Create(c_argc, c_argv.data());

    while (true) {
        doomgeneric_Tick();
    }

    return 0;
}
