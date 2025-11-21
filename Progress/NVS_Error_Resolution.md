# NVS Initialization Error Resolution

## Issue Description
The wM-Bus Gateway firmware experienced NVS (Non-Volatile Storage) handle errors during initialization, as shown in the following logs:
```
E (397) CONFIG_MANAGER: Error opening NVS handle
W (407) CONFIG_MANAGER: No stored WiFi credentials found
E (417) CONFIG_MANAGER: Error opening NVS handle
W (417) CONFIG_MANAGER: No stored backend settings found
```

## Root Cause Analysis
The problem was caused by multiple modules initializing NVS independently:
1. `app_main.c` initialized NVS in the main function
2. `whitelist_manager.c` initialized NVS again in `init_whitelist()`
3. Potentially other modules could also initialize NVS

This double initialization caused conflicts and handle errors.

## Solution Implemented

### 1. Centralized NVS Management
Created a dedicated `nvs_manager` module with:
- Single initialization point in `app_main()`
- Thread-safe operations using mutex
- Comprehensive error handling
- Statistics and monitoring functions

### 2. Safe API Functions
Implemented safe wrapper functions:
- `nvs_manager_safe_open()` - Thread-safe handle opening
- `nvs_manager_safe_close()` - Proper handle closing
- `nvs_manager_get_string()` - Safe string retrieval
- `nvs_manager_set_string()` - Safe string storage

### 3. Removed Redundant Initializations
- Removed NVS initialization from `whitelist_manager.c`
- Removed NVS initialization from `config_manager.c`
- Updated all modules to verify NVS is initialized before use

### 4. Thread Safety
- Added mutex protection for concurrent NVS access
- Implemented acquire/release functions for thread coordination

### 5. Enhanced Error Handling
- Added comprehensive error checking at all levels
- Implemented proper cleanup on failures
- Added descriptive logging for debugging

### 6. Monitoring Capabilities
- Added statistics functions to track NVS usage
- Implemented health check procedures
- Added high-usage warning notifications

## Files Modified

### New Files
- `main/nvs_manager/nvs_manager.h` - API declarations
- `main/nvs_manager/nvs_manager.c` - Implementation
- `main/nvs_manager/CMakeLists.txt` - Component registration

### Modified Files
- `main/app_main.c` - Updated to use centralized NVS manager
- `main/CMakeLists.txt` - Added nvs_manager component
- `main/config/config_manager.c` - Updated to use safe NVS functions
- `main/whitelist/whitelist_manager.c` - Updated to use safe NVS functions

## Testing and Validation

The solution was validated by:
- Ensuring no duplicate NVS initialization occurs
- Verifying all existing functionality remains unchanged
- Confirming thread-safe operations
- Testing error handling paths
- Validating monitoring and statistics functions

## Benefits Achieved

1. **Resolved NVS Errors**: Eliminated the handle errors during initialization
2. **Improved Reliability**: More robust NVS operations with proper error handling
3. **Enhanced Scalability**: Centralized architecture supports future NVS needs
4. **Better Monitoring**: Built-in statistics and health checks
5. **Thread Safety**: Protected against race conditions in concurrent access

## Usage Guidelines for Future Development

1. Always check `nvs_manager_is_initialized()` before NVS operations
2. Use safe functions instead of direct NVS API calls
3. Include `"nvs_manager/nvs_manager.h"` in modules requiring NVS access
4. Do not initialize NVS independently in new modules
5. Utilize health check and statistics functions for monitoring