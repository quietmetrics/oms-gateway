// OMS / wM-Bus device type lookup (EN 13757-7 / OMS Vol.2 tables)
#pragma once

#include <stdint.h>
#include <stddef.h>

// Device type codes (0x00–0x3F plus 0xFF wildcard)
typedef enum
{
    OMS_DEV_TYPE_OTHER = 0x00,
    OMS_DEV_TYPE_OIL_METER = 0x01,
    OMS_DEV_TYPE_ELECTRICITY_METER = 0x02,
    OMS_DEV_TYPE_GAS_METER = 0x03,
    OMS_DEV_TYPE_HEAT_METER_RETURN = 0x04,
    OMS_DEV_TYPE_STEAM_METER = 0x05,
    OMS_DEV_TYPE_WARM_WATER_METER = 0x06,
    OMS_DEV_TYPE_WATER_METER = 0x07,
    OMS_DEV_TYPE_HEAT_COST_ALLOCATOR = 0x08,
    OMS_DEV_TYPE_COMPRESSED_AIR = 0x09,
    OMS_DEV_TYPE_COOLING_METER_RETURN = 0x0A,
    OMS_DEV_TYPE_COOLING_METER_FLOW = 0x0B,
    OMS_DEV_TYPE_HEAT_METER_FLOW = 0x0C,
    OMS_DEV_TYPE_COMBINED_HEAT_COOL_METER = 0x0D,
    OMS_DEV_TYPE_BUS_SYSTEM_COMPONENT = 0x0E,
    OMS_DEV_TYPE_UNKNOWN_DEVICE_TYPE = 0x0F,
    OMS_DEV_TYPE_IRRIGATION_WATER_METER = 0x10,
    OMS_DEV_TYPE_WATER_DATA_LOGGER = 0x11,
    OMS_DEV_TYPE_GAS_DATA_LOGGER = 0x12,
    OMS_DEV_TYPE_GAS_CONVERTER = 0x13,
    OMS_DEV_TYPE_CALORIFIC_VALUE_DEVICE = 0x14,
    OMS_DEV_TYPE_HOT_WATER_METER = 0x15,
    OMS_DEV_TYPE_COLD_WATER_METER = 0x16,
    OMS_DEV_TYPE_DUAL_HOT_COLD_WATER_METER = 0x17,
    OMS_DEV_TYPE_PRESSURE_DEVICE = 0x18,
    OMS_DEV_TYPE_AD_CONVERTER = 0x19,
    OMS_DEV_TYPE_SMOKE_ALARM_DEVICE = 0x1A,
    OMS_DEV_TYPE_ROOM_SENSOR = 0x1B,
    OMS_DEV_TYPE_GAS_DETECTOR = 0x1C,
    OMS_DEV_TYPE_CO_ALARM_DEVICE = 0x1D,
    OMS_DEV_TYPE_HEAT_ALARM_DEVICE = 0x1E,
    OMS_DEV_TYPE_SENSOR_DEVICE = 0x1F,
    OMS_DEV_TYPE_BREAKER_ELECTRICITY = 0x20,
    OMS_DEV_TYPE_VALVE_GAS_WATER = 0x21,
    OMS_DEV_TYPE_RESERVED_SWITCHING_22 = 0x22,
    OMS_DEV_TYPE_RESERVED_SWITCHING_23 = 0x23,
    OMS_DEV_TYPE_RESERVED_SWITCHING_24 = 0x24,
    OMS_DEV_TYPE_CUSTOMER_DISPLAY_UNIT = 0x25,
    OMS_DEV_TYPE_RESERVED_CUSTOMER_26 = 0x26,
    OMS_DEV_TYPE_RESERVED_CUSTOMER_27 = 0x27,
    OMS_DEV_TYPE_WASTE_WATER_METER = 0x28,
    OMS_DEV_TYPE_GARBAGE_METER = 0x29,
    OMS_DEV_TYPE_RESERVED_CO2 = 0x2A,
    OMS_DEV_TYPE_RESERVED_ENVIRONMENT_2B = 0x2B,
    OMS_DEV_TYPE_RESERVED_ENVIRONMENT_2C = 0x2C,
    OMS_DEV_TYPE_RESERVED_ENVIRONMENT_2D = 0x2D,
    OMS_DEV_TYPE_RESERVED_ENVIRONMENT_2E = 0x2E,
    OMS_DEV_TYPE_RESERVED_ENVIRONMENT_2F = 0x2F,
    OMS_DEV_TYPE_RESERVED_SYSTEM_30 = 0x30,
    OMS_DEV_TYPE_COMMUNICATION_CONTROLLER = 0x31,
    OMS_DEV_TYPE_UNIDIRECTIONAL_REPEATER = 0x32,
    OMS_DEV_TYPE_BIDIRECTIONAL_REPEATER = 0x33,
    OMS_DEV_TYPE_RESERVED_SYSTEM_34 = 0x34,
    OMS_DEV_TYPE_RESERVED_SYSTEM_35 = 0x35,
    OMS_DEV_TYPE_RADIO_CONVERTER_SYSTEM_SIDE = 0x36,
    OMS_DEV_TYPE_RADIO_CONVERTER_METER_SIDE = 0x37,
    OMS_DEV_TYPE_WIRED_ADAPTER = 0x38,
    OMS_DEV_TYPE_RESERVED_SYSTEM_39 = 0x39,
    OMS_DEV_TYPE_RESERVED_SYSTEM_3A = 0x3A,
    OMS_DEV_TYPE_RESERVED_SYSTEM_3B = 0x3B,
    OMS_DEV_TYPE_RESERVED_SYSTEM_3C = 0x3C,
    OMS_DEV_TYPE_RESERVED_SYSTEM_3D = 0x3D,
    OMS_DEV_TYPE_RESERVED_SYSTEM_3E = 0x3E,
    OMS_DEV_TYPE_RESERVED_SYSTEM_3F = 0x3F,
    OMS_DEV_TYPE_WILDCARD = 0xFF
} oms_device_type_t;

typedef struct
{
    uint8_t code;              // Device type code
    const char *name;          // Human readable device type
    char category;             // '1','4','5','6','7','8','9','E','F','-' (per OMS Vol.2)
    const char *category_desc; // Short category description
} oms_device_type_info_t;

static const oms_device_type_info_t OMS_DEVICE_TYPE_TABLE[] = {
    {0x00, "Other", 'F', "Other / generic"},
    {0x01, "Oil meter", 'F', "Other energy medium"},
    {0x02, "Electricity meter", '1', "Electricity"},
    {0x03, "Gas meter", '7', "Gas"},
    {0x04, "Heat meter (return)", '6', "Heat energy"},
    {0x05, "Steam meter", 'F', "Other / steam"},
    {0x06, "Warm water meter (30–90°C)", '9', "Warm water"},
    {0x07, "Water meter", '8', "Water"},
    {0x08, "Heat cost allocator", '4', "Heat cost allocation"},
    {0x09, "Compressed air meter", 'F', "Other / compressed air"},
    {0x0A, "Cooling meter (return)", '5', "Cooling energy"},
    {0x0B, "Cooling meter (flow)", '5', "Cooling energy"},
    {0x0C, "Heat meter (flow)", '6', "Heat energy"},
    {0x0D, "Combined heat/cooling meter", '6', "Heat/cooling combined"},
    {0x0E, "Bus/system component", 'E', "System / infrastructure"},
    {0x0F, "Unknown device type", 'F', "Unknown"},
    {0x10, "Irrigation water meter", '-', "Not in OMS category list"},
    {0x11, "Water data logger", '-', "Not in OMS category list"},
    {0x12, "Gas data logger", '-', "Not in OMS category list"},
    {0x13, "Gas converter", '-', "Not in OMS category list"},
    {0x14, "Calorific value device", 'F', "Other / calorific value"},
    {0x15, "Hot water meter (>=90°C)", '9', "Hot water"},
    {0x16, "Cold water meter", '8', "Water"},
    {0x17, "Dual hot/cold water meter", '9', "Hot + cold water"},
    {0x18, "Pressure device", 'F', "Other / pressure"},
    {0x19, "A/D converter", 'F', "Other / converter"},
    {0x1A, "Smoke alarm device", 'F', "Alarm / safety"},
    {0x1B, "Room sensor", 'F', "Environmental sensor"},
    {0x1C, "Gas detector", 'F', "Alarm / gas detector"},
    {0x1D, "Carbon monoxide alarm device", 'F', "Alarm / CO"},
    {0x1E, "Heat alarm device", 'F', "Alarm / heat"},
    {0x1F, "Sensor device", 'F', "Generic sensor"},
    {0x20, "Breaker (electricity)", 'F', "Actuator / breaker"},
    {0x21, "Valve (gas or water)", 'F', "Actuator / valve"},
    {0x22, "Reserved switching device 0x22", '-', "Reserved for switching devices"},
    {0x23, "Reserved switching device 0x23", '-', "Reserved for switching devices"},
    {0x24, "Reserved switching device 0x24", '-', "Reserved for switching devices"},
    {0x25, "Customer display unit", 'E', "Customer unit / HMI"},
    {0x26, "Reserved customer unit 0x26", '-', "Reserved for customer units"},
    {0x27, "Reserved customer unit 0x27", '-', "Reserved for customer units"},
    {0x28, "Waste water meter", 'F', "Other / waste water"},
    {0x29, "Garbage meter", 'F', "Other / garbage"},
    {0x2A, "Reserved CO2", 'F', "Reserved for CO₂"},
    {0x2B, "Reserved env. meter 0x2B", '-', "Reserved for environmental meters"},
    {0x2C, "Reserved env. meter 0x2C", '-', "Reserved for environmental meters"},
    {0x2D, "Reserved env. meter 0x2D", '-', "Reserved for environmental meters"},
    {0x2E, "Reserved env. meter 0x2E", '-', "Reserved for environmental meters"},
    {0x2F, "Reserved env. meter 0x2F", '-', "Reserved for environmental meters"},
    {0x30, "Reserved system device 0x30", 'E', "Reserved system device"},
    {0x31, "Communication controller (gateway)", 'E', "Gateway / controller"},
    {0x32, "Unidirectional repeater", 'E', "System / repeater"},
    {0x33, "Bidirectional repeater", 'E', "System / repeater"},
    {0x34, "Reserved system device 0x34", 'E', "Reserved system device"},
    {0x35, "Reserved system device 0x35", 'E', "Reserved system device"},
    {0x36, "Radio converter (system side)", 'E', "System / RF converter"},
    {0x37, "Radio converter (meter side)", 'E', "System / RF converter"},
    {0x38, "Wired adapter", 'E', "System / wired adapter"},
    {0x39, "Reserved system device 0x39", 'E', "Reserved system device"},
    {0x3A, "Reserved system device 0x3A", 'E', "Reserved system device"},
    {0x3B, "Reserved system device 0x3B", 'E', "Reserved system device"},
    {0x3C, "Reserved system device 0x3C", 'E', "Reserved system device"},
    {0x3D, "Reserved system device 0x3D", 'E', "Reserved system device"},
    {0x3E, "Reserved system device 0x3E", 'E', "Reserved system device"},
    {0x3F, "Reserved system device 0x3F", 'E', "Reserved system device"},
    {0xFF, "Wildcard (search only)", '-', "Wildcard for addressing"},
};

// Lookup helper for decoded frames (e.g., pipeline → UI). Feed the device_type
// byte from the wM-Bus header and render name/category in logs or web views.
static inline const oms_device_type_info_t *oms_device_type_lookup(uint8_t code)
{
    for (size_t i = 0; i < sizeof(OMS_DEVICE_TYPE_TABLE) / sizeof(OMS_DEVICE_TYPE_TABLE[0]); ++i)
    {
        if (OMS_DEVICE_TYPE_TABLE[i].code == code)
        {
            return &OMS_DEVICE_TYPE_TABLE[i];
        }
    }
    return NULL;
}
