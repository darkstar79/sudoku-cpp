// sudoku-cpp - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "toast_notification.h"

#include <imgui.h>

namespace sudoku::view {

void ToastNotification::show(const std::string& message, std::chrono::milliseconds duration) {
    current_toast_ = ToastMessage{.text = message, .show_time = std::chrono::steady_clock::now(), .duration = duration};
}

void ToastNotification::render() {
    if (!current_toast_.has_value()) {
        return;
    }

    // Check expiration
    if (current_toast_->isExpired()) {
        current_toast_.reset();
        return;
    }

    // Render toast at bottom-center of screen
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_size = io.DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(window_size.x * 0.5f, window_size.y * 0.85f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.9f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::Begin("##Toast", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%s", current_toast_->text.c_str());
    }
    ImGui::End();
}

}  // namespace sudoku::view
