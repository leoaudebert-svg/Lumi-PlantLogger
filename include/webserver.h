#pragma once
#include "env_driver.h"
#include "soil.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// Webserver Module — Wi-Fi AP + captive-portal DNS + HTTP dashboard
//
// Responsibilities:
//   - Start a Wi-Fi Access Point ("PlantLogger", 192.168.4.1)
//   - Run a wildcard DNS server so every hostname resolves to the AP IP,
//     which causes phones to show a "Sign in to network" popup automatically
//   - Serve a minimal HTML dashboard on port 80
//   - Expose JSON status and command endpoints
//
// Usage contract:
//   1. Call webserver::begin() once in setup(), after sensor drivers are ready.
//   2. Call webserver::handle() every loop() to service DNS and HTTP clients.
//   3. Call webserver::set_reading() after each sensor read to refresh the
//      values returned by /api/status.
//   4. Check webserver::consume_read_request() each loop; if it returns true,
//      perform a sensor read and call set_reading() with the results.
//   5. Use set_logging() / is_logging() to share the logging flag between the
//      web interface and any physical button logic in main.cpp.
// ---------------------------------------------------------------------------

namespace webserver {

    // Start Wi-Fi AP, wildcard DNS server, and HTTP server on port 80.
    // Must be called once in setup(), after all sensor drivers are initialised.
    void begin();

    // Poll the DNS and HTTP servers. Must be called every loop().
    void handle();

    // Push the latest sensor snapshot so /api/status reflects fresh values.
    // Safe to call from loop(); no locking is needed on a single-core sketch.
    void set_reading(const env::EnvReading& env, uint16_t lux, const soil::SoilReading& s);

    // Shared logging flag — written by /start and /stop endpoints, and also
    // directly by the physical button handler in main.cpp.
    void set_logging(bool active);
    bool is_logging();

    // Returns true exactly once after a /read-now POST is received, then
    // auto-clears so main.cpp can detect the edge and perform one read.
    bool consume_read_request();

} // namespace webserver
