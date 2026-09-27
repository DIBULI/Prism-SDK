/*
Continuous RTK position example -- Prism-SDK 1.2.0
=================================================
Read-only: never starts/stops RTK, opens CORS, changes TimeSync mode, starts
Camera/IMU/LiDAR capture or sets device time. No Agent/SDK library changes needed.
For CORS configuration and explicit RTK start/stop, follow the English usage
comment in the companion rtk_module_control.cpp before running this reader.

Build/run from the SDK root (Host: Linux x86-64/ARM64 or macOS ARM64):
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --target prism-rtk-position -j
  ./build/examples/prism-rtk-position --seconds 60

Build/run on RK (Linux ARM64, local Agent connection instead of USB):
  cmake -S rk-local-sdk -B build-rklocal -DCMAKE_BUILD_TYPE=Release
  cmake --build build-rklocal --target prism-rklocal-rtk-position -j
  ./build-rklocal/prism-rklocal-rtk-position --seconds 60
These direct-link examples are not Windows targets; use RuntimeApi on Windows.

Prerequisites and lifecycle:
  - Agent is running. RTK mode has been selected/applied while capture is stopped.
  - Host requires exactly one Prism USB device; close Viewer/other USB owners.
  - Do not concurrently change RTK configuration or issue controls from elsewhere.
Default duration is 30 seconds; --seconds accepts 1..86400. Ctrl+C exits monitoring
only and leaves RTK/CORS unchanged. Output goes to stdout, not RK disk. Redirect
it explicitly if you want a local text log. Stop separately when required:
  ./build/examples/prism-rtk-module-control stop
On RK use ./build-rklocal/prism-rklocal-rtk-module-control stop instead.

Output and units:
  GGA: receiver GGA position; RTK: independent ADRNAV receiver solution.
       Missing ADRNAV is never replaced with GGA or an Agent-computed solution.
  solution: SINGLE, DGNSS, FLOAT, FIX, or INVALID. receiver_type preserves the
            original receiver label. Running RTK does not guarantee a FIX.
  latitude_deg / longitude_deg: degrees.
  ellipsoidal_height_m: MSL altitude + geoid separation, NOT MSL altitude.
  sigma_n_m / sigma_e_m / sigma_u_m: optional ADRNAV standard deviations in
       meters, not confidence percentages or guarantees of actual accuracy.
       Missing values are not_provided, never invented zeros. This reader does
       not supplement GGA uncertainty using GST.
  utc_hhmmss (GGA): UTC time of day only, without a date.
  gps_week:tow_ms (ADRNAV): GPS week/time-of-week milliseconds, NOT Unix time.
  age_ms: derived from Agent monotonic time, not the host wall clock.
  MODULE: control/freshness/errors, configuration saved/applied, RTCM count/age.
       Check freshness flags; a nonzero count does not imply current corrections.

Polling and validity:
Query gnssObservations(cursor, session) about every 100 ms; this is a polling
interval, not a guarantee of receiver solution rate. Process EVERY record in a
batch, preserving intermediate epochs rather than displaying only the last one.
Only valid positions at most two seconds old emit coordinates. Cursors suppress
repeated batches; session changes discard previous state; gap reports missing
records and accepts the replacement snapshot. Module status is queried at 1 Hz.
Keep GGA and independent RTK results separate and check validity/freshness before
using either. Do not subtract a GPS epoch or Agent monotonic time from host UTC.

Hardware-free tests (no device or CORS connection):
  ./build/examples/prism-rtk-position --self-test
  ctest --test-dir build --output-on-failure
On RK use ./build-rklocal/prism-rklocal-rtk-position --self-test instead.
Fixtures cover batched epochs, solution classes, invalid fixes/checksums, absent
uncertainty, duplicate/stale data, session changes, gaps and cursor rollback.
*/
#ifdef PRISM_EXAMPLE_RKLOCAL
#include <prism/rklocal_sdk.hpp>
#else
#include <prism/usb_sdk.hpp>
#endif
#include <prism/usb/gnss_plot.hpp>

#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }

unsigned duration(const std::string& text) {
  if (text.empty() || text.size() > 5 ||
      text.find_first_not_of("0123456789") != std::string::npos)
    throw std::invalid_argument("--seconds must be an integer in 1..86400");
  const auto seconds = std::stoul(text);
  if (!seconds || seconds > 86400)
    throw std::invalid_argument("--seconds must be an integer in 1..86400");
  return static_cast<unsigned>(seconds);
}

const char* solution(int quality) {
  switch (quality) {
    case 1: return "SINGLE";
    case 2: return "DGNSS";
    case 4: return "FIX";
    case 5: return "FLOAT";
    default: return "INVALID";
  }
}

const char* control(unsigned state) {
  static const char* names[] = {"unknown", "starting", "running", "stopping", "stopped", "error"};
  return state < 6 ? names[state] : "unknown";
}

void optional(std::ostream& out, const char* name, const std::optional<double>& value) {
  out << ' ' << name << '=';
  if (value) out << std::setprecision(4) << *value;
  else out << "not_provided";
}

// Keep the observation cursor/session, process EVERY record in each batch,
// and never emit an unchanged cached position as a new measurement.
class PositionReader {
 public:
  prism::gnss_plot::Model model;
  void consume(const prism::GnssObservations& batch, std::ostream& out) {
    if (model.session && batch.session != model.session) {
      out << "agent_session_changed: discard previous positions\n";
      model = {};
      gnss_available_ = rtk_available_ = false;
    }
    if (batch.gap) out << "observation_gap: some receiver records were lost\n";

    // Apply session/gap and advance device time even when there are no records.
    prism::GnssObservations part;
    part.session = batch.session;
    // A gap may also mean the requested cursor was ahead of the device. Accept
    // its replacement snapshot instead of filtering it using that old cursor.
    part.cursor = batch.gap ? 0 : model.cursor;
    part.device_monotonic_ms = batch.device_monotonic_ms;
    part.gap = batch.gap;
    model.apply(part);
    part.gap = false;
    for (const auto& record : batch.records) {
      if (record.sequence <= model.cursor) continue;
      part.cursor = record.sequence;
      part.records = {record};
      model.apply(part);
      if (model.gnss.sequence == record.sequence)
        position(model.gnss, "GGA", out);
      if (model.rtk.sequence == record.sequence)
        position(model.rtk, "RTK", out);
    }
    part.records.clear();
    part.cursor = batch.cursor;
    model.apply(part);
    availability(model.gnss, gnss_available_, "GGA", out);
    availability(model.rtk, rtk_available_, "RTK", out);
  }

 private:
  bool gnss_available_ = false, rtk_available_ = false;
  bool usable(const prism::gnss_plot::Position& p) const {
    return p.valid && prism::gnss_plot::fresh(model.now, p.ms, 2000);
  }
  void availability(const prism::gnss_plot::Position& p, bool& previous,
                    const char* name, std::ostream& out) {
    const bool current = usable(p);
    if (previous && !current) out << name << " unavailable/stale (no current position)\n";
    previous = current;
  }
  void position(const prism::gnss_plot::Position& p, const char* name, std::ostream& out) {
    out << name << " sequence=" << p.sequence
        << (p.source == "ADRNAV" ? " gps_week:tow_ms=" : " utc_hhmmss=") << p.epoch
        << " solution=" << (usable(p) ? solution(p.quality) : "INVALID")
        << " receiver_type=" << p.solution;
    if (!usable(p)) {
      out << " unavailable/stale\n";
      return; // Invalid/stale cached coordinates must not escape as current data.
    }
    out << std::fixed << std::setprecision(9)
        << " latitude_deg=" << p.latitude << " longitude_deg=" << p.longitude
        << " satellites=" << p.satellites << " age_ms=" << model.now - p.ms;
    optional(out, "ellipsoidal_height_m", p.height);
    optional(out, "sigma_n_m", p.north_sigma);
    optional(out, "sigma_e_m", p.east_sigma);
    optional(out, "sigma_u_m", p.up_sigma);
    out << '\n';
  }
};

void moduleStatus(const prism::TimeSyncRtkStatus& state, std::ostream& out) {
  out << "MODULE linked=" << state.linked
      << " status_fresh=" << state.device_status_fresh
      << " control_fresh=" << state.control_status_fresh
      << " control=" << control(state.control_state)
      << " control_generation=" << state.control_generation
      << " error=" << state.error_code << " control_error=" << unsigned(state.control_error)
      << " cors_saved=" << state.configuration_saved
      << " cors_applied=" << state.configuration_applied
      << " rtcm_frames=" << state.rtcm_frames << " rtcm_age_ms=" << state.rtcm_age_ms
      << " rtcm_errors=" << state.rtcm_errors << '\n';
}

// Synthetic parser fixtures only; no device, account or real coordinates.
std::string checked(const std::string& body) {
  const bool adr = body.front() == '#';
  uint32_t checksum = 0;
  for (size_t i = 1; i < body.size(); ++i) {
    checksum ^= static_cast<uint8_t>(body[i]);
    if (adr) for (int bit = 0; bit < 8; ++bit)
      checksum = (checksum >> 1) ^ ((checksum & 1) ? 0xedb88320u : 0);
  }
  std::ostringstream out;
  out << body << '*' << std::hex << std::setfill('0') << std::setw(adr ? 8 : 2) << checksum;
  return out.str();
}

std::string adr(const char* type, unsigned epoch, const char* status = "SOL_COMPUTED",
                const char* sigma = "0.02") {
  std::vector<std::string> fields(29, "0");
  fields[0] = status; fields[1] = type; fields[2] = "1.25"; fields[3] = "2.5";
  fields[4] = "15"; fields[5] = "5"; fields[6] = "WGS84";
  fields[7] = sigma; fields[8] = "0.03"; fields[9] = "0.04"; fields[14] = "18";
  std::string body = "#ADRNAVA,COM1,GPS,FINE,2400," + std::to_string(epoch) + ",0,0,0,0;";
  for (size_t i = 0; i < fields.size(); ++i) body += (i ? "," : "") + fields[i];
  return checked(body);
}

void selfTest() {
  auto require = [](bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
  };
  for (const char* value : {"", "0", "86401", "-1", "+1", "1.5", "1x", "9999999999999"}) {
    bool rejected = false;
    try { (void)duration(value); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "invalid duration accepted");
  }
  require(duration("1") == 1 && duration("86400") == 86400, "duration boundary failed");
  PositionReader reader;
  prism::GnssObservations batch;
  batch.session = 1; batch.device_monotonic_ms = 1000; batch.cursor = 4;
  batch.records = {{1, 700, checked("$GNGGA,120000.00,0115.0,N,00230.0,E,1,18,0.8,15,M,5,M,,")},
                   {2, 800, adr("SINGLE", 100000)},
                   {3, 900, adr("NARROW_FLOAT", 100100)},
                   {4, 1000, adr("NARROW_INT", 100200)}};
  std::ostringstream out;
  reader.consume(batch, out);
  require(out.str().find("GGA sequence=1") != std::string::npos, "GGA missing");
  for (const char* type : {"SINGLE", "FLOAT", "FIX"})
    require(out.str().find(std::string("solution=") + type) != std::string::npos, "batched epoch lost");
  require(out.str().find("sigma_n_m=0.0200") != std::string::npos, "sigma missing");
  require(out.str().find("ellipsoidal_height_m=20.0000") != std::string::npos, "height datum wrong");
  const auto length = out.str().size();
  reader.consume(batch, out);
  require(out.str().size() == length, "duplicate batch emitted");
  batch.records.clear(); batch.device_monotonic_ms = 3001;
  reader.consume(batch, out);
  require(!reader.model.rtk.valid && out.str().find("RTK unavailable/stale") != std::string::npos,
          "stale result presented as fresh");
  batch.session = 2; batch.cursor = 1; batch.device_monotonic_ms = 500;
  batch.records = {{1, 500, adr("PSRDIFF", 100300, "SOL_COMPUTED", "")}};
  reader.consume(batch, out);
  require(reader.model.rtk.valid && out.str().find("solution=DGNSS") != std::string::npos,
          "session reset failed");
  require(out.str().find("sigma_n_m=not_provided") != std::string::npos, "absent sigma invented");
  batch.cursor = 2; batch.records = {{2, 500, adr("NARROW_INT", 100400, "INSUFFICIENT_OBS")}};
  std::ostringstream invalid;
  reader.consume(batch, invalid);
  require(!reader.model.rtk.valid && invalid.str().find("latitude_deg=") == std::string::npos,
          "invalid position leaked");
  batch.cursor = 3; batch.records = {{3, 500, adr("NARROW_INT", 100500)}};
  batch.records[0].sentence.back() = '!';
  reader.consume(batch, out);
  require(reader.model.rejected == 1 && !reader.model.rtk.valid, "bad checksum accepted");
  batch.cursor = 4; batch.records = {{4, 500, adr("NARROW_INT", 100600)}};
  reader.consume(batch, out);
  batch.gap = true; batch.records.clear();
  reader.consume(batch, out);
  require(!reader.model.rtk.valid && reader.model.gaps == 1, "gap retained valid cached position");
  batch.cursor = 2; batch.records = {{2, 500, adr("NARROW_FLOAT", 100700)}};
  reader.consume(batch, out);
  require(reader.model.rtk.valid && reader.model.cursor == 2 && reader.model.rtk.quality == 5,
          "replacement snapshot after cursor rollback was skipped");
  std::cout << "RTK position example self-test passed\n";
}
} // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--self-test") { selfTest(); return 0; }
    if (argc == 2 && std::string(argv[1]) == "--help") {
      std::cout << "usage: " << argv[0] << " [--seconds 1..86400] | --self-test\n"
                   "Default: read-only positions for 30 seconds. Ctrl+C exits monitoring only.\n"
                   "Use the separate rtk-module-control example to save/start/stop.\n";
      return 0;
    }
    unsigned seconds = 30;
    if (argc != 1) {
      if (argc != 3 || std::string(argv[1]) != "--seconds")
        throw std::invalid_argument("invalid arguments; use --help");
      seconds = duration(argv[2]); // Validate before opening a device.
    }
#ifdef PRISM_EXAMPLE_RKLOCAL
    auto client = prism::rklocal::Client::open();
#else
    const auto devices = prism::Client::enumerate();
    if (devices.size() != 1) throw std::runtime_error("connect exactly one Prism USB device");
    auto client = prism::Client::open(devices.front());
#endif
    const auto port = client.timeSyncPortStatus();
    if (port.mode != prism::TimeSyncPortMode::Rtk || !port.applied)
      throw std::runtime_error("select and apply RTK mode while capture is stopped; this example will not switch it");
    std::signal(SIGINT, interrupt);
    std::signal(SIGTERM, interrupt);
    std::cout << "Read-only monitoring; waiting for fresh GGA/ADRNAV. No FIX is guaranteed.\n";
    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::seconds(seconds);
    auto next_status = Clock::now();
    PositionReader positions;
    do {
      const auto tick = Clock::now();
      if (tick >= next_status) {
        moduleStatus(client.timeSyncRtkStatus(), std::cout);
        next_status = Clock::now() + std::chrono::seconds(1);
      }
      positions.consume(client.gnssObservations(positions.model.cursor, positions.model.session), std::cout);
      std::cout.flush();
      std::this_thread::sleep_until(std::min(deadline, tick + std::chrono::milliseconds(100)));
    } while (!interrupted && Clock::now() < deadline);
    std::cout << "Monitor finished; RTK/CORS state unchanged. rejected=" << positions.model.rejected
              << " gaps=" << positions.model.gaps << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
