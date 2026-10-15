/* The FOMAJAM USB MIDI host box (CC.md): an Adafruit Feather RP2040 with USB Type A Host, a USB hub in its
 * USB-A socket, and the FM-1 and controllers on the hub. Whatever any of them sends goes to all of the others,
 * so there is nothing to set up. The red LED blinks with the traffic. */
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "pio_usb.h"
#include "tusb.h"

#define PIN_DP 16    /* the USB-A socket's D+ (D- is the next pin) */
#define PIN_5V_EN 18 /* its 5 V */
#define PIN_LED 13

static uint32_t led_off_at;

static void forward(void) {
  uint8_t p[4];
  for (uint8_t from = 0; from < CFG_TUH_MIDI; from++) {
    if (!tuh_midi_mounted(from)) {
      continue;
    }
    while (tuh_midi_packet_read(from, p)) {
      p[0] &= 0x0F; /* every message to each device's first port (cable 0) */
      for (uint8_t to = 0; to < CFG_TUH_MIDI; to++) {
        if (to != from && tuh_midi_mounted(to)) {
          tuh_midi_packet_write(to, p);
        }
      }
      gpio_put(PIN_LED, 1);
      led_off_at = to_ms_since_boot(get_absolute_time()) + 20;
    }
  }
  for (uint8_t to = 0; to < CFG_TUH_MIDI; to++) {
    if (tuh_midi_mounted(to)) {
      tuh_midi_write_flush(to);
    }
  }
}

int main(void) {
  set_sys_clock_khz(120000, true); /* PIO USB needs a multiple of 12 MHz */
  gpio_init(PIN_LED);
  gpio_set_dir(PIN_LED, GPIO_OUT);
  gpio_init(PIN_5V_EN);
  gpio_set_dir(PIN_5V_EN, GPIO_OUT);
  gpio_put(PIN_5V_EN, 1);

  pio_usb_configuration_t cfg = PIO_USB_DEFAULT_CONFIG;
  cfg.pin_dp = PIN_DP;
  tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &cfg);
  tuh_init(BOARD_TUH_RHPORT);

  while (1) {
    tuh_task();
    forward();
    if (led_off_at && (int32_t)(to_ms_since_boot(get_absolute_time()) - led_off_at) >= 0) {
      gpio_put(PIN_LED, 0);
      led_off_at = 0;
    }
  }
}
