# MLX90614 component for esp-idf

## Installation

    idf.py add-dependency "larryli/mlx90614"

## Getting Started

### New i2c master

```c
#include "driver/i2c_master.h"

i2c_master_bus_config_t i2c_bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = PORT_NUMBER,
    .scl_io_num = SCL_IO_PIN,
    .sda_io_num = SDA_IO_PIN,
    .flags.enable_internal_pullup = true,
};
i2c_master_bus_handle_t bus_handle;

i2c_new_master_bus(&i2c_bus_config, &bus_handle);
```

### Init MLX90614 device

```c
#include "mlx90614.h"

mlx90614_config_t mlx90614_config = {
    .mlx90614_device.scl_speed_hz = MASTER_FREQUENCY,
    .mlx90614_device.device_address = 0x5A,
};

mlx90614_handle_t mlx90614_handle;
mlx90614_init(bus_handle, &mlx90614_config, &mlx90614_handle);
```

### Get object and ambient temperatures 

```c
float to, ta;

mlx90614_get_to(mlx90614_handle, &to);
mlx90614_get_ta(mlx90614_handle, &ta);
```
