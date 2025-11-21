# Web UI File Structure Improvement

## Current Implementation
The web UI is currently implemented with HTML, CSS, and JavaScript embedded as C strings in:
- `main/config/web_ui.c`
- `main/config/http_handlers.c`

## Recommended Improvement
Store HTML, CSS, and JavaScript in separate files for better maintainability:

### New File Structure
```
main/
└── static/
    ├── index.html          # Main dashboard
    ├── wifi.html           # WiFi configuration
    ├── whitelist.html      # Whitelist management
    ├── radio.html          # Radio settings
    ├── backend.html        # Backend settings
    ├── styles/
    │   └── main.css        # Main styles
    ├── scripts/
    │   ├── main.js         # Main JavaScript
    │   └── utils.js        # Utility functions
    └── assets/
        └── logo.svg        # Static assets
```

### Implementation Approach
The ESP-IDF project supports static file serving through:

1. **SPIFFS Partition** (recommended):
   - Store files in flash memory
   - Access via standard file operations
   - Good for production use

2. **Embedded Files**:
   - Use CMake's `idf_component_register` with `EMBED_TXTFILES`
   - Files embedded directly into firmware
   - Good for simple interfaces

3. **Embedded Binary**:
   - Use `idf_component_register` with `EMBED_FILES`
   - For mixed content types

### Benefits of This Approach
- Separation of concerns (HTML/CSS/JS separate from C code)
- Better maintainability and readability
- Ability to use web development tools
- Easier testing and debugging
- Standard web development workflow

### Migration Process
1. Create static file directories
2. Move HTML/CSS/JS from C strings to separate files
3. Configure ESP-IDF to embed or serve these files
4. Update HTTP handlers to serve static content
5. Add build process for static assets (if needed)

This would significantly improve the code structure and maintainability of the web interface.