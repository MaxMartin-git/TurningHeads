#pragma once

#include <Arduino.h>

namespace th {

enum class Side : uint8_t {
  Left = 0,
  Right = 1,
  Unknown = 255,
};

enum class NodeRole : uint8_t {
  Coordinator = 0,
  Base_L,
  Base_R,
  Satellite_L,
  Satellite_R,
  Generic,
};

struct NodeCapabilities {
  bool hasDcMotor;
  bool hasEyeballServos;
};

struct DeviceProfile {
  uint8_t nodeId;
  NodeRole role;
  Side side;
  NodeCapabilities capabilities;
  const char* label;
};

inline const char* toString(NodeRole role) {
  switch (role) {
    case NodeRole::Coordinator: return "coordinator";
    case NodeRole::Base_L: return "base_l";
    case NodeRole::Base_R: return "base_r";
    case NodeRole::Satellite_L: return "satellite_l";
    case NodeRole::Satellite_R: return "satellite_r";
    case NodeRole::Generic:
    default:
      return "generic";
  }
}

inline bool isBaseRole(NodeRole role) {
  return role == NodeRole::Base_L || role == NodeRole::Base_R || role == NodeRole::Coordinator;
}

inline bool isSatelliteRole(NodeRole role) {
  return role == NodeRole::Satellite_L || role == NodeRole::Satellite_R;
}

inline bool isCoordinatorRole(NodeRole role) {
  return role == NodeRole::Coordinator;
}

inline uint8_t sideToIndex(Side side) {
  return side == Side::Right ? 1 : 0;
}

inline const char* toString(Side side) {
  switch (side) {
    case Side::Left: return "left";
    case Side::Right: return "right";
    case Side::Unknown:
    default:
      return "unknown";
  }
}

inline DeviceProfile getCoordinatorProfile() {
  return {0, NodeRole::Coordinator, Side::Left, {true, true}, "Base_L_as_Coordinator"};
}

inline DeviceProfile getRemoteNodeProfile(uint8_t nodeId) {
  // Target topology (expandable):
  // - Coordinator: Base_L (DC) + optional local eyeball
  // - Node 1: Satellite_L (eyeball)
  // - Node 2: Base_R (DC)
  // - Node 3: Satellite_R (eyeball)
  switch (nodeId) {
    case 1: return {1, NodeRole::Satellite_L, Side::Left, {false, true}, "Satellite_L"};
    case 2: return {2, NodeRole::Base_R, Side::Right, {true, false}, "Base_R"};
    case 3: return {3, NodeRole::Satellite_R, Side::Right, {false, true}, "Satellite_R"};
    default: return {nodeId, NodeRole::Generic, Side::Unknown, {false, false}, "Unknown"};
  }
}

#ifndef NODE_ID
#define NODE_ID 0
#endif

inline DeviceProfile getCurrentNodeProfile() {
#if NODE_ID == 0
  return getCoordinatorProfile();
#else
  return getRemoteNodeProfile(static_cast<uint8_t>(NODE_ID));
#endif
}

}  // namespace th
