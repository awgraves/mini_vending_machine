#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
  Using standard 11-bit CAN message IDs.

  Choosing the following ID structure:
  MSB->LSB: [3 bit class] [4 bit node] [4 bit RESERVED]

  Lowest value ID on the bus always wins arbitration.
  Therefore, lower numbered classes / nodes are prioritized.
*/
#define CAN_MAKE_ID(cls, node) (((cls) << 8) | ((node) << 4))
#define CAN_MASK_CLASS (0x700) // 3 most significant bits
#define CAN_MASK_CLASS_AND_NODE (0x7F0) // 8 most significant bits

enum can_class {
  CAN_CLASS_CMD = 0,
  CAN_CLASS_RESP = 1,
};

enum can_node {
  CAN_NODE_CONTROLLER = 0,
  CAN_NODE_GANTRY = 1,
  CAN_NODE_DISPENSER = 2
};

#define CAN_ID_GANTRY_MOVE CAN_MAKE_ID(CAN_CLASS_CMD, CAN_NODE_GANTRY)
#define CAN_ID_GANTRY_RESP CAN_MAKE_ID(CAN_CLASS_RESP, CAN_NODE_GANTRY)
#define CAN_ID_DISPENSE CAN_MAKE_ID(CAN_CLASS_CMD, CAN_NODE_DISPENSER)
#define CAN_ID_DISPENSE_RESP CAN_MAKE_ID(CAN_CLASS_RESP, CAN_NODE_DISPENSER)

enum can_result {
  CAN_RES_BAD_ARG,
  CAN_RES_BUSY,
  CAN_RES_TIMEOUT,
  CAN_RES_ERR,
  CAN_RES_OK,
};

enum can_target {
  CAN_TARGET_A = 1, // starting at 1 as precaution against zeroed payloads (if something breaks)
  CAN_TARGET_B,
  CAN_TARGET_C,
  CAN_TARGET_D,
  CAN_TARGET_E,
  CAN_TARGET_F,
  CAN_TARGET_HOME // gantry only
};

static bool is_cubby(enum can_target t) { return t >= CAN_TARGET_A && t <= CAN_TARGET_F;}
static bool is_position(enum can_target t) {return t >= CAN_TARGET_A && t <= CAN_TARGET_HOME;}

struct can_gantry_move { uint8_t vending_target;};
struct can_dispense {uint8_t vending_target;};
struct can_response {uint8_t result;};
