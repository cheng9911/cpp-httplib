// Stress Test Client for cpp-httplib Robot Controller
// Simulates multiple UI pages connecting to the robot controller

#include "../httplib.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

struct Stats {
    std::mutex mtx;
    std::vector<double> latencies_us;
    std::atomic<int> success{0};
    std::atomic<int> failure{0};
    std::atomic<int> timeout{0};
    std::atomic<int> connection_error{0};

    void record(double latency_us, bool ok) {
        std::lock_guard<std::mutex> lock(mtx);
        latencies_us.push_back(latency_us);
        if (ok) success++;
        else failure++;
    }

    void print_summary(const std::string &label, int duration_s) {
        std::lock_guard<std::mutex> lock(mtx);
        if (latencies_us.empty()) {
            printf("  [%s] No data\n", label.c_str());
            return;
        }
        std::sort(latencies_us.begin(), latencies_us.end());
        double sum = std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0);
        double avg = sum / latencies_us.size();
        double p50 = latencies_us[latencies_us.size() * 50 / 100];
        double p95 = latencies_us[latencies_us.size() * 95 / 100];
        double p99 = latencies_us[latencies_us.size() * 99 / 100];
        double min_v = latencies_us.front();
        double max_v = latencies_us.back();
        double rps = (double)latencies_us.size() / duration_s;

        printf("  [%s] Requests: %zu | RPS: %.1f\n", label.c_str(), latencies_us.size(), rps);
        printf("         Success: %d | Failure: %d | ConnErr: %d\n",
               success.load(), failure.load(), connection_error.load());
        printf("         Latency (us): avg=%.0f p50=%.0f p95=%.0f p99=%.0f min=%.0f max=%.0f\n",
               avg, p50, p95, p99, min_v, max_v);
        printf("         Latency (ms): avg=%.2f p50=%.2f p95=%.2f p99=%.2f\n",
               avg / 1000, p50 / 1000, p95 / 1000, p99 / 1000);
    }
};

// Simulate a single UI page that polls robot status and sends commands
void run_ui_page(int page_id, const std::string &host, int port,
                 int duration_s, Stats &status_stats, Stats &joint_stats,
                 Stats &command_stats, Stats &sensor_stats, Stats &map_stats) {
    httplib::Client cli(host, port);
    cli.set_connection_timeout(5);
    cli.set_read_timeout(10);
    cli.set_keep_alive(true);

    std::mt19937 rng(page_id);
    std::uniform_int_distribution<> cmd_dist(0, 10);  // 10% chance of sending command
    std::uniform_int_distribution<> map_dist(0, 50);   // 2% chance of requesting map

    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::seconds(duration_s);
    int request_count = 0;

    while (std::chrono::steady_clock::now() < end) {
        auto t0 = std::chrono::steady_clock::now();

        // 1. Poll status (every iteration)
        {
            auto t1 = std::chrono::steady_clock::now();
            auto res = cli.Get("/api/status");
            auto t2 = std::chrono::steady_clock::now();
            double us = std::chrono::duration<double, std::micro>(t2 - t1).count();
            if (res) {
                status_stats.record(us, res->status == 200);
            } else {
                status_stats.failure++;
                status_stats.connection_error++;
            }
        }

        // 2. Poll joints (every iteration)
        {
            auto t1 = std::chrono::steady_clock::now();
            auto res = cli.Get("/api/joints");
            auto t2 = std::chrono::steady_clock::now();
            double us = std::chrono::duration<double, std::micro>(t2 - t1).count();
            if (res) {
                joint_stats.record(us, res->status == 200);
            } else {
                joint_stats.failure++;
                joint_stats.connection_error++;
            }
        }

        // 3. Occasionally send a command
        if (cmd_dist(rng) == 0) {
            auto t1 = std::chrono::steady_clock::now();
            auto res = cli.Post("/api/command", "{\"type\":\"gripper\",\"action\":\"close\"}", "application/json");
            auto t2 = std::chrono::steady_clock::now();
            double us = std::chrono::duration<double, std::micro>(t2 - t1).count();
            if (res) {
                command_stats.record(us, res->status == 200);
            } else {
                command_stats.failure++;
                command_stats.connection_error++;
            }
        }

        // 4. Occasionally request sensor data
        if (cmd_dist(rng) <= 2) {
            auto t1 = std::chrono::steady_clock::now();
            auto res = cli.Get("/api/sensors");
            auto t2 = std::chrono::steady_clock::now();
            double us = std::chrono::duration<double, std::micro>(t2 - t1).count();
            if (res) {
                sensor_stats.record(us, res->status == 200);
            } else {
                sensor_stats.failure++;
                sensor_stats.connection_error++;
            }
        }

        // 5. Rarely request map (large payload)
        if (map_dist(rng) == 0) {
            auto t1 = std::chrono::steady_clock::now();
            auto res = cli.Get("/api/map");
            auto t2 = std::chrono::steady_clock::now();
            double us = std::chrono::duration<double, std::micro>(t2 - t1).count();
            if (res) {
                map_stats.record(us, res->status == 200);
            } else {
                map_stats.failure++;
                map_stats.connection_error++;
            }
        }

        request_count++;

        // Polling interval: ~50ms (20Hz per UI page, realistic for robot control)
        auto elapsed = std::chrono::steady_clock::now() - t0;
        auto remaining = std::chrono::milliseconds(50) - elapsed;
        if (remaining > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(remaining);
        }
    }

    printf("  UI Page %d finished: %d request cycles\n", page_id, request_count);
}

void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -h <host>    Server host (default: 127.0.0.1)\n");
    printf("  -p <port>    Server port (default: 8080)\n");
    printf("  -n <count>   Number of concurrent UI pages (default: 10)\n");
    printf("  -d <seconds> Test duration in seconds (default: 30)\n");
    printf("  --help       Show this help\n");
}

int main(int argc, char *argv[]) {
    std::string host = "127.0.0.1";
    int port = 8080;
    int num_pages = 10;
    int duration = 30;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc) host = argv[++i];
        else if (arg == "-p" && i + 1 < argc) port = std::atoi(argv[++i]);
        else if (arg == "-n" && i + 1 < argc) num_pages = std::atoi(argv[++i]);
        else if (arg == "-d" && i + 1 < argc) duration = std::atoi(argv[++i]);
        else if (arg == "--help") { print_usage(argv[0]); return 0; }
        else { printf("Unknown option: %s\n", arg.c_str()); print_usage(argv[0]); return 1; }
    }

    printf("=== cpp-httplib Stress Test ===\n");
    printf("Server:         %s:%d\n", host.c_str(), port);
    printf("UI Pages:       %d\n", num_pages);
    printf("Duration:       %d seconds\n", duration);
    printf("Polling Rate:   20Hz per page (~%d Hz total)\n", num_pages * 20);
    printf("===============================\n\n");

    // Check server health first
    {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(3);
        auto res = cli.Get("/api/health");
        if (!res || res->status != 200) {
            printf("ERROR: Server not reachable at %s:%d\n", host.c_str(), port);
            return 1;
        }
        printf("Server health check: OK\n\n");
    }

    Stats status_stats, joint_stats, command_stats, sensor_stats, map_stats;

    printf("Starting stress test...\n\n");

    auto test_start = std::chrono::steady_clock::now();

    // Launch UI page threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_pages; i++) {
        threads.emplace_back(run_ui_page, i, host, port, duration,
                             std::ref(status_stats), std::ref(joint_stats),
                             std::ref(command_stats), std::ref(sensor_stats),
                             std::ref(map_stats));
    }

    // Progress reporting
    std::thread progress_thread([&]() {
        while (true) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - test_start).count();
            if (elapsed >= duration) break;
            int total = status_stats.success + status_stats.failure;
            printf("\r  Progress: %lds / %ds | Requests so far: %d", elapsed, duration, total);
            fflush(stdout);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    for (auto &t : threads) t.join();
    progress_thread.join();

    auto test_end = std::chrono::steady_clock::now();
    int actual_duration = std::chrono::duration_cast<std::chrono::seconds>(test_end - test_start).count();

    printf("\n\n=== RESULTS ===\n\n");
    printf("Actual duration: %ds\n\n", actual_duration);
    printf("--- Per-Endpoint Stats ---\n");
    status_stats.print_summary("GET /api/status  ", actual_duration);
    joint_stats.print_summary("GET /api/joints  ", actual_duration);
    command_stats.print_summary("POST /api/command", actual_duration);
    sensor_stats.print_summary("GET /api/sensors ", actual_duration);
    map_stats.print_summary("GET /api/map     ", actual_duration);

    // Aggregate stats
    int total_success = status_stats.success + joint_stats.success + command_stats.success +
                        sensor_stats.success + map_stats.success;
    int total_failure = status_stats.failure + joint_stats.failure + command_stats.failure +
                        sensor_stats.failure + map_stats.failure;
    int total_conn_err = status_stats.connection_error + joint_stats.connection_error +
                         command_stats.connection_error + sensor_stats.connection_error +
                         map_stats.connection_error;

    printf("\n--- Aggregate ---\n");
    printf("  Total requests:  %d\n", total_success + total_failure);
    printf("  Total success:   %d\n", total_success);
    printf("  Total failure:   %d\n", total_failure);
    printf("  Connection err:  %d\n", total_conn_err);
    printf("  Overall RPS:     %.1f\n", (double)(total_success + total_failure) / actual_duration);
    printf("  Success rate:    %.2f%%\n", 100.0 * total_success / (total_success + total_failure));

    printf("\n=== END ===\n");
    return 0;
}
