#pragma once

#include "cws/general.hpp"
#include "cws/map.hpp"
#include "cws/simulation/map_query.hpp"
#include "cws/simulation/general.hpp"
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <thread>

class SimulationMaster;
class SimulationInterface;

class SimulationSlave {
  SimulationMaster & master;
  std::thread worker;

public:
  SimulationSlave(SimulationMaster & master) : master(master) {}

  void run();
  void wait();
  void exit();

private:
  void execute();

  void waitMasterProcess();
  void updateSimulationMap();
  void notifyMasterReady();
};

/* Manages map and passed it to interface */
class SimulationMaster {
  friend SimulationSlave;

  MapQuery mapQuery;
  std::shared_mutex map_mutex;

  SimulationState state;

  SimulationInterface & interface;

  SimulationSlave slave;

  std::jthread worker;

  std::condition_variable cv;

  std::mutex run_mutex;
  bool runReady = false;
  bool runProcessed = false;
  bool runDoExit = false;

public:
  SimulationMaster(SimulationInterface & interface)
      : mapQuery(Dimension{10, 10}), interface(interface), slave(*this) {}

  void run();
  void wait();
  void exit();

public:
  // /* Read and write */
  // std::pair<std::unique_lock<std::shared_mutex> &&, Map &> accessMap() {
  //   std::unique_lock lock(map_mutex);
  //   return std::make_pair(std::move(lock), std::ref(map));
  // }

  // std::pair<std::shared_lock<std::shared_mutex> &&, const Map &> getMap() {
  //   std::shared_lock lock(map_mutex);
  //   return std::make_pair(std::move(lock), map);
  // }

private:
  void execute(std::stop_token stoken);

  bool processStopRequest(const std::stop_token & stoken);

  void updateSimulationState();
  void updateSimulationMap();
  void notifySlaveReady();
  void waitSlaveProcess();
  void waitDurationExceeds(
      const SimulationState & state,
      const std::chrono::time_point<std::chrono::high_resolution_clock> & start);

  void updateSimulationMapEntry(std::unique_ptr<SubjectQuery> && query);
};
