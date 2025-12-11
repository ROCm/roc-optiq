// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class EditableTextField
{
public:
    EditableTextField(std::string id);
    ~EditableTextField() = default;
    void        SetText(std::string text, std::string tooltip = "",
                        std::string reset_tooltip = "");
    void        Render();
    float       ButtonSize() const;
    void        RevertToDefault();
    void        ShowResetButton(bool is_button_shown);
    void        SetOnTextCommit(const std::function<void(const std::string&)>& cb);

private:
    void DrawPlainText();
    void DrawEditingText();
    void AcceptEdit();

    bool        m_editing_mode = false;
    bool        m_request_keyboard_focus = false;
    bool        m_show_reset_button = false;
    std::string m_id;
    std::string m_text;
    std::string m_reset_tooltip;
    std::string m_tooltip_text;    
    std::string m_edit_buf;
    std::function<void(const std::string&)> m_on_text_commit;
};

}  // namespace View
}  // namespace RocProfVis
