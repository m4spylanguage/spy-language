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

#include "spyui.h"
#include <sstream>
#include <fstream>
#include <shlobj.h>
#include <array>

namespace spy {

// ============================================================
// SpyProcess
// ============================================================

bool SpyProcess::startDetached(const std::string& program, const std::vector<std::string>& args) {
#ifdef _WIN32
    std::string cmd = "\"" + program + "\"";
    for (const auto& a : args) cmd += " \"" + a + "\"";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    return CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, FALSE,
                          CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
#else
    pid_t pid = fork();
    if (pid == 0) {
        std::vector<const char*> argv;
        argv.push_back(program.c_str());
        for (const auto& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);
        execvp(program.c_str(), const_cast<char**>(argv.data()));
        _exit(1);
    }
    return pid > 0;
#endif
}

std::string SpyProcess::run(const std::string& command) {
    std::array<char, 4096> buffer{};
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    _pclose(pipe);
#else
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
#endif
    return result;
}

// ============================================================
// SpyPaths
// ============================================================

std::string SpyPaths::home() {
#ifdef _WIN32
    char path[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path);
    return path;
#else
    const char* h = getenv("HOME");
    return h ? h : "/tmp";
#endif
}

std::string SpyPaths::desktop() {
#ifdef _WIN32
    char path[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, 0, path);
    return path;
#else
    return home() + "/Desktop";
#endif
}

std::string SpyPaths::downloads() { return home() + "\\Downloads"; }
std::string SpyPaths::documents() { return home() + "\\Documents"; }

std::string SpyPaths::temp() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    return path;
#else
    return "/tmp";
#endif
}

// ============================================================
// SpyStorage
// ============================================================

std::vector<SpyStorage::VolumeInfo> SpyStorage::mountedVolumes() {
    std::vector<VolumeInfo> result;
#ifdef _WIN32
    char drives[256] = {};
    GetLogicalDriveStringsA(255, drives);
    for (char* d = drives; *d; d += strlen(d) + 1) {
        VolumeInfo vol;
        vol.root_path = d;
        ULARGE_INTEGER free_bytes{}, total_bytes{};
        if (GetDiskFreeSpaceExA(d, &free_bytes, &total_bytes, nullptr)) {
            vol.free_bytes = static_cast<int64_t>(free_bytes.QuadPart);
            vol.total_bytes = static_cast<int64_t>(total_bytes.QuadPart);
        }
        result.push_back(vol);
    }
#endif
    return result;
}

// ============================================================
// SpyDir
// ============================================================

std::string SpyDir::dirName(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
}

std::string SpyDir::homePath() { return SpyPaths::home(); }

bool SpyDir::exists(const std::string& path) {
    return std::filesystem::exists(path);
}

bool SpyDir::isDir(const std::string& path) {
    return std::filesystem::is_directory(path);
}

std::vector<std::string> SpyDir::entryList(const std::string& path) {
    std::vector<std::string> result;
    try {
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            result.push_back(entry.path().filename().string());
        }
    } catch (...) {}
    std::sort(result.begin(), result.end());
    return result;
}

// ============================================================
// SFileSystemModel
// ============================================================

void SFileSystemModel::setRootPath(const std::string& path) {
    root_path_ = path;
    refresh();
}

void SFileSystemModel::refresh() {
    entries_.clear();
    parent_map_.clear();
    next_id_ = 0;

    FileEntry root_entry;
    root_entry.name = "";
    root_entry.full_path = root_path_;
    root_entry.is_directory = true;
    root_entry.size = 0;
    int root_id = next_id_++;
    entries_.push_back(root_entry);

    try {
        for (auto& entry : std::filesystem::directory_iterator(root_path_)) {
            int child_id = next_id_++;
            FileEntry fe;
            fe.name = entry.path().filename().string();
            fe.full_path = entry.path().string();
            fe.is_directory = entry.is_directory();
            fe.size = fe.is_directory ? 0 : static_cast<int64_t>(entry.file_size());

            parent_map_[child_id] = root_id;
            entries_.push_back(fe);
            entries_[root_id].children_indices.push_back(child_id);
        }
    } catch (...) {}

    std::sort(entries_[root_id].children_indices.begin(), entries_[root_id].children_indices.end(),
              [this](int a, int b) { return entries_[a].name < entries_[b].name; });
}

int SFileSystemModel::rowCount(const SModelIndex& parent) const {
    if (!parent.isValid()) return entries_.empty() ? 0 : static_cast<int>(entries_[0].children_indices.size());
    int id = parent.internalId();
    if (id >= 0 && id < static_cast<int>(entries_.size())) {
        return static_cast<int>(entries_[id].children_indices.size());
    }
    return 0;
}

SModelIndex SFileSystemModel::index(int row, int col, const SModelIndex& parent) const {
    int parent_id = parent.isValid() ? parent.internalId() : 0;
    if (parent_id >= 0 && parent_id < static_cast<int>(entries_.size())) {
        const auto& children = entries_[parent_id].children_indices;
        if (row >= 0 && row < static_cast<int>(children.size())) {
            int cid = children[row];
            return SModelIndex(row, col, reinterpret_cast<void*>(static_cast<intptr_t>(cid)), cid);
        }
    }
    return SModelIndex();
}

SModelIndex SFileSystemModel::parent(const SModelIndex& child) const {
    int child_id = child.internalId();
    auto it = parent_map_.find(child_id);
    if (it != parent_map_.end()) {
        int pid = it->second;
        return SModelIndex(0, 0, reinterpret_cast<void*>(static_cast<intptr_t>(pid)), pid);
    }
    return SModelIndex();
}

std::string SFileSystemModel::data(const SModelIndex& index, int role) const {
    int id = index.internalId();
    if (id >= 0 && id < static_cast<int>(entries_.size())) {
        if (role == 0) return entries_[id].name;
    }
    return "";
}

bool SFileSystemModel::isDir(const SModelIndex& index) const {
    int id = index.internalId();
    if (id >= 0 && id < static_cast<int>(entries_.size())) return entries_[id].is_directory;
    return false;
}

std::string SFileSystemModel::filePath(const SModelIndex& index) const {
    int id = index.internalId();
    if (id >= 0 && id < static_cast<int>(entries_.size())) return entries_[id].full_path;
    return "";
}

// ============================================================
// SModelIndex
// ============================================================

SModelIndex SModelIndex::parent() const { return SModelIndex(); }
SModelIndex SModelIndex::child(int, int) const { return SModelIndex(); }

// ============================================================
// SToolButton
// ============================================================

void SToolButton::paint(void* context) {
    if (!context) return;
    HDC dc = (HDC)context;
    HPEN border = CreatePen(PS_SOLID, 1, hovered_ ? RGB(24, 255, 211) : RGB(45, 45, 45));
    HBRUSH bg = CreateSolidBrush(hovered_ ? RGB(28, 28, 28) : RGB(20, 20, 20));
    HPEN oldPen = (HPEN)SelectObject(dc, border);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, bg);
    RoundRect(dc, x_, y_, x_ + w_, y_ + h_, 6, 6);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(bg);
    DeleteObject(border);
    SetTextColor(dc, hovered_ ? RGB(24, 255, 211) : RGB(200, 200, 200));
    SetBkMode(dc, TRANSPARENT);
    RECT r = { x_, y_, x_ + w_, y_ + h_ };
    DrawTextA(dc, label_.c_str(), (int)label_.size(), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void SToolButton::handleEvent(SEvent& e) {
    if (e.type == SEventType::Click) {
        clicked_signal();
    }
    SWidget::handleEvent(e);
}

// ============================================================
// SInput
// ============================================================

void SInput::paint(void* context) {
    if (!context) return;
    HDC dc = (HDC)context;
    HPEN border = CreatePen(PS_SOLID, 1, focused_ ? RGB(24, 255, 211) : RGB(40, 40, 40));
    HBRUSH bg = CreateSolidBrush(RGB(12, 12, 12));
    HPEN oldPen = (HPEN)SelectObject(dc, border);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, bg);
    RoundRect(dc, x_, y_, x_ + w_, y_ + h_, 6, 6);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(bg);
    DeleteObject(border);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, text_.empty() ? RGB(80, 80, 80) : RGB(220, 220, 220));
    RECT r = { x_ + 12, y_, x_ + w_ - 12, y_ + h_ };
    DrawTextA(dc, text_.empty() ? placeholder_.c_str() : text_.c_str(),
              (int)(text_.empty() ? placeholder_.size() : text_.size()),
              &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void SInput::handleEvent(SEvent& e) {
    if (e.type == SEventType::Click) {
        focused_ = true;
    }
    if (e.type == SEventType::KeyPress && e.key == 13) {
        return_pressed(text_);
    }
    if (focused_ && e.type == SEventType::KeyPress) {
        if (e.key == 8) {
            if (!text_.empty()) text_.pop_back();
        } else if (e.key >= 32 && e.key <= 126) {
            text_ += (char)e.key;
        }
        text_changed(text_);
    }
    SWidget::handleEvent(e);
}

// ============================================================
// SLabel
// ============================================================

void SLabel::paint(void* context) {
    if (!context) return;
    HDC dc = (HDC)context;
    SetTextColor(dc, RGB(color_.r, color_.g, color_.b));
    SetBkMode(dc, TRANSPARENT);
    RECT r = { x_, y_, x_ + w_, y_ + h_ };
    DrawTextA(dc, text_.c_str(), (int)text_.size(), &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

// ============================================================
// SFrame
// ============================================================

void SFrame::paint(void* context) {
    if (!context) return;
    HDC dc = (HDC)context;
    HBRUSH bg = CreateSolidBrush(RGB(14, 14, 14));
    RECT r = { x_, y_, x_ + w_, y_ + h_ };
    FillRect(dc, &r, bg);
    DeleteObject(bg);
    SWidget::paint(context);
}

// ============================================================
// SListView
// ============================================================

void SListView::paint(void* context) {
    if (!context || !model_) return;
    HDC dc = (HDC)context;
    int total = model_->rowCount(root_index_);
    int cols = (w_ / grid_size_);
    if (cols < 1) cols = 1;
    int rows = (total + cols - 1) / cols;

    for (int i = 0; i < total; ++i) {
        SModelIndex idx = model_->index(i, 0, root_index_);
        std::string name = model_->data(idx);
        bool dir = model_->isDir(idx);
        int col = i % cols;
        int row = i / cols;
        int ix = x_ + col * grid_size_;
        int iy = y_ + row * grid_size_ + scroll_offset_;

        bool sel = (i == selected_);
        if (sel) {
            HBRUSH sbg = CreateSolidBrush(RGB(24, 24, 24));
            RECT sr = { ix, iy, ix + grid_size_, iy + grid_size_ };
            FillRect(dc, &sr, sbg);
            DeleteObject(sbg);
            HPEN spen = CreatePen(PS_SOLID, 1, RGB(24, 255, 211));
            HPEN sop = (HPEN)SelectObject(dc, spen);
            HBRUSH sob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, ix, iy, ix + grid_size_, iy + grid_size_);
            SelectObject(dc, sob);
            SelectObject(dc, sop);
            DeleteObject(spen);
        }

        COLORREF ic = dir ? RGB(24, 255, 211) : RGB(190, 190, 190);
        HBRUSH ib = CreateSolidBrush(ic);
        int is = icon_size_ / 2;
        int cx = ix + grid_size_ / 2;
        int cy = iy + 20;
        RECT ir = { cx - is, cy - is, cx + is, cy + is };
        FillRect(dc, &ir, ib);
        DeleteObject(ib);

        SetTextColor(dc, sel ? RGB(24, 255, 211) : RGB(190, 190, 190));
        SetBkMode(dc, TRANSPARENT);
        RECT tr = { ix + 5, iy + 40, ix + grid_size_ - 5, iy + grid_size_ };
        DrawTextA(dc, name.c_str(), (int)name.size(), &tr, DT_CENTER | DT_WORDBREAK);
    }
}

void SListView::handleEvent(SEvent& e) {
    if (e.type == SEventType::Click && model_) {
        int col = (e.x - x_) / grid_size_;
        int row = (e.y - y_ - scroll_offset_) / grid_size_;
        int ipr = w_ / grid_size_;
        if (ipr < 1) ipr = 1;
        int item = row * ipr + col;
        if (item >= 0 && item < model_->rowCount(root_index_)) {
            selected_ = item;
            SModelIndex idx = model_->index(item, 0, root_index_);
            clicked(idx);
        }
    }
    if (e.type == SEventType::DoubleClick && model_) {
        if (selected_ >= 0 && selected_ < model_->rowCount(root_index_)) {
            SModelIndex idx = model_->index(selected_, 0, root_index_);
            double_clicked(idx);
        }
    }
    SWidget::handleEvent(e);
}

// ============================================================
// STabs
// ============================================================

void STabs::paint(void* context) {
    if (!context) return;
    HDC dc = (HDC)context;
    int tab_h = 35;
    int cx = x_;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        bool sel = ((int)i == current_);
        HBRUSH bg = CreateSolidBrush(sel ? RGB(18, 18, 18) : RGB(22, 22, 22));
        RECT r = { cx, y_, cx + 120, y_ + tab_h };
        FillRect(dc, &r, bg);
        DeleteObject(bg);
        if (sel) {
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(24, 255, 211));
            HPEN op = (HPEN)SelectObject(dc, pen);
            MoveToEx(dc, cx, y_ + tab_h - 2, NULL);
            LineTo(dc, cx + 120, y_ + tab_h - 2);
            SelectObject(dc, op);
            DeleteObject(pen);
        }
        SetTextColor(dc, sel ? RGB(24, 255, 211) : RGB(102, 102, 102));
        SetBkMode(dc, TRANSPARENT);
        RECT tr = { cx, y_, cx + 120, y_ + tab_h };
        DrawTextA(dc, tabs_[i].title.c_str(), (int)tabs_[i].title.size(), &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        cx += 120;
    }
    if (current_ >= 0 && current_ < (int)tabs_.size() && tabs_[current_].widget) {
        tabs_[current_].widget->paint(context);
    }
}

void STabs::handleEvent(SEvent& e) {
    if (e.type == SEventType::Click) {
        int tab = (e.x - x_) / 120;
        if (tab >= 0 && tab < (int)tabs_.size()) {
            current_ = tab;
            tab_changed(tab);
        }
    }
    SWidget::handleEvent(e);
}

// ============================================================
// SSplitter
// ============================================================

void SSplitter::layout_split(int x, int y, int w, int h) {
    if (widgets_.empty()) return;
    int total_stretch = static_cast<int>(widgets_.size());
    int cy = y;
    for (size_t i = 0; i < widgets_.size(); ++i) {
        int stretch = (i < sizes_.size()) ? sizes_[i] : 1;
        int wh = (h * stretch) / total_stretch;
        widgets_[i]->setBounds(x, cy, w, wh);
        cy += wh;
    }
}

void SSplitter::paint(void* context) {
    if (!context) return;
    HDC dc = (HDC)context;
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(21, 21, 21));
    HPEN op = (HPEN)SelectObject(dc, pen);
    MoveToEx(dc, x_, y_, NULL);
    LineTo(dc, x_, y_ + h_);
    SelectObject(dc, op);
    DeleteObject(pen);
    for (auto* w : widgets_) {
        if (w->isVisible()) w->paint(context);
    }
}

void SSplitter::handleEvent(SEvent& e) {
    SWidget::handleEvent(e);
}

// ============================================================
// SPainter
// ============================================================

void SPainter::drawRect(void*, int, int, int, int, const SColor&, const SColor&) {}
void SPainter::drawText(void*, int, int, const std::string&, const SColor&, int) {}
void SPainter::fillRect(void*, int, int, int, int, const SColor&) {}

// ============================================================
// SpyStyle
// ============================================================

void SpyStyle::apply(const std::string&) {}
void SpyStyle::setDarkNeon() {}

// ============================================================
// SMenu
// ============================================================

void SMenu::add(const std::string& label, const std::vector<MenuItem>& items) {
    title_ = label;
    items_ = items;
}

// ============================================================
// SWindow
// ============================================================

SWindow::SWindow(const std::string& title, int w, int h) : title_(title) {
    w_ = w;
    h_ = h;
}

void SWindow::setLayout(SLayout* l) {
    layout_ = l;
}

void SWindow::paint(void* context) {
    if (!context) return;
    HDC dc = (HDC)context;
    HBRUSH bg = CreateSolidBrush(RGB(18, 18, 18));
    RECT r = { x_, y_, x_ + w_, y_ + h_ };
    FillRect(dc, &r, bg);
    DeleteObject(bg);
    for (auto* child : children_) {
        if (child->isVisible()) {
            child->paint(context);
        }
    }
}

void SWindow::handleEvent(SEvent& e) {
    SWidget::handleEvent(e);
}

// ============================================================
// Win32 Implementation
// ============================================================

#ifdef _WIN32

static std::unordered_map<HWND, SWindow*> g_hwnd_map;
static std::unordered_map<HWND, SWidget*> g_hovered_widget;

static SWidget* hitTestWidget(SWidget* parent, int x, int y) {
    for (auto* child : parent->children()) {
        if (!child->isVisible()) continue;
        if (x >= child->x() && x <= child->x() + child->width() &&
            y >= child->y() && y <= child->y() + child->height()) {
            SWidget* inner = hitTestWidget(child, x, y);
            return inner ? inner : child;
        }
    }
    return nullptr;
}

static void renderWidgetTree(SWindow* win) {
    if (!win->hwnd()) return;
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(win->hwnd(), &ps);
    win->paint(dc);
    EndPaint(win->hwnd(), &ps);
}

static void invalidateWidget(HWND hwnd, SWidget* w) {
    if (!w) return;
    RECT r = { w->x(), w->y(), w->x() + w->width(), w->y() + w->height() };
    InvalidateRect(hwnd, &r, FALSE);
}

static void paintDoubleBuffered(HWND hwnd, SWindow* win) {
    RECT cr;
    GetClientRect(hwnd, &cr);
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bm = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
    HBITMAP old = (HBITMAP)SelectObject(mem, bm);
    win->paint(mem);
    BitBlt(dc, 0, 0, cr.right, cr.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bm);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto it = g_hwnd_map.find(hwnd);
    if (it == g_hwnd_map.end()) return DefWindowProcA(hwnd, msg, wParam, lParam);
    SWindow* win = it->second;

    switch (msg) {
    case WM_PAINT:
        paintDoubleBuffered(hwnd, win);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE: {
        int mx = LOWORD(lParam), my = HIWORD(lParam);
        SWidget* prev = g_hovered_widget[hwnd];
        SWidget* cur = hitTestWidget(win, mx, my);
        if (cur != prev) {
            if (prev) {
                SToolButton* btn = dynamic_cast<SToolButton*>(prev);
                if (btn) { btn->hovered_ = false; invalidateWidget(hwnd, prev); }
            }
            if (cur) {
                SToolButton* btn = dynamic_cast<SToolButton*>(cur);
                if (btn) { btn->hovered_ = true; invalidateWidget(hwnd, cur); }
            }
            g_hovered_widget[hwnd] = cur;
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int mx = LOWORD(lParam), my = HIWORD(lParam);
        SWidget* target = hitTestWidget(win, mx, my);
        if (target) {
            SEvent e;
            e.type = SEventType::Click;
            e.x = mx;
            e.y = my;
            target->handleEvent(e);
            invalidateWidget(hwnd, target);
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        int mx = LOWORD(lParam), my = HIWORD(lParam);
        SWidget* target = hitTestWidget(win, mx, my);
        if (target) {
            SEvent e;
            e.type = SEventType::DoubleClick;
            e.x = mx;
            e.y = my;
            target->handleEvent(e);
            invalidateWidget(hwnd, target);
        }
        return 0;
    }
    case WM_KEYDOWN: {
        SEvent e;
        e.type = SEventType::KeyPress;
        e.key = (int)wParam;
        win->handleEvent(e);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_CHAR: {
        SEvent e;
        e.type = SEventType::KeyPress;
        e.key = (int)wParam;
        win->handleEvent(e);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_CLOSE:
        win->close();
        return 0;
    case WM_SIZE: {
        int nw = LOWORD(lParam), nh = HIWORD(lParam);
        win->setSize(nw, nh);
        win->resize_signal(nw, nh);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_DESTROY:
        g_hovered_widget.erase(hwnd);
        g_hwnd_map.erase(hwnd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void SWindow::show() {
    if (hwnd_) return;

    static bool cls_registered = false;
    if (!cls_registered) {
        WNDCLASSEXA wc = { sizeof(wc) };
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = "SpyUIWindow";
        wc.hbrBackground = CreateSolidBrush(RGB(18, 18, 18));
        RegisterClassExA(&wc);
        cls_registered = true;
    }

    hwnd_ = CreateWindowExA(
        WS_EX_APPWINDOW,
        "SpyUIWindow", title_.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x_, y_, w_, h_,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    g_hwnd_map[hwnd_] = this;
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
}

void SWindow::close() {
    if (hwnd_) {
        g_hovered_widget.erase(hwnd_);
        g_hwnd_map.erase(hwnd_);
        DestroyWindow(hwnd_);
        hwnd_ = NULL;
    }
}

void SWindow::run() {
    running_ = true;
    MSG msg;
    while (running_ && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

#endif

// ============================================================
// SpyApp
// ============================================================

int SpyApp::exec() {
    running_ = true;
    for (auto* w : windows_) w->show();

#ifdef _WIN32
    MSG msg;
    while (running_ && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif
    return 0;
}

// ============================================================
// C API for Spy extern FFI
// ============================================================

extern "C" {
void* spyui_create_window(int width, int height, const char* title) {
    auto* win = new SWindow(std::string(title), width, height);
    return win;
}

void spyui_show_window(void* window) {
    auto* win = static_cast<SWindow*>(window);
    win->show();
}

void spyui_close_window(void* window) {
    auto* win = static_cast<SWindow*>(window);
    win->close();
}

void spyui_set_title(void* window, const char* title) {
    auto* win = static_cast<SWindow*>(window);
    win->setTitle(title);
#ifdef _WIN32
    if (win->hwnd()) SetWindowTextA(win->hwnd(), title);
#endif
}

void spyui_set_size(void* window, int w, int h) {
    auto* win = static_cast<SWindow*>(window);
    win->setSize(w, h);
#ifdef _WIN32
    if (win->hwnd()) SetWindowPos(win->hwnd(), NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
#endif
}

void* spyui_create_button(const char* text) {
    return new SToolButton(std::string(text));
}

void spyui_button_set_text(void* btn, const char* text) {
    static_cast<SToolButton*>(btn)->setText(text);
}

void spyui_button_set_size(void* btn, int w, int h) {
    static_cast<SToolButton*>(btn)->setSize(w, h);
}

void* spyui_create_input(const char* placeholder) {
    return new SInput(std::string(placeholder));
}

const char* spyui_input_text(void* input) {
    static thread_local std::string buf;
    buf = static_cast<SInput*>(input)->text();
    return buf.c_str();
}

void spyui_input_set_text(void* input, const char* text) {
    static_cast<SInput*>(input)->setText(text);
}

void* spyui_create_label(const char* text) {
    return new SLabel(std::string(text));
}

void spyui_label_set_text(void* label, const char* text) {
    static_cast<SLabel*>(label)->setText(text);
}

void* spyui_create_tabs() {
    return new STabs();
}

void spyui_tabs_add(void* tabs, void* widget, const char* title) {
    static_cast<STabs*>(tabs)->addWidget(static_cast<SWidget*>(widget));
}

void* spyui_tabs_current(void* tabs) {
    return static_cast<STabs*>(tabs)->currentWidget();
}

void spyui_tabs_set_tab_text(void* tabs, int idx, const char* text) {
    static_cast<STabs*>(tabs)->setTabText(idx, text);
}

void* spyui_create_fsmodel() {
    return new SFileSystemModel();
}

void spyui_fsmodel_set_root(void* model, const char* path) {
    static_cast<SFileSystemModel*>(model)->setRootPath(path);
}

const char* spyui_fsmodel_file_path(void* model, void* index) {
    static thread_local std::string buf;
    buf = static_cast<SFileSystemModel*>(model)->filePath(*static_cast<SModelIndex*>(index));
    return buf.c_str();
}

int spyui_fsmodel_is_dir(void* model, void* index) {
    return static_cast<SFileSystemModel*>(model)->isDir(*static_cast<SModelIndex*>(index)) ? 1 : 0;
}

void* spyui_create_listview() {
    return new SListView();
}

void spyui_listview_set_model(void* view, void* model) {
    static_cast<SListView*>(view)->setModel(static_cast<SModel*>(model));
}

void spyui_listview_set_root_index(void* view, void* index) {
    static_cast<SListView*>(view)->setRootIndex(*static_cast<SModelIndex*>(index));
}

void* spyui_create_splitter(int orientation) {
    return new SSplitter(orientation);
}

void spyui_splitter_add(void* splitter, void* widget) {
    static_cast<SSplitter*>(splitter)->addWidget(static_cast<SWidget*>(widget));
}

void* spyui_create_vbox() {
    return new SVBoxLayout();
}

void* spyui_create_hbox() {
    return new SHBoxLayout();
}

void spyui_layout_add(void* layout, void* widget) {
    auto* l = static_cast<SLayout*>(layout);
    auto* w = static_cast<SWidget*>(widget);
    if (auto* vbox = dynamic_cast<SVBoxLayout*>(l)) {
        vbox->addWidget(w);
    } else if (auto* hbox = dynamic_cast<SHBoxLayout*>(l)) {
        hbox->addWidget(w);
    }
}

void spyui_window_set_layout(void* window, void* layout) {
    auto* win = static_cast<SWindow*>(window);
    auto* l = static_cast<SLayout*>(layout);
    win->setLayout(l);
}

void spyui_window_set_central(void* window, void* widget) {
    auto* win = static_cast<SWindow*>(window);
    auto* w = static_cast<SWidget*>(widget);
    win->setCentral(w);
}

void spyui_add_to_parent(void* parent, void* child) {
    auto* p = static_cast<SWidget*>(parent);
    auto* c = static_cast<SWidget*>(child);
    p->addWidget(static_cast<SWidget*>(child));
}

const char* spyui_get_cwd() {
    static char buf[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buf);
    return buf;
}

const char* spyui_get_home() {
    static std::string h = spy::SpyPaths::home();
    return h.c_str();
}

const char* spyui_get_desktop() {
    static std::string h = spy::SpyPaths::desktop();
    return h.c_str();
}

const char* spyui_get_downloads() {
    static std::string h = spy::SpyPaths::downloads();
    return h.c_str();
}

const char* spyui_get_documents() {
    static std::string h = spy::SpyPaths::documents();
    return h.c_str();
}

int spyui_process_start(const char* program, const char* args_json) {
    std::vector<std::string> args;
    std::string s(args_json);
    size_t pos = 0;
    while ((pos = s.find(",")) != std::string::npos) {
        args.push_back(s.substr(0, pos));
        s.erase(0, pos + 1);
    }
    if (!s.empty()) args.push_back(s);
    return spy::SpyProcess::startDetached(program, args) ? 1 : 0;
}

double spyui_file_exists(const char* path) {
    return std::filesystem::exists(path) ? 1.0 : 0.0;
}

double spyui_file_size(const char* path) {
    try { return static_cast<double>(std::filesystem::file_size(path)); }
    catch (...) { return 0; }
}

double spyui_mkdir(const char* path) {
    try { return std::filesystem::create_directory(path) ? 1.0 : 0.0; }
    catch (...) { return 0; }
}

double spyui_remove(const char* path) {
    try { return static_cast<double>(std::filesystem::remove_all(path)) > 0 ? 1.0 : 0.0; }
    catch (...) { return 0; }
}

double spyui_rename(const char* old_path, const char* new_path) {
    try { std::filesystem::rename(old_path, new_path); return 1.0; }
    catch (...) { return 0; }
}

const char* spyui_read_file(const char* path) {
    static thread_local char buf[65536];
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    f.read(buf, 65535);
    buf[f.gcount()] = 0;
    return buf;
}

double spyui_write_file(const char* path, const char* data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return 0;
    f.write(data, static_cast<std::streamsize>(strlen(data)));
    return 1.0;
}

void spyui_style_dark_neon() {
}

void spyui_app_run() {
    spy::SpyApp app;
    app.exec();
}

} // extern "C"

} // namespace spy
