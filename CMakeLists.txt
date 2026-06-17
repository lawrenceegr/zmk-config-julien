add_subdirectory_ifdef(CONFIG_IS31FL3733_BRIDGE drivers/led)
target_sources(app PRIVATE src/configurator.c)