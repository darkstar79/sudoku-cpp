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

#pragma once
#include <chrono>
#include <optional>
#include <string>

namespace sudoku::view {

struct ToastMessage {
    std::string text;
    std::chrono::steady_clock::time_point show_time;
    std::chrono::milliseconds duration;

    [[nodiscard]] bool isExpired() const {
        auto now = std::chrono::steady_clock::now();
        return (now - show_time) >= duration;
    }
};

class ToastNotification {
public:
    void show(const std::string& message, std::chrono::milliseconds duration = std::chrono::milliseconds(3000));
    void render();  // Called from MainWindow::render()
    [[nodiscard]] bool isVisible() const {
        return current_toast_.has_value();
    }

private:
    std::optional<ToastMessage> current_toast_;
};

}  // namespace sudoku::view
