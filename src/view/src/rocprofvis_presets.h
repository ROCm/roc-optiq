// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "json.h"
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class NotificationManager;
class PresetComponent;

class PresetManager
{
public:
    enum ComponentType
    {
        ComputeComparison,
        ComputeTableView,
        ComputeKernelMetricTable,
        NumComponentTypes,
    };

    enum Result
    {
        Success,
        Error,
        ErrorInvalidArgument,
        ErrorOverwrite,
        ErrorPresetEmpty,
    };

    PresetManager(const PresetManager&)            = delete;
    PresetManager& operator=(const PresetManager&) = delete;

    /*
     * Returns the singleton PresetManager instance.
     */
    static PresetManager& GetInstance();

    /*
     * Registers a PresetComponent under a project id so that it participates in
     * subsequent Save/Load/Reset operations for that project.
     * @param project_id: The id of the project the component belongs to.
     * @param component: The component to register.
     */
    void RegisterComponent(const std::string& project_id, PresetComponent& component);
    /*
     * Removes a single registered component from a project.
     * @param project_id: The id of the project the component belongs to.
     * @param component: The component to unregister.
     */
    void UnregisterComponent(const std::string& project_id, PresetComponent& component);
    /*
     * Removes every component registered under the given project id.
     * @param project_id: The id of the project whose components should be removed.
     */
    void UnregisterComponents(const std::string& project_id);
    /*
     * Invokes Reset() on every component registered under the given project id.
     * @param project_id: The id of the project whose components should be reset.
     */
    void ResetComponents(const std::string& project_id);

    /*
     * Fills output with the names of all presets currently stored on disk.
     * @param output: Destination vector that receives the preset names.
     */
    void ListPresets(std::vector<std::string>& output /*filter?*/);
    /*
     * Signal components registered under project_id to serialize their state into a
     * preset and writes the preset file to disk. Returns Error/Success.
     * @param project_id: The id of the project whose components should be saved.
     * @param preset_name: The name to save the preset under.
     * @param overwrite: True if preset_name must already exist, false if it must not.
     */
    Result SavePreset(const std::string& project_id, const std::string& preset_name,
                      bool overwrite);
    /*
     * Signal components registered under project_id to deserialize their state from a
     * preset Returns Error/Success.
     * @param project_id: The id of the project whose components should be loaded.
     * @param preset_name: The name of the preset to load.
     */
    Result LoadPreset(const std::string& project_id, const std::string& preset_name);
    /*
     * Removes the named preset from the preset file and writes the file to disk.
     * Returns Error/Success.
     * @param preset_name: The name of the preset to delete.
     */
    Result DeletePreset(const std::string& preset_name);

private:
    PresetManager();
    ~PresetManager() = default;

    /*
     * Write preset into the on-disk preset file.
     * Returns true if the file was written successfully.
     */
    bool WritePresetFile();

    std::unordered_map<std::string, std::unordered_map<ComponentType, PresetComponent*>>
                          m_components;
    std::filesystem::path m_presets_json_path;
    jt::Json              m_presets_json;
};

class PresetComponent
{
public:
    PresetComponent(PresetManager::ComponentType type, const std::string& project_id);
    virtual ~PresetComponent();

    /*
     * Returns the component category this instance was constructed with.
     */
    PresetManager::ComponentType Type() const;

    /*
     * Writes the component's current state into json. An empty output is valid and
     * signals to SavePreset that the component has no data to persist.
     * Returns false on failure.
     * @param json: Destination json payload the component writes its state into.
     */
    virtual bool ToJson(jt::Json& json) = 0;
    /*
     * Restores the component's state from json.
     * Returns false if the payload is invalid.
     * @param json: Source json payload the component reads its state from.
     */
    virtual bool FromJson(jt::Json& json) = 0;
    /*
     * Restores the component to its default state, discarding any runtime
     * customizations.
     */
    virtual void Reset() = 0;

private:
    PresetManager::ComponentType m_type;
    std::string                  m_project_id;
};

class PresetBrowser
{
public:
    PresetBrowser();
    ~PresetBrowser() = default;

    /*
     * Refreshes the cached preset list when it has been marked stale.
     */
    void Update();
    /*
     * Draws the preset browser popup.
     */
    void Render();

    /*
     * Requests the popup to open on the next Render().
     */
    void Show();
    /*
     * Sets the screen-space anchor position the popup will open at.
     * @param pos_x: The horizontal anchor position in screen pixels.
     * @param pos_y: The vertical anchor position in screen pixels.
     */
    void SetPosition(float pos_x, float pos_y);

private:
    std::vector<std::string> m_presets_list;

    bool m_should_open;
    bool m_presets_list_changed;

    float m_pos_x;
    float m_pos_y;
    char  m_text_input[256];

    PresetManager&       m_presets;
    NotificationManager& m_notifications;
    SettingsManager&     m_settings;
};

}  // namespace View
}  // namespace RocProfVis
