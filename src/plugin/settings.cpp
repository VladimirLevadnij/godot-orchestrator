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
#include "settings.h"

#include "common/dictionary_utils.h"
#include "common/logger.h"

#include <godot_cpp/classes/project_settings.hpp>

// Helper setting layouts
#define BOOL_SETTING(n,v) PropertyInfo(Variant::BOOL, n), v
#define COLOR_NO_ALPHA_SETTING(n,v) PropertyInfo(Variant::COLOR, n, PROPERTY_HINT_COLOR_NO_ALPHA), v
#define FILE_SETTING(n,f,v) PropertyInfo(Variant::STRING, n, PROPERTY_HINT_FILE, f), v
#define INT_SETTING(n,v) PropertyInfo(Variant::INT, n), v
#define RANGE_SETTING(n,r,v) PropertyInfo(Variant::INT, n, PROPERTY_HINT_RANGE, r), v
#define SENUM_SETTING(n,r,v) PropertyInfo(Variant::STRING, n, PROPERTY_HINT_ENUM, r), v
#define RESOURCE_SETTING(n,t,v) PropertyInfo(Variant::STRING, n, PROPERTY_HINT_RESOURCE_TYPE, t), v

OrchestratorSettings* OrchestratorSettings::_singleton = nullptr;

OrchestratorSettings::OrchestratorSettings()
{
    _singleton = this;

    _initialize_settings();
    _update_default_settings();
}

OrchestratorSettings::~OrchestratorSettings()
{
    _singleton = nullptr;
}

bool OrchestratorSettings::has_setting(const String& key) const
{
    String path = key;
    if (!path.begins_with("orchestrator/"))
        path = vformat("orchestrator/%s", key);

    bool result = ProjectSettings::get_singleton()->has_setting(path);
    if (!result)
        UtilityFunctions::print("Failed to find key ", path);

    return result;
}

Variant OrchestratorSettings::get_setting(const String& key, const Variant& p_default_value)
{
    String path = key;
    if (!path.begins_with("orchestrator/"))
        path = vformat("orchestrator/%s", key);

    ProjectSettings* ps = ProjectSettings::get_singleton();
    return ps->get_setting(path, p_default_value);
}

PackedStringArray OrchestratorSettings::get_action_favorites()
{
    return ProjectSettings::get_singleton()->get_setting(
        "orchestrator/settings/action_favorites", PackedStringArray());
}

void OrchestratorSettings::add_action_favorite(const String& p_action_name)
{
    ProjectSettings* ps = ProjectSettings::get_singleton();

    const String key = "orchestrator/settings/action_favorites";
    const PropertyInfo pi = PropertyInfo(Variant::PACKED_STRING_ARRAY, key);
    if (!ps->has_setting(key))
    {
        ps->set_setting(key, PackedStringArray());
        ps->set_initial_value(key, PackedStringArray());
        ps->add_property_info(DictionaryUtils::from_property(pi));
        ps->set_as_basic(key, false);
    }

    PackedStringArray actions = get_action_favorites();
    if (!actions.has(p_action_name))
    {
        actions.push_back(p_action_name);
        ps->set_setting(key, actions);
        ps->save();
    }
}

void OrchestratorSettings::remove_action_favorite(const String& p_action_name)
{
    ProjectSettings* ps = ProjectSettings::get_singleton();

    const String key = "orchestrator/settings/action_favorites";
    if (!ps->has_setting(key))
        return;

    PackedStringArray actions = get_action_favorites();
    if (actions.has(p_action_name))
    {
        actions.remove_at(actions.find(p_action_name));
        ps->set_setting(key, actions);
        ps->save();
    }
}

void OrchestratorSettings::_register_deprecated_settings()
{
    // Default settings (Orchestrator v1)
    _removed.emplace_back(FILE_SETTING("run/test_scene", "*.tscn,*.scn", "res://addons/test/test.tscn"));
    _removed.emplace_back(COLOR_NO_ALPHA_SETTING("nodes/colors/background", Color(0.12, 0.15, 0.19)));
    _removed.emplace_back(COLOR_NO_ALPHA_SETTING("nodes/colors/data", Color(0.1686, 0.2824, 0.7882)));
    _removed.emplace_back(COLOR_NO_ALPHA_SETTING("nodes/colors/flow_control", Color(0.2510, 0.4549, 0.2078)));
    _removed.emplace_back(COLOR_NO_ALPHA_SETTING("nodes/colors/logic", Color(0.6784, 0.20, 0.20)));
    _removed.emplace_back(COLOR_NO_ALPHA_SETTING("nodes/colors/terminal", Color(0.2706, 0.3686, 0.4314)));
    _removed.emplace_back(COLOR_NO_ALPHA_SETTING("nodes/colors/utility", Color(0.5765, 0.1686, 0.4275)));
    _removed.emplace_back(COLOR_NO_ALPHA_SETTING("nodes/colors/custom", Color(0.47, 0.27, 0.20)));
}

void OrchestratorSettings::_register_settings()
{
    // Orchestrator v2
    _settings.emplace_back(RESOURCE_SETTING("settings/default_type", "Object", "Node"));
    _settings.emplace_back(SENUM_SETTING("settings/log_level", "FATAL,ERROR,WARN,INFO,DEBUG,TRACE", "INFO"));
    _settings.emplace_back(BOOL_SETTING("settings/save_copy_as_text_resource", false));

    _settings.emplace_back(RANGE_SETTING("settings/runtime/max_call_stack", "256,1024,256", 1024));
    _settings.emplace_back(INT_SETTING("settings/runtime/max_loop_iterations", 1000000));
    _settings.emplace_back(BOOL_SETTING("settings/runtime/tickable", true));

    _settings.emplace_back(BOOL_SETTING("ui/nodes/show_type_icons", true));
    _settings.emplace_back(BOOL_SETTING("ui/nodes/show_conversion_nodes", false));

    // Nodes
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/constants_and_literals", Color(0.271, 0.392, 0.2)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/dialogue", Color(0.318, 0.435, 0.839)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/events", Color(0.467, 0.0, 0.0)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/flow_control", Color(0.132, 0.258, 0.266)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/function_call", Color(0.0, 0.2, 0.396)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/pure_function_call", Color(0.133, 0.302, 0.114)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/function_terminator", Color(0.294, 0.0, 0.506)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/function_result", Color(1.0, 0.65, 0.4)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/math_operations", Color(0.259, 0.408, 0.384)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/properties", Color(.467, 0.28, 0.175)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/resources", Color(0.263, 0.275, 0.359)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/scene", Color(0.208, 0.141, 0.282)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/signals", Color(0.353, 0.0, 0.0)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/variable", Color(0.259, 0.177, 0.249)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/type_cast", Color(0.009, 0.221, 0.203)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/node_colors/comment", Color(0.4, 0.4, 0.4)));

    // Connections
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/execution", Color(1.0, 1.0, 1.0)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/any", Color(0.41, 0.93, 0.74)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/boolean", Color(0.55, 0.65, 0.94)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/integer", Color(0.59, 0.78, 0.94)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/float", Color(0.38, 0.85, 0.96)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/string", Color(0.42, 0.65, 0.93)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/string name", Color(0.42, 0.65, 0.93)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/rect2", Color(0.95, 0.57, 0.65)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/rect2i", Color(0.95, 0.57, 0.65)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/vector2", Color(0.74, 0.57, 0.95)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/vector2i", Color(0.74, 0.57, 0.95)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/vector3", Color(0.84, 0.49, 0.93)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/vector3i", Color(0.84, 0.49, 0.93)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/vector4", Color(0.84, 0.49, 0.94)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/vector4i", Color(0.84, 0.49, 0.94)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/transform2d", Color(0.77, 0.93, 0.41)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/transform3d", Color(0.96, 0.66, 0.43)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/plane", Color(0.97, 0.44, 0.44)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/quaternion", Color(0.93, 0.41, 0.64)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/aabb", Color(0.93, 0.47, 0.57)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/basis", Color(0.89, 0.93, 0.41)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/projection", Color(0.302, 0.655, 0.271)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/color", Color(0.62, 1.00, 0.44)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/nodepath", Color(0.51, 0.58, 0.93)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/rid", Color(0.41, 0.93, 0.60)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/object", Color(0.47, 0.95, 0.91)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/dictionary", Color(0.47, 0.93, 0.69)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/array", Color(0.88, 0.88, 0.88)));
    _settings.emplace_back(COLOR_NO_ALPHA_SETTING("ui/connection_colors/callable", Color(0.47, 0.95, 0.91)));
}

void OrchestratorSettings::_initialize_settings()
{
    _register_deprecated_settings();
    _register_settings();

    // ProjectSettings only writes values that have a value different from its default.
    // So any values that remain set as their default will always be re-added here.
    ProjectSettings* ps = ProjectSettings::get_singleton();
    for (const Setting& setting : _settings)
    {
        const String key = vformat("%s/%s", _get_base_key(), setting.info.name);

        // Adjust the property information name with the qualified key
        PropertyInfo pi = setting.info;
        pi.name = key;

        // If the property does not exist, add it.
        if (!ps->has_setting(key))
            ps->set_setting(key, setting.value);

        // Set these to handle reversion should a user restart the editor and revert a custom
        // setting back to its defaults, since the editor does not persist these details.
        ps->set_initial_value(key, setting.value);
        ps->add_property_info(DictionaryUtils::from_property(pi));
        ps->set_as_basic(key, true);
    }
}

void OrchestratorSettings::_update_default_settings()
{
    ProjectSettings* ps = ProjectSettings::get_singleton();

    // Remove settings that have been deprecated and no longer used.
    for (const Setting& setting : _removed)
    {
        const String key = vformat("%s/%s", _get_base_key(), setting.info.name);
        if (ps->has_setting(key))
            ps->clear(key);
    }
}