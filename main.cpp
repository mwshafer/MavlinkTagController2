#include "CommandHandler.h"
#include "UDPPulseReceiver.h"

#include <chrono>
#include <cstdint>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <iostream>
#include <future>
#include <memory>
#include <thread>

using namespace mavsdk;

int main(int /*argc*/, char* /*argv[]*/)
{
    Mavsdk mavsdk;
    mavsdk.set_configuration(Mavsdk::Configuration(Mavsdk::Configuration::UsageType::CompanionComputer));

    ConnectionResult connection_result;
    connection_result = mavsdk.add_any_connection("udp://0.0.0.0:14561");
    if (connection_result != ConnectionResult::Success) {
        std::cout << "Connection failed: " << connection_result << std::endl;
        return 1;
    }

    std::cout << "Waiting to discover system..." << std::endl;
    auto prom = std::promise<std::shared_ptr<System>>{};
    auto fut = prom.get_future();

    mavsdk.subscribe_on_new_system([&mavsdk, &prom]() {
        auto system = mavsdk.systems().back();

        if (system->has_autopilot()) {
            std::cout << "Discovered autopilot" << std::endl;
            mavsdk.subscribe_on_new_system(nullptr);
            prom.set_value(system);
        }
    });

    // We usually receive heartbeats at 1Hz, therefore we should find a
    // system after around 3 seconds.
    if (fut.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
        std::cout << "No autopilot found, exiting." << std::endl;
        return 1;
    }

    auto system = fut.get();

    auto mavlinkPassthrough = MavlinkPassthrough{*system};
    auto udpPulseReceiver   = UDPPulseReceiver{"127.0.0.1", 30000, mavlinkPassthrough};
    
    CommandHandler{*system, mavlinkPassthrough};

    udpPulseReceiver.start();

    std::cout << "Ready\n";

    while (true) {
        udpPulseReceiver.receive();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

    return 0;
}
