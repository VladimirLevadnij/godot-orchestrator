// This file is part of the Godot Orchestrator project.
//
// Copyright (c) 2023-present Vahera Studios LLC and its contributors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//		http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "graph_node.h"

#include "common/logger.h"
#include "common/scene_utils.h"
#include "graph_edit.h"
#include "graph_node_pin.h"
#include "plugin/settings.h"
#include "script/nodes/editable_pin_node.h"
#include "script/script.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/editor_inspector.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_action.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/script_editor_base.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>

OrchestratorGraphNode::OrchestratorGraphNode(OrchestratorGraphEdit* p_graph, const Ref<OScriptNode>& p_node)
{
    _graph = p_graph;
    _node = p_node;

    // Setup defaults
    set_name(itos(_node->get_id()));
    set_resizable(true);
    set_h_size_flags(SIZE_EXPAND_FILL);
    set_v_size_flags(SIZE_EXPAND_FILL);
    set_meta("__script_node", p_node);

    _update_tooltip();
}

void OrchestratorGraphNode::_bind_methods()
{
}

void OrchestratorGraphNode::_notification(int p_what)
{
    if (p_what == NOTIFICATION_READY)
    {
        // Update the title bar widget layouts
        HBoxContainer* titlebar = get_titlebar_hbox();
        _indicators = memnew(HBoxContainer);
        titlebar->add_child(_indicators);

        Control* spacer = memnew(Control);
        spacer->set_custom_minimum_size(Vector2(3, 0));
        titlebar->add_child(spacer);

        // Used to replicate size/position state to underlying node resource
        connect("dragged", callable_mp(this, &OrchestratorGraphNode::_on_node_moved));
        connect("resized", callable_mp(this, &OrchestratorGraphNode::_on_node_resized));

        // Used to replicate state changes from node resource to the UI
        _node->connect("pins_changed", callable_mp(this, &OrchestratorGraphNode::_on_pins_changed));
        _node->connect("pin_connected", callable_mp(this, &OrchestratorGraphNode::_on_pin_connected));
        _node->connect("pin_disconnected", callable_mp(this, &OrchestratorGraphNode::_on_pin_disconnected));
        _node->connect("changed", callable_mp(this, &OrchestratorGraphNode::_on_changed));

        // Update title bar aspects
        _update_titlebar();
        _update_styles();

        // Update the pin display upon entering
        _update_pins();

        // IMPORTANT
        // The context menu must be attached to the title bar or else this will cause
        // problems with the GraphNode and slot/index logic when calling set_slot
        // functions.
        _context_menu = memnew(PopupMenu);
        _context_menu->connect("id_pressed", callable_mp(this, &OrchestratorGraphNode::_on_context_menu_selection));
        get_titlebar_hbox()->add_child(_context_menu);
    }
}

void OrchestratorGraphNode::_gui_input(const Ref<InputEvent>& p_event)
{
    Ref<InputEventMouseButton> button = p_event;
    if (button.is_null() || !button->is_pressed())
        return;

    if (button->is_double_click() && button->get_button_index() == MOUSE_BUTTON_LEFT)
    {
        if (_node->can_jump_to_definition())
        {
            if (Object* target = _node->get_jump_target_for_double_click())
            {
                _graph->request_focus(target);
                accept_event();
            }
        }
        return;
    }
    else if (button->get_button_index() == MOUSE_BUTTON_RIGHT)
    {
        // Show menu
        _show_context_menu(button->get_position());
        accept_event();
    }
}

OrchestratorGraphEdit* OrchestratorGraphNode::get_graph()
{
    return _graph;
}

int OrchestratorGraphNode::get_script_node_id() const
{
    return _node->get_id();
}

void OrchestratorGraphNode::set_inputs_for_accept_opacity(float p_opacity, OrchestratorGraphNodePin* p_other)
{
    for (int i = 0; i < get_input_port_count(); i++)
    {
        if (is_slot_enabled_left(i))
        {
            OrchestratorGraphNodePin* pin = get_input_pin(i);
            if (!pin->can_accept(p_other))
            {
                Color color = get_input_port_color(i);
                color.a = p_opacity;
                set_slot_color_left(i, color);
            }
        }
    }
}

void OrchestratorGraphNode::set_outputs_for_accept_opacity(float p_opacity, OrchestratorGraphNodePin* p_other)
{
    for (int i = 0; i < get_output_port_count(); i++)
    {
        if (is_slot_enabled_right(i))
        {
            OrchestratorGraphNodePin* pin = get_output_pin(i);
            if (!p_other->can_accept(pin))
            {
                Color color = get_output_port_color(i);
                color.a = p_opacity;
                set_slot_color_right(i, color);
            }
        }
    }
}

void OrchestratorGraphNode::set_all_inputs_opacity(float p_opacity)
{
    for (int i = 0; i < get_input_port_count(); i++)
    {
        if (is_slot_enabled_left(i))
        {
            Color color = get_input_port_color(i);
            color.a = p_opacity;
            set_slot_color_left(i, color);
        }
    }
}

void OrchestratorGraphNode::set_all_outputs_opacity(float p_opacity)
{
    for (int i = 0; i < get_output_port_count(); i++)
    {
        if (is_slot_enabled_right(i))
        {
            Color color = get_output_port_color(i);
            color.a = p_opacity;
            set_slot_color_right(i, color);
        }
    }
}

int OrchestratorGraphNode::get_inputs_with_opacity(float p_opacity)
{
    int count = 0;
    for (int i = 0; i < get_input_port_count(); i++)
    {
        if (is_slot_enabled_left(i))
        {
            Color color = get_input_port_color(i);
            if (UtilityFunctions::is_equal_approx(color.a, p_opacity))
                count++;
        }
    }
    return count;
}

int OrchestratorGraphNode::get_outputs_with_opacity(float p_opacity)
{
    int count = 0;
    for (int i = 0; i < get_input_port_count(); i++)
    {
        if (is_slot_enabled_right(i))
        {
            Color color = get_output_port_color(i);
            if (UtilityFunctions::is_equal_approx(color.a, p_opacity))
                count++;
        }
    }
    return count;
}

void OrchestratorGraphNode::unlink_all()
{
    Vector<Ref<OScriptNodePin>> pins = _node->find_pins();
    for (const Ref<OScriptNodePin>& pin : pins)
        pin->unlink_all();
}

void OrchestratorGraphNode::_update_pins()
{
    if (_is_add_pin_button_visible())
    {
        MarginContainer* margin = memnew(MarginContainer);
        margin->add_theme_constant_override("margin_bottom", 4);
        add_child(margin);

        HBoxContainer* container = memnew(HBoxContainer);
        container->set_h_size_flags(SIZE_EXPAND_FILL);
        container->set_alignment(BoxContainer::ALIGNMENT_END);
        margin->add_child(container);

        Button* button = memnew(Button);
        button->set_button_icon(SceneUtils::get_icon(this, "ZoomMore"));
        button->set_tooltip_text("Add new pin");
        container->add_child(button);

        button->connect("pressed", callable_mp(this, &OrchestratorGraphNode::_on_add_pin_pressed));
    }
}

void OrchestratorGraphNode::_update_indicators()
{
    // Free all child indicators
    for (int i = 0; i < _indicators->get_child_count(); i++)
        _indicators->get_child(i)->queue_free();

    if (_node->get_flags().has_flag(OScriptNode::ScriptNodeFlags::DEVELOPMENT_ONLY))
    {
        TextureRect* notification = memnew(TextureRect);
        notification->set_texture(SceneUtils::get_icon(this, "Notification"));
        notification->set_custom_minimum_size(Vector2(0, 24));
        notification->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
        notification->set_tooltip_text("Node only executes during development builds, not included in exported builds.");
        _indicators->add_child(notification);
    }

    if (_node->get_flags().has_flag(OScriptNode::ScriptNodeFlags::EXPERIMENTAL))
    {
        TextureRect* notification = memnew(TextureRect);
        notification->set_texture(SceneUtils::get_icon(this, "NodeWarning"));
        notification->set_custom_minimum_size(Vector2(0, 24));
        notification->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
        notification->set_tooltip_text("Node is experimental and behavior may change without notice.");
        _indicators->add_child(notification);
    }
}

void OrchestratorGraphNode::_update_titlebar()
{
    HBoxContainer* titlebar = get_titlebar_hbox();

    // This should always be true but sanity check
    if (titlebar->get_child_count() > 0)
    {
        Ref<Texture2D> icon_texture;
        if (!_node->get_icon().is_empty())
            icon_texture = SceneUtils::get_icon(this, _node->get_icon());

        TextureRect* rect = Object::cast_to<TextureRect>(titlebar->get_child(0));
        if (!rect && icon_texture.is_valid())
        {
            // Add node's icon to the UI
            rect = memnew(TextureRect);
            rect->set_custom_minimum_size(Vector2(0, 24));
            rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
            rect->set_texture(icon_texture);

            // Add the icon and move it to the start of the HBox.
            titlebar->add_child(rect);
            titlebar->move_child(rect, 0);
        }
        else if (rect && !icon_texture.is_valid())
        {
            if (!_node->get_icon().is_empty())
            {
                // New icon cannot be changed (make it look broken)
                rect->set_texture(SceneUtils::get_icon(this, "Unknown"));
            }
            else
            {
                // Icon removed, remove this from the UI
                rect->queue_free();
                rect = nullptr;
            }
        }
        else if (rect && icon_texture.is_valid())
        {
            // Changing the texture
            rect->set_texture(icon_texture);
        }

        set_title((rect ? " " : "") + _node->get_node_title() + "   ");
    }

    _update_indicators();
}

void OrchestratorGraphNode::_update_styles()
{
    const String color_name = _node->get_node_title_color_name();
    if (!color_name.is_empty())
    {
        OrchestratorSettings* os = OrchestratorSettings::get_singleton();
        const String key = vformat("ui/node_colors/%s", color_name);
        if (os->has_setting(key))
        {
            Color color = os->get_setting(key);

            Ref<StyleBoxFlat> panel = get_theme_stylebox("panel");
            if (panel.is_valid())
            {
                Ref<StyleBoxFlat> new_panel = panel->duplicate(true);
                if (new_panel.is_valid())
                {
                    new_panel->set_border_color(Color(0.f, 0.f, 0.f));
                    new_panel->set_border_width_all(2);
                    new_panel->set_border_width(Side::SIDE_TOP, 0);
                    new_panel->set_content_margin_all(2);
                    new_panel->set_content_margin(Side::SIDE_BOTTOM, 6);
                    add_theme_stylebox_override("panel", new_panel);

                    Ref<StyleBoxFlat> panel_selected = new_panel->duplicate();
                    if (panel_selected.is_valid())
                    {
                        panel_selected->set_border_color(_get_selection_color());
                        add_theme_stylebox_override("panel_selected", panel_selected);
                    }
                }
            }

            Ref<StyleBoxFlat> titlebar = get_theme_stylebox("titlebar");
            if (titlebar.is_valid())
            {
                Ref<StyleBoxFlat> new_titlebar = titlebar->duplicate(true);
                if (new_titlebar.is_valid())
                {
                    new_titlebar->set_bg_color(color);
                    new_titlebar->set_border_width_all(2);
                    new_titlebar->set_border_width(Side::SIDE_BOTTOM, 0);

                    new_titlebar->set_content_margin_all(4);
                    new_titlebar->set_content_margin(Side::SIDE_LEFT, 12);
                    new_titlebar->set_content_margin(Side::SIDE_RIGHT, 12);
                    new_titlebar->set_border_color(color);

                    add_theme_stylebox_override("titlebar", new_titlebar);

                    Ref<StyleBoxFlat> titlebar_selected = new_titlebar->duplicate();
                    if (titlebar_selected.is_valid())
                    {
                        titlebar_selected->set_border_color(_get_selection_color());
                        add_theme_stylebox_override("titlebar_selected", titlebar_selected);
                    }
                }
            }
        }
    }
}

Color OrchestratorGraphNode::_get_selection_color() const
{
    return Color(0.68f, 0.44f, 0.09f);
}

void OrchestratorGraphNode::_update_node_attributes()
{
    // Attempt to shrink the container
    if (_resize_on_update())
        call_deferred("set_size", Vector2());

    // Some pin changes may affect the titlebar
    // We explicitly update the title here on change to capture that possibility
    _update_titlebar();

    _update_pins();
}

void OrchestratorGraphNode::_update_tooltip()
{
    String tooltip_text = _node->get_node_title();

    if (!_node->get_tooltip_text().is_empty())
        tooltip_text += "\n\n" + _node->get_tooltip_text();

    if (_node->get_flags().has_flag(OScriptNode::ScriptNodeFlags::DEVELOPMENT_ONLY))
        tooltip_text += "\n\nNode only executes during development. Exported builds will not include this node.";
    else if (_node->get_flags().has_flag(OScriptNode::ScriptNodeFlags::EXPERIMENTAL))
        tooltip_text += "\n\nThis node is experimental and may change in the future without warning.";

    tooltip_text += "\n\nID: " + itos(_node->get_id());
    tooltip_text += "\nClass: " + _node->get_class();
    tooltip_text += "\nFlags: " + itos(_node->get_flags());

    set_tooltip_text(SceneUtils::create_wrapped_tooltip_text(tooltip_text));
}

void OrchestratorGraphNode::_show_context_menu(const Vector2& p_position)
{
    // When showing the context-menu, if the current node is not selected, we should clear the
    // selection and the operation will only be applicable for this node and its pin.
    if (!is_selected())
    {
        get_graph()->clear_selection();
        set_selected(true);
    }

    _context_menu->clear();

    // Node actions
    _context_menu->add_separator("Node Actions");

    // todo: could consider a delegation to gather actions even from OrchestratorGraphNode impls
    // Get all node-specific actions, which are not UI-specific actions but rather logical actions that
    // should be taken by the OScriptNode resource rather than the OrchestratorGraphNode UI component.
    int node_action_id = CM_NODE_ACTION;
    _node->get_actions(_context_actions);
    for (const Ref<OScriptAction>& action : _context_actions)
    {
        if (action->get_icon().is_empty())
            _context_menu->add_item(action->get_text(), node_action_id);
        else
            _context_menu->add_icon_item(SceneUtils::get_icon(this, action->get_icon()), action->get_text(), node_action_id);

        node_action_id++;
    }

    // Check the node type
    Ref<OScriptEditablePinNode> editable_node = _node;

    // Comment nodes are group-able, meaning that any node that is contained with the Comment node's rect window
    // can be automatically selected and dragged with the comment node. This can be done in two ways, one by
    // double-clicking the comment node to trigger the selection/deselection process or two by selecting the
    // "Select Group" or "Deselect Group" added here.
    if (is_groupable())
    {
        const String icon = vformat("Theme%sAll", is_group_selected() ? "Deselect" : "Select");
        const String text = vformat("%s Group", is_group_selected() ? "Deselect" : "Select");
        const int32_t id = is_group_selected() ? CM_DESELECT_GROUP : CM_SELECT_GROUP;
        _context_menu->add_icon_item(SceneUtils::get_icon(this, icon), text, id);
    }

    _context_menu->add_icon_item(SceneUtils::get_icon(this, "Remove"), "Delete", CM_DELETE, KEY_DELETE);
    _context_menu->set_item_disabled(_context_menu->get_item_index(CM_DELETE), !_node->can_user_delete_node());

    _context_menu->add_icon_item(SceneUtils::get_icon(this, "ActionCut"), "Cut", CM_CUT, Key(KEY_MASK_CTRL | KEY_X));
    _context_menu->add_icon_item(SceneUtils::get_icon(this, "ActionCopy"), "Copy", CM_COPY, Key(KEY_MASK_CTRL | KEY_C));
    _context_menu->add_icon_item(SceneUtils::get_icon(this, "Duplicate"), "Duplicate", CM_DUPLICATE, Key(KEY_MASK_CTRL | KEY_D));

    _context_menu->add_icon_item(SceneUtils::get_icon(this, "Loop"), "Refresh Nodes", CM_REFRESH);
    _context_menu->add_icon_item(SceneUtils::get_icon(this, "Unlinked"), "Break Node Link(s)", CM_BREAK_LINKS);
    _context_menu->set_item_disabled(_context_menu->get_item_index(CM_BREAK_LINKS), !_node->has_any_connections());

    if (editable_node.is_valid())
        _context_menu->add_item("Add Option Pin", CM_ADD_OPTION_PIN);

    // todo: support breakpoints (See Trello)
    // _context_menu->add_separator("Breakpoints");
    // _context_menu->add_item("Toggle Breakpoint", CM_TOGGLE_BREAKPOINT, KEY_F9);
    // _context_menu->add_item("Add Breakpoint", CM_ADD_BREAKPOINT);

    _context_menu->add_separator("Documentation");
    _context_menu->add_icon_item(SceneUtils::get_icon(this, "Help"), "View Documentation", CM_VIEW_DOCUMENTATION);

    #ifdef _DEBUG
    _context_menu->add_separator("Debugging");
    _context_menu->add_icon_item(SceneUtils::get_icon(this, "Godot"), "Show details", CM_SHOW_DETAILS);
    #endif

    _context_menu->set_position(get_screen_position() + (p_position * (real_t) get_graph()->get_zoom()));
    _context_menu->reset_size();
    _context_menu->popup();
}

void OrchestratorGraphNode::_simulate_action_pressed(const String& p_action_name)
{
    Ref<InputEventAction> action = memnew(InputEventAction());
    action->set_action(p_action_name);
    action->set_pressed(true);

    Input::get_singleton()->parse_input_event(action);
}

void OrchestratorGraphNode::_on_changed()
{
    // Notifications can bubble up to the OrchestratorGraphNode from either the OrchestratorGraphNodePin
    // or the underlying ScriptNode depending on the property that was changed and how
    // it is managed by the node. In this case, it's important that we also listen for
    // this callback and adjust the node-level attributes accordingly.
    _update_node_attributes();
}

bool OrchestratorGraphNode::_is_add_pin_button_visible() const
{
    Ref<OScriptEditablePinNode> editable_node = _node;
    return editable_node.is_valid() && editable_node->can_add_dynamic_pin();
}

List<OrchestratorGraphNode*> OrchestratorGraphNode::get_nodes_within_global_rect()
{
    Rect2 rect = get_global_rect();

    List<OrchestratorGraphNode*> results;
    _graph->for_each_graph_node([&](OrchestratorGraphNode* other) {
        if (other && other != this)
        {
            Rect2 other_rect = other->get_global_rect();
            if (rect.intersects(other_rect))
                results.push_back(other);
        }
    });
    return results;
}

void OrchestratorGraphNode::_on_node_moved([[maybe_unused]] Vector2 p_old_pos, Vector2 p_new_pos)
{
    _node->set_position(p_new_pos);
}

void OrchestratorGraphNode::_on_node_resized()
{
    _node->set_size(get_size());
}

void OrchestratorGraphNode::_on_pins_changed()
{
    // no-op
}

void OrchestratorGraphNode::_on_pin_connected(int p_type, int p_index)
{
    if (OrchestratorGraphNodePin* pin = p_type == PD_Input ? get_input_pin(p_index) : get_output_pin(p_index))
        pin->set_default_value_control_visibility(false);
}

void OrchestratorGraphNode::_on_pin_disconnected(int p_type, int p_index)
{
    if (OrchestratorGraphNodePin* pin = p_type == PD_Input ? get_input_pin(p_index) : get_output_pin(p_index))
        pin->set_default_value_control_visibility(true);
}

void OrchestratorGraphNode::_on_add_pin_pressed()
{
    Ref<OScriptEditablePinNode> editable_node = _node;
    if (editable_node.is_valid() && editable_node->can_add_dynamic_pin())
        editable_node->add_dynamic_pin();
}

void OrchestratorGraphNode::_on_context_menu_selection(int p_id)
{
    if (p_id >= CM_NODE_ACTION)
    {
        int action_index = p_id - CM_NODE_ACTION;
        if (action_index < _context_actions.size())
        {
            const Ref<OScriptAction>& action = _context_actions[action_index];
            if (action->get_handler().is_valid())
                action->get_handler().call();
        }
    }
    else
    {
        switch (p_id)
        {
            case CM_CUT:
            {
                _simulate_action_pressed("ui_copy");
                _simulate_action_pressed("ui_graph_delete");
                break;
            }
            case CM_COPY:
            {
                _simulate_action_pressed("ui_copy");
                break;
            }
            case CM_DUPLICATE:
            {
                _simulate_action_pressed("ui_graph_duplicate");
                break;
            }
            case CM_DELETE:
            {
                if (_node->can_user_delete_node())
                    get_script_node()->get_owning_script()->remove_node(_node->get_id());
                break;
            }
            case CM_REFRESH:
            {
                _node->reconstruct_node();
                break;
            }
            case CM_BREAK_LINKS:
            {
                unlink_all();
                break;
            }
            case CM_VIEW_DOCUMENTATION:
            {
                get_graph()->goto_class_help(_node->get_class());
                break;
            }
            case CM_SELECT_GROUP:
            {
                select_group();
                break;
            }
            case CM_DESELECT_GROUP:
            {
                deselect_group();
                break;
            }
            case CM_ADD_OPTION_PIN:
            {
                Ref<OScriptEditablePinNode> editable = _node;
                if (editable.is_valid())
                    editable->add_dynamic_pin();
                break;
            }
            #ifdef _DEBUG
            case CM_SHOW_DETAILS:
            {
                UtilityFunctions::print("--- Dump Node ", _node->get_class(), " ---");
                UtilityFunctions::print("Position: ", _node->get_position());

                Vector<Ref<OScriptNodePin>> pins = _node->get_all_pins();
                UtilityFunctions::print("Pins: ", pins.size());
                for (const Ref<OScriptNodePin>& pin : pins)
                {
                    UtilityFunctions::print("Pin[", pin->get_pin_name(), "]: ",
                                            pin->is_input() ? "Input" : "Output",
                                            " Default: ", pin->get_effective_default_value(),
                                            " Type: ", pin->get_pin_type_name(), " (", pin->get_type(), ")",
                                            " Target: ", pin->get_target_class(),
                                            " Flags: ", pin->get_flags().operator Variant());
                }
                break;
            }
            #endif
            default:
            {
                WARN_PRINT("Feature not yet implemented");
                break;
            }
        }
    }

    // Cleanup actions
    _context_actions.clear();
}
