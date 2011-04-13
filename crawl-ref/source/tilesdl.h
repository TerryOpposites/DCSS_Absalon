/**
 * @file
 * @brief SDL-related functionality for the tiles port
**/

#ifdef USE_TILE
#ifndef TILESDL_H
#define TILESDL_H

#include "externs.h"

#ifdef USE_TILE_LOCAL

#include "tilereg.h"
#include "tiletex.h"

class Region;
class CRTRegion;
class CRTRegionSingleSelect;
class MenuRegion;
class TileRegion;
class DungeonRegion;
class GridRegion;
class InventoryRegion;
class SpellRegion;
class MemoriseRegion;
class ActorRegion;
class MonsterRegion;
class SkillRegion;
class CommandRegion;
class ActorRegion;
class TabbedRegion;
class MapRegion;
class ControlRegion;
class TitleRegion;
class DollEditRegion;
class StatRegion;
class MessageRegion;

typedef std::map<int, TabbedRegion*>::iterator tab_iterator;

#elif defined(USE_TILE_WEB)

#include "tileweb-text.h"

#endif

struct map_cell;

enum key_mod
{
    MOD_NONE  = 0x0,
    MOD_SHIFT = 0x1,
    MOD_CTRL  = 0x2,
    MOD_ALT   = 0x4,
};

struct MouseEvent
{
    enum mouse_event_type
    {
        PRESS,
        RELEASE,
        MOVE,
    };

    enum mouse_event_button
    {
        NONE        = 0x00,
        LEFT        = 0x01,
        MIDDLE      = 0x02,
        RIGHT       = 0x04,
        SCROLL_UP   = 0x08,
        SCROLL_DOWN = 0x10,
    };

    // Padding for ui_event
    unsigned char type;

    // kind of event
    mouse_event_type event;
    // if PRESS or RELEASE, the button pressed
    mouse_event_button button;
    // bitwise-or of buttons currently pressed
    unsigned short held;
    // bitwise-or of key mods currently pressed
    unsigned char mod;
    // location of events in pixels and in window coordinate space
    unsigned int px;
    unsigned int py;
};

#ifdef USE_TILE_LOCAL
class FontWrapper;
#endif

#ifdef USE_TILE_LOCAL
class crawl_view_buffer;
#elif defined(USE_TILE_WEB)
 #include "viewgeom.h"
#endif

class TilesFramework
{
public:
    TilesFramework();
    virtual ~TilesFramework();

    bool initialise();
    void shutdown();
    void load_dungeon(const crawl_view_buffer &vbuf, const coord_def &gc);
    void load_dungeon(const coord_def &gc);
    int getch_ck();
    void resize();
    void layout_statcol();
    void clrscr();

    void cgotoxy(int x, int y, GotoRegion region = GOTO_CRT);
    GotoRegion get_cursor_region() const;
    int get_number_of_lines();
    int get_number_of_cols();

    void update_minimap(const coord_def &gc);
    void clear_minimap();
    void update_minimap_bounds();
    void toggle_inventory_display();
    void update_tabs();

    void set_need_redraw(unsigned int min_tick_delay = 0);
    bool need_redraw() const;
    void redraw();

    void place_cursor(cursor_type type, const coord_def &gc);
    void clear_text_tags(text_tag_type type);
    void add_text_tag(text_tag_type type, const std::string &tag,
                      const coord_def &gc);
    void add_text_tag(text_tag_type type, const monster* mon);

//    bool initialise_items(); // XXX: Where is this even implemented?

    const coord_def &get_cursor() const;

    void add_overlay(const coord_def &gc, tileidx_t idx);
    void clear_overlays();

    void draw_doll_edit();

#ifdef USE_TILE_LOCAL
    void calculate_default_options(); // XXX: I don't know why this is public

    void draw_title();
    void update_title_msg(std::string load_msg);
    void hide_title();

    MenuRegion *get_menu() { return m_region_menu; }

    FontWrapper* get_crt_font() { return m_fonts.at(m_crt_font).font; }
    CRTRegion* get_crt() { return m_region_crt; }
    const ImageManager* get_image_manager() { return m_image; }

    bool is_fullscreen() { return m_fullscreen; }
#endif

    int to_lines(int num_tiles);

#ifdef USE_TILE_WEB
    void textcolor(int col);
    void textbackground(int col);
    void put_string(char *str);
    void put_ucs_string(ucs_t *str);
    void clear_to_end_of_line();
    int wherex() { return m_print_x + 1; }
    int wherey() { return m_print_y + 1; }
#endif
protected:
    enum TabID
    {
        TAB_ITEM,
        TAB_SPELL,
        TAB_MEMORISE,
        TAB_MONSTER,
        TAB_SKILL,
        TAB_COMMAND,
        TAB_MAX,
    };

    enum LayerID
    {
        LAYER_NORMAL,
        LAYER_CRT,
        LAYER_TILE_CONTROL,
        LAYER_MAX,
    };
    LayerID m_active_layer;

#ifdef USE_TILE_LOCAL
    int load_font(const char *font_file, int font_size,
                  bool default_on_fail, bool outline);
    int handle_mouse(MouseEvent &event);

    void use_control_region(ControlRegion *region);

    // screen pixel dimensions
    coord_def m_windowsz;
    // screen pixels per view cell
    coord_def m_viewsc;

    bool m_fullscreen;
    bool m_need_redraw;

    class Layer
    {
    public:
        // Layers don't own these regions
        std::vector<Region*> m_regions;
    };
    Layer m_layers[LAYER_MAX];

    // Normal layer
    TileRegionInit  m_init;
    DungeonRegion   *m_region_tile;
    StatRegion      *m_region_stat;
    MessageRegion   *m_region_msg;
    MapRegion       *m_region_map;
    TabbedRegion    *m_region_tab;
    InventoryRegion *m_region_inv;
    SpellRegion     *m_region_spl;
    MemoriseRegion  *m_region_mem;
    MonsterRegion   *m_region_mon;
    SkillRegion     *m_region_skl;
    CommandRegion   *m_region_cmd;

    std::map<int, TabbedRegion*> m_tabs;

    // Full-screen CRT layer
    CRTRegion       *m_region_crt;
    MenuRegion      *m_region_menu;

    struct font_info
    {
        std::string name;
        int size;
        bool outline;
        FontWrapper *font;
    };
    std::vector<font_info> m_fonts;
    int m_crt_font;
    int m_msg_font;
    int m_tip_font;
    int m_lbl_font;

    int m_tab_margin;
    int m_stat_col;
    int m_stat_x_divider;
    int m_statcol_top;
    int m_statcol_bottom;
    int m_map_pixels;

    void do_layout();
    int calc_tab_lines(const int num_elements);
    void place_tab(int idx);
    void autosize_minimap();
    void place_minimap();
    void resize_inventory();
    void place_gold_turns();

    ImageManager *m_image;

    // Mouse state.
    unsigned short m_buttons_held;
    unsigned char m_key_mod;
    coord_def m_mouse;
    unsigned int m_last_tick_moved;
    unsigned int m_last_tick_redraw;

    std::string m_tooltip;

    int m_screen_width;
    int m_screen_height;

    struct cursor_loc
    {
        cursor_loc() { reset(); }
        void reset() { reg = NULL; cx = cy = -1; mode = MOUSE_MODE_MAX; }
        bool operator==(const cursor_loc &rhs) const
        {
            return (rhs.reg == reg
                    && rhs.cx == cx
                    && rhs.cy == cy
                    && rhs.mode == mode);
        }
        bool operator!=(const cursor_loc &rhs) const
        {
            return !(*this == rhs);
        }

        Region *reg;
        int cx, cy;
        mouse_mode mode;
    };
    cursor_loc m_cur_loc;
#elif defined(USE_TILE_WEB)
    // TODO: Much of this and tileweb.cc is copied from DungeonRegion -- unify somehow?
    // or at least pull this code into its own class similar to DungeonRegion?
    unsigned int m_last_tick_redraw;
    bool m_need_redraw;

    crawl_view_buffer m_current_view;
    coord_def m_current_gc;

    crawl_view_buffer m_next_view;
    coord_def m_next_gc;

    coord_def m_cursor[CURSOR_MAX];
    coord_def m_last_clicked_grid;

    bool m_has_overlays;

    CRTTextArea m_text_crt;
    StatTextArea m_text_stat;
    MessageTextArea m_text_message;

    GotoRegion m_cursor_region;

    WebTextArea *m_print_area;
    int m_print_x, m_print_y;
    int m_print_fg, m_print_bg;
#endif
};

// Main interface for tiles functions
extern TilesFramework tiles;

#endif
#endif
