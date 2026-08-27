#include "main.h"
#include "APP_LCD_KeyPage.h"

#define LCD_IMAGE_BUFFER_LEN          4096U
#define LCD_DIGIT_DATA_OFFSET         8U
#define LCD_OVERLAY_DIGIT_GAP         0U
#define LCD_OVERLAY_DIGIT_Y_OFFSET    8U
#define LCD_BUSINESS_PORT_COUNT       8U
#define LCD_BUSINESS_PORT_OFF_VALUE   0xF0U
#define LCD_BUSINESS_PORT_VALUE_MAX   140U
#define LCD_BUSINESS_TOTAL_VALUE_MAX  590U
#define LCD_BUSINESS_TEMP_THRESHOLD   0x2DU
#define LCD_BUSINESS_INVALID_VALUE    0xFFFFU
#define LCD_BUSINESS_DIGIT_MAX        999U
#define LCD_BUSINESS_MINUTE_GAP       0U

#define LCD_BUSINESS_DRAW_P0_X  48U
#define LCD_BUSINESS_DRAW_P0_Y  23U
#define LCD_BUSINESS_DRAW_P1_X  164U
#define LCD_BUSINESS_DRAW_P1_Y  23U
#define LCD_BUSINESS_DRAW_P2_X  48U
#define LCD_BUSINESS_DRAW_P2_Y  83U
#define LCD_BUSINESS_DRAW_P3_X  164U
#define LCD_BUSINESS_DRAW_P3_Y  83U
#define LCD_BUSINESS_DRAW_P4_X  48U
#define LCD_BUSINESS_DRAW_P4_Y  143U
#define LCD_BUSINESS_DRAW_P5_X  164U
#define LCD_BUSINESS_DRAW_P5_Y  143U
#define LCD_BUSINESS_DRAW_P6_X  48U
#define LCD_BUSINESS_DRAW_P6_Y  203U
#define LCD_BUSINESS_DRAW_P7_X  164U
#define LCD_BUSINESS_DRAW_P7_Y  203U

#define LCD_BUSINESS_ACCUM_GAP  0U
#define LCD_BUSINESS_ACCUM_X    70U
#define LCD_BUSINESS_ACCUM_Y    60U

#define DIGIT_MINUTE_W          16U
#define DIGIT_MINUTE_H          28U
#define DIGIT_MINUTE_BYTES      (DIGIT_MINUTE_W * DIGIT_MINUTE_H * ST7789_PIXEL_BYTES)
#define DIGIT_ACCUMULATE_W      27U
#define DIGIT_ACCUMULATE_H      66U
#define DIGIT_ACCUMULATE_BYTES  (DIGIT_ACCUMULATE_W * DIGIT_ACCUMULATE_H * ST7789_PIXEL_BYTES)
#define DIGIT_TOTAL_W           46U
#define DIGIT_TOTAL_H           93U
#define DIGIT_TOTAL_BYTES       (DIGIT_TOTAL_W * DIGIT_TOTAL_H * ST7789_PIXEL_BYTES)

#define LCD_PAGE_BOOT_DELAY_MS    4000U
#define LCD_PAGE_POLL_DELAY_MS    5U
#define LCD_PAGE_KEY_DEBOUNCE_MS  150U
#define LCD_PAGE_KEY_STABLE_MS    50U
#define LCD_PAGE_KEY_GPIO_PORT    GPIOB
#define LCD_PAGE_KEY_PIN          GPIO_PIN_2
#define LCD_RESOURCE_LAST_ADDR    0x0071CFFFUL

#if (LCD_IMAGE_BUFFER_LEN != W25QXX_SECTOR_SIZE)
#error "LCD image buffer must match the W25Q sector size."
#endif

#if (LCD_RESOURCE_LAST_ADDR >= W25QXX_FLASH_SIZE_BYTES)
#error "LCD image resources exceed the W25Q flash size."
#endif

typedef struct {
    uint32_t start_addr;
    uint32_t display_len;
    uint16_t width;
    uint16_t height;
} LcdImageDescriptor;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} LcdOverlayRect;

typedef struct {
    uint8_t port_state[LCD_BUSINESS_PORT_COUNT];
    uint8_t port_value[LCD_BUSINESS_PORT_COUNT];
    uint8_t temp_value;
    uint8_t any_open;
    uint16_t max_open_value;
    uint32_t sequence;
} LcdBusinessSnapshot;

typedef struct {
    uint16_t x;
    uint16_t y;
} LcdDigitPosition;

typedef enum {
    LCD_PAGE_BOOT = 0,
    LCD_PAGE_SMILEY,
    LCD_PAGE_DRAWING,
    LCD_PAGE_TEMPERATURE,
    LCD_PAGE_CUMULATIVE_TIME,
    LCD_PAGE_BLACK
} LcdPageId;

#define LCD_FRAME(addr)       {addr, ST7789_FRAME_BYTES, ST7789_WIDTH, ST7789_HEIGHT}
#define LCD_DIGIT(addr, len, w, h) {addr, len, w, h}

static uint8_t lcd_image_buffer[LCD_IMAGE_BUFFER_LEN];
static LcdPageId lcd_page_current = LCD_PAGE_BOOT;
static uint8_t lcd_page_dirty = 1U;
static uint32_t lcd_page_smiley_index = 0U;
static GPIO_PinState lcd_page_key_last_state = GPIO_PIN_RESET;
static uint8_t lcd_page_key_armed = 1U;
static uint32_t lcd_page_key_debounce_deadline = 0U;
static uint32_t lcd_page_key_stable_start = 0U;
static LcdBusinessSnapshot lcd_business_snapshot;
static uint8_t lcd_business_timer_active = 0U;
static uint32_t lcd_business_timer_start_tick = 0U;
static uint16_t lcd_business_elapsed_minutes = 0U;
static uint8_t lcd_page_charging_active = 0U;
static uint32_t lcd_page_charge_image_index = 0U;
static uint16_t lcd_page_charge_value = LCD_BUSINESS_INVALID_VALUE;
static LcdOverlayRect lcd_page_charge_digit_rect;
static uint8_t lcd_page_charge_digit_valid = 0U;
static uint16_t lcd_page_rendered_minutes = LCD_BUSINESS_INVALID_VALUE;
static uint8_t lcd_page_rendered_temp_index = 0xFFU;
static uint32_t lcd_page_rendered_drawing_seq = 0UL;
static uint16_t lcd_page_rendered_drawing_value[LCD_BUSINESS_PORT_COUNT];
static LcdOverlayRect lcd_page_drawing_digit_rect[LCD_BUSINESS_PORT_COUNT];
static uint8_t lcd_page_drawing_digit_valid[LCD_BUSINESS_PORT_COUNT];
static LcdOverlayRect lcd_page_accum_digit_rect;
static uint8_t lcd_page_accum_digit_valid = 0U;

static const LcdImageDescriptor lcd_charge_table[] = {
    LCD_FRAME(0x00002000UL), LCD_FRAME(0x0001F000UL), LCD_FRAME(0x0003C000UL),
    LCD_FRAME(0x00059000UL), LCD_FRAME(0x00076000UL), LCD_FRAME(0x00093000UL),
    LCD_FRAME(0x000B0000UL), LCD_FRAME(0x000CD000UL), LCD_FRAME(0x000EA000UL),
    LCD_FRAME(0x00107000UL), LCD_FRAME(0x00124000UL), LCD_FRAME(0x00141000UL),
    LCD_FRAME(0x0015E000UL), LCD_FRAME(0x0017B000UL), LCD_FRAME(0x00198000UL),
    LCD_FRAME(0x001B5000UL), LCD_FRAME(0x001D2000UL), LCD_FRAME(0x001EF000UL),
    LCD_FRAME(0x0020C000UL), LCD_FRAME(0x00229000UL), LCD_FRAME(0x00246000UL),
    LCD_FRAME(0x00263000UL), LCD_FRAME(0x00280000UL), LCD_FRAME(0x0029D000UL),
    LCD_FRAME(0x002BA000UL), LCD_FRAME(0x002D7000UL), LCD_FRAME(0x002F4000UL),
    LCD_FRAME(0x00311000UL), LCD_FRAME(0x0032E000UL)
};

static const LcdImageDescriptor lcd_palette = LCD_FRAME(0x0034B000UL);
static const LcdImageDescriptor lcd_boot = LCD_FRAME(0x00368000UL);
static const LcdImageDescriptor lcd_total_time = LCD_FRAME(0x00385000UL);
static const LcdImageDescriptor lcd_temperature_table[] = {
    LCD_FRAME(0x003A2000UL), LCD_FRAME(0x003BF000UL), LCD_FRAME(0x003DC000UL)
};

static const LcdImageDescriptor lcd_smiley_table[] = {
    LCD_FRAME(0x003F9000UL), LCD_FRAME(0x00416000UL), LCD_FRAME(0x00433000UL),
    LCD_FRAME(0x0048A000UL), LCD_FRAME(0x004A7000UL), LCD_FRAME(0x004E1000UL),
    LCD_FRAME(0x004FE000UL), LCD_FRAME(0x0051B000UL), LCD_FRAME(0x00538000UL),
    LCD_FRAME(0x00555000UL), LCD_FRAME(0x00572000UL), LCD_FRAME(0x0058F000UL),
    LCD_FRAME(0x005AC000UL), LCD_FRAME(0x005C9000UL), LCD_FRAME(0x005E6000UL),
    LCD_FRAME(0x00603000UL), LCD_FRAME(0x00620000UL), LCD_FRAME(0x0063D000UL),
    LCD_FRAME(0x0065A000UL), LCD_FRAME(0x00677000UL), LCD_FRAME(0x00694000UL),
    LCD_FRAME(0x006B1000UL), LCD_FRAME(0x006B1000UL), LCD_FRAME(0x00694000UL),
    LCD_FRAME(0x00677000UL), LCD_FRAME(0x0065A000UL), LCD_FRAME(0x0063D000UL),
    LCD_FRAME(0x00620000UL), LCD_FRAME(0x00603000UL), LCD_FRAME(0x005E6000UL),
    LCD_FRAME(0x005C9000UL), LCD_FRAME(0x005AC000UL), LCD_FRAME(0x0058F000UL),
    LCD_FRAME(0x00572000UL), LCD_FRAME(0x00555000UL), LCD_FRAME(0x00538000UL),
    LCD_FRAME(0x0051B000UL), LCD_FRAME(0x004FE000UL), LCD_FRAME(0x004E1000UL),
    LCD_FRAME(0x004A7000UL), LCD_FRAME(0x0048A000UL), LCD_FRAME(0x00433000UL),
    LCD_FRAME(0x00416000UL)
};

static const LcdImageDescriptor lcd_digit_minute_table[] = {
    LCD_DIGIT(0x006EB000UL, DIGIT_MINUTE_BYTES, DIGIT_MINUTE_W, DIGIT_MINUTE_H),
    LCD_DIGIT(0x006EC000UL, DIGIT_MINUTE_BYTES, DIGIT_MINUTE_W, DIGIT_MINUTE_H),
    LCD_DIGIT(0x006ED000UL, DIGIT_MINUTE_BYTES, DIGIT_MINUTE_W, DIGIT_MINUTE_H),
    LCD_DIGIT(0x006EE000UL, DIGIT_MINUTE_BYTES, DIGIT_MINUTE_W, DIGIT_MINUTE_H),
    LCD_DIGIT(0x006EF000UL, DIGIT_MINUTE_BYTES, DIGIT_MINUTE_W, DIGIT_MINUTE_H),
    LCD_DIGIT(0x006F0000UL, DIGIT_MINUTE_BYTES, DIGIT_MINUTE_W, DIGIT_MINUTE_H),
    LCD_DIGIT(0x006F1000UL, DIGIT_MINUTE_BYTES, DIGIT_MINUTE_W, DIGIT_MINUTE_H),
    LCD_DIGIT(0x006F2000UL, DIGIT_MINUTE_BYTES, DIGIT_MINUTE_W, DIGIT_MINUTE_H),
    LCD_DIGIT(0x006F3000UL, DIGIT_MINUTE_BYTES, DIGIT_MINUTE_W, DIGIT_MINUTE_H),
    LCD_DIGIT(0x006F4000UL, DIGIT_MINUTE_BYTES, DIGIT_MINUTE_W, DIGIT_MINUTE_H)
};

static const LcdImageDescriptor lcd_digit_accumulate_table[] = {
    LCD_DIGIT(0x006F5000UL, DIGIT_ACCUMULATE_BYTES, DIGIT_ACCUMULATE_W, DIGIT_ACCUMULATE_H),
    LCD_DIGIT(0x006F6000UL, DIGIT_ACCUMULATE_BYTES, DIGIT_ACCUMULATE_W, DIGIT_ACCUMULATE_H),
    LCD_DIGIT(0x006F7000UL, DIGIT_ACCUMULATE_BYTES, DIGIT_ACCUMULATE_W, DIGIT_ACCUMULATE_H),
    LCD_DIGIT(0x006F8000UL, DIGIT_ACCUMULATE_BYTES, DIGIT_ACCUMULATE_W, DIGIT_ACCUMULATE_H),
    LCD_DIGIT(0x006F9000UL, DIGIT_ACCUMULATE_BYTES, DIGIT_ACCUMULATE_W, DIGIT_ACCUMULATE_H),
    LCD_DIGIT(0x006FA000UL, DIGIT_ACCUMULATE_BYTES, DIGIT_ACCUMULATE_W, DIGIT_ACCUMULATE_H),
    LCD_DIGIT(0x006FB000UL, DIGIT_ACCUMULATE_BYTES, DIGIT_ACCUMULATE_W, DIGIT_ACCUMULATE_H),
    LCD_DIGIT(0x006FC000UL, DIGIT_ACCUMULATE_BYTES, DIGIT_ACCUMULATE_W, DIGIT_ACCUMULATE_H),
    LCD_DIGIT(0x006FD000UL, DIGIT_ACCUMULATE_BYTES, DIGIT_ACCUMULATE_W, DIGIT_ACCUMULATE_H),
    LCD_DIGIT(0x006FE000UL, DIGIT_ACCUMULATE_BYTES, DIGIT_ACCUMULATE_W, DIGIT_ACCUMULATE_H)
};

static const LcdImageDescriptor lcd_digit_total_table[] = {
    LCD_DIGIT(0x006FF000UL, DIGIT_TOTAL_BYTES, DIGIT_TOTAL_W, DIGIT_TOTAL_H),
    LCD_DIGIT(0x00702000UL, DIGIT_TOTAL_BYTES, DIGIT_TOTAL_W, DIGIT_TOTAL_H),
    LCD_DIGIT(0x00705000UL, DIGIT_TOTAL_BYTES, DIGIT_TOTAL_W, DIGIT_TOTAL_H),
    LCD_DIGIT(0x00708000UL, DIGIT_TOTAL_BYTES, DIGIT_TOTAL_W, DIGIT_TOTAL_H),
    LCD_DIGIT(0x0070B000UL, DIGIT_TOTAL_BYTES, DIGIT_TOTAL_W, DIGIT_TOTAL_H),
    LCD_DIGIT(0x0070E000UL, DIGIT_TOTAL_BYTES, DIGIT_TOTAL_W, DIGIT_TOTAL_H),
    LCD_DIGIT(0x00711000UL, DIGIT_TOTAL_BYTES, DIGIT_TOTAL_W, DIGIT_TOTAL_H),
    LCD_DIGIT(0x00714000UL, DIGIT_TOTAL_BYTES, DIGIT_TOTAL_W, DIGIT_TOTAL_H),
    LCD_DIGIT(0x00717000UL, DIGIT_TOTAL_BYTES, DIGIT_TOTAL_W, DIGIT_TOTAL_H),
    LCD_DIGIT(0x0071A000UL, DIGIT_TOTAL_BYTES, DIGIT_TOTAL_W, DIGIT_TOTAL_H)
};
static const LcdDigitPosition lcd_drawing_pos[LCD_BUSINESS_PORT_COUNT] = {
    {LCD_BUSINESS_DRAW_P0_X, LCD_BUSINESS_DRAW_P0_Y},
    {LCD_BUSINESS_DRAW_P1_X, LCD_BUSINESS_DRAW_P1_Y},
    {LCD_BUSINESS_DRAW_P2_X, LCD_BUSINESS_DRAW_P2_Y},
    {LCD_BUSINESS_DRAW_P3_X, LCD_BUSINESS_DRAW_P3_Y},
    {LCD_BUSINESS_DRAW_P4_X, LCD_BUSINESS_DRAW_P4_Y},
    {LCD_BUSINESS_DRAW_P5_X, LCD_BUSINESS_DRAW_P5_Y},
    {LCD_BUSINESS_DRAW_P6_X, LCD_BUSINESS_DRAW_P6_Y},
    {LCD_BUSINESS_DRAW_P7_X, LCD_BUSINESS_DRAW_P7_Y}
};

#define LCD_CHARGE_COUNT ((uint32_t)(sizeof(lcd_charge_table) / sizeof(lcd_charge_table[0])))
#define LCD_SMILEY_COUNT ((uint32_t)(sizeof(lcd_smiley_table) / sizeof(lcd_smiley_table[0])))
#define LCD_TEMPERATURE_COUNT ((uint8_t)(sizeof(lcd_temperature_table) / sizeof(lcd_temperature_table[0])))
#define LCD_DIGIT_MINUTE_COUNT ((uint8_t)(sizeof(lcd_digit_minute_table) / sizeof(lcd_digit_minute_table[0])))
#define LCD_DIGIT_ACCUMULATE_COUNT ((uint8_t)(sizeof(lcd_digit_accumulate_table) / sizeof(lcd_digit_accumulate_table[0])))

static uint16_t LcdMinU16(uint32_t value, uint16_t limit)
{
    return (value > limit) ? limit : (uint16_t)value;
}

static void LcdDisplayChunk(const uint8_t *buffer, uint16_t length)
{
    if (length != 0U) {
        ST7789_WriteDataBlock(buffer, length);
    }
}

static void LcdStreamFlash(uint32_t read_addr, uint32_t remaining)
{
    while (remaining != 0UL) {
        uint16_t chunk = LcdMinU16(remaining, LCD_IMAGE_BUFFER_LEN);
        W25QXX_Read(lcd_image_buffer, read_addr, chunk);
        LcdDisplayChunk(lcd_image_buffer, chunk);
        read_addr += chunk;
        remaining -= chunk;
    }
}

static void LcdDisplayImage(const LcdImageDescriptor *image)
{
    if (image == 0) {
        return;
    }
    ST7789_SetFullWindow();
    ST7789_BeginWrite();
    LcdStreamFlash(image->start_addr, image->display_len);
    ST7789_EndWrite();
}

static void LcdDisplayDigit(const LcdImageDescriptor *image, uint16_t x, uint16_t y)
{
    uint32_t pixel_bytes;
    if ((image == 0) || (image->width == 0U) || (image->height == 0U)) {
        return;
    }
    if ((((uint32_t)x + image->width) > ST7789_WIDTH) ||
        (((uint32_t)y + image->height) > ST7789_HEIGHT)) {
        return;
    }
    pixel_bytes = (uint32_t)image->width * image->height * ST7789_PIXEL_BYTES;
    ST7789_Address_Set(x, y,
                       (uint16_t)(x + image->width - 1U),
                       (uint16_t)(y + image->height - 1U));
    ST7789_BeginWrite();
    LcdStreamFlash(image->start_addr + LCD_DIGIT_DATA_OFFSET, pixel_bytes);
    ST7789_EndWrite();
}

static uint8_t LcdRectValid(const LcdOverlayRect *rect)
{
    return ((rect != 0) && (rect->w != 0U) && (rect->h != 0U)) ? 1U : 0U;
}

static void LcdClearRect(const LcdOverlayRect *rect)
{
    if (LcdRectValid(rect) != 0U) {
        ST7789_FillRect(rect->x, rect->y,
                        (uint16_t)(rect->x + rect->w - 1U),
                        (uint16_t)(rect->y + rect->h - 1U), BLACK);
    }
}

static uint8_t LcdSplitDigits(uint16_t value, uint16_t max_value, uint8_t *digits)
{
    if (digits == 0) {
        return 0U;
    }
    if (value > max_value) {
        value = max_value;
    }
    if (value > LCD_BUSINESS_DIGIT_MAX) {
        value = LCD_BUSINESS_DIGIT_MAX;
    }
    if (value >= 100U) {
        digits[0] = (uint8_t)(value / 100U);
        digits[1] = (uint8_t)((value / 10U) % 10U);
        digits[2] = (uint8_t)(value % 10U);
        return 3U;
    }
    if (value >= 10U) {
        digits[0] = (uint8_t)(value / 10U);
        digits[1] = (uint8_t)(value % 10U);
        return 2U;
    }
    digits[0] = (uint8_t)value;
    return 1U;
}

static LcdOverlayRect LcdCalcValueRect(uint16_t value, uint16_t digit_w,
                                       uint16_t digit_h, uint16_t gap,
                                       uint16_t x, uint16_t y,
                                       uint16_t max_value)
{
    uint8_t digits[3];
    uint8_t count = LcdSplitDigits(value, max_value, digits);
    LcdOverlayRect rect = {0U, 0U, 0U, 0U};
    if ((count == 0U) || (((uint32_t)y + digit_h) > ST7789_HEIGHT)) {
        return rect;
    }
    rect.x = x;
    rect.y = y;
    rect.w = (uint16_t)((count * digit_w) + ((count - 1U) * gap));
    rect.h = digit_h;
    if (((uint32_t)rect.x + rect.w) > ST7789_WIDTH) {
        rect.w = 0U;
        rect.h = 0U;
    }
    return rect;
}

static void LcdDrawValue(uint16_t value, const LcdImageDescriptor *table,
                         uint8_t table_count, uint16_t digit_w,
                         uint16_t digit_h, uint16_t gap, uint16_t x,
                         uint16_t y, uint16_t max_value)
{
    uint8_t digits[3];
    uint8_t i;
    uint8_t count;
    if ((table == 0) || (table_count < 10U) ||
        (((uint32_t)y + digit_h) > ST7789_HEIGHT)) {
        return;
    }
    count = LcdSplitDigits(value, max_value, digits);
    for (i = 0U; i < count; i++) {
        uint16_t digit_x = (uint16_t)(x + (i * (digit_w + gap)));
        if (((uint32_t)digit_x + digit_w) <= ST7789_WIDTH) {
            LcdDisplayDigit(&table[digits[i]], digit_x, y);
        }
    }
}

static LcdOverlayRect LcdDrawValueAt(uint16_t value,
                                     const LcdImageDescriptor *table,
                                     uint8_t table_count, uint16_t digit_w,
                                     uint16_t digit_h, uint16_t gap,
                                     uint16_t x, uint16_t y,
                                     uint16_t max_value)
{
    LcdOverlayRect rect = LcdCalcValueRect(value, digit_w, digit_h, gap,
                                           x, y, max_value);
    if (LcdRectValid(&rect) != 0U) {
        LcdDrawValue(value, table, table_count, digit_w, digit_h, gap,
                     x, y, max_value);
    }
    return rect;
}

static LcdOverlayRect LcdDrawValueCentered(uint16_t value,
                                           const LcdImageDescriptor *table,
                                           uint8_t table_count,
                                           uint16_t digit_w,
                                           uint16_t digit_h, uint16_t gap,
                                           uint16_t center_x, uint16_t y,
                                           uint16_t max_value)
{
    uint8_t digits[3];
    uint8_t count = LcdSplitDigits(value, max_value, digits);
    uint16_t total_w = (uint16_t)((count * digit_w) + ((count - 1U) * gap));
    uint16_t x = (center_x > (total_w / 2U)) ?
                 (uint16_t)(center_x - (total_w / 2U)) : 0U;
    return LcdDrawValueAt(value, table, table_count, digit_w, digit_h,
                          gap, x, y, max_value);
}

static LcdOverlayRect LcdDrawChargeValue(uint16_t value)
{
    uint8_t digits[3];
    uint8_t i;
    uint8_t count = LcdSplitDigits(value, LCD_BUSINESS_TOTAL_VALUE_MAX, digits);
    uint16_t total_w;
    LcdOverlayRect rect = {0U, 0U, 0U, 0U};
    if ((count == 0U) || (count > 3U)) {
        return rect;
    }
    total_w = (uint16_t)((count * DIGIT_TOTAL_W) +
                         ((count - 1U) * LCD_OVERLAY_DIGIT_GAP));
    rect.x = (uint16_t)((ST7789_WIDTH - total_w) / 2U);
    rect.y = (uint16_t)(((ST7789_HEIGHT - DIGIT_TOTAL_H) / 2U) -
                        LCD_OVERLAY_DIGIT_Y_OFFSET);
    rect.w = total_w;
    rect.h = DIGIT_TOTAL_H;
    for (i = 0U; i < count; i++) {
        uint16_t x = (uint16_t)(rect.x +
                     (i * (DIGIT_TOTAL_W + LCD_OVERLAY_DIGIT_GAP)));
        LcdDisplayDigit(&lcd_digit_total_table[digits[i]], x, rect.y);
    }
    return rect;
}

static void LcdDisplayFrameRect(const LcdImageDescriptor *image,
                                uint16_t x, uint16_t y,
                                uint16_t width, uint16_t height)
{
    uint16_t row;
    uint16_t row_bytes;
    if ((image == 0) || (width == 0U) || (height == 0U) ||
        (((uint32_t)x + width) > image->width) ||
        (((uint32_t)y + height) > image->height)) {
        return;
    }
    row_bytes = (uint16_t)(width * ST7789_PIXEL_BYTES);
    ST7789_Address_Set(x, y, (uint16_t)(x + width - 1U),
                       (uint16_t)(y + height - 1U));
    ST7789_BeginWrite();
    if ((x == 0U) && (width == image->width)) {
        uint32_t offset = (uint32_t)y * image->width * ST7789_PIXEL_BYTES;
        uint32_t length = (uint32_t)height * image->width * ST7789_PIXEL_BYTES;
        LcdStreamFlash(image->start_addr + offset, length);
    } else {
        for (row = 0U; row < height; row++) {
            uint32_t offset = (((uint32_t)(y + row) * image->width) + x) *
                              ST7789_PIXEL_BYTES;
            W25QXX_Read(lcd_image_buffer, image->start_addr + offset, row_bytes);
            LcdDisplayChunk(lcd_image_buffer, row_bytes);
        }
    }
    ST7789_EndWrite();
}

static void LcdDisplayFrameKeepBand(const LcdImageDescriptor *image,
                                    const LcdOverlayRect *keep)
{
    uint16_t keep_bottom;
    if (image == 0) {
        return;
    }
    if (LcdRectValid(keep) == 0U) {
        LcdDisplayImage(image);
        return;
    }
    keep_bottom = (uint16_t)(keep->y + keep->h);
    LcdDisplayFrameRect(image, 0U, 0U, image->width, keep->y);
    if (keep_bottom < image->height) {
        LcdDisplayFrameRect(image, 0U, keep_bottom, image->width,
                            (uint16_t)(image->height - keep_bottom));
    }
}

static void LcdResetSnapshot(LcdBusinessSnapshot *snapshot)
{
    uint8_t port;
    if (snapshot == 0) {
        return;
    }
    for (port = 0U; port < LCD_BUSINESS_PORT_COUNT; port++) {
        snapshot->port_state[port] = LCD_BUSINESS_PORT_OFF_VALUE;
        snapshot->port_value[port] = 0U;
    }
    snapshot->temp_value = 0U;
    snapshot->any_open = 0U;
    snapshot->max_open_value = 0U;
    snapshot->sequence = 0UL;
}

static void LcdResetDigitCache(void)
{
    uint8_t port;
    for (port = 0U; port < LCD_BUSINESS_PORT_COUNT; port++) {
        lcd_page_rendered_drawing_value[port] = LCD_BUSINESS_INVALID_VALUE;
        lcd_page_drawing_digit_rect[port].x = 0U;
        lcd_page_drawing_digit_rect[port].y = 0U;
        lcd_page_drawing_digit_rect[port].w = 0U;
        lcd_page_drawing_digit_rect[port].h = 0U;
        lcd_page_drawing_digit_valid[port] = 0U;
    }
    lcd_page_rendered_minutes = LCD_BUSINESS_INVALID_VALUE;
    lcd_page_accum_digit_rect.x = 0U;
    lcd_page_accum_digit_rect.y = 0U;
    lcd_page_accum_digit_rect.w = 0U;
    lcd_page_accum_digit_rect.h = 0U;
    lcd_page_accum_digit_valid = 0U;
}

static void LcdUpdateDrawingDigits(uint8_t force)
{
    uint8_t port;
    for (port = 0U; port < LCD_BUSINESS_PORT_COUNT; port++) {
        uint16_t value = lcd_business_snapshot.port_value[port];
        if ((force != 0U) ||
            (value != lcd_page_rendered_drawing_value[port])) {
            if ((force == 0U) &&
                (lcd_page_drawing_digit_valid[port] != 0U)) {
                LcdClearRect(&lcd_page_drawing_digit_rect[port]);
            }
            lcd_page_drawing_digit_rect[port] =
                LcdDrawValueCentered(value, lcd_digit_minute_table,
                                     LCD_DIGIT_MINUTE_COUNT,
                                     DIGIT_MINUTE_W, DIGIT_MINUTE_H,
                                     LCD_BUSINESS_MINUTE_GAP,
                                     lcd_drawing_pos[port].x,
                                     lcd_drawing_pos[port].y,
                                     LCD_BUSINESS_PORT_VALUE_MAX);
            lcd_page_drawing_digit_valid[port] =
                LcdRectValid(&lcd_page_drawing_digit_rect[port]);
            lcd_page_rendered_drawing_value[port] = value;
        }
    }
    lcd_page_rendered_drawing_seq = lcd_business_snapshot.sequence;
}

static void LcdUpdateAccumDigit(uint8_t force)
{
    if ((force != 0U) ||
        (lcd_business_elapsed_minutes != lcd_page_rendered_minutes)) {
        if ((force == 0U) && (lcd_page_accum_digit_valid != 0U)) {
            LcdClearRect(&lcd_page_accum_digit_rect);
        }
        lcd_page_accum_digit_rect =
            LcdDrawValueCentered(lcd_business_elapsed_minutes,
                                 lcd_digit_accumulate_table,
                                 LCD_DIGIT_ACCUMULATE_COUNT,
                                 DIGIT_ACCUMULATE_W, DIGIT_ACCUMULATE_H,
                                 LCD_BUSINESS_ACCUM_GAP,
                                 LCD_BUSINESS_ACCUM_X,
                                 LCD_BUSINESS_ACCUM_Y,
                                 LCD_BUSINESS_DIGIT_MAX);
        lcd_page_accum_digit_valid = LcdRectValid(&lcd_page_accum_digit_rect);
        lcd_page_rendered_minutes = lcd_business_elapsed_minutes;
    }
}

static void LcdResetCharging(void)
{
    lcd_page_charging_active = 0U;
    lcd_page_charge_image_index = 0U;
    lcd_page_charge_value = LCD_BUSINESS_INVALID_VALUE;
    lcd_page_charge_digit_rect.x = 0U;
    lcd_page_charge_digit_rect.y = 0U;
    lcd_page_charge_digit_rect.w = 0U;
    lcd_page_charge_digit_rect.h = 0U;
    lcd_page_charge_digit_valid = 0U;
}

static void LcdRenderCharging(uint16_t value)
{
    if (lcd_page_charging_active == 0U) {
        LcdDisplayImage(&lcd_charge_table[lcd_page_charge_image_index]);
        lcd_page_charge_image_index++;
        lcd_page_charge_value = value;
        lcd_page_charge_digit_rect = LcdDrawChargeValue(value);
        lcd_page_charge_digit_valid = LcdRectValid(&lcd_page_charge_digit_rect);
        lcd_page_charging_active = 1U;
    } else {
        if (value != lcd_page_charge_value) {
            if (lcd_page_charge_digit_valid != 0U) {
                LcdClearRect(&lcd_page_charge_digit_rect);
            }
            lcd_page_charge_value = value;
            lcd_page_charge_digit_rect = LcdDrawChargeValue(value);
            lcd_page_charge_digit_valid = LcdRectValid(&lcd_page_charge_digit_rect);
        }
        LcdDisplayFrameKeepBand(&lcd_charge_table[lcd_page_charge_image_index],
            (lcd_page_charge_digit_valid != 0U) ?
            &lcd_page_charge_digit_rect : 0);
        lcd_page_charge_image_index++;
    }
    if (lcd_page_charge_image_index >= LCD_CHARGE_COUNT) {
        lcd_page_charge_image_index = 0U;
    }
    ST7789_BL_ON();
}

static void LcdUpdateTimer(uint8_t any_open)
{
    if (any_open != 0U) {
        if (lcd_business_timer_active == 0U) {
            lcd_business_timer_active = 1U;
            lcd_business_timer_start_tick = HAL_GetTick();
            lcd_business_elapsed_minutes = 0U;
        } else {
            uint32_t elapsed = HAL_GetTick() - lcd_business_timer_start_tick;
            lcd_business_elapsed_minutes = (uint16_t)(elapsed / 60000U);
            if (lcd_business_elapsed_minutes > LCD_BUSINESS_DIGIT_MAX) {
                lcd_business_elapsed_minutes = LCD_BUSINESS_DIGIT_MAX;
            }
        }
    } else {
        lcd_business_timer_active = 0U;
        lcd_business_timer_start_tick = 0U;
        lcd_business_elapsed_minutes = 0U;
    }
}

static uint8_t LcdTemperatureIndex(uint8_t temperature)
{
    if (temperature < LCD_BUSINESS_TEMP_THRESHOLD) {
        return 0U;
    }
    if (temperature == LCD_BUSINESS_TEMP_THRESHOLD) {
        return 1U;
    }
    return 2U;
}

static void LcdKeyInit(void)
{
    GPIO_InitTypeDef init = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    init.Pin = LCD_PAGE_KEY_PIN;
    init.Mode = GPIO_MODE_INPUT;
    init.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(LCD_PAGE_KEY_GPIO_PORT, &init);
    lcd_page_key_last_state =
        HAL_GPIO_ReadPin(LCD_PAGE_KEY_GPIO_PORT, LCD_PAGE_KEY_PIN);
    lcd_page_key_stable_start = HAL_GetTick();
    lcd_page_key_debounce_deadline = HAL_GetTick();
    lcd_page_key_armed = (lcd_page_key_last_state == GPIO_PIN_RESET) ? 1U : 0U;
}

static uint8_t LcdKeyPoll(void)
{
    GPIO_PinState state =
        HAL_GPIO_ReadPin(LCD_PAGE_KEY_GPIO_PORT, LCD_PAGE_KEY_PIN);
    uint32_t now = HAL_GetTick();
    if (state != lcd_page_key_last_state) {
        lcd_page_key_last_state = state;
        lcd_page_key_stable_start = now;
        return 0U;
    }
    if ((now - lcd_page_key_stable_start) < LCD_PAGE_KEY_STABLE_MS) {
        return 0U;
    }
    if (state == GPIO_PIN_RESET) {
        lcd_page_key_armed = 1U;
        return 0U;
    }
    if ((lcd_page_key_armed != 0U) &&
        ((int32_t)(now - lcd_page_key_debounce_deadline) >= 0)) {
        lcd_page_key_armed = 0U;
        lcd_page_key_debounce_deadline = now + LCD_PAGE_KEY_DEBOUNCE_MS;
        return 1U;
    }
    return 0U;
}

static void LcdPageAdvance(void)
{
    switch (lcd_page_current) {
    case LCD_PAGE_SMILEY:
        lcd_page_current = LCD_PAGE_DRAWING;
        break;
    case LCD_PAGE_DRAWING:
        lcd_page_current = LCD_PAGE_TEMPERATURE;
        break;
    case LCD_PAGE_TEMPERATURE:
        lcd_page_current = LCD_PAGE_CUMULATIVE_TIME;
        break;
    case LCD_PAGE_CUMULATIVE_TIME:
        lcd_page_current = LCD_PAGE_BLACK;
        break;
    case LCD_PAGE_BLACK:
    default:
        lcd_page_current = LCD_PAGE_SMILEY;
        break;
    }
    lcd_page_dirty = 1U;
    LcdResetCharging();
    LcdResetDigitCache();
    lcd_page_rendered_temp_index = 0xFFU;
}

static void LcdRenderCurrentPage(void)
{
    switch (lcd_page_current) {
    case LCD_PAGE_BOOT:
        LcdDisplayImage(&lcd_boot);
        ST7789_BL_ON();
        break;
    case LCD_PAGE_SMILEY:
        if (lcd_business_snapshot.any_open != 0U) {
            LcdRenderCharging(lcd_business_snapshot.max_open_value);
        } else {
            LcdResetCharging();
            LcdDisplayImage(&lcd_smiley_table[lcd_page_smiley_index]);
            lcd_page_smiley_index++;
            if (lcd_page_smiley_index >= LCD_SMILEY_COUNT) {
                lcd_page_smiley_index = 0U;
            }
            ST7789_BL_ON();
        }
        break;
    case LCD_PAGE_DRAWING:
        LcdDisplayImage(&lcd_palette);
        LcdResetDigitCache();
        LcdUpdateDrawingDigits(1U);
        ST7789_BL_ON();
        break;
    case LCD_PAGE_TEMPERATURE:
    {
        uint8_t index = LcdTemperatureIndex(lcd_business_snapshot.temp_value);
        if (index >= LCD_TEMPERATURE_COUNT) {
            index = 0U;
        }
        LcdDisplayImage(&lcd_temperature_table[index]);
        lcd_page_rendered_temp_index = index;
        ST7789_BL_ON();
        break;
    }
    case LCD_PAGE_CUMULATIVE_TIME:
        LcdDisplayImage(&lcd_total_time);
        LcdResetDigitCache();
        LcdUpdateAccumDigit(1U);
        ST7789_BL_ON();
        break;
    case LCD_PAGE_BLACK:
    default:
        LcdResetCharging();
        ST7789_Clear(BLACK);
        ST7789_BL_OFF();
        break;
    }
}

void APP_LCD_KeyPage_Run(void)
{
    LcdResetSnapshot(&lcd_business_snapshot);
    LcdKeyInit();
    LcdResetCharging();
    LcdResetDigitCache();
    lcd_business_timer_active = 0U;
    lcd_business_timer_start_tick = 0U;
    lcd_business_elapsed_minutes = 0U;
    lcd_page_rendered_temp_index = 0xFFU;
    lcd_page_rendered_drawing_seq = 0UL;
    lcd_page_current = LCD_PAGE_BOOT;
    lcd_page_dirty = 1U;
    lcd_page_smiley_index = 0U;

    LcdRenderCurrentPage();
    lcd_page_dirty = 0U;
    HAL_Delay(LCD_PAGE_BOOT_DELAY_MS);
    lcd_page_current = LCD_PAGE_SMILEY;
    lcd_page_dirty = 1U;

    while (1) {
        uint16_t old_minutes = lcd_business_elapsed_minutes;
        LcdUpdateTimer(lcd_business_snapshot.any_open);
        if ((lcd_page_current == LCD_PAGE_CUMULATIVE_TIME) &&
            (old_minutes != lcd_business_elapsed_minutes)) {
            LcdUpdateAccumDigit(0U);
        }
        if (LcdKeyPoll() != 0U) {
            LcdPageAdvance();
        }
        if (lcd_page_current == LCD_PAGE_SMILEY) {
            LcdRenderCurrentPage();
            continue;
        }
        if (lcd_page_current == LCD_PAGE_TEMPERATURE) {
            uint8_t index = LcdTemperatureIndex(lcd_business_snapshot.temp_value);
            if (index != lcd_page_rendered_temp_index) {
                lcd_page_dirty = 1U;
            }
        }
        if ((lcd_page_current == LCD_PAGE_DRAWING) &&
            (lcd_page_rendered_drawing_seq != lcd_business_snapshot.sequence)) {
            LcdUpdateDrawingDigits(0U);
        }
        if (lcd_page_dirty != 0U) {
            LcdRenderCurrentPage();
            lcd_page_dirty = 0U;
        }
        HAL_Delay(LCD_PAGE_POLL_DELAY_MS);
    }
}
