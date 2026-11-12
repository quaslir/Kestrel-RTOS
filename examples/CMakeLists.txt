# Example firmware. Each one is a single main.c using the public kernel API
# and the board drivers; flash with: cmake --build build --target flash_<name>

kestrel_add_firmware(example_blinky blinky/main.c)
kestrel_add_firmware(example_mutex  mutex/main.c)
kestrel_add_firmware(example_queue  queue/main.c)
kestrel_add_firmware(example_isr    isr/main.c)
