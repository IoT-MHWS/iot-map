#pragma once

#include "converters.hpp"
#include "cws/simulation/interface.hpp"
#include "cwspb/service/map.grpc.pb.h"
#include "cwspb/service/map.pb.h"
#include <grpcpp/support/status.h>

class MapService final : public cws::MapService::Service {
private:
  SimulationInterface & interface;

private:
public:
  MapService(SimulationInterface & interface) : interface(interface) {}

  ::grpc::Status GetMapDimension(::grpc::ServerContext * context,
                                 const ::cws::Request * request,
                                 ::cws::ResponseDimension * response) override {
    auto dimension = interface.getDimension();

    if (!dimension.isSet()) {
      auto res = response->mutable_base();
      auto resStatus = res->mutable_status();
      resStatus->set_text("map is not created");
      resStatus->set_type(cws::ErrorType::ERROR_TYPE_BAD_REQUEST);
      return grpc::Status::OK;
    }

    auto rDimension = response->mutable_dimension();
    rDimension->set_width(dimension.get().width);
    rDimension->set_height(dimension.get().height);

    return grpc::Status::OK;
  }

  ::grpc::Status CreateMap(::grpc::ServerContext * context,
                           const ::cws::RequestDimension * request,
                           ::cws::Response * response) override {

    if (!request->has_dimension()) {
      auto rStatus = response->mutable_status();
      rStatus->set_text("dimension is not specified");
      rStatus->set_type(cws::ErrorType::ERROR_TYPE_BAD_REQUEST);
      return grpc::Status::OK;
    }
    Dimension dimension{
        .width = request->dimension().width(),
        .height = request->dimension().height(),
    };

    interface.setDimension(dimension);

    return grpc::Status::OK;
  }

  ::grpc::Status GetCell(::grpc::ServerContext * context,
                         const ::cws::RequestCell * request,
                         ::cws::ResponseCell * response) override {
    auto map = interface.getMap();

    if (!map) {
      auto resStatus = response->mutable_status();
      resStatus->set_text("map is not created");
      resStatus->set_type(cws::ErrorType::ERROR_TYPE_BAD_REQUEST);
      return grpc::Status::OK;
    }

    auto dimension = map->getDimension();

    Coordinates coord;
    fromCoordinates(coord, request->coordinates());

    if (coord.x >= dimension.width || coord.x < 0 || coord.y >= dimension.height ||
        coord.y < 0) {
      auto resStatus = response->mutable_status();
      resStatus->set_text("coordinates out of bounds");
      resStatus->set_type(cws::ErrorType::ERROR_TYPE_BAD_REQUEST);
      return grpc::Status::OK;
    }

    const auto & layers = map->getLayers();
    const auto & tempLayer = layers.getTemperatureLayer();
    const auto & subjectLayer = layers.getSubjectLayer();

    const auto cell = Cell{.coordinates = coord,
                           .temp = tempLayer.getCell(coord).getElement(),
                           .subject = subjectLayer.getCell(coord).getElement()};

    toCell(*response->mutable_cell(), cell);
    return grpc::Status::OK;
  }

  ::grpc::Status GetMap(::grpc::ServerContext * context, const ::cws::Request * request,
                        ::grpc::ServerWriter<::cws::ResponseCell> * writer) override {
    grpc::WriteOptions options;

    auto map = interface.getMap();

    if (!map) {
      cws::ResponseCell response;

      auto resStatus = response.mutable_status();
      resStatus->set_text("map is not created");
      resStatus->set_type(cws::ErrorType::ERROR_TYPE_BAD_REQUEST);
      writer->WriteLast(response, options);
      return grpc::Status::OK;
    }

    auto dimension = map->getDimension();

    const auto & layers = map->getLayers();
    const auto & tempLayer = layers.getTemperatureLayer();
    const auto & subjectLayer = layers.getSubjectLayer();

    for (int x = 0; x < dimension.width; ++x) {
      for (int y = 0; y < dimension.height; ++y) {
        cws::ResponseCell response;

        Coordinates coord{.x = x, .y = y};

        const auto cell = Cell{.coordinates = coord,
                               .temp = tempLayer.getCell(coord).getElement(),
                               .subject = subjectLayer.getCell(coord).getElement()};

        toCell(*response.mutable_cell(), cell);
        writer->Write(response);
      }
    }
    return grpc::Status::OK;
  }
};