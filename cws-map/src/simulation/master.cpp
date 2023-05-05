#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>

#include <cws/simulation/interface.hpp>
#include <cws/simulation/simulation.hpp>

void SimulationMaster::run() {
  std::jthread worker2(std::bind_front(&SimulationMaster::execute, this));
  worker.swap(worker2);
}

void SimulationMaster::wait() { worker.join(); }

// called by outer thread
void SimulationMaster::exit() {
  worker.request_stop();
  this->wait();
}

void SimulationMaster::execute(std::stop_token stoken) {
  this->slave.run();

  while (true) {
    auto clockStart = std::chrono::high_resolution_clock::now();

    if (processStopRequest(stoken)) {
      return;
    }

    updateSimulationState();
    updateSimulationMap();

#ifndef NDEBUG
    std::cout << "master: " << state << std::endl;
#endif

    bool isStatusRunning = state.status == SimulationStatus::RUNNING;
    bool isNotLastTick = state.currentTick < state.lastTick;
    bool isSimTypeINF = state.type == SimulationType::INFINITE;

    if (isStatusRunning && (isSimTypeINF || isNotLastTick)) {
      state.currentTick += 1;
      notifySlaveReady();
      waitSlaveProcess();
    }

    if (state.currentTick == state.lastTick) {
      state.status = SimulationStatus::STOPPED;
    }

    interface.masterSetState(state);
#ifndef NDEBUG
    std::cout << "master: " << "updated interface state"<< std::endl;
#endif

    waitDurationExceeds(state, clockStart);
  };
}

bool SimulationMaster::processStopRequest(const std::stop_token & stoken) {
  bool requested = stoken.stop_requested();
  if (requested) {
    slave.exit();
  }
  return requested;
}

void SimulationMaster::updateSimulationState() {
  SimulationStateIn stateIn = interface.masterGetState();
  if (stateIn.simType.isSet()) {
    state.type = stateIn.simType.get();
  }
  if (stateIn.simStatus.isSet()) {
    state.status = stateIn.simStatus.get();
  }
  if (stateIn.currentTick.isSet()) {
    state.currentTick = stateIn.currentTick.get();
  }
  if (stateIn.lastTick.isSet()) {
    state.lastTick = stateIn.lastTick.get();
  }
  if (stateIn.taskFrequency.isSet()) {
    state.taskFrequency = stateIn.taskFrequency.get();
  }
}

void SimulationMaster::updateSimulationMap() {
  auto [lock, queries] = interface.masterAccessQueries();

#ifndef NDEBUG
  if (!queries.empty()) {
    std::cout << "master: Simulation map updated." << std::endl;
  }
#endif

  while (!queries.empty()) {
    updateSimulationMapEntry(std::move(queries.front()));
    queries.pop();
  }
}

void SimulationMaster::notifySlaveReady() {
  {
    std::unique_lock lock(run_mutex);
    runReady = true;
  }
  cv.notify_one();
}

void SimulationMaster::waitSlaveProcess() {
  std::unique_lock lock(run_mutex);
  cv.wait(lock, [this] { return runProcessed; });
  runProcessed = false;
}

void SimulationMaster::waitDurationExceeds(
    const SimulationState & state,
    const std::chrono::time_point<std::chrono::high_resolution_clock> & start) {

  auto duration = static_cast<int>(1'000'000'000 / state.taskFrequency);
  std::chrono::nanoseconds taskMaxDuration(duration);

  auto leadTime = std::chrono::high_resolution_clock::now() - start;
  auto waitTime = std::max(taskMaxDuration - leadTime, std::chrono::nanoseconds(0));

  std::this_thread::sleep_for(waitTime);
}

void SimulationMaster::updateSimulationMapEntry(
    std::unique_ptr<SubjectQuery> && query) {
  mapQuery.updateMap(std::move(*query.get()));
}
