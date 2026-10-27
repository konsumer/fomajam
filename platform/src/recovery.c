/* SPDX-License-Identifier: GPL-3.0-only */
/* Application-level recovery, not a second firmware bank. The intact app's
 * startup, RAM flash driver and USB stack are still required. TIMER5, audio,
 * settings, samples, projects, editor and normal UI are never started here. */
static uint32_t recovery_last, recovery_fraction, recovery_usb_last;

static void recovery_poll(void)
{
    uint32_t now = fm1_ticks(), elapsed = now - recovery_last;
    recovery_last = now;
    /* Split before adding: also safe across the 32-bit TIMER4 wrap. */
    fm1_ms += elapsed / (1000u * FM1_TICKS_PER_US);
    recovery_fraction += elapsed % (1000u * FM1_TICKS_PER_US);
    fm1_ms += recovery_fraction / (1000u * FM1_TICKS_PER_US);
    recovery_fraction %= 1000u * FM1_TICKS_PER_US;
    fm1_wdt_feed();
    if (now - recovery_usb_last >= 500u * FM1_TICKS_PER_US) {
        recovery_usb_last = now;
        usb_poll();
    }
}

static void recovery_step(void)
{
    recovery_poll();
    usb_retry(fm1_ms);
    if (usb.uboot_req) {
        usb_detach();
        fm1_delay_ms(30);
        bootguard_clear(&bootguard);
        fm1_enter_uboot();
        return;
    }
    ota_service();
    if (usb.ota_req) {
        usb.ota_req = 0;
        if (flash_ok) ota_session();         /* same validated updater as normal mode */
    }
}

/* OCT- held at power-on. The key matrix reads nothing until PA2 is low (the LCD backlight pin, set by
 * fm1_lcd_hw_init: a normal start does that before it reads the keys), so set it first. Then the matrix
 * gets 100 ms to settle, and OCT- alone, seen in any frame of the next 250 ms, counts. */
static int recovery_key(void)
{
    uint32_t start = fm1_ticks(), t;
    int seen = 0;
    fm1_lcd_hw_init();
    fm1_input_init();
    do {
        fm1_wdt_feed();
        fm1_input_scan();
        t = (uint32_t)(fm1_ticks() - start) / (1000u * FM1_TICKS_PER_US);
        if (t >= 100u && bootguard_manual(fm1_in.buttons))
            seen = 1;
    } while (t < 350u && !seen);
    return seen;
}

static void recovery_main(void)
{
    recovery_active = 1;
    bootguard.pending = 2;                   /* WDT/exception here falls back to ROM */
    recovery_last = recovery_usb_last = fm1_ticks();
    recovery_fraction = fm1_ms = 0;
    /* No persist_boot or OTA cleanup: no flash write just for entering rescue. */
#if FOMA_SMK37
    flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());
#else
    flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;
#endif
    lcd_init();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 64, 240, &FONT_S, APP_NAME " USB RESCUE", C_WHITE, 1);
    draw_text_box(0, 96, 240, &FONT_S, "CONNECT USB", C_WHITE, 1);
    draw_text_box(0, 124, 240, &FONT_S,
                  flash_ok ? "OPEN THE INSTALLER" : "UNKNOWN FLASH", C_WHITE, 1);
    draw_text_box(0, 164, 240, &FONT_S, "AUDIO OFF", C_WHITE, 1);
    lcd_sync();
    usb_start();
    for (;;) recovery_step();
}
