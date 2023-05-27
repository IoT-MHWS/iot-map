#pragma once

#include "cws/air/plain.hpp"
#include "cws/physical.hpp"
#include <list>
#include <memory>

namespace Air {

/*
 * Air is moved in container to maintain certain constraints:
 * 1) temperature of everything should be equal
 */
class Container {
  std::list<std::unique_ptr<Plain>> airList;

public:
  Container() = default;

  Container(Container && obj) noexcept { std::swap(this->airList, obj.airList); }

  Container(const Container & obj) noexcept {
    airList.clear();
    for (const auto & e : obj.airList) {
      airList.push_back(std::unique_ptr<Air::Plain>(e->clone()));
    }
  }

  bool hasAir() const;
  double getHeatTransferCoef() const;
  Temperature getTemperature() const;
  void updateTemperature(double heatAirTransfer);

private:
  void getHeatTransferAndTotalWeight(double * totalWeight, double * transferCoef) const;
  void normalizeTemperature();
};

};// namespace Air