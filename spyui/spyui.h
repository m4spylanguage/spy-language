/*
 * Spy Programming Language Engine & Compiler
 * Copyright (C) 2026 Valuvajjala Vivek Vardhan Rao
 *
 * Author: Valuvajjala Vivek Vardhan Rao
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
// SpyUI Framework v1.0 — Modern C++17 UI Framework
// License: MIT
// No Qt. No MOC. No LGPL. Pure modern C++17.

#ifndef SPYUI_H
#define SPYUI_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <regex>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shlwapi.lib")
#elif __APPLE__
#include <Cocoa/Cocoa.h>
#elif __linux__
#include <gtk/gtk.h>
#endif

namespace spy {

// ============================================================
// Core Types
// ============================================================

struct SColor {
    uint8_t r, g, b, a;
    SColor() : r(0), g(0), b(0), a(255) {}
    SColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
    SColor(uint32_t hex) : r((hex >> 16) & 0xFF), g((hex >> 8) & 0xFF), b(hex & 0xFF), a(255) {}
    SColor(const std::string& hex) {
        if (hex.size() >= 7 && hex[0] == '#') {
            r = std::stoi(hex.substr(1, 2), nullptr, 16);
            g = std::stoi(hex.substr(3, 2), nullptr, 16);
            b = std::stoi(hex.substr(5, 2), nullptr, 16);
            a = 255;
        } else { r = g = b = 0; a = 255; }
    }
    uint32_t toUint32() const { return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b; }
    bool operator==(const SColor& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    bool operator!=(const SColor& o) const { return !(*this == o); }
};

struct SPoint {
    int x = 0, y = 0;
    SPoint() = default;
    SPoint(int x, int y) : x(x), y(y) {}
};

struct SRect {
    int x = 0, y = 0, w = 0, h = 0;
    SRect() = default;
    SRect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct SSize {
    int w = 0, h = 0;
    SSize() = default;
    SSize(int w, int h) : w(w), h(h) {}
};

// ============================================================
// SpySignal — signal/callback system (replaces Qt MOC)
// ============================================================

template<typename... Args>
class SpySignal {
public:
    using Callback = std::function<void(Args...)>;
    
    void connect(Callback cb) { callbacks.push_back(std::move(cb)); }
    void connect(void (*fn)(Args...)) { callbacks.push_back(fn); }
    
    void emit(Args... args) {
        for (auto& cb : callbacks) {
            if (cb) cb(args...);
        }
    }
    
    void operator()(Args... args) { emit(args...); }
    bool has_connections() const { return !callbacks.empty(); }
    void disconnect_all() { callbacks.clear(); }

private:
    std::vector<Callback> callbacks;
};

// ============================================================
// SModelIndex — data index (replaces QModelIndex)
// ============================================================

class SModelIndex {
    int row_ = -1;
    int col_ = 0;
    void* internal_ = nullptr;
    int internal_id_ = 0;

public:
    SModelIndex() = default;
    SModelIndex(int row, int col, void* internal, int id)
        : row_(row), col_(col), internal_(internal), internal_id_(id) {}
    
    int row() const { return row_; }
    int column() const { return col_; }
    bool isValid() const { return row_ >= 0; }
    void* internalPointer() const { return internal_; }
    int internalId() const { return internal_id_; }
    
    SModelIndex parent() const;
    SModelIndex child(int row, int col) const;
    
    bool operator==(const SModelIndex& o) const { return row_ == o.row_ && col_ == o.col_ && internal_ == o.internal_; }
    bool operator!=(const SModelIndex& o) const { return !(*this == o); }
};

// ============================================================
// SEvent — event system
// ============================================================

enum class SEventType {
    None, Close, Resize, Paint, MousePress, MouseRelease, MouseMove,
    KeyPress, KeyRelease, Click, DoubleClick, ReturnPressed
};

struct SEvent {
    SEventType type = SEventType::None;
    int x = 0, y = 0;
    int button = 0;
    int key = 0;
    std::string text;
    bool accepted = false;
};

// ============================================================
// SWidget — base widget class
// ============================================================

class SWidget {
protected:
    int x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    bool visible_ = true;
    bool enabled_ = true;
    std::string object_name_;
    SWidget* parent_ = nullptr;
    std::vector<SWidget*> children_;

public:
    SpySignal<> clicked;
    SpySignal<> destroyed;

    virtual ~SWidget() = default;

    void setBounds(int x, int y, int w, int h) { x_ = x; y_ = y; w_ = w; h_ = h; }
    void setPosition(int x, int y) { x_ = x; y_ = y; }
    void setSize(int w, int h) { w_ = w; h_ = h; }
    void setWidth(int w) { w_ = w; }
    void setHeight(int h) { h_ = h; }
    
    int x() const { return x_; }
    int y() const { return y_; }
    int width() const { return w_; }
    int height() const { return h_; }
    SRect rect() const { return SRect(x_, y_, w_, h_); }
    
    bool isVisible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }
    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }
    
    void setObjectName(const std::string& name) { object_name_ = name; }
    const std::string& objectName() const { return object_name_; }
    
    void setParent(SWidget* p) { parent_ = p; }
    SWidget* parent() const { return parent_; }
    
    virtual void addWidget(SWidget* child) {
        child->setParent(this);
        children_.push_back(child);
    }
    
    const std::vector<SWidget*>& children() const { return children_; }
    
    virtual void paint(void* hdc) {
        for (auto* child : children_) {
            if (child->isVisible()) child->paint(hdc);
        }
    }
    virtual void handleEvent(SEvent& e) {
        for (auto* child : children_) {
            if (child->isVisible()) child->handleEvent(e);
        }
    }
    
    SWidget* findChild(const std::string& name) {
        for (auto& child : children_) {
            if (child->objectName() == name) return child;
            auto* found = child->findChild(name);
            if (found) return found;
        }
        return nullptr;
    }
};

// ============================================================
// SLayout — layout base
// ============================================================

class SLayout {
public:
    virtual ~SLayout() = default;
    virtual void layout(int x, int y, int w, int h) = 0;
};

class SHBoxLayout : public SLayout {
    int spacing_ = 5;
    std::vector<SWidget*> widgets_;
public:
    void setSpacing(int s) { spacing_ = s; }
    void addWidget(SWidget* w) { widgets_.push_back(w); }
    
    void layout(int x, int y, int w, int h) override {
        int cx = x;
        for (auto* widget : widgets_) {
            if (!widget->isVisible()) continue;
            widget->setPosition(cx, y);
            cx += widget->width() + spacing_;
        }
    }
};

class SVBoxLayout : public SLayout {
    int spacing_ = 5;
    std::vector<SWidget*> widgets_;
public:
    void setSpacing(int s) { spacing_ = s; }
    void addWidget(SWidget* w) { widgets_.push_back(w); }
    
    void layout(int x, int y, int w, int h) override {
        int cy = y;
        for (auto* widget : widgets_) {
            if (!widget->isVisible()) continue;
            widget->setPosition(x, cy);
            widget->setSize(w, widget->height());
            cy += widget->height() + spacing_;
        }
    }
};

class SFormLayout : public SLayout {
    int spacing_ = 5;
    struct Row { SWidget* label; SWidget* field; };
    std::vector<Row> rows_;
public:
    void setSpacing(int s) { spacing_ = s; }
    void addRow(SWidget* label, SWidget* field) { rows_.push_back({label, field}); }
    
    void layout(int x, int y, int w, int h) override {
        int cy = y;
        int labelW = w / 3;
        for (auto& row : rows_) {
            if (row.label) row.label->setBounds(x, cy, labelW, row.label->height());
            if (row.field) row.field->setBounds(x + labelW + 10, cy, w - labelW - 10, row.field->height());
            cy += 30 + spacing_;
        }
    }
};

// ============================================================
// SToolButton — button widget
// ============================================================

class SToolButton : public SWidget {
    std::string label_;
    bool fixed_size_ = false;

public:
    bool hovered_ = false;

public:
    SpySignal<> clicked_signal;

    SToolButton() = default;
    SToolButton(const std::string& text) : label_(text) {
        w_ = 80;
        h_ = 30;
    }
    
    void setText(const std::string& text) { label_ = text; }
    const std::string& text() const { return label_; }
    
    void setFixedSize(int w, int h) { w_ = w; h_ = h; fixed_size_ = true; }

    void paint(void* hdc) override;
    void handleEvent(SEvent& e) override;
};

// ============================================================
// SInput — text input widget
// ============================================================

class SInput : public SWidget {
    std::string text_;
    std::string placeholder_;
    bool focused_ = false;

public:
    SpySignal<std::string> return_pressed;
    SpySignal<std::string> text_changed;

    SInput() = default;
    SInput(const std::string& placeholder) : placeholder_(placeholder) {
        w_ = 200;
        h_ = 30;
    }
    
    void setText(const std::string& text) { text_ = text; text_changed(text_); }
    const std::string& text() const { return text_; }
    void setPlaceholder(const std::string& p) { placeholder_ = p; }
    
    void paint(void* hdc) override;
    void handleEvent(SEvent& e) override;
};

// ============================================================
// SLabel — label widget
// ============================================================

class SLabel : public SWidget {
    std::string text_;
    SColor color_;

public:
    SLabel() = default;
    SLabel(const std::string& text) : text_(text), color_(190, 190, 190) {
        w_ = text.size() * 8 + 10;
        h_ = 20;
    }
    
    void setText(const std::string& text) { text_ = text; }
    const std::string& text() const { return text_; }
    void setColor(const SColor& c) { color_ = c; }
    
    void paint(void* hdc) override;
};

// ============================================================
// SFrame — container widget
// ============================================================

class SFrame : public SWidget {
public:
    SFrame() = default;
    SFrame(int w, int h) : SWidget() { w_ = w; h_ = h; }
    void paint(void* hdc) override;
};

// ============================================================
// SListView — list/icon view
// ============================================================

class SListView;

class SModel {
public:
    virtual ~SModel() = default;
    virtual int rowCount(const SModelIndex& parent = SModelIndex()) const = 0;
    virtual int columnCount(const SModelIndex& parent = SModelIndex()) const = 0;
    virtual SModelIndex index(int row, int col, const SModelIndex& parent = SModelIndex()) const = 0;
    virtual SModelIndex parent(const SModelIndex& child) const = 0;
    virtual std::string data(const SModelIndex& index, int role = 0) const = 0;
    virtual bool isDir(const SModelIndex& index) const { return false; }
    virtual std::string filePath(const SModelIndex& index) const { return ""; }
};

class SFileSystemModel : public SModel {
public:
    struct FileEntry {
        std::string name;
        std::string full_path;
        bool is_directory;
        int64_t size;
        std::vector<int> children_indices;
    };
    
    std::vector<FileEntry> entries_;
    std::string root_path_;
    std::unordered_map<int, int> parent_map_;
    int next_id_ = 0;

public:
    void setRootPath(const std::string& path);
    void refresh();
    
    int rowCount(const SModelIndex& parent = SModelIndex()) const override;
    int columnCount(const SModelIndex& parent = SModelIndex()) const override { return 1; }
    SModelIndex index(int row, int col, const SModelIndex& parent = SModelIndex()) const override;
    SModelIndex parent(const SModelIndex& child) const override;
    std::string data(const SModelIndex& index, int role = 0) const override;
    bool isDir(const SModelIndex& index) const override;
    std::string filePath(const SModelIndex& index) const override;
    std::string rootPath() const { return root_path_; }
};

class SDelegate {
public:
    virtual ~SDelegate() = default;
    virtual void paint(void* painter, const SRect& rect, const std::string& text, bool is_dir, bool is_selected) = 0;
    virtual SSize sizeHint() const { return SSize(110, 110); }
};

class SListView : public SWidget {
    SModel* model_ = nullptr;
    SDelegate* delegate_ = nullptr;
    int selected_ = -1;
    int scroll_offset_ = 0;
    int icon_size_ = 48;
    int grid_size_ = 110;
    int spacing_ = 10;
    std::string view_mode_ = "icon";

public:
    SpySignal<SModelIndex> clicked;
    SpySignal<SModelIndex> double_clicked;

    void setModel(SModel* m) { model_ = m; }
    SModel* model() const { return model_; }
    void setDelegate(SDelegate* d) { delegate_ = d; }
    void setIconMode(int icon_sz, int grid_sz, int sp) { icon_size_ = icon_sz; grid_size_ = grid_sz; spacing_ = sp; }
    void setViewMode(const std::string& mode) { view_mode_ = mode; }
    
    SModelIndex rootIndex() const { return root_index_; }
    void setRootIndex(const SModelIndex& idx) { root_index_ = idx; }
    
    int selectedRow() const { return selected_; }
    
    void paint(void* hdc) override;
    void handleEvent(SEvent& e) override;

private:
    SModelIndex root_index_;
};

// ============================================================
// STabs — tab widget
// ============================================================

class STabs : public SWidget {
    struct Tab { std::string title; SWidget* widget; bool closable = true; };
    std::vector<Tab> tabs_;
    int current_ = 0;

public:
    SpySignal<int> tab_changed;

    void addTab(SWidget* widget, const std::string& title) {
        tabs_.push_back({title, widget, true});
        widget->setParent(this);
    }
    
    void removeTab(int index) {
        if (index >= 0 && index < (int)tabs_.size()) {
            tabs_.erase(tabs_.begin() + index);
            if (current_ >= (int)tabs_.size()) current_ = tabs_.size() - 1;
        }
    }
    
    void setCurrentIndex(int idx) { current_ = idx; }
    int currentIndex() const { return current_; }
    void setTabText(int idx, const std::string& text) {
        if (idx >= 0 && idx < (int)tabs_.size()) tabs_[idx].title = text;
    }
    void setClosable(bool c) { /* global closable */ }
    
    int tabCount() const { return tabs_.size(); }
    const std::string& tabTitle(int idx) const { return tabs_[idx].title; }
    
    SWidget* currentWidget() const {
        if (current_ >= 0 && current_ < (int)tabs_.size())
            return tabs_[current_].widget;
        return nullptr;
    }
    
    void paint(void* hdc) override;
    void handleEvent(SEvent& e) override;
};

// ============================================================
// SSplitter — splitter widget
// ============================================================

class SSplitter : public SWidget {
    std::vector<SWidget*> widgets_;
    std::vector<int> sizes_;
    int handle_width_ = 1;
    bool horizontal_ = true;

public:
    static constexpr int HORIZONTAL = 1;
    static constexpr int VERTICAL = 0;

    SSplitter(int orientation = HORIZONTAL) : horizontal_(orientation == HORIZONTAL) {}
    
    void addWidget(SWidget* w) override { 
        widgets_.push_back(w);
        SWidget::addWidget(w);
    }
    void setHandleWidth(int w) { handle_width_ = w; }
    void setStretchFactor(int index, int stretch) { /* simplified */ }
    
    void layout_split(int x, int y, int w, int h);
    void paint(void* hdc) override;
    void handleEvent(SEvent& e) override;
};

// ============================================================
// SPainter — drawing utilities
// ============================================================

class SPainter {
public:
    static void drawRect(void* dc, int x, int y, int w, int h, const SColor& fill, const SColor& border = SColor());
    static void drawText(void* dc, int x, int y, const std::string& text, const SColor& color, int font_size = 12);
    static void fillRect(void* dc, int x, int y, int w, int h, const SColor& color);
};

// ============================================================
// SpyProcess — process launcher
// ============================================================

class SpyProcess {
public:
    static bool startDetached(const std::string& program, const std::vector<std::string>& args);
    static std::string run(const std::string& command);
};

// ============================================================
// SpyPaths — standard paths
// ============================================================

class SpyPaths {
public:
    static std::string home();
    static std::string desktop();
    static std::string downloads();
    static std::string documents();
    static std::string temp();
};

// ============================================================
// SpyStorage — storage info
// ============================================================

class SpyStorage {
public:
    struct VolumeInfo {
        std::string name;
        std::string root_path;
        int64_t total_bytes;
        int64_t free_bytes;
    };
    static std::vector<VolumeInfo> mountedVolumes();
};

// ============================================================
// SpyDir — directory operations
// ============================================================

class SpyDir {
public:
    static std::string dirName(const std::string& path);
    static std::string homePath();
    static bool exists(const std::string& path);
    static bool isDir(const std::string& path);
    static std::vector<std::string> entryList(const std::string& path);
};

// ============================================================
// SpyStyle — CSS-like styling
// ============================================================

class SpyStyle {
public:
    static void apply(const std::string& css);
    static void setDarkNeon();
};

// ============================================================
// SMenu — forward declare before SWindow
// ============================================================

class SMenu {
public:
    struct MenuItem {
        std::string label;
        std::function<void()> action;
    };
    
    void add(const std::string& label, const std::vector<MenuItem>& items = {});
    const std::string& title() const { return title_; }
    void setTitle(const std::string& t) { title_ = t; }
    const std::vector<MenuItem>& items() const { return items_; }
    
private:
    std::string title_;
    std::vector<MenuItem> items_;
};

// ============================================================
// SWindow — main window
// ============================================================

class SWindow : public SWidget {
    std::string title_;
    SWidget* central_widget_ = nullptr;
    SLayout* layout_ = nullptr;
    SMenu* menu_bar_ = nullptr;
    bool running_ = false;
#ifdef _WIN32
    HWND hwnd_ = NULL;
#endif

public:
    SpySignal<> close_signal;
    SpySignal<int, int> resize_signal;

    SWindow() = default;
    SWindow(const std::string& title, int w, int h);
    virtual ~SWindow() = default;
    
    void setTitle(const std::string& t) { title_ = t; }
    const std::string& title() const { return title_; }
    
    void setCentral(SWidget* w) { central_widget_ = w; }
    SWidget* central() const { return central_widget_; }
    
    void setLayout(SLayout* l);
    void setMenu(SMenu* m) { menu_bar_ = m; }
    
    void show();
    void close();
    void run();
    
#ifdef _WIN32
    HWND hwnd() const { return hwnd_; }
#endif
    
    void paint(void* hdc) override;
    void handleEvent(SEvent& e) override;
};

// ============================================================
// SpyApp — application class
// ============================================================

class SpyApp {
    std::vector<SWindow*> windows_;
    bool running_ = false;

public:
    SpyApp() = default;
    
    void addWindow(SWindow* w) { windows_.push_back(w); }
    int exec();
    void quit() { running_ = false; }
};

} // namespace spy

// ============================================================
// C API for Spy extern FFI
// ============================================================

extern "C" {
    void* spyui_create_window(int width, int height, const char* title);
    void spyui_show_window(void* window);
    void spyui_close_window(void* window);
    void spyui_set_title(void* window, const char* title);
    void spyui_set_size(void* window, int w, int h);
    
    void* spyui_create_button(const char* text);
    void spyui_button_set_text(void* btn, const char* text);
    void spyui_button_set_size(void* btn, int w, int h);
    
    void* spyui_create_input(const char* placeholder);
    const char* spyui_input_text(void* input);
    void spyui_input_set_text(void* input, const char* text);
    
    void* spyui_create_label(const char* text);
    void spyui_label_set_text(void* label, const char* text);
    
    void* spyui_create_tabs();
    void spyui_tabs_add(void* tabs, void* widget, const char* title);
    void* spyui_tabs_current(void* tabs);
    void spyui_tabs_set_tab_text(void* tabs, int idx, const char* text);
    
    void* spyui_create_fsmodel();
    void spyui_fsmodel_set_root(void* model, const char* path);
    const char* spyui_fsmodel_file_path(void* model, void* index);
    int spyui_fsmodel_is_dir(void* model, void* index);
    
    void* spyui_create_listview();
    void spyui_listview_set_model(void* view, void* model);
    void spyui_listview_set_root_index(void* view, void* index);
    
    void* spyui_create_splitter(int orientation);
    void spyui_splitter_add(void* splitter, void* widget);
    
    void* spyui_create_vbox();
    void* spyui_create_hbox();
    void spyui_layout_add(void* layout, void* widget);
    
    void spyui_window_set_layout(void* window, void* layout);
    void spyui_window_set_central(void* window, void* widget);
    void spyui_add_to_parent(void* parent, void* child);
    
    const char* spyui_get_cwd();
    const char* spyui_get_home();
    const char* spyui_get_desktop();
    const char* spyui_get_downloads();
    const char* spyui_get_documents();
    
    int spyui_process_start(const char* program, const char* args_json);
    
    double spyui_file_exists(const char* path);
    double spyui_file_size(const char* path);
    double spyui_mkdir(const char* path);
    double spyui_remove(const char* path);
    double spyui_rename(const char* old_path, const char* new_path);
    const char* spyui_read_file(const char* path);
    double spyui_write_file(const char* path, const char* data);
    
    void spyui_style_dark_neon();
    
    void spyui_app_run();
}

#endif // SPYUI_H
