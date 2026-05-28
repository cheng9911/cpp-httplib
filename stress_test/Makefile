CXX ?= g++
CXXFLAGS = -std=c++11 -O2 -Wall -pthread
LDFLAGS = -pthread

# If OpenSSL is available, enable it for the server/client
# Uncomment these if your httplib is compiled with CPPHTTPLIB_OPENSSL_SUPPORT
# CXXFLAGS += -DCPPHTTPLIB_OPENSSL_SUPPORT -lssl -lcrypto

TARGETS = mock_robot_server stress_test stability_test

all: $(TARGETS)

mock_robot_server: mock_robot_server.cc ../httplib.h
	$(CXX) $(CXXFLAGS) -o $@ mock_robot_server.cc $(LDFLAGS)

stress_test: stress_test.cc ../httplib.h
	$(CXX) $(CXXFLAGS) -o $@ stress_test.cc $(LDFLAGS)

stability_test: stability_test.cc ../httplib.h
	$(CXX) $(CXXFLAGS) -o $@ stability_test.cc $(LDFLAGS)

# Quick test: start server, run stress test, stop server
quick-test: all
	@echo "=== Quick Stress Test (10 UI pages, 15s) ==="
	./mock_robot_server 8080 8 &
	SERVER_PID=$$!; \
	sleep 1; \
	./stress_test -n 10 -d 15; \
	kill $$SERVER_PID 2>/dev/null; \
	wait $$SERVER_PID 2>/dev/null

# Full test: multiple concurrency levels
full-test: all
	@echo "=== Full Stress Test Suite ==="
	./mock_robot_server 8080 8 &
	SERVER_PID=$$!; \
	sleep 1; \
	echo "--- 5 UI pages ---"; \
	./stress_test -n 5 -d 20; \
	echo ""; \
	echo "--- 10 UI pages ---"; \
	./stress_test -n 10 -d 20; \
	echo ""; \
	echo "--- 20 UI pages ---"; \
	./stress_test -n 20 -d 20; \
	echo ""; \
	echo "--- 50 UI pages ---"; \
	./stress_test -n 50 -d 20; \
	kill $$SERVER_PID 2>/dev/null; \
	wait $$SERVER_PID 2>/dev/null

# Stability test: long-running
stability-run: all
	@echo "=== Stability Test (10 clients, 5 min) ==="
	./mock_robot_server 8080 8 &
	SERVER_PID=$$!; \
	sleep 1; \
	./stability_test -n 10 -d 5; \
	kill $$SERVER_PID 2>/dev/null; \
	wait $$SERVER_PID 2>/dev/null

# Using wrk for raw HTTP throughput (if wrk is installed)
wrk-test: all
	@echo "=== wrk Throughput Test ==="
	./mock_robot_server 8080 8 &
	SERVER_PID=$$!; \
	sleep 1; \
	echo "--- GET /api/status (10 connections, 30s) ---"; \
	wrk -t4 -c10 -d30s http://127.0.0.1:8080/api/status; \
	echo ""; \
	echo "--- GET /api/joints (20 connections, 30s) ---"; \
	wrk -t4 -c20 -d30s http://127.0.0.1:8080/api/joints; \
	echo ""; \
	echo "--- GET /api/sensors (10 connections, 30s) ---"; \
	wrk -t4 -c10 -d30s http://127.0.0.1:8080/api/sensors; \
	kill $$SERVER_PID 2>/dev/null; \
	wait $$SERVER_PID 2>/dev/null

clean:
	rm -f $(TARGETS)

.PHONY: all quick-test full-test stability-run wrk-test clean
