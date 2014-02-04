#include "pebble.h"

static Window* window;
static GBitmap *background_image_container;

static Layer *minute_display_layer;
static Layer *hour_display_layer;
static Layer *center_display_layer;
static Layer *second_display_layer;
static TextLayer *day_layer;
static TextLayer *date_layer;
static TextLayer *battery_layer;

static char day_text[] ="SAT";
static char date_text[] = "13";

#define GRECT_FULL_WINDOW GRect(0,0,144,168)

static Layer *background_layer;
static Layer *window_layer;

const GPathInfo MINUTE_HAND_PATH_POINTS = {
  4,
  (GPoint []) {
    {-3, 0},
    {3, 0},
    {3, -52},
    {-3, -52},
  }
};

const GPathInfo HOUR_HAND_PATH_POINTS = {
  4,
  (GPoint []) {
    {-3, 0},
    {3, 0},
    {3, -38},
    {-3, -38},
  }
};

static GPath *hour_hand_path;
static GPath *minute_hand_path;

static AppTimer *timer_handle;
#define COOKIE_MY_TIMER 1
static int my_cookie = COOKIE_MY_TIMER;
#define ANIM_IDLE 0
#define ANIM_START 1
#define ANIM_HOURS 2
#define ANIM_MINUTES 3
#define ANIM_SECONDS 4
#define ANIM_DONE 5
int init_anim = ANIM_DONE;
int32_t second_angle_anim = 0;
unsigned int minute_angle_anim = 0;
unsigned int hour_angle_anim = 0;

void handle_timer(void* vdata) {

        int *data = (int *) vdata;

        if (*data == my_cookie) {
                if (init_anim == ANIM_START) {
                        init_anim = ANIM_HOURS;
                        timer_handle = app_timer_register(50 /* milliseconds */,
                                        &handle_timer, &my_cookie);
                } else if (init_anim == ANIM_HOURS) {
                        layer_mark_dirty(hour_display_layer);
                        timer_handle = app_timer_register(50 /* milliseconds */,
                                        &handle_timer, &my_cookie);
                } else if (init_anim == ANIM_MINUTES) {
                        layer_mark_dirty(minute_display_layer);
                        timer_handle = app_timer_register(50 /* milliseconds */,
                                        &handle_timer, &my_cookie);
                } else if (init_anim == ANIM_SECONDS) {
                        layer_mark_dirty(second_display_layer);
                        timer_handle = app_timer_register(50 /* milliseconds */,
                                        &handle_timer, &my_cookie);
                }
        }

}

void second_display_layer_update_callback(Layer *me, GContext* ctx) {
        (void) me;

        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        int32_t second_angle = t->tm_sec * (0xffff / 60);
        int second_hand_length = 12;
        GPoint center = {72,139};
        GPoint second = GPoint(center.x, center.y - second_hand_length);

        if (init_anim < ANIM_SECONDS) {
                second = GPoint(center.x, center.y - 70);
        } else if (init_anim == ANIM_SECONDS) {
                second_angle_anim += 0xffff / 60;
                if (second_angle_anim >= second_angle) {
                        init_anim = ANIM_DONE;
                        second =
                                        GPoint(center.x + second_hand_length * sin_lookup(second_angle)/0xffff,
                                                        center.y + (-second_hand_length) * cos_lookup(second_angle)/0xffff);
                } else {
                        second =
                                        GPoint(center.x + second_hand_length * sin_lookup(second_angle_anim)/0xffff,
                                                        center.y + (-second_hand_length) * cos_lookup(second_angle_anim)/0xffff);
                }
        } else {
                second =
                                GPoint(center.x + second_hand_length * sin_lookup(second_angle)/0xffff,
                                                center.y + (-second_hand_length) * cos_lookup(second_angle)/0xffff);
        }

        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_draw_line(ctx, center, second);
	    graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, center, 1);
}

void center_display_layer_update_callback(Layer *me, GContext* ctx) {
        (void) me;

        GPoint center = grect_center_point(&GRECT_FULL_WINDOW);
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, center, 3); 
}

void minute_display_layer_update_callback(Layer *me, GContext* ctx) {
        (void) me;

        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        unsigned int angle = t->tm_min * 6 + t->tm_sec / 10;

        if (init_anim < ANIM_MINUTES) {
                angle = 0;
        } else if (init_anim == ANIM_MINUTES) {
                minute_angle_anim += 6;
                if (minute_angle_anim >= angle) {
                        init_anim = ANIM_SECONDS;
                } else {
                        angle = minute_angle_anim;
                }
        }

        gpath_rotate_to(minute_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_context_set_stroke_color(ctx, GColorWhite);

        gpath_draw_filled(ctx, minute_hand_path);
        gpath_draw_outline(ctx, minute_hand_path);
}

void hour_display_layer_update_callback(Layer *me, GContext* ctx) {
        (void) me;

        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        unsigned int angle = t->tm_hour * 30 + t->tm_min / 2;

        if (init_anim < ANIM_HOURS) {
                angle = 0;
        } else if (init_anim == ANIM_HOURS) {
                if (hour_angle_anim == 0 && t->tm_hour >= 12) {
                        hour_angle_anim = 360;
                }
                hour_angle_anim += 6;
                if (hour_angle_anim >= angle) {
                        init_anim = ANIM_MINUTES;
                } else {
                        angle = hour_angle_anim;
                }
        }

        gpath_rotate_to(hour_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_context_set_stroke_color(ctx, GColorWhite);

        gpath_draw_filled(ctx, hour_hand_path);
        gpath_draw_outline(ctx, hour_hand_path);
}

static void handle_battery(BatteryChargeState charge_state) {
        static char battery_text[] = "90";

	    if (charge_state.charge_percent == 100) {
                snprintf(battery_text, sizeof(battery_text), "FC");
        } else if (charge_state.is_charging) {
                snprintf(battery_text, sizeof(battery_text), "CH");
        } else {
                snprintf(battery_text, sizeof(battery_text), "%d", charge_state.charge_percent);
        }
        text_layer_set_text(battery_layer, battery_text);
}

void draw_day() {

        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        strftime(day_text, sizeof(day_text), "%a", t);

        text_layer_set_text(day_layer, day_text);
}


void draw_date() {

        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        strftime(date_text, sizeof(date_text), "%d", t);

        text_layer_set_text(date_layer, date_text);
}

void draw_background_callback(Layer *layer, GContext *ctx) {
        graphics_context_set_compositing_mode(ctx, GCompOpAssign);
        graphics_draw_bitmap_in_rect(ctx, background_image_container,
                        GRECT_FULL_WINDOW);
}

void init() {

        // Window
        window = window_create();
        window_stack_push(window, true /* Animated */);
        window_layer = window_get_root_layer(window);

        // Background image
        background_image_container = gbitmap_create_with_resource(
                        RESOURCE_ID_IMAGE_BACKGROUND);
        background_layer = layer_create(GRECT_FULL_WINDOW);
        layer_set_update_proc(background_layer, &draw_background_callback);
        layer_add_child(window_layer, background_layer);
	

	    battery_state_service_subscribe(&handle_battery);


	    // Day setup
        day_layer = text_layer_create(GRect(1, 77, 25, 15));
        text_layer_set_text_color(day_layer, GColorWhite);
        text_layer_set_text_alignment(day_layer, GTextAlignmentCenter);
        text_layer_set_background_color(day_layer, GColorClear);
	    text_layer_set_font(day_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_OPENSANS_REGULAR_12)));
        layer_add_child(window_layer, text_layer_get_layer(day_layer));

        draw_day();
	 
	
	    // Date setup
        date_layer = text_layer_create(GRect(121, 77, 20, 15));
        text_layer_set_text_color(date_layer, GColorWhite);
        text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
        text_layer_set_background_color(date_layer, GColorClear);
	    text_layer_set_font(date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_OPENSANS_REGULAR_12)));
        layer_add_child(window_layer, text_layer_get_layer(date_layer));

        draw_date();
	
	    // Battery setup
	    battery_layer = text_layer_create(GRect(62, 53, 20, 15));
	    text_layer_set_text_color(battery_layer, GColorWhite);
        text_layer_set_text_alignment(battery_layer, GTextAlignmentCenter);
        text_layer_set_background_color(battery_layer, GColorClear);
	    text_layer_set_font(battery_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_OPENSANS_REGULAR_12)));
        layer_add_child(window_layer, text_layer_get_layer(battery_layer));
		
	
        // Hands setup
        hour_display_layer = layer_create(GRECT_FULL_WINDOW);
        layer_set_update_proc(hour_display_layer,
                        &hour_display_layer_update_callback);
        layer_add_child(window_layer, hour_display_layer);

        hour_hand_path = gpath_create(&HOUR_HAND_PATH_POINTS);
        gpath_move_to(hour_hand_path, grect_center_point(&GRECT_FULL_WINDOW));

        minute_display_layer = layer_create(GRECT_FULL_WINDOW);
        layer_set_update_proc(minute_display_layer,
                        &minute_display_layer_update_callback);
        layer_add_child(window_layer, minute_display_layer);

        minute_hand_path = gpath_create(&MINUTE_HAND_PATH_POINTS);
        gpath_move_to(minute_hand_path, grect_center_point(&GRECT_FULL_WINDOW));

        center_display_layer = layer_create(GRECT_FULL_WINDOW);
        layer_set_update_proc(center_display_layer,
                        &center_display_layer_update_callback);
        layer_add_child(window_layer, center_display_layer);

        second_display_layer = layer_create(GRECT_FULL_WINDOW);
        layer_set_update_proc(second_display_layer,
                        &second_display_layer_update_callback);
        layer_add_child(window_layer, second_display_layer);

        BatteryChargeState charge = battery_state_service_peek();
        handle_battery(charge);
}

void deinit() {
        battery_state_service_unsubscribe();
        window_destroy(window);
        gbitmap_destroy(background_image_container);
        text_layer_destroy(day_layer);
        text_layer_destroy(date_layer);
	    text_layer_destroy(battery_layer);
        layer_destroy(minute_display_layer);
        layer_destroy(hour_display_layer);
        layer_destroy(center_display_layer);
        layer_destroy(second_display_layer);
        layer_destroy(background_layer);
        layer_destroy(window_layer);
        gpath_destroy(hour_hand_path);
        gpath_destroy(minute_hand_path);
}

void handle_tick(struct tm *tick_time, TimeUnits units_changed) {

        if (init_anim == ANIM_IDLE) {
                init_anim = ANIM_START;
                timer_handle = app_timer_register(50 /* milliseconds */, &handle_timer,
                                &my_cookie);
        } else if (init_anim == ANIM_DONE) {
                if (tick_time->tm_sec % 10 == 0) {
                        layer_mark_dirty(minute_display_layer);

                        if (tick_time->tm_sec == 0) {
                                if (tick_time->tm_min % 2 == 0) {
                                        layer_mark_dirty(hour_display_layer);
                                        if (tick_time->tm_min == 0 && tick_time->tm_hour == 0) {
                                                draw_date();
                                        }
                                }
                        }
                }

                layer_mark_dirty(second_display_layer);
        }
}

int main(void) {
	init();
	tick_timer_service_subscribe(SECOND_UNIT, &handle_tick);
	app_event_loop();
	deinit();
}
