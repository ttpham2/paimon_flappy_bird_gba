/*
 * Copyright (c) 2020-2021 Gustavo Valiente gustavo.valiente@protonmail.com
 * zlib License, see LICENSE file.
 */

#include "bn_log.h"
#include "bn_core.h"
#include "bn_math.h"
#include "bn_keypad.h"
#include "bn_optional.h"
#include "bn_display.h"
#include "bn_blending.h"
#include "bn_bgs_mosaic.h"
#include "bn_bg_palettes.h"
#include "bn_music_actions.h"
#include "bn_camera_actions.h"
#include "bn_string_view.h"
#include "bn_vector.h"
#include "bn_string.h"
#include <bn_random.h>
#include <cstring>

#include "bn_regular_bg_actions.h"
#include "bn_regular_bg_builder.h"
#include "bn_regular_bg_attributes.h"
#include "bn_regular_bg_position_hbe_ptr.h"
#include "bn_regular_bg_attributes_hbe_ptr.h"
#include "demo_polygon.h"
#include "polygon_sprite.h"
#include "bn_color.h"

#include "bn_sprite_actions.h"
#include "bn_sprite_builder.h"
#include "bn_sprite_text_generator.h"
#include "bn_sprite_animate_actions.h"
#include "bn_sprite_first_attributes.h"
#include "bn_sprite_third_attributes.h"
#include "bn_sprite_position_hbe_ptr.h"
#include "bn_sprite_first_attributes_hbe_ptr.h"
#include "bn_sprite_third_attributes_hbe_ptr.h"
#include "bn_sprite_affine_second_attributes.h"
#include "bn_sprite_regular_second_attributes.h"
#include "bn_sprite_affine_second_attributes_hbe_ptr.h"
#include "bn_sprite_regular_second_attributes_hbe_ptr.h"


#include "bn_music_items.h"
#include "bn_sound_items.h"
#include "bn_regular_bg_items_route99.h"
#include "bn_regular_bg_items_monstadt_bg.h"
#include "bn_regular_bg_items_paimon_tower.h"

#include "bn_sprite_items_paimon.h"
#include "bn_sprite_items_cursor.h"

#include "info.h"
#include "variable_8x16_sprite_font.h"

namespace
{
    
    
    bn::fixed modulo(bn::fixed a, bn::fixed m)
    {

        return a - m * ((a/m).right_shift_integer());
    }

    bn::fixed get_map_index(bn::fixed x, bn::fixed y, bn::fixed map_size)
    {

        return modulo((y.safe_division(8).right_shift_integer() * map_size/8 + x/8), map_size*8);
        //return ((y.safe_division(8) * (map_size/8)) + (x/8));
        //return (y * map_size/8 + x/8
    }
    
    class Collider{
        
        public:
        bn::fixed_point point;
        bn::sprite_shape_size size;
        int top_y_offset = 15;
        int bottom_y_offset = 19;
        bn::fixed_point tl,tr,bl,br;
        
        
        Collider(bn::sprite_ptr sprite) : 
        size(sprite.shape_size()),
        point(sprite.position()){
            // point = sprite.position();
            // size = sprite.shape_size();
            tl.set_x(point.x() - (size.width()/2));
            tl.set_y(point.y() + top_y_offset);
            tr.set_x(point.x() + (size.width()/2) - 1);
            tr.set_y(point.y() + top_y_offset);
            bl.set_x(point.x() - (size.width()/2));
            bl.set_y(point.y() + bottom_y_offset);
            br.set_x(point.x() + (size.width()/2) - 1);
            br.set_y(point.y() + bottom_y_offset);

        }
        
        void add_x(int px){
            tl.set_x(tl.x() + px);
            tr.set_x(tr.x() + px);
            bl.set_x(bl.x() + px);
            br.set_x(br.x() + px);
        }
        
        void add_y(int px){
            tl.set_y(tl.y() + px);
            tr.set_y(tr.y() + px);
            bl.set_y(bl.y() + px);
            br.set_y(br.y() + px);
        
        }
        
        void log_vertices(){
            BN_LOG("tl:", tl.x().integer(), tl.y().integer());
            BN_LOG("tr:", tr.x().integer(), tr.y().integer());
            BN_LOG("bl:", bl.x().integer(), bl.y().integer());
            BN_LOG("br:", br.x().integer(), br.y().integer());
        }
        
        
        
        
    };
    
    int map_index(bn::fixed_point point, bn::regular_bg_ptr& map)
    {
        const bn::fixed_point bgPos = map.position();
        const int bgWidth = map.dimensions().width();
        const int bgHeight = map.dimensions().height();

        BN_ASSERT(bgWidth % 256 == 0 && bgHeight % 256 == 0, "width or height of bg is not multiple of 256");

        bn::fixed x = point.x() - (bgPos.x() - bgWidth / 2);
        bn::fixed y = point.y() - (bgPos.y() - bgHeight / 2);
        while (x < 0)
            x += bgWidth;
        while (y < 0)
            y += bgHeight;
        int ix = (x / 8).integer();
        int iy = (y / 8).integer();
        const int nRow = bgHeight / 8;
        const int nCol = bgWidth / 8;
        ix = (ix + nCol) % nCol;
        iy = (iy + nRow) % nRow;
        return ix + iy * nCol;
    }
    
    void check_collideable(bn::regular_bg_ptr& map){
        int i = 0;
        for(auto cell : map.map().cells_ref().value()){
            // BN_LOG("INDEX:", i);
            i++;
            if(cell != 0){
                BN_LOG("INDEX:", i);
                BN_LOG("COLLIDEABLE CELL: ", cell);
            }
            else{
                //BN_LOG("TRANSPARENT CELL: ", cell);
            }
        }
    }
    void check_collisions(bn::sprite_ptr& sprite, bn::regular_bg_ptr& map)
    {
        // gets the current map cell the sprite is on.
        //BN_LOG("MAP WIDTH:", map.dimensions().width());
        //BN_LOG("MAP HEIGHT:", map.dimensions().height());
        //BN_LOG("SPRITE X:", sprite.x());
        //BN_LOG("SPRITE Y:", sprite.y());
        // bn::fixed current_cell = get_map_index(sprite.x(), sprite.y(), map.dimensions().width());
        int current_cell = map_index(sprite.position(), map);

        // at the moment all I am doing is checking if the cell is not the sky cell (aka not zero)
        bool is_hit = map.map().cells_ref().value().at(current_cell) != 0;
        //BN_LOG("CURRENT CELL", current_cell);
        //BN_LOG(map.map().cells_ref().value().at(current_cell.integer()));
        
        if(is_hit){
            //sprite.set_x(sprite.x() - 1);
            // while(1){}
            BN_LOG("Paimon hit a wall!");
            BN_LOG("SPRITE X:", sprite.x());
            BN_LOG("SPRITE Y:", sprite.y());
        }
    }
    void check_collisions(Collider& collider, bn::regular_bg_ptr& map)
    {
        // gets the current map cell each collider's vertex
        int tl_cell = map_index(collider.tl, map);
        int tr_cell = map_index(collider.tr, map);
        int bl_cell = map_index(collider.bl, map);
        int br_cell = map_index(collider.br, map);

        // at the moment all I am doing is checking if the cell is not the sky cell (aka not zero)
        bool is_hit = map.map().cells_ref().value().at(tl_cell) != 0 ||
                 map.map().cells_ref().value().at(tr_cell) != 0 ||
                 map.map().cells_ref().value().at(bl_cell) != 0 || 
                 map.map().cells_ref().value().at(br_cell) != 0;
                 
        if(map.map().cells_ref().value().at(tl_cell) != 0){
            BN_LOG("tl collision at cell: ", tl_cell);
        }
        else if(map.map().cells_ref().value().at(tr_cell) != 0){
            BN_LOG("tr collision at cell: ", tr_cell,
                    "\n tileidx: ", map.map().cells_ref().value().at(tr_cell));
        }
        else if(map.map().cells_ref().value().at(bl_cell) != 0){
            BN_LOG("bl collision at cell: ", bl_cell);
        } 
        else if(map.map().cells_ref().value().at(br_cell) != 0){
            BN_LOG("br collision at cell: ", br_cell);
        }
        
    }
    
    bn::fixed current_cell(bn::sprite_ptr& sprite, bn::regular_bg_ptr& map){ 
        return get_map_index(sprite.x(), sprite.y(), map.dimensions().width());
    }
    
    void _move_vertex(polygon& polygon, polygon_sprite& polygon_sprite)
    {
        if(bn::keypad::left_held())
        {
            for(auto vertex : polygon.vertices()){
                vertex.set_x(vertex.x() - 1);
            }

            polygon_sprite.reload_polygons();
        }
        else if(bn::keypad::right_held())
        {
            for(auto vertex : polygon.vertices()){
                vertex.set_x(vertex.x() + 1);
            }

            polygon_sprite.reload_polygons();

        }

        if(bn::keypad::up_held())
        {
            
            for(auto vertex : polygon.vertices()){
                vertex.set_y(vertex.y() - 1);
            }

            polygon_sprite.reload_polygons();
        }
        else if(bn::keypad::down_held())
        {
            
            for(auto vertex : polygon.vertices()){
                vertex.set_y(vertex.y() + 1);
            }

            polygon_sprite.reload_polygons();
        }
    }
     void regular_bgs_visibility_scene(bn::sprite_text_generator& text_generator)
    {
        //text
        constexpr const bn::string_view info_text_lines[] = {
            "A: hide/show BG",
            "",
            "START: go to next scene",
        };

        //bn::fixed current_cell = 0;
        bn::vector<bn::sprite_ptr, 32> text_sprites;
        
        text_generator.set_center_alignment();
        //info info("Regular BGs visibility", info_text_lines, text_generator);
        
        //backgrounds
        bn::vector<bn::optional<bn::regular_bg_ptr>, 3> towers;
        bn::regular_bg_ptr bg = bn::regular_bg_items::monstadt_bg.create_bg(0, 0);
        
        // bn::optional<bn::regular_bg_ptr> tower1 = bn::regular_bg_items::paimon_tower.create_bg(-60, 0);
        // bn::optional<bn::regular_bg_ptr> tower2 = bn::regular_bg_items::paimon_tower.create_bg(95, 0);
        
        
        bn::optional<bn::regular_bg_ptr> tower1 = bn::regular_bg_items::paimon_tower.create_bg(256, 256);
        bn::optional<bn::regular_bg_ptr> tower2 = bn::regular_bg_items::paimon_tower.create_bg(256, 256);
        
        towers.push_back(tower1);
        towers.push_back(tower2);       
        //sprites      
        //bn::sprite_ptr paimon = bn::sprite_items::paimon.create_sprite(-80,60);
        int const BASE_X = 0;
        int const BASE_Y = 0;
        
        bn::sprite_ptr paimon = bn::sprite_items::paimon.create_sprite(BASE_X,BASE_Y);
        bn::sprite_ptr cursor = bn::sprite_items::cursor.create_sprite(BASE_X,BASE_Y);
        
        //colliders
        Collider collider = Collider(paimon);
        collider.log_vertices();
        
        //polygon for visual hitbox
        //bn::fixed_point vertices[] = {collider.tl, collider.tr, collider.bl, collider.br};
        const int Y_SHAPE_OFFSET = 80;
        const bn::fixed_point vertices[] = {
            bn::fixed_point(collider.tl.x() + 120,collider.tl.y() +  Y_SHAPE_OFFSET),
            bn::fixed_point(collider.tr.x() + 120,collider.tr.y() +  Y_SHAPE_OFFSET),
            bn::fixed_point(collider.br.x() + 120,collider.br.y() +  Y_SHAPE_OFFSET),
            bn::fixed_point(collider.bl.x() + 120,collider.bl.y() +  Y_SHAPE_OFFSET)
        };
        polygon hitbox_polygon(vertices);
        polygon_sprite hitbox_polygon_sprite(hitbox_polygon, 0,1);
        
        //cameras
        bn::camera_ptr camera = bn::camera_ptr::create(0, 0);
        
        paimon.set_camera(camera);
        cursor.set_camera(camera);
        bg.set_camera(camera);
        tower1.value().set_camera(camera);
        tower2.value().set_camera(camera);
        
        
        
        //music
        int tower_count = 0;
        int rand;
        int sign;
        bn::random rand_gen = bn::random();
        
        
        //paimon.set_visible(false);
        check_collideable(tower1.value());
        //main loop
        //while(1){}
        while(! bn::keypad::start_pressed())
        {
            // check_collisions(paimon, tower1.value());
            // check_collisions(paimon, tower2.value());
            if(bn::keypad::a_pressed()){
                paimon.set_y(paimon.y() - 12);
            }
            else if(bn::keypad::up_held()){
                paimon.set_y(paimon.y() - 1);
                cursor.set_y(cursor.y() - 1);
                camera.set_y(camera.y() - 1);
                collider.add_y(-1);
                _move_vertex(hitbox_polygon, hitbox_polygon_sprite);
            }
            else if(bn::keypad::down_held()){
                paimon.set_y(paimon.y() + 1);
                cursor.set_y(cursor.y() + 1);
                camera.set_y(camera.y() + 1);
                collider.add_y(1);
                _move_vertex(hitbox_polygon, hitbox_polygon_sprite);
            }
            else if(bn::keypad::left_held()){
                paimon.set_x(paimon.x() - 1);
                cursor.set_x(cursor.x() - 1);
                camera.set_x(camera.x() - 1);
                collider.add_x(-1);
                _move_vertex(hitbox_polygon, hitbox_polygon_sprite);
            }
            else if(bn::keypad::right_held()){
                paimon.set_x(paimon.x() + 1);
                cursor.set_x(cursor.x() + 1);
                camera.set_x(camera.x() + 1);
                collider.add_x(1);
                _move_vertex(hitbox_polygon, hitbox_polygon_sprite);
            }
            
            //paimon.set_y(paimon.y() + 0.5);
            // camera.set_x(camera.x() + 1);
            // paimon.set_x(paimon.x() + 1);
            
            
            text_sprites.clear();

            auto map_cell = map_index(paimon.position(), tower1.value());
            auto cell_str = std::to_string(map_index(paimon.position(), tower1.value())) + ", ";
            auto tile_str = std::to_string(tower1.value().map().cells_ref().value().at(map_cell));
            auto str_view = (cell_str + tile_str).c_str();
            text_generator.generate(0, -40, 
            bn::string_view(str_view),
            text_sprites);
           
            for(auto tower : towers){
                //check_collisions(paimon, tower.value());
                check_collisions(collider, tower.value());


                // if((paimon.x().integer() % 512) == 511){
                    // //paimon.set_x(-80);
                    // //camera.set_x(0);
                    // rand = rand_gen.get();
                    // sign = rand % 2;
                    // if(sign == 0){
                        // tower.value().set_y((rand % 6) * 8);
                    // }
                    // else{
                        // tower.value().set_y(-(rand % 6) * 8);
                    // }
                // }
            }
            
            hitbox_polygon_sprite.update();
            bn::core::update();
        }
    }
}
    
int main()
{
    bn::core::init();

    bn::sprite_text_generator text_generator(variable_8x16_sprite_font);
    bn::bg_palettes::set_transparent_color(bn::color(16, 16, 16));

    while(true)
    {
        regular_bgs_visibility_scene(text_generator);
        bn::core::update();
    }
}
