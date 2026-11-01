# Demo firmware: links main.c with the startup code, drivers and linker script.
# kestrel_add_firmware() is defined in the root CMakeLists.txt.
kestrel_add_firmware(kestrel_demo main.c)
