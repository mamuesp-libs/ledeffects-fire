#include "mgos.h"
#include "led_master.h"

static void mgos_intern_set_pixel_heat_color(mgos_rgbleds* leds, int x, int y, uint8_t temperature)
{
    // Scale 'heat' down from 0-255 to 0-191
    uint8_t t192 = round((temperature / 255.0) * 191);

    // calculate ramp up from
    uint8_t heatramp = t192 & 0x3F; // 0..63
    heatramp <<= 2; // scale up to 0..252

    tools_rgb_data curr_pix;

    // figure out which third of the spectrum we're in:
    int hottest = 0x40; // 0x80
    int medium = 0x20; // 0x40
    if (t192 > hottest) { // hottest
        curr_pix.r = 255;
        curr_pix.g = 255;
        curr_pix.b = heatramp;
    } else if (t192 > medium) { // middle
        curr_pix.r = 255;
        curr_pix.g = heatramp;
        curr_pix.b = 0;
    } else { // coolest
        curr_pix.r = heatramp;
        curr_pix.g = 0;
        curr_pix.b = 0;
    }
    mgos_rgbleds_plot_pixel(leds, x, leds->panel_height - 1 - y, curr_pix, false);
}

static void mgos_intern_fire_init(mgos_rgbleds* leds)
{
    leds->timeout = mgos_sys_config_get_ledeffects_fire_timeout();
    leds->dim_all = mgos_sys_config_get_ledeffects_fire_dim_all();
}

static void mgos_intern_fire_loop(mgos_rgbleds* leds)
{
    uint32_t num_pixels = leds->panel_height;
    int cooling = mgos_sys_config_get_ledeffects_fire_cooling();
    int sparkling = mgos_sys_config_get_ledeffects_fire_sparkling();

    uint8_t* heat = leds->color_values;
    int cooldown;
    
    int run = leds->internal_loops;
    run = run <= 0 ? 1 : run;

    while (run--) {
        // Step 1.  Cool down every cell a little
        for (int i = 0; i < num_pixels; i++) {
            cooldown = tools_get_random(0, ((cooling * 10) / num_pixels) + 2);
            if (cooldown > heat[i]) {
                heat[i] = 0;
            } else {
                heat[i] = heat[i] - cooldown;
            }
        }

        // Step 2.  Heat from each cell drifts 'up' and diffuses a little
        for (int k = num_pixels - 1; k >= 2; k--) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }

        // Step 3.  Randomly ignite new 'sparks' near the bottom
        if (tools_get_random(0, 255) < sparkling) {
            int y = tools_get_random(0, 7);
            int new_heat = heat[y] + tools_get_random(160, 255);
            heat[y] = new_heat > 255 ? 255 : new_heat;
        }

            // Step 4.  Convert heat to LED colors
        for (int x_pos = 0; x_pos < leds->panel_width; x_pos++) {
            for (int tgRunner = 0; tgRunner < num_pixels; tgRunner++) {
                mgos_intern_set_pixel_heat_color(leds, x_pos, tgRunner, heat[tgRunner]);
            }
        }
        mgos_rgbleds_show(leds);
        mgos_wdt_feed();
    }
}

void mgos_ledeffects_fire(void* param, mgos_rgbleds_action action)
{
    static bool do_time = false;
    static uint32_t max_time = 0;
    uint32_t time = (mgos_uptime_micros() / 1000);
    mgos_rgbleds* leds = (mgos_rgbleds*)param;

    switch (action) {
    case MGOS_RGBLEDS_ACT_INIT:
        mgos_intern_fire_init(leds);
        break;
    case MGOS_RGBLEDS_ACT_EXIT:
        break;
    case MGOS_RGBLEDS_ACT_LOOP:
        mgos_intern_fire_loop(leds);
        if (do_time) {
            time = (mgos_uptime_micros() /1000) - time;
            max_time = (time > max_time) ? time : max_time;
            LOG(LL_VERBOSE_DEBUG, ("Fire loop duration: %d milliseconds, max: %d ...", time / 1000, max_time / 1000));
        }
        break;
    }
}


bool mgos_fire_init(void) {
  LOG(LL_INFO, ("mgos_fire_init ..."));
  ledmaster_add_effect("ANIM_FIRE", mgos_ledeffects_fire);
  return true;
}
