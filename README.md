# Real-Time Environmental Monitor on STM32F446RE with FreeRTOS

A deterministic, multi-tasking telemetry system implemented on an ARM Cortex-M4 microcontroller (`STM32F446RE`) running the FreeRTOS kernel. This project interfaces a Texas Instruments HDC1080 sensor (over `I2C1`) and an SSD1306 128x64 OLED display (over `I2C3`), showcasing thread-safe hardware resource sharing, periodic task scheduling, and inter-task queue communications.

## Key Highlights

* **Deterministic Scheduling:** Periodic task execution utilizing `vTaskDelayUntil()` to eliminate timing drift introduced by hardware driver conversion delays.
* **Hardware Concurrency & Mutual Exclusion:** Diagnosed and resolved an asynchronous I2C shared bus hazard where equal-priority preemption corrupted two-stage sensor conversions, using a FreeRTOS Mutex with Priority Inheritance.
* **Dedicated HAL Timebase Architecture:** Isolated STM32 HAL tick generation onto a dedicated hardware timer (`TIM6` via `stm32f4xx_hal_timebase_tim.c`) to prevent interrupt priority inversion and `SysTick` collisions with the FreeRTOS scheduler.
* **Zero-Copy / Thread-Safe Pipeline:** Tagged inter-task message queues decouple real-time sensor acquisition from display and telemetry logging tasks.

## System Architecture

The firmware utilizes a decoupled Producer-Consumer pattern coordinated through a FreeRTOS Queue and a hardware Mutex.

```text
       +-----------------------+
       |      vTempTask        |
       |  (Priority 3, 1000ms) |
       +-----------------------+
                   |
     [Takes hdc1080_i2c_mutex]
     [Reads HDC1080 via I2C1 ]
     [Gives hdc1080_i2c_mutex]
                   |
                   | (Tagged SensorMsg_t: SENSOR_TYPE_TEMP)
                   v
       +-----------------------+       +------------------------------------+
       |     vHumidityTask     |       |                                    |
       |  (Priority 2, 1000ms) |------>|            xSensorQueue            |
       +-----------------------+       |   (Queue Depth: 10 SensorMsg_t)    |
                   |                   |                                    |
     [Takes hdc1080_i2c_mutex]         +------------------------------------+
     [Reads HDC1080 via I2C1 ]                            |
     [Gives hdc1080_i2c_mutex]                            | (Non-blocking drain)
                   |                                      v
       (Tagged SensorMsg_t:                    +----------------------+
        SENSOR_TYPE_HUMIDITY)                  |     vDisplayTask     |
                                               | (Priority 1, 500ms)  |
                                               +----------------------+
                                                          |
                                           +--------------+--------------+
                                           |                             |
                                           v                             v
                                  [ SSD1306 OLED via I2C3 ]    [ Telemetry Log via USART2 ]
```

## Hardware Pinout & Peripheral Configuration

| Peripheral | Bus / Instance | Speed | STM32 Pins | Description |
| :--- | :--- | :--- | :--- | :--- |
| **HDC1080 Sensor** | `I2C1` | 100 kHz (Standard) | `PB8` (SCL), `PB9` (SDA) | Ambient Temperature & Relative Humidity sensor |
| **SSD1306 OLED** | `I2C3` | 400 kHz (Fast Mode) | `PA8` (SCL), `PC9` (SDA) | 128x64 monochrome graphical display |
| **ST-Link VCP** | `USART2` | 115200 8-N-1 | `PA2` (TX), `PA3` (RX) | Diagnostic serial console (`USE_USART_DEBUG`) |
| **HAL Timebase** | `TIM6` | 1 kHz (1 ms period) | Internal Interrupt | Dedicated `HAL_IncTick()` source (`stm32f4xx_hal_timebase_tim.c`) |
| **FreeRTOS Kernel** | `SysTick` | 1 kHz (1 ms tick) | Internal Interrupt | Context switching and task timebase (Priority 15) |

## Task Specifications & Scheduling

| Task Name | Priority | Period | Target Subsystem | Sync Primitive | Stack Size |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`vTempTask`** | 3 (High) | 1000 ms | HDC1080 (`hi2c1`) | `hdc1080_i2c_mutex` + `xSensorQueue` | 256 words |
| **`vHumidityTask`** | 2 (Med) | 1000 ms | HDC1080 (`hi2c1`) | `hdc1080_i2c_mutex` + `xSensorQueue` | 256 words |
| **`vDisplayTask`** | 1 (Low) | 500 ms | SSD1306 (`hi2c3`), UART | `xSensorQueue` | 512 words |

## Deep-Dive: Concurrency, Bus Hazards & Lessons Learned

### 1. The Shared Bus Hazard (+51.5 °C Race Condition)

The HDC1080 measurement sequence is inherently split into two phases separated by an internal physical conversion time:

1. **Trigger Phase:** The master writes the register pointer (`0x00` for Temperature, `0x01` for Humidity).
2. **Conversion Phase:** The master must wait $\approx 15\text{ ms}$ for the ADC conversion to settle.
3. **Read Phase:** The master reads 2 raw data bytes from the sensor.

**The Bug:** When `vTempTask` and `vHumidityTask` ran at equal priority (`2`) without a mutex, FreeRTOS Round-Robin time slicing (`configUSE_TIME_SLICING = 1`) interrupted `vTempTask` during its 15 ms wait loop. The scheduler switched to `vHumidityTask`, which immediately transmitted register pointer `0x01` over `I2C1`. This aborted the ongoing temperature measurement inside the sensor silicon. When `vTempTask` resumed, it read raw humidity data and evaluated it against the temperature polynomial:

$$
\text{Temp (C)} = \left(\frac{\text{RAW}}{65536}\right) \times 165 - 40
$$

A typical indoor relative humidity of $70\%$ resulted in an erroneous reading of:

$$
(0.70 \times 165) - 40 = 75.5^\circ\text{C} \quad (\approx +51.5^\circ\text{C}\text{ error above nominal})
$$

**The Fix:** A FreeRTOS Mutex (`xSemaphoreCreateMutex()`) was integrated into the Board Support Package utility wrapper (`Core/BSP/utils/utils_hdc1080.c`). The entire Trigger $\rightarrow$ Wait $\rightarrow$ Read sequence is atomic relative to other RTOS tasks, preventing register corruption regardless of task priorities or preemption intervals.

### 2. Cortex-M BASEPRI Masking Before Scheduler Launch

FreeRTOS critical sections manipulate the ARM Cortex-M `BASEPRI` register to mask interrupts at and below `configMAX_SYSCALL_INTERRUPT_PRIORITY`. When a kernel synchronization primitive (like `xSemaphoreCreateMutex()`) is created before the scheduler is running, the kernel's critical nesting counter (`uxCriticalNesting = 0xaaaaaaaa`) prevents interrupts from being unmasked prior to `vTaskStartScheduler()`.

If hardware drivers relying on `HAL_Delay()` are called after mutex creation but before scheduler launch, `HAL_GetTick()` ceases to increment, leading to an infinite busy-wait hang. The issue was permanently mitigated by:

1. Enforcing strict boot sequence order: Hardware initialization (`HDC1080_Init()`) runs **prior** to RTOS object instantiation (`xSemaphoreCreateMutex()`).
2. Migrating the HAL timebase from `SysTick` to `TIM6` (`stm32f4xx_hal_timebase_tim.c`) set to high interrupt priority (above the FreeRTOS syscall threshold).

## Project Directory Layout

```text
FreeRTOS_STM32_TH_Display/
├── .gitignore
├── LICENSE
├── README.md
├── FreeRTOS_STM32_TH_Display.ioc          # STM32CubeMX device configuration
├── STM32F446RETX_FLASH.ld                 # GNU Flash Linker script
├── STM32F446RETX_RAM.ld                   # GNU RAM Linker script
├── Core/
│   ├── BSP/                               # Board Support Package
│   │   ├── utils/                         # Thread-Safe RTOS Abstraction & Tasks
│   │   │   ├── task_handler.c             # FreeRTOS task implementations & queue logic
│   │   │   ├── task_handler.h             # Task prototypes and queue structures
│   │   │   ├── utils_hdc1080.c            # Mutex-protected HDC1080 sensor API
│   │   │   ├── utils_hdc1080.h
│   │   │   ├── utils_oled.c               # High-level OLED UI formatting API
│   │   │   └── utils_oled.h
│   │   ├── hdc1080.c                      # Low-level HDC1080 I2C register driver
│   │   ├── hdc1080.h
│   │   ├── ssd1306.c                      # Low-level SSD1306 I2C graphics driver
│   │   ├── ssd1306.h
│   │   ├── ssd1306_conf.h                 # Display geometry, bus, and font selections
│   │   ├── ssd1306_fonts.c                # Monospaced font lookup tables
│   │   └── ssd1306_fonts.h
│   ├── Inc/
│   │   ├── FreeRTOSConfig.h               # FreeRTOS kernel & tick configuration
│   │   ├── main.h                         # Peripheral defines & USE_USART_DEBUG macro
│   │   ├── stm32f4xx_hal_conf.h
│   │   └── stm32f4xx_it.h
│   ├── Src/
│   │   ├── main.c                         # Peripherals init, task creation & scheduler start
│   │   ├── stm32f4xx_hal_msp.c            # HAL MSP GPIO/Clock initialization
│   │   ├── stm32f4xx_hal_timebase_tim.c   # TIM6 1kHz timebase for HAL_IncTick()
│   │   ├── stm32f4xx_it.c                 # Interrupt Service Routines
│   │   ├── syscalls.c
│   │   ├── sysmem.c
│   │   └── system_stm32f4xx.c
│   ├── Startup/                           # Cortex-M4 startup assembly
│   └── Third_Party/
│       └── FreeRTOS/                      # Native FreeRTOS kernel source & portable layer
└── Drivers/
    ├── CMSIS/                             # ARM Cortex-M4 CMSIS headers
    └── STM32F4xx_HAL_Driver/              # STM32F4xx HAL peripheral drivers
```

## Build & Run Instructions

### Prerequisites

* **Toolchain:** `arm-none-eabi-gcc` (GNU Tools for STM32)
* **IDE:** STM32CubeIDE
* **Hardware:** NUCLEO-F446RE development board, TI HDC1080 breakout, SSD1306 0.96" I2C OLED

### Compilation Settings

1. Open the project in STM32CubeIDE via **File** $\rightarrow$ **Import** $\rightarrow$ **Existing Projects into Workspace**.
2. Verify include paths in **Project** $\rightarrow$ **Properties** $\rightarrow$ **C/C++ General** $\rightarrow$ **Paths and Symbols** $\rightarrow$ **Includes** use workspace-relative paths (`../Core/BSP`, `../Core/BSP/utils`, and the `../Core/Third_Party/FreeRTOS` header folders) rather than local absolute drive paths.
3. Verify floating-point formatting is enabled for `snprintf`:
   * Navigate to **Project** $\rightarrow$ **Properties** $\rightarrow$ **C/C++ Build** $\rightarrow$ **Settings** $\rightarrow$ **Tool Settings** $\rightarrow$ **MCU GCC Linker** $\rightarrow$ **Miscellaneous**.
   * Ensure the flag `-u _printf_float` is enabled.
4. Build the project: **Project** $\rightarrow$ **Build Project** (`Ctrl+B`).
5. Flash to target: Click **Run** $\rightarrow$ **Debug** (`F11`) with the Nucleo connected over USB.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details. Third-party drivers (`stm32-ssd1306` and `FreeRTOS`) remain under their respective original licenses.