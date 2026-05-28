// Mock Robot Controller Server
// Simulates a real robot controller's REST API for stress testing

#include "../httplib.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

// Simulated robot state
struct RobotState {
    std::mutex mtx;
    double joint_angles[6] = {0, 0, 0, 0, 0, 0};
    double position[3] = {0, 0, 0};
    double velocity[3] = {0, 0, 0};
    int battery_level = 100;
    std::string status = "idle";
    std::atomic<int> command_count{0};
    std::atomic<int> request_count{0};

    // Simulate state updates (as if robot is moving)
    void tick() {
        std::lock_guard<std::mutex> lock(mtx);
        auto now = std::chrono::steady_clock::now();
        double t = std::chrono::duration<double>(now.time_since_epoch()).count();
        for (int i = 0; i < 6; i++) {
            joint_angles[i] = 30.0 * std::sin(t * 0.5 + i * 0.7);
        }
        position[0] = 100.0 * std::cos(t * 0.1);
        position[1] = 100.0 * std::sin(t * 0.1);
        position[2] = 50.0 + 10.0 * std::sin(t * 0.3);
        velocity[0] = -10.0 * std::sin(t * 0.1);
        velocity[1] = 10.0 * std::cos(t * 0.1);
        velocity[2] = 3.0 * std::cos(t * 0.3);
        battery_level = std::max(0, 100 - (int)(t) % 100);
    }
};

// Simulate processing delay (mimics real controller response time)
void simulate_work(int min_us = 100, int max_us = 2000) {
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(min_us, max_us);
    std::this_thread::sleep_for(std::chrono::microseconds(dist(gen)));
}

int main(int argc, char *argv[]) {
    int port = 8080;
    int threads = 8;
    if (argc > 1) port = std::atoi(argv[1]);
    if (argc > 2) threads = std::atoi(argv[2]);

    httplib::Server svr;
    RobotState robot;

    // Background thread to update robot state
    std::atomic<bool> running{true};
    std::thread state_thread([&]() {
        while (running) {
            robot.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // GET /api/status - Robot overall status (lightweight, high frequency)
    svr.Get("/api/status", [&](const httplib::Request &, httplib::Response &res) {
        robot.request_count++;
        simulate_work(50, 200);
        std::lock_guard<std::mutex> lock(robot.mtx);
        std::ostringstream json;
        json << "{\"status\":\"" << robot.status
             << "\",\"battery\":" << robot.battery_level
             << ",\"uptime\":" << std::chrono::steady_clock::now().time_since_epoch().count() / 1000000000
             << ",\"commands_executed\":" << robot.command_count.load()
             << ",\"requests_served\":" << robot.request_count.load()
             << "}";
        res.set_content(json.str(), "application/json");
    });

    // GET /api/joints - Joint positions (high frequency polling target)
    svr.Get("/api/joints", [&](const httplib::Request &, httplib::Response &res) {
        robot.request_count++;
        simulate_work(100, 500);
        std::lock_guard<std::mutex> lock(robot.mtx);
        std::ostringstream json;
        json << "{\"joints\":[";
        for (int i = 0; i < 6; i++) {
            if (i) json << ",";
            json << robot.joint_angles[i];
        }
        json << "],\"timestamp\":" << std::chrono::steady_clock::now().time_since_epoch().count() / 1000
             << "}";
        res.set_content(json.str(), "application/json");
    });

    // GET /api/pose - End effector pose
    svr.Get("/api/pose", [&](const httplib::Request &, httplib::Response &res) {
        robot.request_count++;
        simulate_work(100, 500);
        std::lock_guard<std::mutex> lock(robot.mtx);
        std::ostringstream json;
        json << std::fixed;
        json << "{\"position\":[" << robot.position[0] << "," << robot.position[1] << "," << robot.position[2] << "]"
             << ",\"velocity\":[" << robot.velocity[0] << "," << robot.velocity[1] << "," << robot.velocity[2] << "]"
             << "}";
        res.set_content(json.str(), "application/json");
    });

    // POST /api/command - Send command to robot
    svr.Post("/api/command", [&](const httplib::Request &req, httplib::Response &res) {
        robot.request_count++;
        robot.command_count++;
        // Simulate command processing (more expensive)
        simulate_work(500, 5000);
        std::ostringstream json;
        json << "{\"success\":true,\"command_id\":" << robot.command_count.load()
             << ",\"message\":\"Command received\"}";
        res.set_content(json.str(), "application/json");
    });

    // POST /api/move - Movement command (heavier processing)
    svr.Post("/api/move", [&](const httplib::Request &req, httplib::Response &res) {
        robot.request_count++;
        robot.command_count++;
        simulate_work(1000, 10000);
        std::lock_guard<std::mutex> lock(robot.mtx);
        robot.status = "moving";
        std::ostringstream json;
        json << "{\"success\":true,\"target_reached\":false,\"command_id\":" << robot.command_count.load() << "}";
        res.set_content(json.str(), "application/json");
    });

    // GET /api/sensors - Sensor data (medium payload)
    svr.Get("/api/sensors", [&](const httplib::Request &, httplib::Response &res) {
        robot.request_count++;
        simulate_work(200, 1000);
        std::ostringstream json;
        json << "{\"force_torque\":[1.23,-0.45,9.81,0.01,0.02,-0.01]"
             << ",\"proximity\":[1.5,2.3,0.8,1.1,3.0,2.7]"
             << ",\"temperature\":[25.3,26.1,24.8,25.9,26.5,25.2]"
             << ",\"current\":[1.2,0.8,1.5,0.9,0.3,0.1]"
             << "}";
        res.set_content(json.str(), "application/json");
    });

    // GET /api/map - Large payload (map/point cloud data)
    svr.Get("/api/map", [&](const httplib::Request &, httplib::Response &res) {
        robot.request_count++;
        simulate_work(5000, 20000);
        // Generate a larger payload (~100KB of point cloud data)
        std::ostringstream json;
        json << "{\"points\":[";
        for (int i = 0; i < 3000; i++) {
            if (i) json << ",";
            json << "[" << (std::rand() % 1000) / 10.0
                 << "," << (std::rand() % 1000) / 10.0
                 << "," << (std::rand() % 500) / 10.0 << "]";
        }
        json << "],\"resolution\":0.05,\"width\":1000,\"height\":1000}";
        res.set_content(json.str(), "application/json");
    });

    // GET /api/health - Health check (minimal latency)
    svr.Get("/api/health", [&](const httplib::Request &, httplib::Response &res) {
        res.set_content("{\"healthy\":true}", "application/json");
    });

    // CORS headers for browser access
    svr.set_post_routing_handler([](const httplib::Request &, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });

    // Web Dashboard
    svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
        res.set_content(R"HTML(<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Robot Controller Dashboard</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: 'Segoe UI', system-ui, sans-serif; background: #0a0e17; color: #e0e0e0; }
.header { background: linear-gradient(135deg, #1a1f35, #0d1220); padding: 16px 24px; border-bottom: 1px solid #2a3050; display: flex; align-items: center; gap: 16px; }
.header h1 { font-size: 20px; color: #fff; }
.status-dot { width: 10px; height: 10px; border-radius: 50%; background: #22c55e; animation: pulse 2s infinite; }
@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }
.grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 16px; padding: 16px; }
.card { background: #111827; border: 1px solid #1e293b; border-radius: 12px; padding: 20px; }
.card h2 { font-size: 14px; color: #64748b; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 16px; }
.metric { display: flex; justify-content: space-between; align-items: baseline; padding: 8px 0; border-bottom: 1px solid #1e293b; }
.metric:last-child { border-bottom: none; }
.metric-label { color: #94a3b8; font-size: 13px; }
.metric-value { font-size: 18px; font-weight: 600; font-family: 'JetBrains Mono', monospace; }
.metric-value.ok { color: #22c55e; }
.metric-value.warn { color: #f59e0b; }
.metric-value.err { color: #ef4444; }
.joint-bar { display: flex; align-items: center; gap: 12px; padding: 6px 0; }
.joint-label { width: 60px; font-size: 12px; color: #64748b; }
.joint-track { flex: 1; height: 8px; background: #1e293b; border-radius: 4px; position: relative; overflow: hidden; }
.joint-fill { position: absolute; height: 100%; border-radius: 4px; transition: all 0.15s ease; }
.joint-val { width: 70px; text-align: right; font-size: 13px; font-family: monospace; }
.pose-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
.pose-item { text-align: center; padding: 12px; background: #0f172a; border-radius: 8px; }
.pose-item .axis { font-size: 11px; color: #64748b; margin-bottom: 4px; }
.pose-item .val { font-size: 20px; font-weight: 700; font-family: monospace; }
.pose-item.x .val { color: #ef4444; }
.pose-item.y .val { color: #22c55e; }
.pose-item.z .val { color: #3b82f6; }
.sensor-row { display: flex; gap: 8px; flex-wrap: wrap; margin-bottom: 8px; }
.sensor-chip { padding: 6px 12px; background: #0f172a; border-radius: 6px; font-size: 12px; font-family: monospace; }
.btn-row { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 8px; }
.btn { padding: 10px 20px; border: none; border-radius: 8px; font-size: 14px; font-weight: 600; cursor: pointer; transition: all 0.15s; }
.btn:hover { transform: translateY(-1px); }
.btn:active { transform: translateY(0); }
.btn-primary { background: #3b82f6; color: #fff; }
.btn-primary:hover { background: #2563eb; }
.btn-danger { background: #ef4444; color: #fff; }
.btn-danger:hover { background: #dc2626; }
.btn-success { background: #22c55e; color: #fff; }
.btn-success:hover { background: #16a34a; }
.log-box { background: #0a0e17; border: 1px solid #1e293b; border-radius: 8px; padding: 12px; max-height: 200px; overflow-y: auto; font-family: monospace; font-size: 12px; line-height: 1.6; }
.log-entry { color: #64748b; }
.log-entry.cmd { color: #3b82f6; }
.log-entry.ok { color: #22c55e; }
.log-entry.err { color: #ef4444; }
.stats-bar { display: flex; gap: 24px; padding: 8px 16px; background: #111827; border-top: 1px solid #1e293b; font-size: 12px; color: #64748b; position: fixed; bottom: 0; left: 0; right: 0; }
.stats-bar span { font-family: monospace; }
</style>
</head>
<body>
<div class="header">
  <div class="status-dot" id="connDot"></div>
  <h1>Robot Controller Dashboard</h1>
  <span style="color:#64748b;font-size:13px" id="connStatus">Connecting...</span>
  <span style="margin-left:auto;color:#64748b;font-size:12px" id="clock"></span>
</div>
<div class="grid">
  <div class="card">
    <h2>System Status</h2>
    <div class="metric"><span class="metric-label">State</span><span class="metric-value ok" id="mStatus">--</span></div>
    <div class="metric"><span class="metric-label">Battery</span><span class="metric-value" id="mBattery">--</span></div>
    <div class="metric"><span class="metric-label">Uptime</span><span class="metric-value" id="mUptime">--</span></div>
    <div class="metric"><span class="metric-label">Commands</span><span class="metric-value" id="mCmdCount">--</span></div>
    <div class="metric"><span class="metric-label">Requests</span><span class="metric-value" id="mReqCount">--</span></div>
  </div>
  <div class="card">
    <h2>Joint Angles (deg)</h2>
    <div id="jointsPanel"></div>
  </div>
  <div class="card">
    <h2>End Effector Pose</h2>
    <div class="pose-grid">
      <div class="pose-item x"><div class="axis">X</div><div class="val" id="px">--</div></div>
      <div class="pose-item y"><div class="axis">Y</div><div class="val" id="py">--</div></div>
      <div class="pose-item z"><div class="axis">Z</div><div class="val" id="pz">--</div></div>
      <div class="pose-item x"><div class="axis">Vx</div><div class="val" id="vx">--</div></div>
      <div class="pose-item y"><div class="axis">Vy</div><div class="val" id="vy">--</div></div>
      <div class="pose-item z"><div class="axis">Vz</div><div class="val" id="vz">--</div></div>
    </div>
  </div>
  <div class="card">
    <h2>Sensors</h2>
    <div id="sensorPanel">Loading...</div>
  </div>
  <div class="card">
    <h2>Commands</h2>
    <div class="btn-row">
      <button class="btn btn-primary" onclick="sendCmd('gripper','open')">Gripper Open</button>
      <button class="btn btn-primary" onclick="sendCmd('gripper','close')">Gripper Close</button>
      <button class="btn btn-success" onclick="sendCmd('home','go')">Go Home</button>
      <button class="btn btn-danger" onclick="sendCmd('estop','trigger')">E-Stop</button>
    </div>
    <div class="btn-row">
      <button class="btn btn-primary" onclick="sendMove()">Move To Pose</button>
    </div>
  </div>
  <div class="card">
    <h2>Event Log</h2>
    <div class="log-box" id="logBox"></div>
  </div>
</div>
<div class="stats-bar">
  <span>Latency: <b id="sLatency">--</b> ms</span>
  <span>Requests/s: <b id="sRps">--</b></span>
  <span>Total: <b id="sTotal">0</b></span>
  <span>Errors: <b id="sErrors">0</b></span>
</div>
<script>
const BASE = '';
let reqCount = 0, errCount = 0, lastReqTime = Date.now();
let latencySamples = [];

function log(msg, cls) {
  const box = document.getElementById('logBox');
  const d = document.createElement('div');
  d.className = 'log-entry ' + (cls||'');
  d.textContent = new Date().toLocaleTimeString() + ' | ' + msg;
  box.appendChild(d);
  box.scrollTop = box.scrollHeight;
  if (box.children.length > 200) box.removeChild(box.firstChild);
}

async function api(method, path, body) {
  const t0 = performance.now();
  try {
    const opts = { method };
    if (body) { opts.headers = {'Content-Type':'application/json'}; opts.body = JSON.stringify(body); }
    const res = await fetch(BASE + path, opts);
    const dt = performance.now() - t0;
    latencySamples.push(dt);
    if (latencySamples.length > 50) latencySamples.shift();
    reqCount++;
    const data = await res.json();
    return data;
  } catch(e) {
    errCount++;
    log('Error: ' + path + ' - ' + e.message, 'err');
    return null;
  }
}

async function pollStatus() {
  const d = await api('GET', '/api/status');
  if (!d) return;
  document.getElementById('mStatus').textContent = d.status;
  document.getElementById('mStatus').className = 'metric-value ' + (d.status === 'idle' ? 'ok' : 'warn');
  const bat = d.battery;
  const batEl = document.getElementById('mBattery');
  batEl.textContent = bat + '%';
  batEl.className = 'metric-value ' + (bat > 30 ? 'ok' : bat > 10 ? 'warn' : 'err');
  document.getElementById('mUptime').textContent = d.uptime + 's';
  document.getElementById('mCmdCount').textContent = d.commands_executed;
  document.getElementById('mReqCount').textContent = d.requests_served;
}

async function pollJoints() {
  const d = await api('GET', '/api/joints');
  if (!d) return;
  const panel = document.getElementById('jointsPanel');
  if (!panel.children.length) {
    for (let i = 0; i < 6; i++) {
      const row = document.createElement('div');
      row.className = 'joint-bar';
      row.innerHTML = '<span class="joint-label">J'+(i+1)+'</span>'
        + '<div class="joint-track"><div class="joint-fill" id="jf'+i+'"></div></div>'
        + '<span class="joint-val" id="jv'+i+'">0</span>';
      panel.appendChild(row);
    }
  }
  for (let i = 0; i < 6; i++) {
    const v = d.joints[i];
    const pct = (v + 60) / 120 * 100;
    const fill = document.getElementById('jf'+i);
    fill.style.width = Math.max(0, Math.min(100, pct)) + '%';
    fill.style.background = Math.abs(v) > 20 ? '#ef4444' : Math.abs(v) > 10 ? '#f59e0b' : '#22c55e';
    document.getElementById('jv'+i).textContent = v.toFixed(1) + ' deg';
  }
}

async function pollPose() {
  const d = await api('GET', '/api/pose');
  if (!d) return;
  document.getElementById('px').textContent = d.position[0].toFixed(1);
  document.getElementById('py').textContent = d.position[1].toFixed(1);
  document.getElementById('pz').textContent = d.position[2].toFixed(1);
  document.getElementById('vx').textContent = d.velocity[0].toFixed(2);
  document.getElementById('vy').textContent = d.velocity[1].toFixed(2);
  document.getElementById('vz').textContent = d.velocity[2].toFixed(2);
}

async function pollSensors() {
  const d = await api('GET', '/api/sensors');
  if (!d) return;
  const panel = document.getElementById('sensorPanel');
  const labels = ['Force','Proximity','Temp','Current'];
  const keys = ['force_torque','proximity','temperature','current'];
  let html = '';
  for (let k = 0; k < 4; k++) {
    html += '<div style="margin-bottom:8px"><span style="color:#64748b;font-size:12px">'+labels[k]+'</span><div class="sensor-row">';
    const arr = d[keys[k]];
    for (let i = 0; i < arr.length; i++) {
      html += '<span class="sensor-chip">'+arr[i].toFixed(2)+'</span>';
    }
    html += '</div></div>';
  }
  panel.innerHTML = html;
}

async function sendCmd(type, action) {
  log('CMD: ' + type + '.' + action, 'cmd');
  const d = await api('POST', '/api/command', {type, action});
  if (d && d.success) log('OK: cmd#' + d.command_id, 'ok');
}

async function sendMove() {
  log('MOVE: target pose', 'cmd');
  const d = await api('POST', '/api/move', {target:[100,200,300]});
  if (d && d.success) log('OK: move cmd#' + d.command_id, 'ok');
}

function updateStats() {
  const avg = latencySamples.length ? (latencySamples.reduce((a,b)=>a+b,0)/latencySamples.length) : 0;
  document.getElementById('sLatency').textContent = avg.toFixed(1);
  const now = Date.now();
  const dt = (now - lastReqTime) / 1000;
  document.getElementById('sRps').textContent = dt > 0 ? (reqCount/dt).toFixed(0) : '--';
  document.getElementById('sTotal').textContent = reqCount;
  document.getElementById('sErrors').textContent = errCount;
  document.getElementById('clock').textContent = new Date().toLocaleTimeString();
  document.getElementById('connDot').style.background = errCount === 0 ? '#22c55e' : '#f59e0b';
  document.getElementById('connStatus').textContent = errCount === 0 ? 'Connected' : errCount + ' errors';
}

setInterval(pollStatus, 200);
setInterval(pollJoints, 200);
setInterval(pollPose, 200);
setInterval(pollSensors, 1000);
setInterval(updateStats, 500);

log('Dashboard initialized');
pollStatus(); pollJoints(); pollPose(); pollSensors();
</script>
</body>
</html>)HTML", "text/html");
    });

    svr.set_read_timeout(30);
    svr.set_write_timeout(30);
    svr.set_keep_alive_max_count(1000);
    svr.set_keep_alive_timeout(60);

    printf("Mock Robot Server listening on port %d with %d threads\n", port, threads);
    printf("Endpoints:\n");
    printf("  GET  /api/status    - Robot status (lightweight)\n");
    printf("  GET  /api/joints    - Joint positions (high freq)\n");
    printf("  GET  /api/pose      - End effector pose\n");
    printf("  POST /api/command   - Send command\n");
    printf("  POST /api/move      - Movement command\n");
    printf("  GET  /api/sensors   - Sensor data\n");
    printf("  GET  /api/map       - Map data (large payload)\n");
    printf("  GET  /api/health    - Health check\n");

    svr.listen("0.0.0.0", port);

    running = false;
    state_thread.join();
    return 0;
}
