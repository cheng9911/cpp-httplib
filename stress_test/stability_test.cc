// Long-running Stability Test
// Tests connection stability over extended periods, simulating a real robot
// control session where UI pages stay connected for hours.

#include "../httplib.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

struct StabilityReport {
    std::mutex mtx;
    int total_requests = 0;
    int total_success = 0;
    int total_failures = 0;
    int total_reconnects = 0;
    double max_latency_ms = 0;
    double total_latency_ms = 0;
    int latency_samples = 0;

    void record(double latency_ms, bool ok) {
        std::lock_guard<std::mutex> lock(mtx);
        total_requests++;
        total_latency_ms += latency_ms;
        latency_samples++;
        if (latency_ms > max_latency_ms) max_latency_ms = latency_ms;
        if (ok) total_success++;
        else total_failures++;
    }

    void reconnect() {
        std::lock_guard<std::mutex> lock(mtx);
        total_reconnects++;
    }

    void print(const std::string &label) {
        std::lock_guard<std::mutex> lock(mtx);
        printf("  [%s]\n", label.c_str());
        printf("    Requests:    %d\n", total_requests);
        printf("    Success:     %d (%.2f%%)\n", total_success,
               total_requests ? 100.0 * total_success / total_requests : 0);
        printf("    Failures:    %d\n", total_failures);
        printf("    Reconnects:  %d\n", total_reconnects);
        printf("    Avg Latency: %.2f ms\n", latency_samples ? total_latency_ms / latency_samples : 0);
        printf("    Max Latency: %.2f ms\n", max_latency_ms);
    }
};

void run_stable_client(int client_id, const std::string &host, int port,
                       int duration_min, StabilityReport &report) {
    auto end_time = std::chrono::steady_clock::now() + std::chrono::minutes(duration_min);
    std::mt19937 rng(client_id);
    std::uniform_int_distribution<> endpoint_dist(0, 3);

    int consecutive_failures = 0;
    int request_count = 0;

    while (std::chrono::steady_clock::now() < end_time) {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(5);
        cli.set_read_timeout(10);
        cli.set_keep_alive(true);

        // Each "session" lasts until too many failures or randomly reconnect
        int session_requests = 0;
        int session_failures = 0;

        while (std::chrono::steady_clock::now() < end_time) {
            std::string path;
            switch (endpoint_dist(rng)) {
                case 0: path = "/api/status"; break;
                case 1: path = "/api/joints"; break;
                case 2: path = "/api/pose"; break;
                case 3: path = "/api/sensors"; break;
            }

            auto t1 = std::chrono::steady_clock::now();
            auto res = cli.Get(path.c_str());
            auto t2 = std::chrono::steady_clock::now();
            double latency_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

            if (res && res->status == 200) {
                report.record(latency_ms, true);
                session_failures = 0;
            } else {
                report.record(latency_ms, false);
                session_failures++;
                consecutive_failures++;
            }

            session_requests++;
            request_count++;

            // If too many consecutive failures, reconnect
            if (session_failures > 5) {
                report.reconnect();
                printf("  Client %d: reconnecting after %d failures\n", client_id, session_failures);
                break;
            }

            // Random jitter: 20-80ms polling
            std::uniform_int_distribution<> sleep_dist(20, 80);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_dist(rng)));
        }

        if (session_requests > 0) {
            printf("  Client %d: session ended (%d requests, %d failures)\n",
                   client_id, session_requests, session_failures);
        }
    }

    printf("  Client %d: finished (%d total requests)\n", client_id, request_count);
}

void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -h <host>       Server host (default: 127.0.0.1)\n");
    printf("  -p <port>       Server port (default: 8080)\n");
    printf("  -n <count>      Number of concurrent clients (default: 10)\n");
    printf("  -d <minutes>    Test duration in minutes (default: 5)\n");
    printf("  --help          Show this help\n");
}

int main(int argc, char *argv[]) {
    std::string host = "127.0.0.1";
    int port = 8080;
    int num_clients = 10;
    int duration_min = 5;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc) host = argv[++i];
        else if (arg == "-p" && i + 1 < argc) port = std::atoi(argv[++i]);
        else if (arg == "-n" && i + 1 < argc) num_clients = std::atoi(argv[++i]);
        else if (arg == "-d" && i + 1 < argc) duration_min = std::atoi(argv[++i]);
        else if (arg == "--help") { print_usage(argv[0]); return 0; }
        else { printf("Unknown: %s\n", arg.c_str()); return 1; }
    }

    printf("=== Stability Test ===\n");
    printf("Server:   %s:%d\n", host.c_str(), port);
    printf("Clients:  %d\n", num_clients);
    printf("Duration: %d minutes\n", duration_min);
    printf("======================\n\n");

    // Health check
    {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(3);
        auto res = cli.Get("/api/health");
        if (!res || res->status != 200) {
            printf("ERROR: Server not reachable\n");
            return 1;
        }
        printf("Health check: OK\n\n");
    }

    StabilityReport report;

    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_clients; i++) {
        threads.emplace_back(run_stable_client, i, host, port, duration_min, std::ref(report));
    }

    // Periodic status
    std::thread status_thread([&]() {
        while (true) {
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= duration_min) break;
            std::this_thread::sleep_for(std::chrono::seconds(30));
            printf("\n--- Status at %ld min ---\n", elapsed);
            report.print("Running");
            printf("\n");
        }
    });

    for (auto &t : threads) t.join();
    status_thread.join();

    printf("\n=== FINAL REPORT ===\n");
    report.print("All Clients");
    printf("\n====================\n");

    return 0;
}
