// Copyright 2024-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

#include <algorithm>
#include <clocale>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <functional>
#include <thread>
#include <unordered_map>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QProcess>
#include <QPropertyAnimation>
#include <QStyleFactory>
#include <QSysInfo>
#include <QtConcurrent/QtConcurrentMap>
#include <QtConcurrent/QtConcurrentRun>
#include <QtGui>
#include <QtWidgets>
#include <boost/algorithm/string/replace.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#ifdef __APPLE__
#include <unistd.h> // for chdir
#endif
#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <getopt.h>
#endif
#ifdef __unix__
#include <QVariant>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QtDBus>
#include "common/linux/gamemode.h"
#endif
#include "citra_meta/common_strings.h"
#include "citra_qt/aboutdialog.h"
#include "citra_qt/applets/mii_selector.h"
#include "citra_qt/applets/swkbd.h"
#include "citra_qt/bootmanager.h"
#include "citra_qt/console_modal.h"
#include "citra_qt/camera/qt_multimedia_camera.h"
#include "citra_qt/camera/still_image_camera.h"
#include "citra_qt/citra_qt.h"
#include "citra_qt/compatibility_list.h"
#include "citra_qt/configuration/config.h"
#include "citra_qt/configuration/configure_dialog.h"
#include "citra_qt/configuration/configure_per_game.h"
#include "citra_qt/debugger/console.h"
#include "citra_qt/debugger/graphics/graphics.h"
#include "citra_qt/debugger/graphics/graphics_breakpoints.h"
#include "citra_qt/debugger/graphics/graphics_cmdlists.h"
#include "citra_qt/debugger/graphics/graphics_surface.h"
#include "citra_qt/debugger/graphics/graphics_tracing.h"
#include "citra_qt/debugger/graphics/graphics_vertex_shader.h"
#include "citra_qt/debugger/ipc/recorder.h"
#include "citra_qt/debugger/lle_service_modules.h"
#if MICROPROFILE_ENABLED
#include "citra_qt/debugger/profiler.h"
#endif
#include "citra_qt/debugger/registers.h"
#include "citra_qt/debugger/wait_tree.h"
#ifdef ENABLE_DISCORD_RPC
#include "citra_qt/discord.h"
#endif
#ifdef ENABLE_FFMPEG
#include "citra_qt/dumping/dumping_dialog.h"
#endif
#include "citra_qt/game_list.h"
#include "citra_qt/home_screen.h"
#include "citra_qt/hotkeys.h"
#include "citra_qt/loading_screen.h"
#include "citra_qt/movie/movie_play_dialog.h"
#include "citra_qt/movie/movie_record_dialog.h"
#include "citra_qt/multiplayer/state.h"
#include "citra_qt/qt_image_interface.h"
#include "citra_qt/qt_swizzle.h"
#include "citra_qt/uisounds.h"
#include "citra_qt/uisettings.h"
#include "common/play_time_manager.h"
#ifdef ENABLE_QT_UPDATE_CHECKER
#include "citra_qt/update_checker.h"
#endif
#include "citra_qt/util/clickable_label.h"
#include "citra_qt/util/graphics_device_info.h"
#include "citra_qt/util/util.h"
#ifdef Q_OS_MAC
#include "citra_qt/util/metal_util.h"
#endif
#include "common/arch.h"
#include "common/common_paths.h"
#include "common/dynamic_library.h"
#include "common/file_util.h"
#include "common/literals.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/memory_detect.h"
#include "common/param_package.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#if CITRA_ARCH(x86_64)
#include "common/x64/cpu_detect.h"
#endif
#include "common/settings.h"
#include "common/string_util.h"
#include "common/zstd_compression.h"
#include "core/arm/exception_handler.h"
#include "core/core.h"
#include "core/dumping/backend.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_source_sd_savedata.h"
#include "core/frontend/applets/default_applets.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/savestate.h"
#include "core/system_titles.h"
#include "input_common/main.h"
#include "ui_main.h"
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"

#ifdef __APPLE__
#include "common/apple_authorization.h"
#include "common/apple_utils.h"
Q_IMPORT_PLUGIN(QDarwinCameraPermissionPlugin);
#endif

#ifdef ENABLE_DISCORD_RPC
#include "citra_qt/discord_impl.h"
#endif

#ifdef QT_STATICPLUGIN
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif

#ifdef HAVE_SDL2
#include <SDL.h>
#endif

constexpr int default_mouse_timeout = 2500;

/**
 * "Callouts" are one-time instructional messages shown to the user. In the config settings, there
 * is a bitfield "callout_flags" options, used to track if a message has already been shown to the
 * user. This is 32-bits - if we have more than 32 callouts, we should retire and recycle old ones.
 */

const int GMainWindow::max_recent_files_item;

// There is a bug in the QT implementation on MSYS2 builds
// that cause corners to appear when the app is switched to
// fullscreen. The following code aims to fix that issue
// until it is addressed upstream. It works by manually
// disabling corners through the DWM API.
// TODO(PabloMK7): Remove once the upstream bug is solved.
#if defined(_WIN32) && !defined(_MSC_VER)
#define NEEDS_ROUND_CORNERS_FIX
#endif

#ifdef NEEDS_ROUND_CORNERS_FIX
#include <dwmapi.h>
class WindowCornerManager {
public:
    static WindowCornerManager& instance() {
        static WindowCornerManager inst;
        return inst;
    }

    void blockRoundedCorners(QWidget* widget, bool block) {
        HWND hwnd = reinterpret_cast<HWND>(widget->winId());
        DWORD pref;

        if (block) {
            pref = DWMWCP_DEFAULT;
            if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref,
                                                sizeof(pref)))) {
                original_prefs[hwnd] = pref;
            } else {
                original_prefs[hwnd] = DWMWCP_DEFAULT;
            }

            pref = DWMWCP_DONOTROUND;
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
        } else {
            auto it = original_prefs.find(hwnd);
            if (it == original_prefs.end())
                return;

            pref = it->second;

            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));

            original_prefs.erase(it);
        }
    }

private:
    WindowCornerManager() = default;
    ~WindowCornerManager() = default;

    std::unordered_map<HWND, DWORD> original_prefs;
};
#endif

static QString PrettyProductName() {
#ifdef _WIN32
    // After Windows 10 Version 2004, Microsoft decided to switch to a different notation: 20H2
    // With that notation change they changed the registry key used to denote the current version
    QSettings windows_registry(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
        QSettings::NativeFormat);
    const QString release_id = windows_registry.value(QStringLiteral("ReleaseId")).toString();
    if (release_id == QStringLiteral("2009")) {
        const u32 current_build = windows_registry.value(QStringLiteral("CurrentBuild")).toUInt();
        const QString display_version =
            windows_registry.value(QStringLiteral("DisplayVersion")).toString();
        const u32 ubr = windows_registry.value(QStringLiteral("UBR")).toUInt();
        const u32 version = current_build >= 22000 ? 11 : 10;
        return QStringLiteral("Windows %1 Version %2 (Build %3.%4)")
            .arg(QString::number(version), display_version, QString::number(current_build),
                 QString::number(ubr));
    }
#endif
    return QSysInfo::prettyProductName();
}

void GMainWindow::ShowCommandOutput(std::string title, std::string message) {
#ifdef _WIN32
    boost::replace_all(message, " ", "\u00a0"); // Non-breaking space
    boost::replace_all(message, "-", "\u2011"); // Non-breaking hyphen
    QMessageBox::information(this, QString::fromStdString(title), QString::fromStdString(message));
#else
    std::cout << message << std::endl;
#endif
}

bool IsPrereleaseBuild() {
    return ((strstr(Common::g_build_fullname, "alpha") != NULL) ||
            (strstr(Common::g_build_fullname, "beta") != NULL) ||
            (strstr(Common::g_build_fullname, "rc") != NULL) ||
            (strstr(Common::g_build_fullname, "test") != NULL));
}

#ifdef ENABLE_QT_UPDATE_CHECKER
static bool ShouldCheckForPrereleaseUpdates() {
    const bool update_channel = UISettings::values.update_check_channel.GetValue();
    const bool using_prerelease_channel =
        (update_channel == UISettings::UpdateCheckChannels::PRERELEASE);
    return (IsPrereleaseBuild() || using_prerelease_channel);
}

static int GetMajorVersion(const std::string& version) {
    size_t dot = version.find('.');
    try {
        return std::stoi(version.substr(0, dot));
    } catch (...) {
        return 0;
    }
}
#endif

class OverlayHomeButton : public QToolButton {
public:
    explicit OverlayHomeButton(QWidget* parent = nullptr) : QToolButton(parent) {
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setAttribute(Qt::WA_Hover, true);
        setMouseTracking(true);
        setStyleSheet(QStringLiteral("QToolButton { background: transparent; border: none; }"));
    }

    void SetTheme(bool is_white) {
        if (white == is_white) {
            return;
        }
        white = is_white;
        update();
    }

protected:
    void enterEvent(QEnterEvent* event) override {
        QToolButton::enterEvent(event);
        const bool h = hitButton(event->position().toPoint());
        if (h != hovered) {
            hovered = h;
            update();
        }
    }

    void leaveEvent(QEvent* event) override {
        QToolButton::leaveEvent(event);
        if (hovered) {
            hovered = false;
            update();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        QToolButton::mouseMoveEvent(event);
        const bool h = hitButton(event->pos());
        if (h != hovered) {
            hovered = h;
            update();
        }
    }

    bool hitButton(const QPoint& pos) const override {
        const qreal cx = width() / 2.0;
        const qreal cy = height() / 2.0;
        const qreal dx = pos.x() - cx;
        const qreal dy = pos.y() - cy;
        constexpr qreal radius = 23.5;
        return (dx * dx + dy * dy) <= (radius * radius);
    }

    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        // Letterbox surrounding background color
        const float bg_r = Settings::values.bg_red.GetValue();
        const float bg_g = Settings::values.bg_green.GetValue();
        const float bg_b = Settings::values.bg_blue.GetValue();
        const float lum = bg_r * 0.299f + bg_g * 0.587f + bg_b * 0.114f;
        const bool is_surround_dark = !white || (lum < 0.5f);

        const QColor surround = is_surround_dark
                                    ? QColor::fromRgbF(std::clamp(bg_r, 0.0f, 1.0f),
                                                       std::clamp(bg_g, 0.0f, 1.0f),
                                                       std::clamp(bg_b, 0.0f, 1.0f))
                                    : QColor(0xff, 0xff, 0xff);

        // Fill bounding rect with surrounding background so corners seamlessly match letterbox
        p.fillRect(rect(), surround);

        // Inside the circular ring: matches the background when idle, with subtle hover/press feedback
        const bool is_hover = hovered && underMouse();
        const QColor disc_bg = is_surround_dark
                                   ? (isDown() ? QColor(0x3a, 0x3a, 0x42)
                                               : (is_hover ? QColor(0x24, 0x24, 0x2a) : surround))
                                   : (isDown() ? QColor(0xd2, 0xd2, 0xd8)
                                               : (is_hover ? QColor(0xe8, 0xe8, 0xee) : surround));

        // Circular ring: white if background is black, black otherwise
        const QColor ring_color = is_surround_dark ? QColor(0xff, 0xff, 0xff) : QColor(0x00, 0x00, 0x00);

        const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        p.setPen(QPen(ring_color, 1.5));
        p.setBrush(disc_bg);
        p.drawEllipse(r);

        const QIcon ic = icon();
        if (!ic.isNull()) {
            const QSize isz = iconSize();
            const QRect icon_rect((width() - isz.width()) / 2, (height() - isz.height()) / 2,
                                  isz.width(), isz.height());
            ic.paint(&p, icon_rect);
        }
    }

private:
    bool white = false;
    bool hovered = false;
};

class OverlayStatusWidget : public QWidget {
public:
    explicit OverlayStatusWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setMouseTracking(true);
        setAttribute(Qt::WA_Hover, true);
        setFocusPolicy(Qt::NoFocus);
        setFixedHeight(32);
        setFixedWidth(190);

        update_timer = new QTimer(this);
        connect(update_timer, &QTimer::timeout, this, &OverlayStatusWidget::RefreshStatus);
        update_timer->start(2000);
        RefreshStatus();
    }

    void SetTheme(bool is_white) {
        if (white == is_white) {
            return;
        }
        white = is_white;
        update();
    }

    void SetPopoverOpen(bool open) {
        if (popover_open != open) {
            popover_open = open;
            update();
        }
    }

    void RefreshStatus() {
        battery_state = ReadHostBatteryState();
        const QString format = (UISettings::values.profile_time_format_24h == 0)
                                   ? QStringLiteral("h:mm AP")
                                   : QStringLiteral("hh:mm");
        time_string = QDateTime::currentDateTime().toString(format);
        battery_string = QStringLiteral("%1%").arg(battery_state.percent);

        QFont f = font();
        f.setPointSize(11);
        f.setBold(true);
        QFontMetrics fm_bold(f);
        int time_w = fm_bold.horizontalAdvance(time_string);

        f.setBold(false);
        QFontMetrics fm_reg(f);
        int batt_w = fm_reg.horizontalAdvance(battery_string);

        // 5 (left pad) + 25 (bluetooth) + 4 (space) + 1 (divider) + 5 (space) + 26 (battery pill) + 6 + batt_w + 8 + 8 (dot) + 8 + time_w + 14 (right pad)
        int needed_w = 5 + 25 + 4 + 1 + 5 + 26 + 6 + batt_w + 8 + 8 + 8 + time_w + 14;
        if (needed_w < 180) {
            needed_w = 180;
        }

        if (width() != needed_w) {
            setFixedWidth(needed_w);
            if (parentWidget()) {
                QMetaObject::invokeMethod(window(), "RepositionInGameStatus", Qt::DirectConnection);
            }
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        // Letterbox surrounding background color
        const float bg_r = Settings::values.bg_red.GetValue();
        const float bg_g = Settings::values.bg_green.GetValue();
        const float bg_b = Settings::values.bg_blue.GetValue();
        const float lum = bg_r * 0.299f + bg_g * 0.587f + bg_b * 0.114f;
        const bool is_surround_dark = !white || (lum < 0.5f);

        const QColor surround = is_surround_dark
                                    ? QColor::fromRgbF(std::clamp(bg_r, 0.0f, 1.0f),
                                                       std::clamp(bg_g, 0.0f, 1.0f),
                                                       std::clamp(bg_b, 0.0f, 1.0f))
                                    : QColor(0xff, 0xff, 0xff);

        // Fill bounding rect with surrounding background so corners seamlessly match letterbox
        p.fillRect(rect(), surround);

        // Ring, sprite, and font colors: white if background is black, black otherwise
        const QColor ring_color = is_surround_dark ? QColor(0xff, 0xff, 0xff) : QColor(0x00, 0x00, 0x00);
        const QColor fg = is_surround_dark ? QColor(0xff, 0xff, 0xff) : QColor(0x00, 0x00, 0x00);
        const QColor fg_muted = is_surround_dark ? QColor(0xee, 0xee, 0xee) : QColor(0x22, 0x22, 0x22);
        const QColor dot_color = is_surround_dark ? QColor(0xaa, 0xae, 0xb8) : QColor(0x60, 0x64, 0x70);

        // Draw outer pill capsule ring with surrounding background fill
        const QRectF r = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
        p.setPen(QPen(ring_color, 1.5));
        p.setBrush(surround);
        p.drawRoundedRect(r, 15.0, 15.0);

        // Bluetooth button rect:
        const QRect bt_rect(5, 4, 25, 24);

        if (bt_hovered || popover_open) {
            const QRectF bt_pill = QRectF(bt_rect).adjusted(1.0, 1.0, -1.0, -1.0);
            p.setPen(Qt::NoPen);
            p.setBrush(is_surround_dark
                           ? QColor(255, 255, 255, popover_open ? 55 : 30)
                           : QColor(0, 0, 0, popover_open ? 40 : 20));
            p.drawRoundedRect(bt_pill, 11.0, 11.0);
        }

        // Draw Bluetooth vector symbol
        const qreal cx = bt_rect.center().x();
        const qreal cy = bt_rect.center().y();
        const qreal h_half = 6.5;
        const qreal arm = 4.0;

        QPainterPath bt_path;
        bt_path.moveTo(cx - arm, cy - arm);
        bt_path.lineTo(cx + arm, cy + arm);
        bt_path.lineTo(cx, cy + h_half);
        bt_path.lineTo(cx, cy - h_half);
        bt_path.lineTo(cx + arm, cy - arm);
        bt_path.lineTo(cx - arm, cy + arm);

        p.setPen(QPen(fg, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(bt_path);

        // Subtle vertical separator between Bluetooth and Battery
        const int sep_x = bt_rect.right() + 4;
        p.setPen(QPen(is_surround_dark ? QColor(255, 255, 255, 55) : QColor(0, 0, 0, 45), 1.0));
        p.drawLine(sep_x, 8, sep_x, height() - 8);

        // Draw battery icon
        const qreal dpr = devicePixelRatioF();
        QPixmap batt_icon = RenderBatteryPillIcon(battery_state, fg, dpr);
        int cur_x = sep_x + 6;
        p.drawPixmap(QPoint(cur_x, (height() - 13) / 2), batt_icon);
        cur_x += 26 + 6;

        // Draw battery percentage text
        QFont regular_f = font();
        regular_f.setPointSize(11);
        regular_f.setBold(false);
        p.setFont(regular_f);
        p.setPen(fg_muted);
        QFontMetrics fm_reg(regular_f);
        int batt_w = fm_reg.horizontalAdvance(battery_string);
        p.drawText(QRect(cur_x, 0, batt_w, height()), Qt::AlignVCenter | Qt::AlignLeft, battery_string);
        cur_x += batt_w;

        // Draw subtle separator dot •
        cur_x += 8;
        p.setPen(dot_color);
        p.drawText(QRect(cur_x, 0, 8, height()), Qt::AlignVCenter | Qt::AlignCenter, QStringLiteral("•"));
        cur_x += 12;

        // Draw time string (heavy/bold for big weight jump, Rule 8)
        QFont bold_f = font();
        bold_f.setPointSize(11);
        bold_f.setBold(true);
        p.setFont(bold_f);
        p.setPen(fg);
        QFontMetrics fm_bold(bold_f);
        int time_w = fm_bold.horizontalAdvance(time_string);
        p.drawText(QRect(cur_x, 0, time_w, height()), Qt::AlignVCenter | Qt::AlignLeft, time_string);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        const QRect bt_rect(5, 4, 25, 24);
        bool hovered = bt_rect.contains(event->pos());
        if (hovered != bt_hovered) {
            bt_hovered = hovered;
            setCursor(bt_hovered ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
        event->accept();
    }

    void leaveEvent(QEvent* event) override {
        Q_UNUSED(event);
        if (bt_hovered) {
            bt_hovered = false;
            setCursor(Qt::ArrowCursor);
            update();
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        const QRect bt_rect(5, 4, 25, 24);
        if (event->button() == Qt::LeftButton && bt_rect.contains(event->pos())) {
            QMetaObject::invokeMethod(window(), "ToggleControllersPopover", Qt::DirectConnection);
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

private:
    bool white = false;
    bool bt_hovered = false;
    bool popover_open = false;
    HostBatteryState battery_state;
    QString battery_string;
    QString time_string;
    QTimer* update_timer = nullptr;
};

class DeviceIconWidget : public QWidget {
public:
    explicit DeviceIconWidget(bool gamepad, bool is_white, QWidget* parent = nullptr)
        : QWidget(parent), is_gamepad(gamepad), white(is_white) {
        setFixedSize(28, 28);
    }

    void SetTheme(bool is_white) {
        white = is_white;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Circle badge
        const QColor bg = white ? QColor(0, 0, 0, 16) : QColor(255, 255, 255, 24);
        p.setBrush(bg);
        p.setPen(Qt::NoPen);
        p.drawEllipse(rect().adjusted(1, 1, -1, -1));

        const QColor icon_color = white ? QColor(50, 50, 55) : QColor(220, 220, 225);

        if (is_gamepad) {
            // Gamepad body
            p.setPen(QPen(icon_color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            const QRectF gp_rect(6.0, 9.0, 16.0, 10.0);
            p.drawRoundedRect(gp_rect, 3.5, 3.5);

            // Dpad
            p.setPen(Qt::NoPen);
            p.setBrush(icon_color);
            p.fillRect(QRectF(8.5, 13.0, 3.0, 1.2), icon_color);
            p.fillRect(QRectF(9.4, 12.1, 1.2, 3.0), icon_color);

            // Face buttons (A, B, X, Y)
            p.drawEllipse(QPointF(17.0, 12.5), 0.8, 0.8);
            p.drawEllipse(QPointF(19.0, 14.0), 0.8, 0.8);
            p.drawEllipse(QPointF(17.0, 15.5), 0.8, 0.8);
            p.drawEllipse(QPointF(15.0, 14.0), 0.8, 0.8);
        } else {
            // Keyboard
            p.setPen(QPen(icon_color, 1.3));
            p.setBrush(Qt::NoBrush);
            const QRectF kb(5.0, 9.0, 18.0, 10.0);
            p.drawRoundedRect(kb, 2.5, 2.5);
            p.setPen(Qt::NoPen);
            p.setBrush(icon_color);
            for (int r = 0; r < 2; ++r) {
                for (int c = 0; c < 4; ++c) {
                    p.fillRect(QRectF(7.5 + c * 3.4, 11.0 + r * 3.0, 1.8, 1.6), icon_color);
                }
            }
        }
    }

private:
    bool is_gamepad = true;
    bool white = false;
};

class ControllersPopoverWidget : public QWidget {
public:
    explicit ControllersPopoverWidget(GMainWindow* main_win, QWidget* parent = nullptr);
    ~ControllersPopoverWidget() override;

    void SetTheme(bool is_white);
    void RefreshDevices();
    void AutoMapPrimaryController();
    void OpenBluetoothPreferences();
    void OpenControllerSettings();
    void OnTopProfileChanged(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void ApplyStyles();

    GMainWindow* main_window = nullptr;
    bool white = false;
    bool suppress_profile_events = false;

    QVBoxLayout* root_layout = nullptr;
    QLabel* title_label = nullptr;
    QPushButton* close_btn = nullptr;
    QLabel* devices_header = nullptr;
    QWidget* devices_container = nullptr;
    QVBoxLayout* devices_layout = nullptr;
    QPushButton* pair_btn = nullptr;
    QPushButton* refresh_btn = nullptr;
    QFrame* divider1 = nullptr;
    QLabel* assign_header = nullptr;
    QLabel* top_label = nullptr;
    QComboBox* top_combo = nullptr;
    QLabel* bottom_label = nullptr;
    QComboBox* bottom_combo = nullptr;
    QPushButton* automap_btn = nullptr;
    QLabel* automap_status = nullptr;
    QFrame* divider2 = nullptr;
    QPushButton* bt_settings_link = nullptr;
    QPushButton* all_settings_link = nullptr;
};

GMainWindow::GMainWindow(Core::System& system_)
    : ui{std::make_unique<Ui::MainWindow>()}, system{system_}, movie{system.Movie()},
      user_data_migrator{this}, config{std::make_unique<QtConfig>()}, emu_thread{nullptr} {
    Common::Log::Initialize();
    Common::Log::Start();

    Debugger::ToggleConsole();

    QStringList args = QApplication::arguments();
    QString game_path;
    std::optional<bool> fullscreen_override;
    for (int i = 1; i < args.size(); ++i) {
        // Preserves drag/drop functionality
        if (args.size() == 2 && !args[1].startsWith(QChar::fromLatin1('-'))) {
            game_path = args[1];
            break;
        }

        // Dump video
        if (args[i] == QStringLiteral("--dump-video") || args[i] == QStringLiteral("-d")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            video_dumping_path = args[++i];
            video_dumping_on_start = true;
            continue;
        }

        // Launch game in fullscreen mode
        if (args[i] == QStringLiteral("--fullscreen") || args[i] == QStringLiteral("-f")) {
            fullscreen_override = true;
            continue;
        }

        // Enable GDB stub
        if (args[i] == QStringLiteral("--gdbport") || args[i] == QStringLiteral("-g")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            gdbport_from_arg = strtoul(args[++i].toLatin1(), NULL, 0);
            continue;
        }

        if (args[i] == QStringLiteral("--help") || args[i] == QStringLiteral("-h")) {
            ShowCommandOutput("Help", fmt::format(Common::help_string, args[0].toStdString()));
            exit(0);
        }

        if (args[i] == QStringLiteral("--install") || args[i] == QStringLiteral("-i")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            Service::AM::InstallStatus result = Service::AM::InstallCIA(args[++i].toStdString());
            if (result != Service::AM::InstallStatus::Success) {
                std::string failure_reason;

                if (result == Service::AM::InstallStatus::ErrorFailedToOpenFile)
                    failure_reason = "Unable to open file.";

                if (result == Service::AM::InstallStatus::ErrorFileNotFound)
                    failure_reason = "File not found.";

                if (result == Service::AM::InstallStatus::ErrorAborted)
                    failure_reason = "Install was aborted.";

                if (result == Service::AM::InstallStatus::ErrorInvalid)
                    failure_reason = "CIA is invalid.";

                if (result == Service::AM::InstallStatus::ErrorEncrypted)
                    failure_reason = "CIA is encrypted.";

                std::string failure_string = "Failed to install CIA: " + failure_reason;
                ShowCommandOutput("Failure", failure_string);
                exit((int)result +
                     2); // 2 is added here to avoid stepping on the toes of
                         // exit codes 1 and 2 which have pre-established conventional meanings
            }
            ShowCommandOutput("Success", "Installed CIA successfully.");
            exit(0);
        }

        if (args[i] == QStringLiteral("--movie-play") || args[i] == QStringLiteral("-p")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            movie_playback_path = args[++i];
            movie_playback_on_start = true;
            continue;
        }

        if (args[i] == QStringLiteral("--movie-record") || args[i] == QStringLiteral("-r")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            movie_record_path = args[++i];
            movie_record_on_start = true;
            continue;
        }

        if (args[i] == QStringLiteral("--movie-record-author") || args[i] == QStringLiteral("-a")) {
            if (i >= args.size() - 1 || args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }
            movie_record_author = args[++i];
            continue;
        }

        if (args[i] == QStringLiteral("--multiplayer") || args[i] == QStringLiteral("-m")) {
            std::cout << "Warning: The --multiplayer option is not yet implemented for the Qt "
                         "frontend; Ignoring."
                      << std::endl;
            if (i < args.size() - 1 && !args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                i++;
            }
            continue;
        }

        if (args[i] == QStringLiteral("--version") || args[i] == QStringLiteral("-v")) {
            const std::string version_string = std::string("Extendo ") + Common::g_build_fullname;
            ShowCommandOutput("Version", version_string);
            exit(0);
        }

        // Launch game in windowed mode
        if (args[i] == QStringLiteral("--windowed") || args[i] == QStringLiteral("-w")) {
            fullscreen_override = false;
            continue;
        }

        // Launch game at path
        if (i == args.size() - 1 && !args[i].startsWith(QChar::fromLatin1('-'))) {
            game_path = args[i];
            continue;
        }
    }

#ifdef __unix__
    SetGamemodeEnabled(Settings::values.enable_gamemode.GetValue());
#endif

    // register types to use in slots and signals
    qRegisterMetaType<std::size_t>("std::size_t");
    qRegisterMetaType<Service::AM::InstallStatus>("Service::AM::InstallStatus");

    // Register CameraFactory
    qt_cameras = std::make_shared<Camera::QtMultimediaCameraHandlerFactory>();
    Camera::RegisterFactory("image", std::make_unique<Camera::StillImageCameraFactory>());
    Camera::RegisterFactory("qt", std::make_unique<Camera::QtMultimediaCameraFactory>(qt_cameras));

    system.RegisterInfoLEDColorChanged([this]() { emit InfoLEDColorChanged(); });

    LoadTranslation();

    if (Settings::values.pica_debugging) {
        Pica::g_debug_context = Pica::DebugContext::Construct();
    } else {
        Pica::g_debug_context.reset();
    }
    setAcceptDrops(true);
    ui->setupUi(this);
    statusBar()->hide();

    default_theme_paths = QIcon::themeSearchPaths();
    UpdateUITheme();

#ifdef ENABLE_DISCORD_RPC
    SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
    discord_rpc->Update(false);
#endif

    play_time_manager = std::make_unique<PlayTime::PlayTimeManager>();

    Network::Init();

    movie.SetPlaybackCompletionCallback([this] {
        QMetaObject::invokeMethod(this, "OnMoviePlaybackCompleted", Qt::BlockingQueuedConnection);
    });

    // Installed before the widgets are built so every button created from here on, including
    // those inside dialogs generated from .ui files, gets hover and click feedback.
    UiSounds::InstallGlobalFilter();
    UiSounds::Preload();

    InitializeWidgets();
    InitializeDebugWidgets();
    InitializeRecentFileMenuActions();
    InitializeSaveStateMenuActions();
    InitializeHotkeys();

    SetDefaultUIGeometry();
    RestoreUIState();

    ui->action_Dump_Video->setChecked(video_dumping_on_start);
    if (fullscreen_override) {
        ui->action_Fullscreen->setChecked(*fullscreen_override);
    }
    ui->action_Close_Movie->setEnabled(movie_playback_on_start || movie_record_on_start);

    ConnectAppEvents();
    ConnectMenuEvents();
    ConnectWidgetEvents();

    LOG_INFO(Frontend, "Extendo Version: {} | {}-{}", Common::g_build_fullname,
             Common::g_scm_branch, Common::g_scm_desc);
    std::string build_variant = Common::g_build_variant;
    if (build_variant == "") {
        build_variant = "N/A";
    }
    LOG_INFO(Frontend, "Build Variant: {}", build_variant);
#if CITRA_ARCH(x86_64)
    const auto& caps = Common::GetCPUCaps();
    std::string cpu_string = caps.cpu_string;
    if (caps.avx || caps.avx2 || caps.avx512) {
        cpu_string += " | AVX";
        if (caps.avx512) {
            cpu_string += "512";
        } else if (caps.avx2) {
            cpu_string += '2';
        }
        if (caps.fma) {
            cpu_string += " | FMA";
        }
    }
    LOG_INFO(Frontend, "Host CPU: {}", cpu_string);
#endif
    LOG_INFO(Frontend, "Host OS: {}", PrettyProductName().toStdString());
    const auto& mem_info = Common::GetMemInfo();
    using namespace Common::Literals;
    LOG_INFO(Frontend, "Host RAM: {:.2f} GiB", mem_info.total_physical_memory / f64{1_GiB});
    LOG_INFO(Frontend, "Host Swap: {:.2f} GiB", mem_info.total_swap_memory / f64{1_GiB});
    UpdateWindowTitle();

    QIcon azahar_icon = QIcon(QString::fromStdString(":/icons/default/256x256/azahar.png"));
    render_window->setWindowIcon(azahar_icon);
    secondary_window->setWindowIcon(azahar_icon);

    show();

#ifdef __APPLE__
    if (AppleUtils::IsRunningFromTerminal()) {
        QMessageBox::warning(
            this, tr("Warning"),
            tr("The `extendo` executable is being run directly rather than via the Extendo.app "
               "bundle.\n\n"
               "When run this way, the app may be missing certain functionality such as camera "
               "emulation.\n\n"
               "It is recommended to instead run Extendo using the `open` command, e.g.:\n"
               "`open ./Extendo.app`"));
    }
#endif

#ifdef ENABLE_QT_UPDATE_CHECKER
    if (UISettings::values.check_for_update_on_start) {
        update_future = QtConcurrent::run([]() -> QString {
            const std::optional<std::string> latest_release_tag =
                UpdateChecker::GetLatestRelease(ShouldCheckForPrereleaseUpdates());

            if (latest_release_tag && latest_release_tag.value() != Common::g_build_fullname) {
                const int latest_major_version = GetMajorVersion(latest_release_tag.value());
                const int current_major_version = GetMajorVersion(Common::g_build_fullname);
                if (current_major_version <= latest_major_version) {
                    return QString::fromStdString(latest_release_tag.value());
                }
            }
            return QString{};
        });
        QObject::connect(&update_watcher, &QFutureWatcher<QString>::finished, this,
                         &GMainWindow::OnEmulatorUpdateAvailable);
        update_watcher.setFuture(update_future);
    }
#endif

    mouse_hide_timer.setInterval(default_mouse_timeout);
    connect(&mouse_hide_timer, &QTimer::timeout, this, &GMainWindow::HideMouseCursor);
    connect(ui->menubar, &QMenuBar::hovered, this, &GMainWindow::OnMouseActivity);

#ifdef ENABLE_OPENGL
    gl_renderer = GetOpenGLRenderer();
#endif

#ifdef ENABLE_VULKAN
    physical_devices = GetVulkanPhysicalDevices();
#endif

    if (!game_path.isEmpty()) {
        BootGame(game_path);
    }
}

GMainWindow::~GMainWindow() {
    // Will get automatically deleted otherwise
    if (!render_window->parent()) {
        delete render_window;
    }

    Pica::g_debug_context.reset();
    Network::Shutdown();
}

void GMainWindow::InitializeWidgets() {
    render_window = new GRenderWindow(this, emu_thread.get(), system, false);
    secondary_window = new GRenderWindow(this, emu_thread.get(), system, true);
    render_window->hide();
    secondary_window->hide();
    secondary_window->setParent(nullptr);

#ifndef ENABLE_FFMPEG
    ui->action_Dump_Video->setVisible(false);
#endif
    game_list = new GameList(*play_time_manager, this);
    ui->horizontalLayout->addWidget(game_list);
    game_list->hide();

    game_list_placeholder = new GameListPlaceholder(this);
    ui->horizontalLayout->addWidget(game_list_placeholder);
    game_list_placeholder->setVisible(false);

    home_screen = new HomeScreenWidget(game_list->GetModel(), this);
    ui->horizontalLayout->addWidget(home_screen);
    connect(home_screen, &HomeScreenWidget::GameChosen, this, &GMainWindow::OnGameListLoadFile);
    connect(home_screen, &HomeScreenWidget::AddDirectory, this,
            &GMainWindow::OnGameListAddDirectory);
    connect(home_screen, &HomeScreenWidget::OpenSettings, this, &GMainWindow::OnQuickSettings);
    connect(home_screen, &HomeScreenWidget::OpenProfile, this, &GMainWindow::ShowProfileOverlay);
    connect(home_screen, &HomeScreenWidget::PowerOff, this, &QMainWindow::close);
    connect(home_screen, &HomeScreenWidget::BackgroundImageChanged, this,
            [this] { config->Save(); });
    connect(home_screen, &HomeScreenWidget::ToggleControllers, this,
            &GMainWindow::ToggleControllersPopover);
    connect(game_list, &GameList::PopulatingCompleted, home_screen, &HomeScreenWidget::Refresh);
    home_screen->show();

    // Floating Home button, shown only while a game is running (the home screen itself already
    // has its own navigation) — lets you get back to the game shelf without hunting for the
    // menu bar, matching the physical Home button.
    home_button = new OverlayHomeButton(ui->centralwidget);
    // No tooltip: these two buttons are only ever shown on top of the running game's
    // Vulkan/MoltenVK surface, where native QToolTip popups render corrupted.
    home_button->setFixedSize(48, 48);
    home_button->setFocusPolicy(Qt::NoFocus);
    // Icon smaller than the 48px button so it sits inside the themed disc drawn by
    // ApplyOverlayButtonTheme() instead of running into its edge.
    home_button->setIconSize(QSize(31, 31));
    home_button->hide();
    home_button->raise();
    connect(home_button, &QToolButton::clicked, this, &GMainWindow::OnHomeButtonClicked);

    ingame_status = new OverlayStatusWidget(ui->centralwidget);
    ingame_status->hide();
    ingame_status->raise();

    ApplyOverlayButtonTheme();

    // Thin draggable bar at the seam between the top and bottom 3DS screens, letting the user
    // resize their relative heights by hand instead of only through the Layout settings tab.
    screen_divider = new QWidget(ui->centralwidget);
    screen_divider->setFixedHeight(8);
    screen_divider->setCursor(Qt::SizeVerCursor);
    screen_divider->setStyleSheet(QStringLiteral(
        "background-color: rgba(10, 132, 255, 0);")); // invisible until hovered, see eventFilter
    screen_divider->hide();
    screen_divider->raise();
    screen_divider->installEventFilter(this);

    auto* escape_shortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape_shortcut->setContext(Qt::ApplicationShortcut);
    connect(escape_shortcut, &QShortcut::activated, this, &GMainWindow::OnEscapePressed);

    loading_screen = new LoadingScreen(this);
    loading_screen->hide();
    ui->horizontalLayout->addWidget(loading_screen);
    connect(loading_screen, &LoadingScreen::Hidden, this, [&] {
        loading_screen->Clear();
        if (emulation_running) {
            render_window->show();
            render_window->setFocus();
            render_window->activateWindow();
            RepositionHomeButton();
            home_button->show();
            home_button->raise();
            RepositionInGameStatus();
            if (ingame_status) {
                ingame_status->RefreshStatus();
                ingame_status->show();
                ingame_status->raise();
            }
        }
    });

    InputCommon::Init();
    multiplayer_state = new MultiplayerState(system, this, game_list->GetModel(),
                                             ui->action_Leave_Room, ui->action_Show_Room);
    multiplayer_state->setVisible(false);

    UpdateBootHomeMenuState();

    // Create status bar
    message_label = new QLabel();
    // Configured separately for left alignment
    message_label->setFrameStyle(QFrame::NoFrame);
    message_label->setContentsMargins(4, 0, 4, 0);
    message_label->setAlignment(Qt::AlignLeft);
    statusBar()->addPermanentWidget(message_label, 1);

    progress_bar = new QProgressBar();
    progress_bar->hide();
    statusBar()->addPermanentWidget(progress_bar);

    loading_shaders_label = new QLabel();

    artic_traffic_label = new QLabel();
    artic_traffic_label->setToolTip(
        tr("Current Artic traffic speed. Higher values indicate bigger transfer loads."));

    emu_speed_label = new QLabel();
    emu_speed_label->setToolTip(tr("Current emulation speed. Values higher or lower than 100% "
                                   "indicate emulation is running faster or slower than a 3DS."));
    game_fps_label = new QLabel();
    game_fps_label->setToolTip(tr("How many frames per second the app is currently displaying. "
                                  "This will vary from app to app and scene to scene."));
    emu_frametime_label = new QLabel();
    emu_frametime_label->setToolTip(
        tr("Time taken to emulate a 3DS frame, not counting framelimiting or v-sync. For "
           "full-speed emulation this should be at most 16.67 ms."));

    for (auto& label : {loading_shaders_label, artic_traffic_label, emu_speed_label, game_fps_label,
                        emu_frametime_label}) {
        label->setVisible(false);
        label->setFrameStyle(QFrame::NoFrame);
        label->setContentsMargins(4, 0, 4, 0);
        statusBar()->addPermanentWidget(label);
    }

    // Setup Graphics API button
    graphics_api_button = new QPushButton();
    graphics_api_button->setObjectName(QStringLiteral("GraphicsAPIStatusBarButton"));
    graphics_api_button->setFocusPolicy(Qt::NoFocus);
    UpdateAPIIndicator();

    connect(graphics_api_button, &QPushButton::clicked, this, [this] { UpdateAPIIndicator(true); });

    statusBar()->insertPermanentWidget(0, graphics_api_button);

    volume_popup = new QWidget(this);
    volume_popup->setWindowFlags(Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::Popup);
    volume_popup->setLayout(new QVBoxLayout());
    volume_popup->setMinimumWidth(200);

    volume_slider = new QSlider(Qt::Horizontal);
    volume_slider->setObjectName(QStringLiteral("volume_slider"));
    volume_slider->setMaximum(100);
    volume_slider->setPageStep(5);
    connect(volume_slider, &QSlider::valueChanged, this, [this](int percentage) {
        Settings::values.audio_muted = false;
        const auto value = static_cast<float>(percentage) / volume_slider->maximum();
        Settings::values.volume.SetValue(value);
        UpdateVolumeUI();
    });
    volume_popup->layout()->addWidget(volume_slider);

    volume_button = new QPushButton();
    volume_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    volume_button->setFocusPolicy(Qt::NoFocus);
    volume_button->setCheckable(true);
    UpdateVolumeUI();
    connect(volume_button, &QPushButton::clicked, this, [&] {
        UpdateVolumeUI();
        volume_popup->setVisible(!volume_popup->isVisible());
        QRect rect = volume_button->geometry();
        QPoint bottomLeft = statusBar()->mapToGlobal(rect.topLeft());
        bottomLeft.setY(bottomLeft.y() - volume_popup->geometry().height());
        volume_popup->setGeometry(QRect(bottomLeft, QSize(rect.width(), rect.height())));
    });
    statusBar()->insertPermanentWidget(1, volume_button);

    statusBar()->addPermanentWidget(multiplayer_state->GetStatusText());
    statusBar()->addPermanentWidget(multiplayer_state->GetStatusIcon());

    QFrame* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setFixedHeight(16);
    statusBar()->addPermanentWidget(sep);

    notification_led = new LedWidget();
    notification_led->setToolTip(tr("Emulated notification LED"));
    statusBar()->addPermanentWidget(notification_led);
    connect(this, &GMainWindow::InfoLEDColorChanged, this, [this] {
        auto led_color = system.GetInfoLEDColor();
        notification_led->setColor(QColor(led_color.r(), led_color.g(), led_color.b()));
    });

    statusBar()->setVisible(true);

    // Removes an ugly inner border from the status bar widgets under Linux
    setStyleSheet(QStringLiteral("QStatusBar::item{border: none;}"));

    QActionGroup* actionGroup_ScreenLayouts = new QActionGroup(this);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Default);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Single_Screen);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Large_Screen);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Side_by_Side);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Separate_Windows);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Hybrid_Screen);
    actionGroup_ScreenLayouts->addAction(ui->action_Screen_Layout_Custom_Layout);

    QActionGroup* actionGroup_SmallPositions = new QActionGroup(this);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_TopRight);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_MiddleRight);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_BottomRight);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_TopLeft);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_MiddleLeft);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_BottomLeft);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_Above);
    actionGroup_SmallPositions->addAction(ui->action_Small_Screen_Below);
}

void GMainWindow::InitializeDebugWidgets() {
    if (Pica::g_debug_context) {
        connect(ui->action_Create_Pica_Surface_Viewer, &QAction::triggered, this,
                &GMainWindow::OnCreateGraphicsSurfaceViewer);
    } else {
        ui->action_Create_Pica_Surface_Viewer->setEnabled(false);
    }

    QMenu* debug_menu = ui->menu_View_Debugging;

#if MICROPROFILE_ENABLED
    microProfileDialog = new MicroProfileDialog(this);
    microProfileDialog->hide();
    debug_menu->addAction(microProfileDialog->toggleViewAction());
#endif

    registersWidget = new RegistersWidget(system, this);
    addDockWidget(Qt::RightDockWidgetArea, registersWidget);
    registersWidget->hide();
    debug_menu->addAction(registersWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting, registersWidget,
            &RegistersWidget::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, registersWidget,
            &RegistersWidget::OnEmulationStopping);

    if (Pica::g_debug_context) {
        graphicsWidget = new GPUCommandStreamWidget(system, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsWidget);
        graphicsWidget->hide();
        debug_menu->addAction(graphicsWidget->toggleViewAction());

        graphicsCommandsWidget = new GPUCommandListWidget(system, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsCommandsWidget);
        graphicsCommandsWidget->hide();
        debug_menu->addAction(graphicsCommandsWidget->toggleViewAction());

        graphicsBreakpointsWidget = new GraphicsBreakPointsWidget(Pica::g_debug_context, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsBreakpointsWidget);
        graphicsBreakpointsWidget->hide();
        debug_menu->addAction(graphicsBreakpointsWidget->toggleViewAction());

        graphicsVertexShaderWidget =
            new GraphicsVertexShaderWidget(system, Pica::g_debug_context, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsVertexShaderWidget);
        graphicsVertexShaderWidget->hide();
        debug_menu->addAction(graphicsVertexShaderWidget->toggleViewAction());

        graphicsTracingWidget = new GraphicsTracingWidget(system, Pica::g_debug_context, this);
        addDockWidget(Qt::RightDockWidgetArea, graphicsTracingWidget);
        graphicsTracingWidget->hide();
        debug_menu->addAction(graphicsTracingWidget->toggleViewAction());
        connect(this, &GMainWindow::EmulationStarting, graphicsTracingWidget,
                &GraphicsTracingWidget::OnEmulationStarting);
        connect(this, &GMainWindow::EmulationStopping, graphicsTracingWidget,
                &GraphicsTracingWidget::OnEmulationStopping);
    }

    waitTreeWidget = new WaitTreeWidget(system, this);
    addDockWidget(Qt::LeftDockWidgetArea, waitTreeWidget);
    waitTreeWidget->hide();
    debug_menu->addAction(waitTreeWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting, waitTreeWidget,
            &WaitTreeWidget::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, waitTreeWidget,
            &WaitTreeWidget::OnEmulationStopping);

    lleServiceModulesWidget = new LLEServiceModulesWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, lleServiceModulesWidget);
    lleServiceModulesWidget->hide();
    debug_menu->addAction(lleServiceModulesWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting,
            [this] { lleServiceModulesWidget->setDisabled(true); });
    connect(this, &GMainWindow::EmulationStopping, waitTreeWidget,
            [this] { lleServiceModulesWidget->setDisabled(false); });

    ipcRecorderWidget = new IPCRecorderWidget(system, this);
    addDockWidget(Qt::RightDockWidgetArea, ipcRecorderWidget);
    ipcRecorderWidget->hide();
    debug_menu->addAction(ipcRecorderWidget->toggleViewAction());
    connect(this, &GMainWindow::EmulationStarting, ipcRecorderWidget,
            &IPCRecorderWidget::OnEmulationStarting);
}

void GMainWindow::InitializeRecentFileMenuActions() {
    for (int i = 0; i < max_recent_files_item; ++i) {
        actions_recent_files[i] = new QAction(this);
        actions_recent_files[i]->setVisible(false);
        connect(actions_recent_files[i], &QAction::triggered, this, &GMainWindow::OnMenuRecentFile);

        ui->menu_recent_files->addAction(actions_recent_files[i]);
    }
    ui->menu_recent_files->addSeparator();
    QAction* action_clear_recent_files = new QAction(this);
    action_clear_recent_files->setText(tr("Clear Recent Files"));
    connect(action_clear_recent_files, &QAction::triggered, this, [this] {
        UISettings::values.recent_files.clear();
        UpdateRecentFiles();
    });
    ui->menu_recent_files->addAction(action_clear_recent_files);

    UpdateRecentFiles();
}

void GMainWindow::InitializeSaveStateMenuActions() {
    for (u32 i = 0; i < Core::SaveStateSlotCount; ++i) {
        actions_load_state[i] = new QAction(this);
        actions_load_state[i]->setData(i);
        connect(actions_load_state[i], &QAction::triggered, this, &GMainWindow::OnLoadState);
        if (i > 0)
            ui->menu_Load_State->addAction(actions_load_state[i]);
        actions_save_state[i] = new QAction(this);
        actions_save_state[i]->setData(i);
        connect(actions_save_state[i], &QAction::triggered, this, &GMainWindow::OnSaveState);
        if (i > 0)
            ui->menu_Save_State->addAction(actions_save_state[i]);
    }

    connect(ui->action_Load_from_Newest_Slot, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        if (newest_slot != 0) {
            actions_load_state[newest_slot]->trigger();
        }
    });
    connect(ui->action_Save_to_Oldest_Slot, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        actions_save_state[oldest_slot]->trigger();
    });

    // Quick save / load uses slot
    connect(ui->action_Quick_Save, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        actions_save_state[0]->trigger();
    });
    connect(ui->action_Quick_Load, &QAction::triggered, this, [this] {
        UpdateSaveStates();
        actions_load_state[0]->trigger();
    });

    connect(ui->menu_Load_State->menuAction(), &QAction::hovered, this,
            &GMainWindow::UpdateSaveStates);
    connect(ui->menu_Save_State->menuAction(), &QAction::hovered, this,
            &GMainWindow::UpdateSaveStates);

    autosave_timer.setSingleShot(false);
    connect(&autosave_timer, &QTimer::timeout, this, &GMainWindow::OnAutoSaveTick);

    UpdateSaveStates();
}

void GMainWindow::InitializeHotkeys() {
    hotkey_registry.LoadHotkeys();
    hotkey_registry.buttonMonitor.start(16);
    LOG_DEBUG(Frontend, "Initializing hotkeys");
    const QString main_window = QStringLiteral("Main Window");
    const QString fullscreen = QStringLiteral("Fullscreen");

    // QAction Hotkeys
    const auto link_action_shortcut = [&](QAction* action, const QString& action_name,
                                          const bool primary_only = false,
                                          const bool auto_repeat = false) {
        static const QString main_window = QStringLiteral("Main Window");
        auto context = hotkey_registry.GetShortcutContext(main_window, action_name);
        auto shortcut = hotkey_registry.GetKeySequence(main_window, action_name);
        action->setShortcut(shortcut);
        action->setShortcutContext(context);
        action->setAutoRepeat(auto_repeat);
        this->addAction(action);
        // handle the shortcuts that are different per-screen
        if (context == Qt::WidgetShortcut) {
            render_window->addAction(action);
            if (!primary_only) {
                secondary_window->addAction(action);
            }
        }
        hotkey_registry.SetAction(main_window, action_name, action);
    };

    link_action_shortcut(ui->action_Load_File, QStringLiteral("Load File"));
    link_action_shortcut(ui->action_Load_Amiibo, QStringLiteral("Load Amiibo"));
    link_action_shortcut(ui->action_Remove_Amiibo, QStringLiteral("Remove Amiibo"));
    link_action_shortcut(ui->action_Exit, QStringLiteral("Exit Extendo"));
    link_action_shortcut(ui->action_Restart, QStringLiteral("Restart Emulation"));
    link_action_shortcut(ui->action_Pause, QStringLiteral("Continue/Pause Emulation"));
    link_action_shortcut(ui->action_Stop, QStringLiteral("Stop Emulation"));
    link_action_shortcut(ui->action_Show_Filter_Bar, QStringLiteral("Toggle Filter Bar"));
    link_action_shortcut(ui->action_Show_Status_Bar, QStringLiteral("Toggle Status Bar"));
    link_action_shortcut(ui->action_Fullscreen, fullscreen);
    link_action_shortcut(ui->action_Capture_Screenshot, QStringLiteral("Capture Screenshot"));
    link_action_shortcut(ui->action_Debug_Pause, QStringLiteral("Debug Pause"));
    link_action_shortcut(ui->action_Debug_Resume, QStringLiteral("Debug Resume"));
    link_action_shortcut(ui->action_Debug_Step, QStringLiteral("Debug Step"), false, true);
    link_action_shortcut(ui->action_Debug_Unschedule_All, QStringLiteral("Debug Unschedule All"));
    link_action_shortcut(ui->action_Debug_Schedule_All, QStringLiteral("Debug Schedule All"));
    link_action_shortcut(ui->action_Screen_Layout_Swap_Screens, QStringLiteral("Swap Screens"));
    link_action_shortcut(ui->action_Screen_Layout_Upright_Screens,
                         QStringLiteral("Rotate Screens Upright"));
    link_action_shortcut(ui->action_Advance_Frame, QStringLiteral("Advance Frame"));
    link_action_shortcut(ui->action_Load_from_Newest_Slot,
                         QStringLiteral("Load from Newest Non-Quicksave Slot"));
    link_action_shortcut(ui->action_Save_to_Oldest_Slot,
                         QStringLiteral("Save to Oldest Non-Quicksave Slot"));
    link_action_shortcut(ui->action_Quick_Save, QStringLiteral("Quick Save"));
    link_action_shortcut(ui->action_Quick_Load, QStringLiteral("Quick Load"));
    link_action_shortcut(ui->action_View_Lobby, QStringLiteral("Multiplayer Browse Public Rooms"));
    link_action_shortcut(ui->action_Start_Room, QStringLiteral("Multiplayer Create Room"));
    link_action_shortcut(ui->action_Connect_To_Room,
                         QStringLiteral("Multiplayer Direct Connect to Room"));
    link_action_shortcut(ui->action_Show_Room, QStringLiteral("Multiplayer Show Current Room"));
    link_action_shortcut(ui->action_Leave_Room, QStringLiteral("Multiplayer Leave Room"));

    // QShortcut Hotkeys
    const auto connect_shortcut = [&](const QString& action_name, const auto& function) {
        const auto* hotkey = hotkey_registry.GetHotkey(main_window, action_name, this);
        connect(hotkey, &QShortcut::activated, this, function);
    };

    connect_shortcut(QStringLiteral("Toggle Screen Layout"), &GMainWindow::ToggleScreenLayout);
    connect_shortcut(QStringLiteral("Exit Fullscreen"), [&] {
        if (emulation_running) {
            if (secondary_window->isActiveWindow()) {
                secondary_window->showNormal();
            } else {
                ui->action_Fullscreen->setChecked(false);
                ToggleFullscreen();
            }
        }
    });

    connect_shortcut(QStringLiteral("Toggle Per-Application Speed"), [&] {
        if (!hotkey_registry
                 .GetKeySequence(QStringLiteral("Main Window"), QStringLiteral("Toggle Turbo Mode"))
                 .isEmpty()) {
            return;
        }
        Settings::values.frame_limit.SetGlobal(!Settings::values.frame_limit.UsingGlobal());
        UpdateStatusBar();
    });
    connect_shortcut(QStringLiteral("Toggle Texture Dumping"),
                     [&] { Settings::values.dump_textures = !Settings::values.dump_textures; });
    connect_shortcut(QStringLiteral("Toggle Custom Textures"),
                     [&] { Settings::values.custom_textures = !Settings::values.custom_textures; });

    connect_shortcut(QStringLiteral("Toggle Turbo Mode"),
                     [&] { GMainWindow::SetTurboEnabled(!GMainWindow::IsTurboEnabled()); });

    connect_shortcut(QStringLiteral("Increase Speed Limit"), [&] { AdjustSpeedLimit(true); });
    connect_shortcut(QStringLiteral("Decrease Speed Limit"), [&] { AdjustSpeedLimit(false); });

    connect_shortcut(QStringLiteral("Audio Mute/Unmute"), &GMainWindow::OnMute);
    connect_shortcut(QStringLiteral("Audio Volume Down"), &GMainWindow::OnDecreaseVolume);
    connect_shortcut(QStringLiteral("Audio Volume Up"), &GMainWindow::OnIncreaseVolume);

    // We use "static" here in order to avoid capturing by lambda due to a MSVC bug, which makes the
    // variable hold a garbage value after this function exits
    static constexpr u16 FACTOR_3D_STEP = 5;
    connect_shortcut(QStringLiteral("Decrease 3D Factor"), [this] {
        const auto factor_3d = Settings::values.factor_3d.GetValue();
        if (factor_3d > 0) {
            if (factor_3d % FACTOR_3D_STEP != 0) {
                Settings::values.factor_3d = factor_3d - (factor_3d % FACTOR_3D_STEP);
            } else {
                Settings::values.factor_3d = factor_3d - FACTOR_3D_STEP;
            }
            UpdateStatusBar();
        }
    });
    connect_shortcut(QStringLiteral("Increase 3D Factor"), [this] {
        const auto factor_3d = Settings::values.factor_3d.GetValue();
        if (factor_3d < 255) {
            if (factor_3d % FACTOR_3D_STEP != 0) {
                Settings::values.factor_3d =
                    factor_3d + FACTOR_3D_STEP - (factor_3d % FACTOR_3D_STEP);
            } else {
                Settings::values.factor_3d = factor_3d + FACTOR_3D_STEP;
            }
            UpdateStatusBar();
        }
    });
}

void GMainWindow::SetDefaultUIGeometry() {
    // geometry: 55% of the window contents are in the upper screen half, 45% in the lower half
    const QRect screenRect = screen()->geometry();

    const int w = screenRect.width() * 2 / 3;
    const int h = screenRect.height() / 2;
    const int x = (screenRect.x() + screenRect.width()) / 2 - w / 2;
    const int y = (screenRect.y() + screenRect.height()) / 2 - h * 55 / 100;

    setGeometry(x, y, w, h);
}

void GMainWindow::RestoreUIState() {
    restoreGeometry(UISettings::values.geometry);
    restoreState(UISettings::values.state);
    render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
    secondary_window->restoreGeometry(UISettings::values.secondarywindow_geometry);
#if MICROPROFILE_ENABLED
    microProfileDialog->restoreGeometry(UISettings::values.microprofile_geometry);
    microProfileDialog->setVisible(UISettings::values.microprofile_visible.GetValue());
#endif

    game_list->LoadInterfaceLayout();

    ui->action_Single_Window_Mode->setChecked(UISettings::values.single_window_mode.GetValue());
    ToggleWindowMode();

    ui->action_Fullscreen->setChecked(UISettings::values.fullscreen.GetValue());
    SyncMenuUISettings();

    ui->action_Display_Dock_Widget_Headers->setChecked(
        UISettings::values.display_titlebar.GetValue());
    OnDisplayTitleBars(ui->action_Display_Dock_Widget_Headers->isChecked());

    ui->action_Show_Filter_Bar->setChecked(UISettings::values.show_filter_bar.GetValue());
    game_list->SetFilterVisible(ui->action_Show_Filter_Bar->isChecked());

    ui->action_Show_Status_Bar->setChecked(UISettings::values.show_status_bar.GetValue());
    statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
}

void GMainWindow::OnAppFocusStateChanged(Qt::ApplicationState state) {
    if (state != Qt::ApplicationHidden && state != Qt::ApplicationInactive &&
        state != Qt::ApplicationActive) {
        LOG_DEBUG(Frontend, "ApplicationState unusual flag: {} ", state);
    }
    if (!emulation_running) {
        return;
    }
    if (UISettings::values.pause_when_in_background) {
        if (emu_thread->IsRunning() &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            auto_paused = true;
            OnPauseGame();
        } else if (!emu_thread->IsRunning() && auto_paused && state == Qt::ApplicationActive) {
            auto_paused = false;
            OnResumeGame(false);
        }
    }
    if (UISettings::values.mute_when_in_background) {
        if (!Settings::values.audio_muted &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            Settings::values.audio_muted = true;
            auto_muted = true;
        } else if (auto_muted && state == Qt::ApplicationActive) {
            Settings::values.audio_muted = false;
            auto_muted = false;
        }
        UpdateVolumeUI();
    }
}

bool GApplicationEventFilter::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::FileOpen) {
        emit FileOpen(static_cast<QFileOpenEvent*>(event));
        return true;
    }
    return false;
}

void GMainWindow::ConnectAppEvents() {
    const auto filter = new GApplicationEventFilter();
    QGuiApplication::instance()->installEventFilter(filter);

    connect(filter, &GApplicationEventFilter::FileOpen, this, &GMainWindow::OnFileOpen);
}

void GMainWindow::ConnectWidgetEvents() {
    connect(game_list, &GameList::GameChosen, this, &GMainWindow::OnGameListLoadFile);
    connect(game_list, &GameList::OpenDirectory, this, &GMainWindow::OnGameListOpenDirectory);
    connect(game_list, &GameList::OpenFolderRequested, this, &GMainWindow::OnGameListOpenFolder);
    connect(game_list, &GameList::RemovePlayTimeRequested, this,
            &GMainWindow::OnGameListRemovePlayTimeData);
    connect(game_list, &GameList::CreateShortcut, this, &GMainWindow::OnGameListCreateShortcut);
    connect(game_list, &GameList::DumpRomFSRequested, this, &GMainWindow::OnGameListDumpRomFS);
    connect(game_list, &GameList::AddDirectory, this, &GMainWindow::OnGameListAddDirectory);
    connect(game_list_placeholder, &GameListPlaceholder::AddDirectory, this,
            &GMainWindow::OnGameListAddDirectory);
    connect(game_list, &GameList::ShowList, this, &GMainWindow::OnGameListShowList);
    connect(game_list, &GameList::PopulatingCompleted, this,
            [this] { multiplayer_state->UpdateGameList(game_list->GetModel()); });
#ifdef ENABLE_DEVELOPER_OPTIONS
    connect(game_list, &GameList::StartingLaunchStressTest, this,
            &GMainWindow::StartLaunchStressTest);
#endif

    connect(game_list, &GameList::OpenPerGameGeneralRequested, this,
            &GMainWindow::OnGameListOpenPerGameProperties);

    connect(this, &GMainWindow::EmulationStarting, render_window,
            &GRenderWindow::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, render_window,
            &GRenderWindow::OnEmulationStopping);
    connect(this, &GMainWindow::EmulationStarting, secondary_window,
            &GRenderWindow::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, secondary_window,
            &GRenderWindow::OnEmulationStopping);

    connect(&status_bar_update_timer, &QTimer::timeout, this, &GMainWindow::UpdateStatusBar);

    connect(this, &GMainWindow::UpdateProgress, this, &GMainWindow::OnUpdateProgress);
    connect(this, &GMainWindow::CIAInstallReport, this, &GMainWindow::OnCIAInstallReport);
    connect(this, &GMainWindow::CIAInstallFinished, this, &GMainWindow::OnCIAInstallFinished);
    connect(this, &GMainWindow::CompressFinished, this, &GMainWindow::OnCompressFinished);
    connect(this, &GMainWindow::BundleFinished, this, &GMainWindow::OnBundleFinished);
    connect(this, &GMainWindow::UpdateThemedIcons, multiplayer_state,
            &MultiplayerState::UpdateThemedIcons);
}

void GMainWindow::ConnectMenuEvents() {
    const auto connect_menu = [&](QAction* action, const auto& event_fn,
                                  QAction::MenuRole role = QAction::NoRole) {
        action->setMenuRole(role);
        connect(action, &QAction::triggered, this, event_fn);
        // Add actions to this window so that hiding menus in fullscreen won't disable them
        addAction(action);
        // Add actions to the render window so that they work outside of single window mode
        render_window->addAction(action);
    };

    // File
    connect_menu(ui->action_Load_File, &GMainWindow::OnMenuLoadFile);
    connect_menu(ui->action_Install_CIA, &GMainWindow::OnMenuInstallCIA);
    connect_menu(ui->action_Connect_Artic, &GMainWindow::OnMenuConnectArticBase);
    connect_menu(ui->action_Setup_System_Files, &GMainWindow::OnMenuSetUpSystemFiles);
    for (u32 region = 0; region < Core::NUM_SYSTEM_TITLE_REGIONS; region++) {
        connect_menu(ui->menu_Boot_Home_Menu->actions().at(region),
                     [this, region] { OnMenuBootHomeMenu(region); });
    }
    connect_menu(ui->action_Exit, &QMainWindow::close, QAction::QuitRole);
    connect_menu(ui->action_Load_Amiibo, &GMainWindow::OnLoadAmiibo);
    connect_menu(ui->action_Remove_Amiibo, &GMainWindow::OnRemoveAmiibo);
    connect_menu(ui->action_Open_Citra_Folder, &GMainWindow::OnOpenCitraFolder);
    connect_menu(ui->action_Open_NAND_Folder, &GMainWindow::OnOpenNANDFolder);
    connect_menu(ui->action_Open_SDMC_Folder, &GMainWindow::OnOpenSDMCFolder);

    // Emulation
    connect_menu(ui->action_Pause, &GMainWindow::OnPauseContinueGame);
    connect_menu(ui->action_Stop, &GMainWindow::PromptQuitGame);
    connect_menu(ui->action_Restart, [this] { BootGame(QString(game_path)); });
    connect_menu(ui->action_Report_Compatibility, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://github.com/azahar-emu/compatibility-list/blob/master/CONTRIBUTING.md")));
    });
    connect_menu(ui->action_Configure, &GMainWindow::OnConfigure, QAction::PreferencesRole);
    connect_menu(ui->action_Configure_Current_Game, &GMainWindow::OnConfigurePerGame);

    // View
    connect_menu(ui->action_Single_Window_Mode, &GMainWindow::ToggleWindowMode);
    connect_menu(ui->action_Display_Dock_Widget_Headers, &GMainWindow::OnDisplayTitleBars);
    connect_menu(ui->action_Show_Filter_Bar, &GMainWindow::OnToggleFilterBar);
    connect(ui->action_Show_Status_Bar, &QAction::triggered, statusBar(), &QStatusBar::setVisible);

    // Multiplayer
    connect(ui->action_View_Lobby, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnViewLobby);
    connect(ui->action_Start_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnCreateRoom);
    connect(ui->action_Leave_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnCloseRoom);
    connect(ui->action_Connect_To_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnDirectConnectToRoom);
    connect(ui->action_Show_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnOpenNetworkRoom);

    connect_menu(ui->action_Fullscreen, &GMainWindow::ToggleFullscreen);
    connect_menu(ui->action_Screen_Layout_Default, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Single_Screen, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Large_Screen, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Hybrid_Screen, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Side_by_Side, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Separate_Windows, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Custom_Layout, &GMainWindow::ChangeScreenLayout);
    connect_menu(ui->action_Screen_Layout_Swap_Screens, &GMainWindow::OnSwapScreens);
    connect_menu(ui->action_Screen_Layout_Upright_Screens, &GMainWindow::OnRotateScreens);
    connect_menu(ui->action_Small_Screen_TopRight, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_MiddleRight, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_BottomRight, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_TopLeft, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_MiddleLeft, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_BottomLeft, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_Above, &GMainWindow::ChangeSmallScreenPosition);
    connect_menu(ui->action_Small_Screen_Below, &GMainWindow::ChangeSmallScreenPosition);

    // Movie
    connect_menu(ui->action_Record_Movie, &GMainWindow::OnRecordMovie);
    connect_menu(ui->action_Play_Movie, &GMainWindow::OnPlayMovie);
    connect_menu(ui->action_Close_Movie, &GMainWindow::OnCloseMovie);
    connect_menu(ui->action_Save_Movie, &GMainWindow::OnSaveMovie);
    connect_menu(ui->action_Movie_Read_Only_Mode,
                 [this](bool checked) { movie.SetReadOnly(checked); });
    connect_menu(ui->action_Advance_Frame, [this] {
        if (emulation_running && system.frame_limiter.IsFrameAdvancing()) {
            system.frame_limiter.AdvanceFrame();
        }
    });
    connect_menu(ui->action_Capture_Screenshot, &GMainWindow::OnCaptureScreenshot);
#ifdef ENABLE_FFMPEG
    connect_menu(ui->action_Dump_Video, &GMainWindow::OnDumpVideo);
#endif

    // Tools debug
    connect_menu(ui->action_Debug_Pause, [this] {
        if (emu_thread) {
            emu_thread->SetRunning(false);
        }
    });
    connect_menu(ui->action_Debug_Resume, [this] {
        if (emu_thread) {
            emu_thread->SetRunning(true);
        }
    });
    connect_menu(ui->action_Debug_Step, [this] {
        if (emu_thread) {
            emu_thread->ExecStep();
        }
    });
    connect_menu(ui->action_Debug_Unschedule_All,
                 [this] { system.DebugUnscheduleAllThreadsFromFrontend(true); });
    connect_menu(ui->action_Debug_Schedule_All,
                 [this] { system.DebugUnscheduleAllThreadsFromFrontend(false); });

    // Tools
    connect_menu(ui->action_Compress_ROM_File, &GMainWindow::OnCompressFile);
    connect_menu(ui->action_Decompress_ROM_File, &GMainWindow::OnDecompressFile);
    connect_menu(ui->action_Create_ROM_Bundle, &GMainWindow::OnCreateBundle);
    connect_menu(ui->action_Extract_ROM_Bundle, &GMainWindow::OnExtractBundle);

    // Help
    connect_menu(ui->action_Open_Log_Folder, []() {
        QString path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::LogDir));
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    connect_menu(ui->action_FAQ, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://azahar-emu.org/pages/faq/")));
    });
    connect_menu(ui->action_About, &GMainWindow::OnMenuAboutCitra, QAction::AboutRole);
}

void GMainWindow::UpdateMenuState() {
    const bool is_paused =
        !emu_thread || !emu_thread->IsRunning() || system.frame_limiter.IsFrameAdvancing();

    const std::array running_actions{
        ui->action_Stop,
        ui->action_Restart,
        ui->action_Configure_Current_Game,
        ui->action_Report_Compatibility,
        ui->action_Load_Amiibo,
        ui->action_Remove_Amiibo,
        ui->action_Pause,
        ui->action_Advance_Frame,
    };

    for (QAction* action : running_actions) {
        action->setEnabled(emulation_running);
    }

    ui->action_Capture_Screenshot->setEnabled(emulation_running);
    ui->action_Advance_Frame->setEnabled(emulation_running && is_paused);

    if (emulation_running && is_paused) {
        ui->action_Pause->setText(tr("Continue"));
    } else {
        ui->action_Pause->setText(tr("Pause"));
    }
}

void GMainWindow::OnDisplayTitleBars(bool show) {
    QList<QDockWidget*> widgets = findChildren<QDockWidget*>();

    if (show) {
        for (QDockWidget* widget : widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(nullptr);
            if (old) {
                delete old;
            }
        }
    } else {
        for (QDockWidget* widget : widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(new QWidget());
            if (old) {
                delete old;
            }
        }
    }
}

#if defined(HAVE_SDL2) && defined(__unix__) && !defined(__APPLE__)
static std::optional<QDBusObjectPath> HoldWakeLockLinux(u32 window_id = 0) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        return {};
    }
    // reference: https://flatpak.github.io/xdg-desktop-portal/#gdbus-org.freedesktop.portal.Inhibit
    QDBusInterface xdp(QStringLiteral("org.freedesktop.portal.Desktop"),
                       QStringLiteral("/org/freedesktop/portal/desktop"),
                       QStringLiteral("org.freedesktop.portal.Inhibit"));
    if (!xdp.isValid()) {
        LOG_WARNING(Frontend, "Couldn't connect to XDP D-Bus endpoint");
        return {};
    }
    QVariantMap options = {};
    //: TRANSLATORS: This string is shown to the user to explain why Citra needs to prevent the
    //: computer from sleeping
    options.insert(QString::fromLatin1("reason"),
                   QCoreApplication::translate("GMainWindow", "Extendo is running an application"));
    // 0x4: Suspend lock; 0x8: Idle lock
    QDBusReply<QDBusObjectPath> reply =
        xdp.call(QString::fromLatin1("Inhibit"),
                 QString::fromLatin1("x11:") + QString::number(window_id, 16), 12U, options);

    if (reply.isValid()) {
        return reply.value();
    }
    LOG_WARNING(Frontend, "Couldn't read Inhibit reply from XDP: {}",
                reply.error().message().toStdString());
    return {};
}

static void ReleaseWakeLockLinux(const QDBusObjectPath& lock) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        return;
    }
    QDBusInterface unlocker(QString::fromLatin1("org.freedesktop.portal.Desktop"), lock.path(),
                            QString::fromLatin1("org.freedesktop.portal.Request"));
    unlocker.call(QString::fromLatin1("Close"));
}
#endif // __unix__

void GMainWindow::PreventOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#elif defined(HAVE_SDL2)
    SDL_DisableScreenSaver();
#if defined(__unix__) && !defined(__APPLE__)
    auto reply = HoldWakeLockLinux(winId());
    if (reply) {
        wake_lock = std::move(reply.value());
    }
#endif // defined(__unix__) && !defined(__APPLE__)
#endif // _WIN32
}

void GMainWindow::AllowOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS);
#elif defined(HAVE_SDL2)
    SDL_EnableScreenSaver();
#if defined(__unix__) && !defined(__APPLE__)
    if (!wake_lock.path().isEmpty()) {
        ReleaseWakeLockLinux(wake_lock);
    }
#endif // defined(__unix__) && !defined(__APPLE__)
#endif // _WIN32
}

bool GMainWindow::LoadROM(const QString& filename) {
    // Shutdown previous session if the emu thread is still active...
    if (emu_thread) {
        ShutdownGame();
    }

    if (!render_window->InitRenderTarget() || !secondary_window->InitRenderTarget()) {
        LOG_CRITICAL(Frontend, "Failed to initialize render targets!");
        return false;
    }

    const auto scope = render_window->Acquire();

    if (!UISettings::values.inserted_cartridge.GetValue().empty()) {
        system.InsertCartridge(UISettings::values.inserted_cartridge.GetValue());
    }

    const Core::System::ResultStatus result{
        system.Load(*render_window, filename.toStdString(), secondary_window)};

    if (result != Core::System::ResultStatus::Success) {
        QString invalid_format = tr("Invalid application format");
        QString invalid_format_description =
            tr("The application file format not supported.<br>Please make sure you are using one "
               "of the compatible file formats:<ul><li>Cartridge images: "
               "<b>.cci/.zcci/.bcci/.3ds</b></li><li>Installable archives: "
               "<b>.cia/.zcia/.bcia</b></li><li>Homebrew titles: <b>.3dsx/.z3dsx</b></li><li>NCCH "
               "containers: <b>.cxi/.zcxi/.bcxi/.app</b></li><li>ELF files: "
               "<b>.elf/.axf</b></li></ul>");

        switch (result) {
        case Core::System::ResultStatus::ErrorGetLoader:
            LOG_CRITICAL(Frontend, "Failed to obtain loader for {}", filename.toStdString());
            QMessageBox::critical(this, invalid_format, invalid_format_description);
            break;

        case Core::System::ResultStatus::ErrorSystemMode:
            LOG_CRITICAL(Frontend, "Failed to load application!");
            QMessageBox::critical(this, invalid_format, invalid_format_description);
            break;

        case Core::System::ResultStatus::ErrorLoader_ErrorEncrypted: {
            QMessageBox::critical(this, tr("Encrypted application"),
                                  tr("Encrypted applications are not supported.<br/>"
                                     "<a "
                                     "href='https://azahar-emu.org/blog/game-loading-changes/'>"
                                     "Please check our blog for more info.</a>"));
            break;
        }
        case Core::System::ResultStatus::ErrorLoader_ErrorInvalidFormat:
            QMessageBox::critical(this, invalid_format, invalid_format_description);
            break;

        case Core::System::ResultStatus::ErrorLoader_ErrorGbaTitle:
            QMessageBox::critical(this, tr("Unsupported application"),
                                  tr("GBA Virtual Console is not supported by Azahar."));
            break;

        case Core::System::ResultStatus::ErrorArticDisconnected:
            QMessageBox::critical(
                this, tr("Artic Server"),
                tr(fmt::format(
                       "An error has occurred whilst communicating with the Artic Server.\n{}",
                       system.GetStatusDetails())
                       .c_str()));
            break;
        case Core::System::ResultStatus::ErrorN3DSApplication:
            QMessageBox::critical(this, tr("Invalid system mode"),
                                  tr("New 3DS exclusive applications cannot be loaded without "
                                     "enabling the New 3DS mode."));
            break;
        case Core::System::ResultStatus::ErrorLoader:
            QMessageBox::critical(this, tr("Generic load error"),
                                  tr("A generic load error occurred while loading the "
                                     "application.<br/>Please check the log for more details."));
            break;
        case Core::System::ResultStatus::ErrorLoader_ErrorPatches:
            QMessageBox::critical(this, tr("Error applying patches"),
                                  tr("A generic error occurred while applying a patch to the "
                                     "application.<br/>Please check the log for more details."));
            break;
        case Core::System::ResultStatus::ErrorLoader_ErrorPatchesInvalidTitle:
            QMessageBox::critical(
                this, tr("Error applying patches"),
                tr("Failed to apply a patch because it is designed for a different "
                   "application.<br/>Please make sure you are using the patches for "
                   "the right application, region and version."));
            break;
        default:
            QMessageBox::critical(
                this, tr("Error while loading application"),
                tr("An unknown error occurred.<br/>Please see the log for more details."));
            break;
        }
        return false;
    }

    std::string title;
    system.GetAppLoader().ReadTitle(title, true);
    game_title = QString::fromStdString(title);
    UpdateWindowTitle();

    u64 title_id;
    system.GetAppLoader().ReadProgramId(title_id);

    game_path = filename;
    game_title_id = title_id;

    return true;
}

void GMainWindow::BootGame(const QString& filename) {
    if (emu_thread) {
        ShutdownGame();
    }

    // NOTE: We could make this not hardcoded, but it's a temp thing anyway so probs not worth. -OS
    if (Settings::values.graphics_api.GetValue() == Settings::GraphicsAPI::Vulkan &&
        Settings::values.texture_filter.GetValue() == Settings::TextureFilter::Anime4K) {
        const auto choice = ConsoleUI::ShowConfirmModal(
            this, tr("Incompatible texture filter"),
            tr("The selected texture filter (Anime4K) is not compatible with the Vulkan "
               "renderer.\nThe texture filter will be set to None.\nProceed?"),
            tr("Proceed"), {}, tr("Cancel"));

        if (choice != ConsoleUI::ActionChoice::Primary) {
            return;
        }
        Settings::values.texture_filter = Settings::TextureFilter::NoFilter;
    }

    const bool is_artic = filename.startsWith(QString::fromStdString("articbase:/")) ||
                          filename.startsWith(QString::fromStdString("articinio:/")) ||
                          filename.startsWith(QString::fromStdString("articinin:/"));

    if (!is_artic && filename.endsWith(QStringLiteral(".cia"))) {
        const auto choice = ConsoleUI::ShowConfirmModal(
            this, tr("CIA must be installed before usage"),
            tr("Before using this CIA, you must install it. Do you want to install it now?"),
            tr("Install"), {}, tr("Cancel"));

        if (choice == ConsoleUI::ActionChoice::Primary)
            InstallCIA(QStringList(filename));

        return;
    }

    show_artic_label = is_artic;

    LOG_INFO(Frontend, "Extendo starting...");
    if (!is_artic) {
        StoreRecentFile(filename); // Put the filename on top of the list
    }

    if (movie_record_on_start) {
        movie.PrepareForRecording();
    }
    if (movie_playback_on_start) {
        movie.PrepareForPlayback(movie_playback_path.toStdString());
    }

    const std::string path = filename.toStdString();
    auto loader = Loader::GetLoader(path);

    u64 title_id{0};
    Loader::ResultStatus res =
        loader ? loader->ReadProgramId(title_id) : Loader::ResultStatus::Error;

    if (Loader::ResultStatus::Success == res) {
        // Load per game settings
        const std::string name{is_artic ? "" : FileUtil::GetFilename(filename.toStdString())};
        const std::string config_file_name =
            title_id == 0 ? name : fmt::format("{:016X}", title_id);
        LOG_INFO(Frontend, "Loading per application config file for title {}", config_file_name);
        QtConfig per_game_config(config_file_name, QtConfig::ConfigType::PerGameConfig);
    }

    // Artic Server cannot accept a client multiple times, so multiple loaders are not
    // possible. Instead register the app loader early and do not create it again on system load.
    if (loader && !loader->SupportsMultipleInstancesForSameFile()) {
        system.RegisterAppLoaderEarly(loader);
    }

    // Override GDB settings if emulator was launched with
    // GDB port option.
    if (gdbport_from_arg != -1) {
        system.SetGDBPortOverride(gdbport_from_arg);
        system.SetDebugNextProcessFlag();
    }

    system.ApplySettings();

    Settings::LogSettings();

    // Save configurations
    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    config->Save();

    if (!LoadROM(filename)) {
        render_window->ReleaseRenderTarget();
        secondary_window->ReleaseRenderTarget();
        return;
    }

    // Set everything up
    if (movie_record_on_start) {
        movie.StartRecording(movie_record_path.toStdString(), movie_record_author.toStdString());
        movie_record_on_start = false;
        movie_record_path.clear();
        movie_record_author.clear();
    }
    if (movie_playback_on_start) {
        movie.StartPlayback(movie_playback_path.toStdString());
        movie_playback_on_start = false;
        movie_playback_path.clear();
    }

    ui->action_Advance_Frame->setEnabled(false);

#ifdef ENABLE_FFMPEG
    if (video_dumping_on_start) {
        StartVideoDumping(video_dumping_path);
        video_dumping_on_start = false;
        video_dumping_path.clear();
    }
#endif()

    // Register debug widgets
    if (graphicsWidget && graphicsWidget->isVisible()) {
        graphicsWidget->Register();
    }

    // Checked once at boot rather than per tick: the archive gains content as soon as the game
    // saves, and a title that starts saving mid-session should not silently lose the forced
    // protection it was given.
    forced_autosave_title = TitleLacksInGameSave();
    if (forced_autosave_title) {
        LOG_INFO(Frontend, "Title {:016X} keeps no save data; forcing auto-resume saves",
                 game_title_id);
    }

    bool should_auto_resume = false;
    u32 resume_slot = Core::AutoResumeSlot;
    auto resume_info = Core::GetSaveStateInfo(game_title_id, 0, Core::AutoResumeSlot);
    if (resume_info.slot != Core::AutoResumeSlot) {
        resume_info = Core::GetSaveStateInfo(game_title_id, 0, 0);
        if (resume_info.slot == 0) {
            resume_slot = 0;
        }
    }
    if (resume_info.slot == resume_slot) {
        const auto choice = ConsoleUI::ShowConfirmModal(
            this, tr("Resume Session"),
            tr("A previous saved session was found for this game.\nWould you like to resume where you left off?"),
            tr("Resume"), {}, tr("Start from Beginning"));
        if (choice == ConsoleUI::ActionChoice::Primary) {
            should_auto_resume = true;
        } else {
            const auto resume_path = Core::GetSaveStatePath(game_title_id, 0, resume_slot);
            FileUtil::Delete(resume_path);
        }
    }

    // Create and start the emulation thread
    emu_thread = std::make_unique<EmuThread>(system, *render_window);
    emit EmulationStarting(emu_thread.get());
    emu_thread->start();

    connect(render_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);
    connect(render_window, &GRenderWindow::MouseActivity, this, &GMainWindow::OnMouseActivity);
    connect(secondary_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);
    connect(secondary_window, &GRenderWindow::MouseActivity, this, &GMainWindow::OnMouseActivity);

    // BlockingQueuedConnection is important here, it makes sure we've finished refreshing our views
    // before the CPU continues
    connect(emu_thread.get(), &EmuThread::DebugModeEntered, registersWidget,
            &RegistersWidget::OnDebugModeEntered, Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), &EmuThread::DebugModeEntered, waitTreeWidget,
            &WaitTreeWidget::OnDebugModeEntered, Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), &EmuThread::DebugModeLeft, registersWidget,
            &RegistersWidget::OnDebugModeLeft, Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), &EmuThread::DebugModeLeft, waitTreeWidget,
            &WaitTreeWidget::OnDebugModeLeft, Qt::BlockingQueuedConnection);

    connect(emu_thread.get(), &EmuThread::LoadProgress, loading_screen,
            &LoadingScreen::OnLoadProgress, Qt::QueuedConnection);
    connect(emu_thread.get(), &EmuThread::SwitchDiskResources, this,
            &GMainWindow::OnSwitchDiskResources, Qt::QueuedConnection);
    connect(emu_thread.get(), &EmuThread::HideLoadingScreen, loading_screen,
            &LoadingScreen::OnLoadComplete);

    // Update the GUI
    registersWidget->OnDebugModeEntered();
    if (ui->action_Single_Window_Mode->isChecked()) {
        game_list->hide();
        game_list_placeholder->hide();
        home_screen->hide();
    }
    status_bar_update_timer.start(1000);

    if (UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
        setMouseTracking(true);
    }

    loading_screen->Prepare(system.GetAppLoader());
    loading_screen->show();

    emulation_running = true;
    if (ui->action_Fullscreen->isChecked()) {
        ShowFullscreen();
    }

    RepositionHomeButton();
    home_button->show();
    home_button->raise();

    RepositionInGameStatus();
    if (ingame_status) {
        ingame_status->RefreshStatus();
        ingame_status->show();
        ingame_status->raise();
    }

    OnResumeGame(true);

    if (should_auto_resume) {
        system.SendSignal(Core::System::Signal::Load, resume_slot);
        system.frame_limiter.AdvanceFrame();
    }
}

void GMainWindow::ShutdownGame() {
    if (!emulation_running) {
        return;
    }

    autosave_timer.stop();

    if (ui->action_Fullscreen->isChecked()) {
        HideFullscreen();
    }

#ifdef ENABLE_FFMPEG
    auto video_dumper = system.GetVideoDumper();
    if (video_dumper && video_dumper->IsDumping()) {
        game_shutdown_delayed = true;
        OnStopVideoDumping();
        return;
    }
#endif

    AllowOSSleep();

#ifdef ENABLE_DISCORD_RPC
    discord_rpc->Pause();
#endif

    emu_thread->RequestStop();

    // Release emu threads from any breakpoints
    // This belongs after RequestStop() and before wait() because if emulation stops on a GPU
    // breakpoint after (or before) RequestStop() is called, the emulation would never be able
    // to continue out to the main loop and terminate. Thus wait() would hang forever.
    // TODO(bunnei): This function is not thread safe, but it's being used as if it were
    if (Pica::g_debug_context) {
        Pica::g_debug_context->ClearBreakpoints();
    }

    // Unregister debug widgets
    if (graphicsWidget && graphicsWidget->isVisible()) {
        graphicsWidget->Unregister();
    }

    // Frame advancing must be cancelled in order to release the emu thread from waiting
    system.frame_limiter.SetFrameAdvancing(false);

    emit EmulationStopping();

    // Wait for emulation thread to complete and delete it
    emu_thread->wait();
    emu_thread = nullptr;

    system.EjectCartridge();

    OnCloseMovie();

#ifdef ENABLE_DISCORD_RPC
    discord_rpc->Update(false);
#endif
#ifdef __unix__
    Common::Linux::StopGamemode();
#endif

    // The emulation is stopped, so closing the window or not does not matter anymore
    disconnect(render_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);
    disconnect(secondary_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);

    render_window->hide();
    secondary_window->hide();
    loading_screen->hide();
    loading_screen->Clear();

    game_list_placeholder->hide();
    game_list->hide();
    home_screen->Refresh();
    home_screen->show();

    setMouseTracking(false);

    // Disable status bar updates
    status_bar_update_timer.stop();
    message_label_used_for_movie = false;
    show_artic_label = false;
    loading_shaders_label->setVisible(false);
    artic_traffic_label->setVisible(false);
    emu_speed_label->setVisible(false);
    game_fps_label->setVisible(false);
    emu_frametime_label->setVisible(false);
    notification_led->setColor(QColor(0, 0, 0));

    UpdateSaveStates();

    emulation_running = false;
    home_button->hide();
    if (ingame_status) {
        ingame_status->hide();
    }
    HideControllersPopover();
    screen_divider->hide();

    game_title.clear();
    UpdateWindowTitle();

    game_path.clear();
    game_title_id = 0;

    // Update the GUI
    UpdateMenuState();

    // When closing the game, destroy the GLWindow to clear the context after the game is closed
    render_window->ReleaseRenderTarget();
    secondary_window->ReleaseRenderTarget();
}

#ifdef ENABLE_DEVELOPER_OPTIONS
void GMainWindow::StartLaunchStressTest(const QString& game_path) {
    QThreadPool::globalInstance()->start([this, game_path] {
        do {
            ui->action_Stop->trigger();
            emit game_list->GameChosen(game_path);
            QThread::sleep(2);
        } while (emulation_running);
    });
}
#endif

void GMainWindow::StoreRecentFile(const QString& filename) {
    UISettings::values.recent_files.prepend(filename);
    UISettings::values.recent_files.removeDuplicates();
    while (UISettings::values.recent_files.size() > max_recent_files_item) {
        UISettings::values.recent_files.removeLast();
    }

    UpdateRecentFiles();
}

void GMainWindow::UpdateRecentFiles() {
    const int num_recent_files =
        std::min(static_cast<int>(UISettings::values.recent_files.size()), max_recent_files_item);

    for (int i = 0; i < num_recent_files; i++) {
        const QString text = QStringLiteral("&%1. %2").arg(i + 1).arg(
            QFileInfo(UISettings::values.recent_files[i]).fileName());
        actions_recent_files[i]->setText(text);
        actions_recent_files[i]->setData(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setToolTip(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setVisible(true);
    }

    for (int j = num_recent_files; j < max_recent_files_item; ++j) {
        actions_recent_files[j]->setVisible(false);
    }

    // Enable the recent files menu if the list isn't empty
    ui->menu_recent_files->setEnabled(num_recent_files != 0);
}

void GMainWindow::UpdateSaveStates() {
    if (!system.IsPoweredOn()) {
        ui->menu_Load_State->setEnabled(false);
        ui->menu_Save_State->setEnabled(false);
        return;
    }

    ui->menu_Load_State->setEnabled(true);
    ui->menu_Save_State->setEnabled(true);
    ui->action_Load_from_Newest_Slot->setEnabled(false);

    oldest_slot = newest_slot = 1;
    oldest_slot_time = std::numeric_limits<u64>::max();
    newest_slot_time = 0;

    u64 title_id;
    if (system.GetAppLoader().ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        return;
    }
    auto savestates = Core::ListSaveStates(title_id, movie.GetCurrentMovieID());
    for (u32 i = 0; i < Core::SaveStateSlotCount; ++i) {
        actions_load_state[i]->setEnabled(false);
        if (i == 0) {
            actions_load_state[i]->setText(tr("Quick Load"));
            actions_save_state[i]->setText(tr("Quick Save"));
        } else {
            actions_load_state[i]->setText(tr("Slot %1").arg(i));
            actions_save_state[i]->setText(tr("Slot %1").arg(i));
        }
    }
    for (const auto& savestate : savestates) {
        if (savestate.slot >= Core::SaveStateSlotCount) {
            continue;
        }
        actions_load_state[savestate.slot]->setEnabled(true);
        if (savestate.slot == 0) {
            const auto text = QStringLiteral("%2")
                                  .arg(QDateTime::fromSecsSinceEpoch(savestate.time)
                                           .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")))
                                  .trimmed();
            ui->action_Quick_Save->setText(tr("Quick Save - %1").arg(text).trimmed());
            ui->action_Quick_Load->setText(tr("Quick Load - %1").arg(text).trimmed());
            continue;
        }
        const auto text = tr("Slot %1 - %2")
                              .arg(savestate.slot)
                              .arg(QDateTime::fromSecsSinceEpoch(savestate.time)
                                       .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")))
                              .trimmed();

        actions_load_state[savestate.slot]->setText(text);
        actions_save_state[savestate.slot]->setText(text);

        ui->action_Load_from_Newest_Slot->setEnabled(true);
        if (savestate.time > newest_slot_time) {
            newest_slot = savestate.slot;
            newest_slot_time = savestate.time;
        }
        if (savestate.time < oldest_slot_time) {
            oldest_slot = savestate.slot;
            oldest_slot_time = savestate.time;
        }
    }
    // Value as 1 because quicksave slot is not used for this calculation
    for (u32 i = 1; i < Core::SaveStateSlotCount; ++i) {
        if (!actions_load_state[i]->isEnabled()) {
            // Prefer empty slot
            oldest_slot = i;
            oldest_slot_time = 0;
            break;
        }
    }
}

void GMainWindow::OnGameListLoadFile(QString game_path) {
    if (ConfirmChangeGame()) {
        BootGame(game_path);
    }
}

void GMainWindow::OnGameListOpenFolder(u64 data_id, GameListOpenTarget target) {
    std::string path;
    std::string open_target;

    switch (target) {
    case GameListOpenTarget::SAVE_DATA: {
        open_target = "Save Data";
        std::string sdmc_dir = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
        path = FileSys::ArchiveSource_SDSaveData::GetSaveDataPathFor(sdmc_dir, data_id);
        break;
    }
    case GameListOpenTarget::EXT_DATA: {
        open_target = "Extra Data";
        std::string sdmc_dir = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
        path = FileSys::GetExtDataPathFromId(sdmc_dir, data_id);
        break;
    }
    case GameListOpenTarget::APPLICATION: {
        open_target = "Application";
        auto media_type = Service::AM::GetTitleMediaType(data_id);
        path = Service::AM::GetTitlePath(media_type, data_id) + "content/";
        break;
    }
    case GameListOpenTarget::UPDATE_DATA: {
        open_target = "Update Data";
        path = Service::AM::GetTitlePath(Service::FS::MediaType::SDMC, data_id + 0xe00000000) +
               "content/";
        break;
    }
    case GameListOpenTarget::TEXTURE_DUMP: {
        open_target = "Dumped Textures";
        path = fmt::format("{}textures/{:016X}/",
                           FileUtil::GetUserPath(FileUtil::UserPath::DumpDir), data_id);
        break;
    }
    case GameListOpenTarget::TEXTURE_LOAD: {
        open_target = "Custom Textures";
        path = fmt::format("{}textures/{:016X}/",
                           FileUtil::GetUserPath(FileUtil::UserPath::LoadDir), data_id);
        break;
    }
    case GameListOpenTarget::MODS: {
        open_target = "Mods";
        path = fmt::format("{}mods/{:016X}/", FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                           data_id);
        break;
    }
    case GameListOpenTarget::DLC_DATA: {
        open_target = "DLC Data";
        path = fmt::format("{}Nintendo 3DS/00000000000000000000000000000000/"
                           "00000000000000000000000000000000/title/0004008c/{:08x}/content/",
                           FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), data_id);
        break;
    }
    case GameListOpenTarget::SHADER_CACHE: {
        open_target = "Shader Cache";
        path = FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir);
        break;
    }
    default:
        LOG_ERROR(Frontend, "Unexpected target {}", static_cast<int>(target));
        return;
    }

    QString qpath = QString::fromStdString(path);

    QDir dir(qpath);
    if (!dir.exists()) {
        ConsoleUI::ShowAlertModal(
            this, tr("Error Opening %1 Folder").arg(QString::fromStdString(open_target)),
            tr("Folder does not exist!"), ConsoleUI::AlertType::Error);
        return;
    }

    LOG_INFO(Frontend, "Opening {} path for data_id={:016x}", open_target, data_id);

    QDesktopServices::openUrl(QUrl::fromLocalFile(qpath));
}

void GMainWindow::OnGameListRemovePlayTimeData(u64 program_id) {
    if (ConsoleUI::ShowConfirmModal(this, tr("Remove Play Time Data"), tr("Reset play time?"),
                                     tr("Reset"), {}, tr("Cancel")) !=
        ConsoleUI::ActionChoice::Primary) {
        return;
    }

    play_time_manager->ResetProgramPlayTime(program_id);
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

bool GMainWindow::CreateShortcutLink(const std::filesystem::path& shortcut_path,
                                     const std::string& comment,
                                     const std::filesystem::path& icon_path,
                                     const std::string& command, const std::string& arguments,
                                     const std::string& categories, const std::string& keywords,
                                     const std::string& name, const bool& skip_tryexec) try {
#if defined(__linux__) || defined(__FreeBSD__) // Linux and FreeBSD
    std::filesystem::path shortcut_path_full = shortcut_path / (name + ".desktop");
    std::ofstream shortcut_stream(shortcut_path_full, std::ios::binary | std::ios::trunc);
    if (!shortcut_stream.is_open()) {
        LOG_ERROR(Frontend, "Failed to create shortcut");
        return false;
    }
    // TODO: Migrate fmt::print to std::print in futures STD C++ 23.
    fmt::print(shortcut_stream, "[Desktop Entry]\n");
    fmt::print(shortcut_stream, "Type=Application\n");
    fmt::print(shortcut_stream, "Version=1.0\n");
    fmt::print(shortcut_stream, "Name={}\n", name);
    if (!comment.empty()) {
        fmt::print(shortcut_stream, "Comment={}\n", comment);
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        fmt::print(shortcut_stream, "Icon={}\n", icon_path.string());
    }
    if (!skip_tryexec) {
        fmt::print(shortcut_stream, "TryExec={}\n", command);
    }
    fmt::print(shortcut_stream, "Exec={} {}\n", command, arguments);
    if (!categories.empty()) {
        fmt::print(shortcut_stream, "Categories={}\n", categories);
    }
    if (!keywords.empty()) {
        fmt::print(shortcut_stream, "Keywords={}\n", keywords);
    }
    return true;
#elif defined(_WIN32) // Windows
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        LOG_ERROR(Frontend, "CoInitialize failed");
        return false;
    }
    SCOPE_EXIT({ CoUninitialize(); });
    IShellLinkW* ps1 = nullptr;
    IPersistFile* persist_file = nullptr;
    SCOPE_EXIT({
        if (persist_file != nullptr) {
            persist_file->Release();
        }
        if (ps1 != nullptr) {
            ps1->Release();
        }
    });
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                    reinterpret_cast<void**>(&ps1));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to create IShellLinkW instance");
        return false;
    }
    hres = ps1->SetPath(Common::UTF8ToUTF16W(command).data());
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to set path");
        return false;
    }
    if (!arguments.empty()) {
        hres = ps1->SetArguments(Common::UTF8ToUTF16W(arguments).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set arguments");
            return false;
        }
    }
    if (!comment.empty()) {
        hres = ps1->SetDescription(Common::UTF8ToUTF16W(comment).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set description");
            return false;
        }
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        hres = ps1->SetIconLocation(icon_path.c_str(), 0);
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set icon location");
            return false;
        }
    }
    hres = ps1->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persist_file));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to get IPersistFile interface");
        return false;
    }
    hres = persist_file->Save(
        std::filesystem::path{shortcut_path / (Common::UTF8ToUTF16W(name) + L".lnk")}.c_str(),
        TRUE);
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to save shortcut");
        return false;
    }
    return true;
#else                 // Unsupported platform
    return false;
#endif
} catch (const std::exception& e) {
    LOG_ERROR(Frontend, "Failed to create shortcut: {}", e.what());
    return false;
}

// Messages in pre-defined message boxes for less code spaghetti
bool GMainWindow::CreateShortcutMessagesGUI(QWidget* parent, int message,
                                            const QString& game_title) {
    int result = 0;
    QMessageBox::StandardButtons buttons;
    switch (message) {
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_FULLSCREEN_PROMPT:
        buttons = QMessageBox::Yes | QMessageBox::No;
        result = QMessageBox::information(
            parent, tr("Create Shortcut"),
            tr("Do you want to launch the application in fullscreen?"), buttons);
        return result == QMessageBox::Yes;
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_SUCCESS:
        QMessageBox::information(parent, tr("Create Shortcut"),
                                 tr("Successfully created a shortcut to %1").arg(game_title));
        return false;
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_APPIMAGE_VOLATILE_WARNING:
        buttons = QMessageBox::StandardButton::Ok | QMessageBox::StandardButton::Cancel;
        result =
            QMessageBox::warning(this, tr("Create Shortcut"),
                                 tr("This will create a shortcut to the current AppImage. This may "
                                    "not work well if you update. Continue?"),
                                 buttons);
        return result == QMessageBox::Ok;
    default:
        buttons = QMessageBox::Ok;
        QMessageBox::critical(parent, tr("Create Shortcut"),
                              tr("Failed to create a shortcut to %1").arg(game_title), buttons);
        return false;
    }
}

bool GMainWindow::MakeShortcutIcoPath(const u64 program_id, const std::string_view game_file_name,
                                      std::filesystem::path& out_icon_path) {
    // Get path to Citra icons directory & icon extension
    std::string ico_extension = "png";
#if defined(_WIN32)
    out_icon_path = FileUtil::GetUserPath(FileUtil::UserPath::IconsDir);
    ico_extension = "ico";
#elif defined(__linux__) || defined(__FreeBSD__)
    out_icon_path = FileUtil::GetUserDirectory("XDG_DATA_HOME") + "/icons/hicolor/256x256/";
#endif
    // Create icons directory if it doesn't exist
    if (!FileUtil::CreateFullPath(out_icon_path.string())) {
        QMessageBox::critical(
            this, tr("Create Icon"),
            tr("Cannot create icon file. Path \"%1\" does not exist and cannot be created.")
                .arg(QString::fromStdString(out_icon_path.string())),
            QMessageBox::StandardButton::Ok);
        out_icon_path.clear();
        return false;
    }

    // Create icon file path
    out_icon_path /= (program_id == 0 ? fmt::format("citra-{}.{}", game_file_name, ico_extension)
                                      : fmt::format("citra-{:016X}.{}", program_id, ico_extension));
    return true;
}

void GMainWindow::OnGameListCreateShortcut(u64 program_id, const std::string& game_path,
                                           GameListShortcutTarget target) {
    std::string citra_command{};
    bool skip_tryexec = false;
    const char* env_flatpak_id = getenv("FLATPAK_ID");
    if (env_flatpak_id) {
        citra_command = fmt::format("flatpak run {}", env_flatpak_id);
        skip_tryexec = true;
    } else {
        // Get path to Citra executable
        const QStringList args = QApplication::arguments();
        citra_command = args[0].toStdString();
        // If relative path, make it an absolute path
        if (citra_command.c_str()[0] == '.') {
            citra_command = FileUtil::GetCurrentDir().value_or("") + DIR_SEP + citra_command;
        }
    }

    // Shortcut path
    std::filesystem::path shortcut_path{};
    if (target == GameListShortcutTarget::Desktop) {
        shortcut_path =
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation).toStdString();
    } else if (target == GameListShortcutTarget::Applications) {
        shortcut_path = GetApplicationsDirectory();
    }

    // Icon path and title
    if (!std::filesystem::exists(shortcut_path)) {
        CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_ERROR, {});
        LOG_ERROR(Frontend, "Invalid shortcut target");
        return;
    }

    // Get title from game file
    const auto loader = Loader::GetLoader(game_path);
    std::string game_title = fmt::format("{:016X}", program_id);
    if (loader->ReadTitle(game_title, false) != Loader::ResultStatus::Success) {
        game_title = fmt::format("{:016x}", program_id);
    }

    // Delete illegal characters from title
    const std::string illegal_chars = "<>:\"/\\|?*.";
    for (auto it = game_title.rbegin(); it != game_title.rend(); ++it) {
        if (illegal_chars.find(*it) != std::string::npos) {
            game_title.erase(it.base() - 1);
        }
    }

    // Get icon from game file
    std::vector<u8> icon_image_file;
    if (loader->ReadIcon(icon_image_file, false) != Loader::ResultStatus::Success) {
        LOG_WARNING(Frontend, "Could not read icon from {:s}", game_path);
    }

    const QPixmap pixmap = GetQPixmapFromSMDH(icon_image_file);
    const QImage icon_data = pixmap.toImage();
    std::filesystem::path out_icon_path;
    if (MakeShortcutIcoPath(program_id, game_title, out_icon_path)) {
        if (!SaveIconToFile(out_icon_path, icon_data)) {
            LOG_ERROR(Frontend, "Could not write icon to file");
        }
    }

    const auto qt_game_title = QString::fromStdString(game_title);
#if defined(__linux__)
    // Special case for AppImages
    // Warn once if we are making a shortcut to a volatile AppImage
    const std::string appimage_ending =
        std::string(Common::g_scm_rev).substr(0, 9).append(".AppImage");
    if (citra_command.ends_with(appimage_ending) && !UISettings::values.shortcut_already_warned) {
        if (CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_APPIMAGE_VOLATILE_WARNING,
                                      qt_game_title)) {
            return;
        }
        UISettings::values.shortcut_already_warned = true;
    }
#endif // __linux__
    // Create shortcut
    std::string arguments = fmt::format("\"{:s}\"", game_path);
    if (CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_FULLSCREEN_PROMPT, qt_game_title)) {
        arguments = "-f " + arguments;
    }
    const std::string comment = fmt::format("Start {:s} with the Extendo Emulator", game_title);
    const std::string categories = "Game;Emulator;Qt;";
    const std::string keywords = "3ds;Nintendo;";

    if (CreateShortcutLink(shortcut_path, comment, out_icon_path, citra_command, arguments,
                           categories, keywords, game_title, skip_tryexec)) {
        CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_SUCCESS, qt_game_title);
        return;
    }
    CreateShortcutMessagesGUI(this, CREATE_SHORTCUT_MSGBOX_ERROR, qt_game_title);
}

void GMainWindow::OnGameListDumpRomFS(QString game_path, u64 program_id) {
    auto* dialog = new QProgressDialog(tr("Dumping..."), tr("Cancel"), 0, 0, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowFlags(dialog->windowFlags() &
                           ~(Qt::WindowCloseButtonHint | Qt::WindowContextHelpButtonHint));
    dialog->setCancelButton(nullptr);
    dialog->setMinimumDuration(0);
    dialog->setValue(0);

    const auto base_path = fmt::format(
        "{}romfs/{:016X}", FileUtil::GetUserPath(FileUtil::UserPath::DumpDir), program_id);
    const auto update_path =
        fmt::format("{}romfs/{:016X}", FileUtil::GetUserPath(FileUtil::UserPath::DumpDir),
                    program_id | 0x0004000e00000000);
    using FutureWatcher = QFutureWatcher<std::pair<Loader::ResultStatus, Loader::ResultStatus>>;
    auto* future_watcher = new FutureWatcher(this);
    connect(future_watcher, &FutureWatcher::finished, this,
            [this, dialog, base_path, update_path, future_watcher] {
                dialog->hide();
                const auto& [base, update] = future_watcher->result();
                if (base != Loader::ResultStatus::Success) {
                    QMessageBox::critical(
                        this, QStringLiteral("Extendo"),
                        tr("Could not dump base RomFS.\nRefer to the log for details."));
                    return;
                }
                QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(base_path)));
                if (update == Loader::ResultStatus::Success) {
                    QDesktopServices::openUrl(
                        QUrl::fromLocalFile(QString::fromStdString(update_path)));
                }
            });

    auto future = QtConcurrent::run([game_path, base_path, update_path] {
        std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(game_path.toStdString());
        return std::make_pair(loader->DumpRomFS(base_path), loader->DumpUpdateRomFS(update_path));
    });
    future_watcher->setFuture(future);
}

void GMainWindow::OnGameListOpenDirectory(const QString& directory) {
    QString path;
    if (directory == QStringLiteral("INSTALLED")) {
        path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) +
                                      "Nintendo "
                                      "3DS/00000000000000000000000000000000/"
                                      "00000000000000000000000000000000/title/00040000");
    } else if (directory == QStringLiteral("SYSTEM")) {
        path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir) +
                                      "00000000000000000000000000000000/title/00040010");
    } else {
        path = directory;
    }
    if (!QFileInfo::exists(path)) {
        QMessageBox::critical(this, tr("Error Opening %1").arg(path), tr("Folder does not exist!"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void GMainWindow::OnGameListAddDirectory() {
    const QString dir_path = QFileDialog::getExistingDirectory(this, tr("Select Directory"));
    if (dir_path.isEmpty())
        return;

    // Compare resolved paths so a symlink to an already-added folder, or a path spelled
    // differently, is recognised as the same directory instead of listing every title twice.
    const QString resolved = QFileInfo(dir_path).canonicalFilePath();
    const QString new_path = resolved.isEmpty() ? dir_path : resolved;

    const auto& dirs = UISettings::values.game_dirs;
    const bool already_listed =
        std::any_of(dirs.begin(), dirs.end(), [&new_path](const UISettings::GameDir& existing) {
            if (existing.path == QStringLiteral("INSTALLED") ||
                existing.path == QStringLiteral("SYSTEM")) {
                return false;
            }
            const QString existing_resolved = QFileInfo(existing.path).canonicalFilePath();
            return (existing_resolved.isEmpty() ? existing.path : existing_resolved) == new_path;
        });

    if (already_listed) {
        LOG_WARNING(Frontend, "Selected directory is already in the application list");
        QMessageBox::information(this, tr("Folder Already Added"),
                                 tr("This folder is already in your library, so it was not added "
                                    "again."));
        return;
    }

    UISettings::values.game_dirs.append(UISettings::GameDir{new_path, false, true});
    game_list->PopulateAsync(UISettings::values.game_dirs);
    config->Save();
}

void GMainWindow::OnGameListShowList(bool show) {
    // home_screen always stays visible when idle; it swaps its own internal empty/populated
    // sub-states via Refresh(). This signal predates that and no longer needs to hide anything.
    if (emulation_running && ui->action_Single_Window_Mode->isChecked())
        return;
    home_screen->show();
};

void GMainWindow::OnGameListOpenPerGameProperties(const QString& file) {
    const auto loader = Loader::GetLoader(file.toStdString());

    u64 title_id{};
    if (!loader || loader->ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        QMessageBox::information(this, tr("Properties"),
                                 tr("The application properties could not be loaded."));
        return;
    }

    OpenPerGameConfiguration(title_id, file);
}

void GMainWindow::OnMenuLoadFile() {
    const QString extensions = QStringLiteral("*.").append(
        GameList::supported_file_extensions.join(QStringLiteral(" *.")));
    const QString file_filter = tr("3DS Executable") + QStringLiteral(" (%1)").arg(extensions) +
                                QStringLiteral(";;") + tr("All Files") + QStringLiteral(" (*.*)");
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Load File"), UISettings::values.roms_path, file_filter);

    if (filename.isEmpty()) {
        return;
    }

    UISettings::values.roms_path = QFileInfo(filename).path();
    BootGame(filename);
}

void GMainWindow::OnMenuSetUpSystemFiles() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Set Up System Files"));

    QVBoxLayout layout(&dialog);

    QLabel label_description(
        tr("<p>Azahar needs console unique data and firmware files from a real console to be "
           "able to use some of its features.<br>Such files and data can be set up with the <a "
           "href=https://github.com/azahar-emu/ArticSetupTool>Azahar "
           "Artic Setup Tool</a><br>Notes:<ul><li><b>This operation will install console unique "
           "data to Azahar, do not share your user or nand folders<br>after performing the setup "
           "process!</b></li><li>While doing the setup process, Azahar will link to the console "
           "running the setup tool. You can unlink the<br>console later from the System tab in the "
           "emulator configuration menu.</li><li>Do not go online with both Azahar and your 3DS "
           "console at the same time after setting up system files,<br>as it could cause "
           "issues.</li><li>Old 3DS setup is needed for the New 3DS setup to work (doing both "
           "setup modes is recommended).</li><li>Both setup modes will work regardless of the "
           "model of the console running the setup tool.</li></ul><hr></p>"),
        &dialog);
    label_description.setOpenExternalLinks(true);
    layout.addWidget(&label_description);

    QHBoxLayout layout_h(&dialog);
    layout.addLayout(&layout_h);

    QLabel label_enter(tr("Enter Azahar Artic Setup Tool address:"), &dialog);

    layout_h.addWidget(&label_enter);

    QLineEdit textInput(UISettings::values.last_artic_base_addr, &dialog);
    layout_h.addWidget(&textInput);

    QLabel label_select(QStringLiteral("<br>") + tr("Choose setup mode:"), &dialog);
    layout.addWidget(&label_select);

    std::pair<bool, bool> install_state = Core::AreSystemTitlesInstalled();

    QRadioButton radio1(&dialog);
    QRadioButton radio2(&dialog);
    QString new3dsSetupString = tr("New 3DS setup");
    QString old3dsSetupString = tr("Old 3DS setup");
    QString availableIcon = QStringLiteral("(\u2139\uFE0F) ");
    QString unavailableIcon = QStringLiteral("(\u26A0) ");
    QString installedIcon = QStringLiteral("(\u2705) ");
    if (!install_state.first) {
        radio1.setChecked(true);

        radio1.setText(availableIcon + old3dsSetupString);
        radio1.setToolTip(tr("Setup is possible."));

        radio2.setText(unavailableIcon + new3dsSetupString);
        radio2.setToolTip(tr("Old 3DS setup is required first."));
        radio2.setEnabled(false);
    } else {
        radio1.setText(installedIcon + old3dsSetupString);
        radio1.setToolTip(tr("Setup completed."));

        if (!install_state.second) {
            radio2.setChecked(true);

            radio2.setText(availableIcon + new3dsSetupString);
            radio2.setToolTip(tr("Setup is possible."));
        } else {
            radio1.setChecked(true);

            radio2.setText(installedIcon + new3dsSetupString);
            radio2.setToolTip(tr("Setup completed."));
        }
    }
    layout.addWidget(&radio1);
    layout.addWidget(&radio2);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout.addWidget(&buttonBox);

    int res = dialog.exec();
    if (res == QDialog::Accepted) {
        bool is_o3ds = radio1.isChecked();
        if ((is_o3ds && install_state.first) || (!is_o3ds && install_state.second)) {
            QMessageBox::StandardButton answer =
                QMessageBox::question(this, tr("Set Up System Files"),
                                      tr("The system files for the selected mode are already set "
                                         "up.\nReinstall the files anyway?"),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }
        }
        Core::UninstallSystemFiles(is_o3ds ? Core::SystemTitleSet::Old3ds
                                           : Core::SystemTitleSet::New3ds);
        QString addr = textInput.text();
        UISettings::values.last_artic_base_addr = addr;
        BootGame(QString::fromStdString(is_o3ds ? "articinio://" : "articinin://").append(addr));
    }
}

void GMainWindow::OnMenuInstallCIA() {
    QStringList filepaths = QFileDialog::getOpenFileNames(
        this, tr("Load Files"), UISettings::values.roms_path,
        tr("3DS Installation File") + QStringLiteral(" (*.cia *.zcia *.bcia);;") + tr("All Files") +
            QStringLiteral(" (*.*)"));

    if (filepaths.isEmpty()) {
        return;
    }

    UISettings::values.roms_path = QFileInfo(filepaths[0]).path();
    InstallCIA(filepaths);
}

void GMainWindow::OnMenuConnectArticBase() {
    bool ok = false;
    auto res = QInputDialog::getText(this, tr("Connect to Artic Base"),
                                     tr("Enter Artic Base server address:"), QLineEdit::Normal,
                                     UISettings::values.last_artic_base_addr, &ok);
    if (ok) {
        UISettings::values.last_artic_base_addr = res;
        BootGame(QString::fromStdString("articbase://").append(res));
    }
}

void GMainWindow::OnMenuBootHomeMenu(u32 region) {
    BootGame(QString::fromStdString(Core::GetHomeMenuNcchPath(region)));
}

void GMainWindow::InstallCIA(QStringList filepaths) {
    ui->action_Install_CIA->setEnabled(false);
    game_list->SetDirectoryWatcherEnabled(false);

    emit UpdateProgress(0, 0);

    (void)QtConcurrent::run([this, filepaths] {
        Service::AM::InstallStatus status;
        const auto cia_progress = [&](std::size_t written, std::size_t total) {
            emit UpdateProgress(written, total);
        };
        for (const auto& current_path : filepaths) {
            status = Service::AM::InstallCIA(current_path.toStdString(), cia_progress);
            emit CIAInstallReport(status, current_path);
        }
        emit CIAInstallFinished();
    });
}

void GMainWindow::OnUpdateProgress(std::size_t written, std::size_t total) {
    if (written == 0 and total == 0) {
        progress_bar->show();
        progress_bar->setValue(0);
        progress_bar->setMaximum(INT_MAX);
    }
    progress_bar->setValue(
        static_cast<int>(INT_MAX * (static_cast<double>(written) / static_cast<double>(total))));
}

void GMainWindow::OnCIAInstallReport(Service::AM::InstallStatus status, QString filepath) {
    QString filename = QFileInfo(filepath).fileName();
    switch (status) {
    case Service::AM::InstallStatus::Success:
        this->statusBar()->showMessage(tr("%1 has been installed successfully.").arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorFailedToOpenFile:
        QMessageBox::critical(this, tr("Unable to open File"),
                              tr("Could not open %1").arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorAborted:
        QMessageBox::critical(
            this, tr("Installation aborted"),
            tr("The installation of %1 was aborted. Please see the log for more details")
                .arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorInvalid:
        QMessageBox::critical(this, tr("Invalid File"), tr("%1 is not a valid CIA").arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorEncrypted:
        QMessageBox::critical(this, tr("CIA Encrypted"),
                              tr("Your CIA file is encrypted.<br/>"
                                 "<a "
                                 "href='https://azahar-emu.org/blog/game-loading-changes/'>"
                                 "Please check our blog for more info.</a>"));
        break;
    case Service::AM::InstallStatus::ErrorFileNotFound:
        QMessageBox::critical(this, tr("Unable to find File"),
                              tr("Could not find %1").arg(filename));
        break;
    case Service::AM::InstallStatus::ErrorCannotCheckBundle:
        break;
    }
}

void GMainWindow::OnCompressFinished(bool is_compress, bool success) {
    progress_bar->hide();
    progress_bar->setValue(0);

    if (!success) {
        if (is_compress) {
            QMessageBox::critical(this, tr("Z3DS Compression"),
                                  tr("Failed to compress some files, check log for details."));
        } else {
            QMessageBox::critical(this, tr("Z3DS Compression"),
                                  tr("Failed to decompress some files, check log for details."));
        }
    } else {
        if (is_compress) {
            QMessageBox::information(this, tr("Z3DS Compression"),
                                     tr("All files have been compressed successfully."));
        } else {
            QMessageBox::information(this, tr("Z3DS Compression"),
                                     tr("All files have been decompressed successfully."));
        }
    }
}

void GMainWindow::OnBundleFinished(bool is_bundle, bool success) {
    progress_bar->hide();
    progress_bar->setValue(0);

    if (!success) {
        if (is_bundle) {
            QMessageBox::critical(this, tr("Bundle File"),
                                  tr("Failed to create the bundle, check log for details."));
        } else {
            QMessageBox::critical(this, tr("Bundle File"),
                                  tr("Failed to extract the bundle, check log for details."));
        }
    } else {
        if (is_bundle) {
            QMessageBox::information(this, tr("Bundle File"),
                                     tr("Bundle has been created successfully."));
        } else {
            QMessageBox::information(this, tr("Bundle File"),
                                     tr("Bundle has been extracted successfully."));
        }
    }
}

void GMainWindow::OnCIAInstallFinished() {
    progress_bar->hide();
    progress_bar->setValue(0);
    game_list->SetDirectoryWatcherEnabled(true);
    ui->action_Install_CIA->setEnabled(true);
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

void GMainWindow::UninstallTitles(
    const std::vector<std::tuple<Service::FS::MediaType, u64, QString>>& titles) {
    if (titles.empty()) {
        return;
    }

    // Select the first title in the list as representative.
    const auto first_name = std::get<QString>(titles[0]);

    QProgressDialog progress(tr("Uninstalling '%1'...").arg(first_name), tr("Cancel"), 0,
                             static_cast<int>(titles.size()), this);
    progress.setWindowModality(Qt::WindowModal);

    QFutureWatcher<void> future_watcher;
    QObject::connect(&future_watcher, &QFutureWatcher<void>::finished, &progress,
                     &QProgressDialog::reset);
    QObject::connect(&progress, &QProgressDialog::canceled, &future_watcher,
                     &QFutureWatcher<void>::cancel);
    QObject::connect(&future_watcher, &QFutureWatcher<void>::progressValueChanged, &progress,
                     &QProgressDialog::setValue);

    auto failed = false;
    QString failed_name;

    const auto uninstall_title = [&future_watcher, &failed, &failed_name](const auto& title) {
        const auto name = std::get<QString>(title);
        const auto media_type = std::get<Service::FS::MediaType>(title);
        const auto program_id = std::get<u64>(title);

        const auto result = Service::AM::UninstallProgram(media_type, program_id);
        if (result.IsError()) {
            LOG_ERROR(Frontend, "Failed to uninstall '{}': 0x{:08X}", name.toStdString(),
                      result.raw);
            failed = true;
            failed_name = name;
            future_watcher.cancel();
        }
    };

    future_watcher.setFuture(QtConcurrent::map(titles, uninstall_title));
    progress.exec();
    future_watcher.waitForFinished();

    if (failed) {
        QMessageBox::critical(this, QStringLiteral("Extendo"),
                              tr("Failed to uninstall '%1'.").arg(failed_name));
    } else if (!future_watcher.isCanceled()) {
        QMessageBox::information(this, QStringLiteral("Extendo"),
                                 tr("Successfully uninstalled '%1'.").arg(first_name));
        emit InstalledTitlesChanged();
    }
}

void GMainWindow::OnMenuRecentFile() {
    QAction* action = qobject_cast<QAction*>(sender());
    ASSERT(action);

    const QString filename = action->data().toString();
    if (QFileInfo::exists(filename)) {
        BootGame(filename);
    } else {
        // Display an error message and remove the file from the list.
        QMessageBox::information(this, tr("File not found"),
                                 tr("File \"%1\" not found").arg(filename));

        UISettings::values.recent_files.removeOne(filename);
        UpdateRecentFiles();
    }
}

void GMainWindow::OnResumeGame(bool first_start) {
    qt_cameras->ResumeCameras();

    PreventOSSleep();

    emu_thread->SetRunning(true);
    system.frame_limiter.SetFrameAdvancing(false);
    graphics_api_button->setEnabled(false);
    qRegisterMetaType<Core::System::ResultStatus>("Core::System::ResultStatus");
    qRegisterMetaType<std::string>("std::string");
    connect(emu_thread.get(), &EmuThread::ErrorThrown, this, &GMainWindow::OnCoreError);

    UpdateMenuState();

    play_time_manager->SetProgramId(game_title_id);
    play_time_manager->Start();

    if (first_start) {
#ifdef ENABLE_DISCORD_RPC
        discord_rpc->Update(true);
#endif
    }

#ifdef __unix__
    Common::Linux::StartGamemode();
#endif

    UpdateSaveStates();
    UpdateAutoSaveTimer();
    UpdateStatusButtons();
    if (home_button && emulation_running) {
        RepositionHomeButton();
        home_button->show();
        home_button->raise();
    }
    if (ingame_status && emulation_running) {
        RepositionInGameStatus();
        ingame_status->RefreshStatus();
        ingame_status->show();
        ingame_status->raise();
    }
}

void GMainWindow::OnRestartGame() {
    if (!system.IsPoweredOn()) {
        return;
    }
    // Make a copy since BootGame edits game_path
    BootGame(QString(game_path));
}

void GMainWindow::OnPauseGame() {
    system.frame_limiter.SetFrameAdvancing(true);
    qt_cameras->PauseCameras();

    autosave_timer.stop();
    play_time_manager->Stop();

    UpdateMenuState();
    AllowOSSleep();

#ifdef __unix__
    Common::Linux::StopGamemode();
#endif
}

void GMainWindow::OnPauseContinueGame() {
    if (emulation_running) {
        if (emu_thread->IsRunning() && !system.frame_limiter.IsFrameAdvancing()) {
            OnPauseGame();
        } else {
            OnResumeGame(false);
        }
    }
}

void GMainWindow::OnStopGame() {
    SetTurboEnabled(false);

    play_time_manager->Stop();
    // Update game list to show new play time
    game_list->PopulateAsync(UISettings::values.game_dirs);

    ShutdownGame();
    graphics_api_button->setEnabled(true);
    Settings::RestoreGlobalState(false);
    UpdateStatusButtons();
}

void GMainWindow::OnLoadComplete() {
    loading_screen->OnLoadComplete();
    UpdateSecondaryWindowVisibility();
    RepositionHomeButton();
    home_button->show();
    home_button->raise();
    RepositionInGameStatus();
    if (ingame_status) {
        ingame_status->RefreshStatus();
        ingame_status->show();
        ingame_status->raise();
    }
}

void GMainWindow::ToggleFullscreen() {
    if (!emulation_running) {
        return;
    }
    if (secondary_window->isVisible() && secondary_window->isActiveWindow()) {
        // undo the action and fullscreen secondary manually
        ui->action_Fullscreen->toggle();
        ToggleSecondaryFullscreen();
    } else {
        if (ui->action_Fullscreen->isChecked()) {
            ShowFullscreen();
        } else {
            HideFullscreen();
        }
    }
}

void GMainWindow::ToggleSecondaryFullscreen() {
    if (!emulation_running) {
        return;
    }
    if (secondary_window->isFullScreen()) {
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(secondary_window, false);
#endif
        secondary_window->restoreGeometry(UISettings::values.secondarywindow_geometry);
        secondary_window->showNormal();
    } else {
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(secondary_window, true);
#endif
        UISettings::values.secondarywindow_geometry = secondary_window->saveGeometry();
        LOG_INFO(Frontend, "Attempting to fullscreen secondary window");
        secondary_window->showFullScreen();
    }
}

void GMainWindow::ShowFullscreen() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        UISettings::values.geometry = saveGeometry();
        ui->menubar->hide();
        statusBar()->hide();
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(this, true);
#endif
        showFullScreen();
        RepositionHomeButton();
        if (home_button && emulation_running) {
            home_button->raise();
        }
        RepositionInGameStatus();
        if (ingame_status && emulation_running) {
            ingame_status->raise();
        }
    } else {
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(render_window, true);
#endif
        render_window->showFullScreen();
    }
}

void GMainWindow::HideFullscreen() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
        ui->menubar->show();
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(this, false);
#endif
        showNormal();
        restoreGeometry(UISettings::values.geometry);
        RepositionHomeButton();
        if (home_button && emulation_running) {
            home_button->raise();
        }
        RepositionInGameStatus();
        if (ingame_status && emulation_running) {
            ingame_status->raise();
        }
    } else {
#ifdef NEEDS_ROUND_CORNERS_FIX
        WindowCornerManager::instance().blockRoundedCorners(render_window, false);
#endif
        render_window->showNormal();
        render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
    }
}

void GMainWindow::ToggleWindowMode() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        // Render in the main window...
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
        ui->horizontalLayout->addWidget(render_window);
        render_window->setFocusPolicy(Qt::StrongFocus);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->setFocus();
            home_screen->hide();
        }

    } else {
        // Render in a separate window...
        ui->horizontalLayout->removeWidget(render_window);
        render_window->setParent(nullptr);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
            home_screen->show();
        }
    }
}

void GMainWindow::UpdateSecondaryWindowVisibility() {
    if (!emulation_running) {
        return;
    }
    if (Settings::values.layout_option.GetValue() == Settings::LayoutOption::SeparateWindows) {
        secondary_window->restoreGeometry(UISettings::values.secondarywindow_geometry);
        secondary_window->show();
    } else {
        UISettings::values.secondarywindow_geometry = secondary_window->saveGeometry();
        secondary_window->hide();
    }
    // make sure focus is on primary window whenever this changes
    if (UISettings::values.single_window_mode.GetValue()) {
        QApplication::setActiveWindow(this);
    } else {
        QApplication::setActiveWindow(render_window);
    }
}

void GMainWindow::ChangeScreenLayout() {
    Settings::LayoutOption new_layout = Settings::LayoutOption::Default;
    if (ui->action_Screen_Layout_Default->isChecked()) {
        new_layout = Settings::LayoutOption::Default;
    } else if (ui->action_Screen_Layout_Single_Screen->isChecked()) {
        new_layout = Settings::LayoutOption::SingleScreen;
    } else if (ui->action_Screen_Layout_Large_Screen->isChecked()) {
        new_layout = Settings::LayoutOption::LargeScreen;
        ui->menu_Small_Screen_Position->setEnabled(true);
    } else if (ui->action_Screen_Layout_Hybrid_Screen->isChecked()) {
        new_layout = Settings::LayoutOption::HybridScreen;
    } else if (ui->action_Screen_Layout_Side_by_Side->isChecked()) {
        new_layout = Settings::LayoutOption::SideScreen;
    } else if (ui->action_Screen_Layout_Separate_Windows->isChecked()) {
        new_layout = Settings::LayoutOption::SeparateWindows;
    } else if (ui->action_Screen_Layout_Custom_Layout->isChecked()) {
        new_layout = Settings::LayoutOption::CustomLayout;
    }

    Settings::values.layout_option = new_layout;
    SyncMenuUISettings();
    system.ApplySettings();
    UpdateSecondaryWindowVisibility();
}

void GMainWindow::ChangeSmallScreenPosition() {
    Settings::SmallScreenPosition new_position = Settings::SmallScreenPosition::BottomRight;

    if (ui->action_Small_Screen_TopRight->isChecked()) {
        new_position = Settings::SmallScreenPosition::TopRight;
    } else if (ui->action_Small_Screen_MiddleRight->isChecked()) {
        new_position = Settings::SmallScreenPosition::MiddleRight;
    } else if (ui->action_Small_Screen_BottomRight->isChecked()) {
        new_position = Settings::SmallScreenPosition::BottomRight;
    } else if (ui->action_Small_Screen_TopLeft->isChecked()) {
        new_position = Settings::SmallScreenPosition::TopLeft;
    } else if (ui->action_Small_Screen_MiddleLeft->isChecked()) {
        new_position = Settings::SmallScreenPosition::MiddleLeft;
    } else if (ui->action_Small_Screen_BottomLeft->isChecked()) {
        new_position = Settings::SmallScreenPosition::BottomLeft;
    } else if (ui->action_Small_Screen_Above->isChecked()) {
        new_position = Settings::SmallScreenPosition::AboveLarge;
    } else if (ui->action_Small_Screen_Below->isChecked()) {
        new_position = Settings::SmallScreenPosition::BelowLarge;
    }

    Settings::values.small_screen_position = new_position;
    SyncMenuUISettings();
    system.ApplySettings();
    UpdateSecondaryWindowVisibility();
}

bool GMainWindow::IsTurboEnabled() {
    return turbo_mode_active;
}

void GMainWindow::SetTurboEnabled(bool state) {
    turbo_mode_active = state;
    GMainWindow::ReloadTurbo();
}

void GMainWindow::ReloadTurbo() {
    if (IsTurboEnabled()) {
        Settings::temporary_frame_limit = Settings::values.turbo_limit.GetValue();
        Settings::is_temporary_frame_limit = true;
    } else {
        Settings::is_temporary_frame_limit = false;
    }

    UpdateStatusBar();
}

// TODO: This should probably take in something more descriptive than a bool. -OS
void GMainWindow::AdjustSpeedLimit(bool increase) {
    const int SPEED_LIMIT_STEP = 5;
    auto active_limit =
        IsTurboEnabled() ? &Settings::values.turbo_limit : &Settings::values.frame_limit;
    const auto active_limit_value = active_limit->GetValue();

    if (increase) {
        if (active_limit_value < 995) {
            active_limit->SetValue(active_limit_value + SPEED_LIMIT_STEP);
        }
    } else {
        if (active_limit_value > SPEED_LIMIT_STEP) {
            active_limit->SetValue(active_limit_value - SPEED_LIMIT_STEP);
        }
    }

    if (IsTurboEnabled()) {
        ReloadTurbo();
    }

    UpdateStatusBar();
}

void GMainWindow::ToggleScreenLayout() {
    const Settings::LayoutOption new_layout = []() {
        const Settings::LayoutOption current_layout = Settings::values.layout_option.GetValue();
        std::vector<Settings::LayoutOption> layouts_to_cycle =
            Settings::values.layouts_to_cycle.GetValue();
        const auto current_pos =
            distance(layouts_to_cycle.begin(),
                     std::find(layouts_to_cycle.begin(), layouts_to_cycle.end(), current_layout));
        // if the layouts_to_cycle setting has somehow been
        // cleared out, add just default back in
        if (layouts_to_cycle.size() == 0) {
            layouts_to_cycle.push_back(Settings::LayoutOption::Default);
        }
        if (current_pos >= layouts_to_cycle.size() - 1) {
            // either this layout wasn't found or it was last so move to the beginning
            return layouts_to_cycle[0];
        } else {
            return layouts_to_cycle[current_pos + 1];
        }
    }();

    Settings::values.layout_option = new_layout;
    SyncMenuUISettings();
    system.ApplySettings();
    UpdateSecondaryWindowVisibility();
}

void GMainWindow::OnSwapScreens() {
    Settings::values.swap_screen = ui->action_Screen_Layout_Swap_Screens->isChecked();
    system.ApplySettings();
}

void GMainWindow::OnRotateScreens() {
    Settings::values.upright_screen = ui->action_Screen_Layout_Upright_Screens->isChecked();
    system.ApplySettings();
}

void GMainWindow::TriggerSwapScreens() {
    ui->action_Screen_Layout_Swap_Screens->trigger();
}

void GMainWindow::TriggerRotateScreens() {
    ui->action_Screen_Layout_Upright_Screens->trigger();
}

void GMainWindow::OnSaveState() {
    if (!system.IsPoweredOn()) {
        return;
    }

    QAction* action = qobject_cast<QAction*>(sender());
    ASSERT(action);

    system.SendSignal(Core::System::Signal::Save, action->data().toUInt());
    system.frame_limiter.AdvanceFrame();
    newest_slot = action->data().toUInt();
}

void GMainWindow::OnLoadState() {
    if (!system.IsPoweredOn()) {
        return;
    }

    QAction* action = qobject_cast<QAction*>(sender());
    ASSERT(action);

    if (UISettings::values.save_state_warning) {
        QMessageBox::warning(
            this, tr("Savestates"),
            tr("Warning: Savestates are NOT a replacement for in-application saves, "
               "and are not meant to be reliable.\n\nUse at your own risk!"));
        UISettings::values.save_state_warning = false;
        config->Save();
    }

    system.SendSignal(Core::System::Signal::Load, action->data().toUInt());
    system.frame_limiter.AdvanceFrame();
}

bool GMainWindow::TitleLacksInGameSave() const {
    if (game_title_id == 0) {
        return false;
    }

    const std::string save_path = FileSys::ArchiveSource_SDSaveData::GetSaveDataPathFor(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), game_title_id);
    if (!FileUtil::IsDirectory(save_path)) {
        // The game never even created its save archive.
        return true;
    }

    // A formatted-but-unwritten archive is a directory tree with no actual file content, so
    // counting bytes rather than entries is what distinguishes "has a save" from "has a save
    // slot". Stops at the first real file.
    u64 bytes_found = 0;
    std::function<void(const std::string&, unsigned)> scan = [&](const std::string& directory,
                                                                 unsigned depth) {
        if (bytes_found > 0 || depth == 0) {
            return;
        }
        FileUtil::ForeachDirectoryEntry(
            nullptr, directory,
            [&](u64*, const std::string& parent, const std::string& name) -> bool {
                const std::string entry = parent + DIR_SEP + name;
                if (FileUtil::IsDirectory(entry)) {
                    scan(entry, depth - 1);
                } else if (FileUtil::GetSize(entry) > 0) {
                    bytes_found += FileUtil::GetSize(entry);
                }
                return bytes_found == 0;
            });
    };
    scan(save_path, 8);

    return bytes_found == 0;
}

void GMainWindow::UpdateAutoSaveTimer() {
    u32 minutes = UISettings::values.autosave_interval_minutes.GetValue();
    // A title that keeps no save data of its own has nothing to fall back on, so autosaving is
    // forced for it even when the user turned autosaving off, and runs more often.
    if (forced_autosave_title) {
        minutes = (minutes == 0) ? ForcedAutoSaveMinutes : std::min(minutes, ForcedAutoSaveMinutes);
    }
    const bool should_run = emulation_running && system.IsPoweredOn() && emu_thread &&
                            emu_thread->IsRunning() && !system.frame_limiter.IsFrameAdvancing() &&
                            minutes > 0;

    if (!should_run) {
        autosave_timer.stop();
        return;
    }

    // Restart rather than leave the old interval running, in case the setting changed.
    autosave_timer.start(static_cast<int>(minutes) * 60 * 1000);
}

void GMainWindow::OnAutoSaveTick() {
    if (!emulation_running || !system.IsPoweredOn()) {
        autosave_timer.stop();
        return;
    }
    // Don't stack a second save on top of one still being written.
    if (system.IsSaveStatePending() || system.IsSignalPending(Core::System::Signal::Save)) {
        return;
    }

    // Writing to AutoResumeSlot always targets the same file, so the previous autosave is
    // replaced in place -- one state per title, no stale timestamps piling up.
    LOG_INFO(Frontend, "Autosaving auto-resume state for title {:016X}", game_title_id);
    system.SendSignal(Core::System::Signal::Save, Core::AutoResumeSlot);
    system.frame_limiter.AdvanceFrame();
}

void GMainWindow::SaveAutoResumeState() {
    if (!emulation_running || !system.IsPoweredOn()) {
        return;
    }
    LOG_INFO(Frontend, "Saving auto-resume state for title {:016X}", game_title_id);
    system.SendSignal(Core::System::Signal::Save, Core::AutoResumeSlot);

    const auto start = std::chrono::steady_clock::now();
    // 1. Wait until the emu thread picks up the signal and transitions to SAVING
    while (system.IsSignalPending(Core::System::Signal::Save)) {
        system.frame_limiter.AdvanceFrame();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
            LOG_ERROR(Frontend, "Timed out waiting for save signal to start");
            break;
        }
    }

    // 2. Wait until the emu thread finishes saving the state to disk
    while (system.IsSaveStatePending()) {
        system.frame_limiter.AdvanceFrame();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
            LOG_ERROR(Frontend, "Timed out waiting for save state to finish");
            break;
        }
    }

    LOG_INFO(Frontend, "Auto-resume save completed for title {:016X}", game_title_id);
}

void GMainWindow::PromptQuitGame() {
    if (!emulation_running) {
        return;
    }

    const bool was_home_visible = home_button && home_button->isVisible();
    const bool was_status_visible = ingame_status && ingame_status->isVisible();
    HideControllersPopover();
    if (was_home_visible) {
        home_button->hide();
    }
    if (was_status_visible) {
        ingame_status->hide();
    }

    const bool was_running = emu_thread && emu_thread->IsRunning();
    if (was_running) {
        OnPauseGame();
    }

    // A title that keeps no save data of its own has nothing but this state, so discarding is
    // not offered: "Quit" there would silently throw away the whole session.
    const auto choice =
        forced_autosave_title
            ? ConsoleUI::ShowConfirmModal(
                  this, QString(),
                  tr("This game does not save progress on its own, so your session is saved "
                     "automatically before quitting."),
                  tr("Save & Quit"), {}, tr("Cancel"))
            : ConsoleUI::ShowConfirmModal(
                  this, QString(),
                  tr("Do you want to save your progress before quitting?\nSaving creates an auto-resume state so you can continue right where you left off next time."),
                  tr("Save & Quit"), tr("Quit"), tr("Cancel"));

    if (choice == ConsoleUI::ActionChoice::Primary) {
        SaveAutoResumeState();
        OnStopGame();
    } else if (choice == ConsoleUI::ActionChoice::Secondary) {
        const auto resume_path = Core::GetSaveStatePath(game_title_id, 0, Core::AutoResumeSlot);
        FileUtil::Delete(resume_path);
        OnStopGame();
    } else {
        if (was_home_visible) {
            home_button->show();
            home_button->raise();
        }
        if (was_status_visible) {
            ingame_status->show();
            ingame_status->raise();
        }
        if (was_running) {
            OnResumeGame(false);
        }
    }
}

void GMainWindow::OnEscapePressed() {
    if (settings_overlay) {
        if (auto* dialog = settings_overlay->findChild<QDialog*>()) {
            dialog->reject();
        }
        return;
    }
    if (!emulation_running) {
        close();
        return;
    }
    PromptQuitGame();
}

void GMainWindow::OnHomeButtonClicked() {
    UiSounds::Play(UiSounds::Name::Back);
    PromptQuitGame();
}

void GMainWindow::OnConfigure() {
    ShowConfigureOverlay(0, !multiplayer_state->IsHostingPublicRoom());
}

void GMainWindow::OnQuickSettings() {
    ShowConfigureOverlay(5, !multiplayer_state->IsHostingPublicRoom()); // Graphics tab
}

// Action button prompts: a small colored circular glyph (B/A) before the label,
// replacing the generic rectangular Cancel/OK buttons.
static QIcon MakeActionBadge(const QString& letter, const QColor& color) {
    constexpr int BadgeSize = 22;
    QPixmap pixmap(BadgeSize, BadgeSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(0, 0, BadgeSize, BadgeSize);
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(11);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(0, 0, BadgeSize, BadgeSize), Qt::AlignCenter, letter);
    return QIcon(pixmap);
}

// Settings used to be a separate modal QDialog (QDialog::exec()). To make it feel like part of
// the main window instead of a floating OS window, the dialog is reparented into a plain
// embedded widget (Qt::Widget flag, no title bar) placed inside a centered card over a dimmed
// backdrop. Since exec()'s blocking event loop no longer applies, the former synchronous
// "apply after exec() returns" flow becomes two lambdas hung off the dialog's own
// accepted()/rejected() signals (still emitted normally by its OK/Cancel buttons).
void GMainWindow::ShowConfigureOverlay(int initial_tab, bool enable_web_config) {
    if (settings_overlay) {
        settings_overlay->raise();
        return;
    }

    game_list->SetDirectoryWatcherEnabled(false);
    Settings::SetConfiguringGlobal(true);

    auto* dialog = new ConfigureDialog(nullptr, hotkey_registry, system, gl_renderer,
                                       physical_devices, enable_web_config);
    dialog->setWindowFlags(Qt::Widget);
    dialog->SetActiveTab(initial_tab);
    // Restyles the stock native Settings dialog to match the home screen's dark, rounded,
    // pill-accented look instead of the default system chrome.
    // Fully custom White/Dark theming (no native palette): pick the color set from the current
    // theme, then build the whole settings-panel stylesheet from it. White = white surfaces,
    // black text/accents; otherwise the dark set.
    // Taste rule 2: one hue owns the screen. Every surface below is the SAME base colour
    // mixed toward the foreground by a fixed amount, rather than a family of hand-picked
    // greys — which is what produced mismatched slabs sitting next to each other before.
    const bool white = UISettings::values.theme == QStringLiteral("white");
    const QColor base = white ? QColor(0xff, 0xff, 0xff) : QColor(0x16, 0x18, 0x1d);
    const QColor toward = white ? QColor(0, 0, 0) : QColor(0xff, 0xff, 0xff);
    const auto tint = [&base, &toward](qreal amount) {
        return QColor(qRound(base.red() + (toward.red() - base.red()) * amount),
                      qRound(base.green() + (toward.green() - base.green()) * amount),
                      qRound(base.blue() + (toward.blue() - base.blue()) * amount))
            .name();
    };

    const QString bg = base.name();
    const QString text = tint(0.92);
    const QString listtext = tint(0.74);
    const QString selbg = tint(white ? 1.0 : 0.17);
    const QString seltext = white ? QStringLiteral("#ffffff") : tint(1.0);
    const QString hover = tint(0.07);
    const QString btn = tint(0.11);
    const QString btnhover = tint(0.16);
    const QString btnpress = tint(0.05);
    const QString accent = white ? QStringLiteral("#000000") : QStringLiteral("#0a84ff");
    const QString accenth = white ? QStringLiteral("#333333") : QStringLiteral("#2f95ff");
    const QString field = tint(0.04);
    const QString border = tint(0.13);
    const QString groove = tint(0.13);
    const QString handle = white ? QStringLiteral("#000000") : QStringLiteral("#ffffff");
    const QString focus = white ? QStringLiteral("#000000") : QStringLiteral("#4dd9e8");

    QString qss;
    // Pin the font family here too: Qt resets it to the system default whenever any other font
    // property (e.g. font-weight below) is set via QSS.
    qss += QStringLiteral("* { font-family: 'Roboto', sans-serif; }");
    qss += QStringLiteral("QDialog, QWidget#configureDialog { background-color: %1; }").arg(bg);
    qss += QStringLiteral("QGroupBox { background: transparent; border: none; font-weight: 600;"
                          " color: %1; margin-top: 20px; }")
               .arg(text);
    qss += QStringLiteral("QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top"
                          " left; left: 0px; top: 0px; color: %1; padding-left: 0px; }")
               .arg(text);
    qss += QStringLiteral("QTabWidget::pane { border: none; background: transparent; }");
    // The per-tab pages and their scroll-area wrappers are plain QWidgets, so without an
    // explicit rule they inherit the active theme's app-wide stylesheet background instead of
    // this panel's — rendering the content area as a lighter slab beside the sidebar.
    qss += QStringLiteral("QScrollArea, QScrollArea > QWidget > QWidget, QTabWidget > QWidget {"
                          " background-color: %1; }")
               .arg(bg);
    qss += QStringLiteral("QListWidget { background-color: transparent; border: none; color: %1;"
                          " padding: 6px; outline: none; }")
               .arg(listtext);
    qss += QStringLiteral("QListWidget::item { padding: 9px 12px; border-radius: 12px;"
                          " margin: 2px 4px; }");
    qss += QStringLiteral("QListWidget::item:selected { background-color: %1; color: %2; }")
               .arg(selbg, seltext);
    qss += QStringLiteral("QListWidget::item:hover:!selected { background-color: %1; }").arg(hover);
    qss += QStringLiteral("QPushButton { background-color: %1; color: %2; border: none;"
                          " border-radius: 10px; padding: 7px 18px; font-weight: 600; }")
               .arg(btn, text);
    qss += QStringLiteral("QPushButton:hover { background-color: %1; }").arg(btnhover);
    qss += QStringLiteral("QPushButton:pressed { background-color: %1; }").arg(btnpress);
    qss += QStringLiteral("QPushButton:default { background-color: %1; color: %2; }")
               .arg(accent, seltext);
    qss += QStringLiteral("QPushButton:default:hover { background-color: %1; }").arg(accenth);
    qss += QStringLiteral("QLineEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {"
                          " background-color: %1; color: %2; border: 1px solid %3;"
                          " border-radius: 10px; padding: 4px 6px; min-height: 20px; }")
               .arg(field, text, border);
    qss += QStringLiteral("QComboBox::drop-down { border: none; }");
    qss += QStringLiteral("QSlider::groove:horizontal { height: 4px; background: %1;"
                          " border-radius: 2px; }")
               .arg(groove);
    qss += QStringLiteral("QSlider::sub-page:horizontal { background: %1; border-radius: 2px; }")
               .arg(accent);
    qss += QStringLiteral("QSlider::handle:horizontal { background: %1; width: 16px; height: 16px;"
                          " margin: -6px 0; border-radius: 8px; }")
               .arg(handle);
    // Scrollbars hidden (0px) but scrolling still works via wheel/trackpad.
    qss += QStringLiteral("QScrollBar:vertical, QScrollBar:horizontal { width: 0px; height: 0px;"
                          " background: transparent; }");
    qss += QStringLiteral("QScrollBar::handle { background: transparent; }");
    qss += QStringLiteral("QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }");
    qss += QStringLiteral("QLabel { color: %1; }").arg(text);
    qss +=
        QStringLiteral("QCheckBox:focus, QRadioButton:focus, QComboBox:focus, QLineEdit:focus,"
                       " QSpinBox:focus, QDoubleSpinBox:focus, QSlider:focus, QPushButton:focus {"
                       " border: 1px solid %1; border-radius: 6px; }")
            .arg(focus);
    // Flat Back/OK buttons: flat, no chrome — just the circular glyph (set in code below) + text.
    qss += QStringLiteral("QDialogButtonBox QPushButton { background: transparent; border: none;"
                          " border-radius: 6px; color: %1; padding: 4px 10px; font-weight: 600; }")
               .arg(text);
    qss += QStringLiteral("QDialogButtonBox QPushButton:focus { border: none; }");
    qss += QStringLiteral("QDialogButtonBox QPushButton:default { background: transparent;"
                          " color: %1; }")
               .arg(text);
    qss += QStringLiteral("QDialogButtonBox QPushButton:default:hover { background-color: %1; }")
               .arg(hover);
    qss +=
        QStringLiteral("QDialogButtonBox QPushButton:hover { background-color: %1; }").arg(hover);
    qss += QStringLiteral("QDialogButtonBox { border: none; padding-top: 10px; }");
    dialog->setStyleSheet(qss);

    // The .ui files each carry their own group-box margins, so section titles and the rows
    // underneath them start at different x positions from page to page — a visibly ragged left
    // edge. Zeroing the horizontal margins on every group's layout puts each row's leading edge
    // (label text, or a checkbox's indicator) on the same line as its section title.
    for (QGroupBox* group : dialog->findChildren<QGroupBox*>()) {
        if (QLayout* group_layout = group->layout()) {
            const QMargins margins = group_layout->contentsMargins();
            group_layout->setContentsMargins(0, margins.top(), 0, margins.bottom());
        }
    }

    if (auto* button_box = dialog->findChild<QDialogButtonBox*>(QStringLiteral("buttonBox"))) {
        if (auto* cancel_button = button_box->button(QDialogButtonBox::Cancel)) {
            cancel_button->setText(tr("Back"));
            cancel_button->setIcon(MakeActionBadge(QStringLiteral("B"), QColor(0xe0, 0x3c, 0x3c)));
            cancel_button->setIconSize(QSize(22, 22));
        }
        if (auto* ok_button = button_box->button(QDialogButtonBox::Ok)) {
            ok_button->setIcon(MakeActionBadge(QStringLiteral("A"), QColor(0x3c, 0x8c, 0xe0)));
            ok_button->setIconSize(QSize(22, 22));
        }
    }

    // Only the per-tab content should ever scroll — the sidebar, the General/Network/Debug/UI
    // tab row, and the Reset/Back/OK row at the bottom must all stay fixed in place. So each
    // tab PAGE gets wrapped in its own scroll area (not the dialog as a whole): the sidebar and
    // button row are siblings of tabWidget in the dialog's own layout and are never touched,
    // and the tab bar is tabWidget's own native chrome, which this doesn't wrap either — only
    // the content below it (the page widget) does.
    //
    // The sidebar swaps tabWidget's pages via clear()+addTab() every time the category changes
    // (see ConfigureDialog::UpdateVisibleTabs), which re-adds the RAW page widgets and would
    // undo any wrapping — so this has to re-wrap on every category change too, not just once.
    if (auto* tab_widget = dialog->findChild<QTabWidget*>(QStringLiteral("tabWidget"))) {
        // documentMode removes the tab widget's pane frame — the lighter bordered box that made
        // the content area look like a separate panel from the sidebar.
        tab_widget->setDocumentMode(true);
        auto wrap_and_relayout = [tab_widget] {
            const int old_current = tab_widget->currentIndex();
            for (int i = 0; i < tab_widget->count(); ++i) {
                QWidget* page = tab_widget->widget(i);
                if (!page || qobject_cast<QScrollArea*>(page)) {
                    continue; // already wrapped, or nothing to wrap
                }
                const QString title = tab_widget->tabText(i);
                tab_widget->removeTab(i);
                auto* scroll = new QScrollArea(tab_widget);
                scroll->setWidgetResizable(true);
                scroll->setFrameShape(QFrame::NoFrame);
                // AsNeeded, not AlwaysOff: with widgetResizable(true), forcing the horizontal
                // bar off makes the scroll area pin its widget to the viewport width, but any
                // child whose minimum width exceeds the viewport (e.g. the Debug tab) forces it
                // back wider — an infinite resize war that overflows the stack and crashes.
                // AsNeeded lets the width settle instead of oscillating.
                scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                scroll->setStyleSheet(QStringLiteral(
                    "QScrollArea { background: transparent; border: none; }"
                    // Hidden scrollbars (0px) — wheel/trackpad scrolling still works.
                    "QScrollBar:vertical, QScrollBar:horizontal { width: 0px; height: 0px;"
                    " background: transparent; margin: 0; }"
                    "QScrollBar::handle { background: transparent; }"
                    "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
                    "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }"));
                scroll->setWidget(page);
                tab_widget->insertTab(i, scroll, title);
                if (page->layout()) {
                    page->layout()->invalidate();
                    page->layout()->activate();
                }
            }
            tab_widget->setCurrentIndex(qMax(0, old_current));
        };
        wrap_and_relayout();
        connect(tab_widget, &QTabWidget::currentChanged, dialog,
                [wrap_and_relayout](int) { wrap_and_relayout(); });
        if (auto* selector_list = dialog->findChild<QListWidget*>(QStringLiteral("selectorList"))) {
            selector_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            connect(selector_list, &QListWidget::itemSelectionChanged, dialog, wrap_and_relayout);
        }
    }
    auto* settings_button_box = dialog->findChild<QDialogButtonBox*>(QStringLiteral("buttonBox"));
    connect(dialog, &ConfigureDialog::LanguageChanged, this,
            [this, settings_button_box](const QString& locale) {
                OnLanguageChanged(locale);
                if (settings_button_box) {
                    if (auto* cancel_button = settings_button_box->button(QDialogButtonBox::Cancel)) {
                        cancel_button->setText(tr("Back"));
                    }
                    if (auto* ok_button = settings_button_box->button(QDialogButtonBox::Ok)) {
                        ok_button->setText(tr("OK"));
                    }
                }
            });

    settings_overlay = new QWidget(ui->centralwidget);
    settings_overlay->setObjectName(QStringLiteral("settingsOverlayBackdrop"));
    settings_overlay->setStyleSheet(
        QStringLiteral("#settingsOverlayBackdrop { background-color: rgba(0, 0, 0, 150); }"));
    settings_overlay->setAttribute(Qt::WA_StyledBackground, true);
    settings_overlay->installEventFilter(this);

    settings_card = new QWidget(settings_overlay);
    settings_card->setObjectName(QStringLiteral("settingsOverlayCard"));
    settings_card->setStyleSheet(
        QStringLiteral("#settingsOverlayCard { background-color: %1; border: 1px solid %2;"
                       " border-radius: 20px; }")
            .arg(bg, border));
    settings_card->setAttribute(Qt::WA_StyledBackground, true);
    auto* card_layout = new QVBoxLayout(settings_card);
    card_layout->setContentsMargins(1, 1, 1, 1);
    // Scrolling now happens per-tab-page (see wrap_and_relayout above), not on the dialog as a
    // whole, so the sidebar/tab-bar/button-row stay fixed — only each tab's own content area
    // scrolls when it's taller than the card.
    card_layout->addWidget(dialog);

    RepositionSettingsOverlay();
    settings_overlay->show();
    settings_overlay->raise();
    dialog->setFocus();

    // Entrance animation: the card rises and expands into place so the panel reads as opening
    // rather than popping in. It animates to whatever geometry RepositionSettingsOverlay just
    // computed, so it lands exactly where the static layout would have put it.
    {
        const QRect target = settings_card->geometry();
        QRect from = target;
        from.setSize(target.size() * 0.96);
        from.moveCenter(target.center() + QPoint(0, 14));
        auto* rise = new QPropertyAnimation(settings_card, "geometry", settings_card);
        rise->setDuration(190);
        rise->setStartValue(from);
        rise->setEndValue(target);
        rise->setEasingCurve(QEasingCurve::OutCubic);
        rise->start(QAbstractAnimation::DeleteWhenStopped);
    }

    const auto old_theme = UISettings::values.theme;
    const int old_input_profile_index = Settings::values.current_input_profile_index;
    const auto old_input_profiles = Settings::values.input_profiles;
    const auto old_touch_from_button_maps = Settings::values.touch_from_button_maps;
#ifdef ENABLE_DISCORD_RPC
    const bool old_discord_presence = UISettings::values.enable_discord_presence.GetValue();
#endif
#ifdef __unix__
    const bool old_gamemode = Settings::values.enable_gamemode.GetValue();
#endif

    connect(dialog, &QDialog::accepted, this,
            [this, dialog, old_theme
#ifdef ENABLE_DISCORD_RPC
             ,
             old_discord_presence
#endif
#ifdef __unix__
             ,
             old_gamemode
#endif
    ] {
                dialog->ApplyConfiguration();
                InitializeHotkeys();
                if (UISettings::values.theme != old_theme) {
                    UpdateUITheme();
                }
#ifdef ENABLE_DISCORD_RPC
                if (UISettings::values.enable_discord_presence.GetValue() != old_discord_presence) {
                    SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
                    discord_rpc->Update(system.IsPoweredOn());
                }
#endif
#ifdef __unix__
                if (Settings::values.enable_gamemode.GetValue() != old_gamemode) {
                    SetGamemodeEnabled(Settings::values.enable_gamemode.GetValue());
                }
#endif
                if (!multiplayer_state->IsHostingPublicRoom())
                    multiplayer_state->UpdateCredentials();
                emit UpdateThemedIcons();
                SyncMenuUISettings();
                game_list->RefreshGameDirectory();
                config->Save();
                if (UISettings::values.hide_mouse && emulation_running) {
                    setMouseTracking(true);
                    mouse_hide_timer.start();
                } else {
                    setMouseTracking(false);
                }
                ReloadTurbo();
                UpdateSecondaryWindowVisibility();
                UpdateBootHomeMenuState();
                UpdateStatusButtons();
                home_screen->ApplyAppearanceSettings();

                game_list->SetDirectoryWatcherEnabled(true);
                settings_overlay->deleteLater();
                settings_overlay = nullptr;
                settings_card = nullptr;
            });

    connect(dialog, &QDialog::rejected, this,
            [this, old_input_profile_index, old_input_profiles, old_touch_from_button_maps] {
                Settings::values.input_profiles = old_input_profiles;
                Settings::values.touch_from_button_maps = old_touch_from_button_maps;
                Settings::LoadProfile(old_input_profile_index);

                game_list->SetDirectoryWatcherEnabled(true);
                settings_overlay->deleteLater();
                settings_overlay = nullptr;
                settings_card = nullptr;
            });
}

static QPixmap MakeRoundedPixmap(const QPixmap& src, const QSize& size, int radius) {
    if (src.isNull()) {
        return QPixmap();
    }
    QPixmap scaled = src.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int x = qMax(0, (scaled.width() - size.width()) / 2);
    const int y = qMax(0, (scaled.height() - size.height()) / 2);
    QPixmap cropped = scaled.copy(x, y, size.width(), size.height());

    QPixmap dest(size);
    dest.fill(Qt::transparent);
    QPainter painter(&dest);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, size.width(), size.height()), radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, cropped);
    return dest;
}

static int ProfileLanguageFromLocale(const QString& locale) {
    if (locale.startsWith(QStringLiteral("ja"))) return 0;
    if (locale.startsWith(QStringLiteral("en"))) return 1;
    if (locale.startsWith(QStringLiteral("fr"))) return 2;
    if (locale.startsWith(QStringLiteral("de"))) return 3;
    if (locale.startsWith(QStringLiteral("it"))) return 4;
    if (locale.startsWith(QStringLiteral("es"))) return 5;
    if (locale == QStringLiteral("zh_CN") || locale == QStringLiteral("zh")) return 6;
    if (locale.startsWith(QStringLiteral("ko"))) return 7;
    if (locale.startsWith(QStringLiteral("nl"))) return 8;
    if (locale.startsWith(QStringLiteral("pt"))) return 9;
    if (locale.startsWith(QStringLiteral("ru"))) return 10;
    if (locale == QStringLiteral("zh_TW")) return 11;
    return -1;
}

static QString LocaleFromProfileLanguage(int lang_idx) {
    static const QStringList locales = {
        QStringLiteral("ja"),      // 0: Japanese
        QStringLiteral("en"),      // 1: English
        QStringLiteral("fr"),      // 2: French
        QStringLiteral("de"),      // 3: German
        QStringLiteral("it"),      // 4: Italian
        QStringLiteral("es"),      // 5: Spanish
        QStringLiteral("zh_CN"),   // 6: Simplified Chinese
        QStringLiteral("ko"),      // 7: Korean
        QStringLiteral("nl"),      // 8: Dutch
        QStringLiteral("pt_BR"),   // 9: Portuguese
        QStringLiteral("ru"),      // 10: Russian
        QStringLiteral("zh_TW"),   // 11: Traditional Chinese
    };
    if (lang_idx >= 0 && lang_idx < locales.size()) {
        return locales.at(lang_idx);
    }
    return QStringLiteral("en");
}

struct ProfileRow {
    QPushButton* btn = nullptr;
    QLabel* title_lbl = nullptr;
    QLabel* val_lbl = nullptr;
};

static const QStringList s_profile_lang_names = {
    QStringLiteral("Japanese (日本語)"),
    QStringLiteral("English"),
    QStringLiteral("French (français)"),
    QStringLiteral("German (Deutsch)"),
    QStringLiteral("Italian (italiano)"),
    QStringLiteral("Spanish (español)"),
    QStringLiteral("Simplified Chinese (简体中文)"),
    QStringLiteral("Korean (한국어)"),
    QStringLiteral("Dutch (Nederlands)"),
    QStringLiteral("Portuguese (português)"),
    QStringLiteral("Russian (Русский)"),
    QStringLiteral("Traditional Chinese (正體中文)"),
};

static QStringList GetProfileRegionNames() {
    return {
        QCoreApplication::translate("GMainWindow", "Auto-select"),
        QCoreApplication::translate("GMainWindow", "Japan"),
        QCoreApplication::translate("GMainWindow", "USA"),
        QCoreApplication::translate("GMainWindow", "Europe"),
        QCoreApplication::translate("GMainWindow", "Australia"),
        QCoreApplication::translate("GMainWindow", "China"),
        QCoreApplication::translate("GMainWindow", "Korea"),
        QCoreApplication::translate("GMainWindow", "Taiwan"),
    };
}

static QString GetProfileRegionText() {
    int val = Settings::values.region_value.GetValue();
    int idx = val + 1;
    const auto region_names = GetProfileRegionNames();
    if (idx >= 0 && idx < region_names.size()) {
        return region_names.at(idx);
    }
    return QCoreApplication::translate("GMainWindow", "Auto-select");
}

static QString GetProfileFullNameText() {
    return UISettings::values.profile_full_name.isEmpty()
               ? QCoreApplication::translate("GMainWindow", "Not Set")
               : UISettings::values.profile_full_name;
}

static QStringList GetProfileGenderNames() {
    return {
        QCoreApplication::translate("GMainWindow", "Male"),
        QCoreApplication::translate("GMainWindow", "Female"),
        QCoreApplication::translate("GMainWindow", "Not Specified"),
    };
}

static QString GetProfileGenderText() {
    int g = UISettings::values.profile_gender;
    const auto gender_names = GetProfileGenderNames();
    if (g >= 0 && g < gender_names.size()) {
        return gender_names.at(g);
    }
    return gender_names.at(0);
}

static QStringList GetProfileTimeFormatNames() {
    return {
        QCoreApplication::translate("GMainWindow", "12-Hour (AM/PM)"),
        QCoreApplication::translate("GMainWindow", "24-Hour"),
    };
}

static QString GetProfileTimeFormatText() {
    int tf = UISettings::values.profile_time_format_24h;
    const auto time_format_names = GetProfileTimeFormatNames();
    if (tf == 1) {
        return time_format_names.at(1);
    }
    return time_format_names.at(0);
}

static QString GetProfileLanguageText(Core::System& system) {
    auto cfg = Service::CFG::GetModule(system);
    int idx = 1;
    if (cfg) {
        idx = static_cast<int>(cfg->GetSystemLanguage());
    } else {
        idx = UISettings::values.profile_language;
    }
    if (idx >= 0 && idx < s_profile_lang_names.size()) {
        return s_profile_lang_names.at(idx);
    }
    return s_profile_lang_names.at(1);
}

static QString GetProfileDobText(Core::System& system) {
    int m = UISettings::values.profile_birthmonth;
    int d = UISettings::values.profile_birthday;
    if (m < 1 || m > 12 || d < 1 || d > 31) {
        auto cfg = Service::CFG::GetModule(system);
        if (cfg) {
            auto [cm, cd] = cfg->GetBirthday();
            if (cm >= 1 && cm <= 12 && cd >= 1 && cd <= 31) {
                m = cm;
                d = cd;
            }
        }
    }
    if (m >= 1 && m <= 12 && d >= 1 && d <= 31) {
        return QLocale().toString(QDate(2000, m, d), QCoreApplication::translate("GMainWindow", "MMMM d"));
    }
    return QCoreApplication::translate("GMainWindow", "January 1");
}

static QString GetProfileNickname(Core::System& system) {
    if (!UISettings::values.profile_nickname.trimmed().isEmpty()) {
        return UISettings::values.profile_nickname;
    }
    auto cfg = Service::CFG::GetModule(system);
    if (cfg) {
        QString cfg_name = QString::fromStdU16String(cfg->GetUsername()).trimmed();
        if (!cfg_name.isEmpty()) {
            return cfg_name;
        }
    }
    return QCoreApplication::translate("GMainWindow", "Me");
}

static QPixmap GetProfileAvatarPixmap(const QSize& size, int radius) {
    QPixmap src;
    if (!UISettings::values.profile_avatar_path.isEmpty() &&
        QFile::exists(UISettings::values.profile_avatar_path)) {
        src.load(UISettings::values.profile_avatar_path);
    }
    if (src.isNull()) {
        const QString default_icon =
            QCoreApplication::applicationDirPath() +
            QStringLiteral("/../Resources/home_icons/default_avatar.png");
        if (QFile::exists(default_icon)) {
            src.load(default_icon);
        } else {
            src.load(QStringLiteral(
                "/Users/shoroz/Downloads/azahar-src/dist/home_screen_icons/default_avatar.png"));
        }
    }
    if (src.isNull()) {
        src = QPixmap(size);
        src.fill(QColor(0x28, 0xbf, 0x28));
    }
    return MakeRoundedPixmap(src, size, radius);
}

void GMainWindow::ShowProfileOverlay() {
    if (settings_overlay) {
        settings_overlay->raise();
        return;
    }

    auto* dialog = new QDialog();
    dialog->setObjectName(QStringLiteral("profileDialog"));
    dialog->setWindowFlags(Qt::Widget);

    auto* dialog_layout = new QVBoxLayout(dialog);
    dialog_layout->setContentsMargins(0, 0, 0, 0);
    dialog_layout->setSpacing(0);

    // Header: "User Settings"
    auto* header_widget = new QWidget(dialog);
    auto* header_layout = new QVBoxLayout(header_widget);
    header_layout->setContentsMargins(36, 22, 36, 12);
    auto* title_label = new QLabel(tr("User Settings"), header_widget);
    title_label->setStyleSheet(QStringLiteral(
        "font-size: 22px; font-weight: 600; color: #2C2C2C; border: none; background: transparent;"));
    header_layout->addWidget(title_label);
    dialog_layout->addWidget(header_widget);

    // Top horizontal separator line
    auto* top_line = new QFrame(dialog);
    top_line->setFrameShape(QFrame::HLine);
    top_line->setFrameShadow(QFrame::Plain);
    top_line->setStyleSheet(QStringLiteral(
        "background-color: #D2D2D2; border: none; height: 1px; min-height: 1px; max-height: 1px;"));
    dialog_layout->addWidget(top_line);

    // Main Content: Two Columns
    auto* content_widget = new QWidget(dialog);
    auto* content_layout = new QHBoxLayout(content_widget);
    content_layout->setContentsMargins(44, 24, 44, 20);
    content_layout->setSpacing(48);

    // Left Column: Avatar + Nickname
    auto* left_col = new QWidget(content_widget);
    left_col->setFixedWidth(200);
    auto* left_layout = new QVBoxLayout(left_col);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    struct HoverEventFilter : public QObject {
        QWidget* target_badge;
        HoverEventFilter(QWidget* badge, QObject* parent) : QObject(parent), target_badge(badge) {}
        bool eventFilter(QObject* watched, QEvent* event) override {
            if (event->type() == QEvent::Enter) {
                target_badge->setVisible(true);
            } else if (event->type() == QEvent::Leave) {
                target_badge->setVisible(false);
            }
            return QObject::eventFilter(watched, event);
        }
    };

    auto* avatar_btn = new QPushButton(left_col);
    avatar_btn->setFixedSize(160, 160);
    avatar_btn->setCursor(Qt::PointingHandCursor);
    avatar_btn->setStyleSheet(QStringLiteral("background: transparent; border: none; padding: 0;"));

    auto* avatar_label = new QLabel(avatar_btn);
    avatar_label->setFixedSize(160, 160);
    avatar_label->setPixmap(GetProfileAvatarPixmap(QSize(160, 160), 8));
    avatar_label->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    avatar_label->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* pencil_badge = new QLabel(avatar_btn);
    pencil_badge->setFixedSize(160, 160);
    pencil_badge->setAlignment(Qt::AlignCenter);
    pencil_badge->setText(QString::fromUtf8("✏"));
    pencil_badge->setStyleSheet(QStringLiteral(
        "background-color: rgba(0, 0, 0, 0.45); color: #FFFFFF; font-size: 42px; border-radius: 8px;"));
    pencil_badge->setAttribute(Qt::WA_TransparentForMouseEvents);
    pencil_badge->setVisible(false);

    avatar_btn->installEventFilter(new HoverEventFilter(pencil_badge, avatar_btn));
    left_layout->addWidget(avatar_btn, 0, Qt::AlignHCenter);

    left_layout->addSpacing(14);
    auto* nickname_display = new QLabel(GetProfileNickname(system), left_col);
    nickname_display->setAlignment(Qt::AlignCenter);
    nickname_display->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: 600; color: #2C2C2C; border: none; background: transparent;"));
    left_layout->addWidget(nickname_display, 0, Qt::AlignHCenter);
    left_layout->addStretch();
    content_layout->addWidget(left_col);

    // Right Column: Profile Settings (Nick Name, Full Name, Region, Date of Birth, Gender, Time Format, Language)
    auto* right_col = new QWidget(content_widget);
    auto* right_layout = new QVBoxLayout(right_col);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(0);

    // Section title with vertical accent bar
    auto* sec_widget = new QWidget(right_col);
    auto* sec_layout = new QHBoxLayout(sec_widget);
    sec_layout->setContentsMargins(0, 0, 0, 10);
    sec_layout->setSpacing(8);
    auto* accent_bar = new QFrame(sec_widget);
    accent_bar->setFixedSize(3, 18);
    accent_bar->setStyleSheet(QStringLiteral(
        "background-color: #2C2C2C; border-radius: 1px; border: none;"));
    sec_layout->addWidget(accent_bar);
    auto* sec_title = new QLabel(tr("Profile Settings"), sec_widget);
    sec_title->setStyleSheet(QStringLiteral(
        "font-size: 16px; font-weight: 600; color: #2C2C2C; border: none; background: transparent;"));
    sec_layout->addWidget(sec_title);
    sec_layout->addStretch();
    right_layout->addWidget(sec_widget);

    auto make_row_divider = [](QWidget* parent) {
        auto* sep = new QFrame(parent);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(QStringLiteral(
            "background-color: #D8D8D8; border: none; height: 1px; min-height: 1px; max-height: 1px;"));
        return sep;
    };

    right_layout->addWidget(make_row_divider(right_col));

    auto make_row = [&](const QString& title_text, const QString& initial_val) -> ProfileRow {
        auto* btn = new QPushButton(right_col);
        btn->setFixedHeight(46);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; border: none; border-radius: 4px; padding: 0 8px; }"
            "QPushButton:hover { background-color: rgba(0, 0, 0, 0.05); }"
            "QPushButton:pressed { background-color: rgba(0, 0, 0, 0.09); }"));
        auto* row_layout = new QHBoxLayout(btn);
        row_layout->setContentsMargins(8, 0, 8, 0);
        auto* lbl_title = new QLabel(title_text, btn);
        lbl_title->setStyleSheet(QStringLiteral(
            "font-size: 15px; font-weight: 500; color: #2C2C2C; background: transparent; border: none;"));
        row_layout->addWidget(lbl_title);
        row_layout->addStretch();
        auto* lbl_val = new QLabel(initial_val, btn);
        lbl_val->setStyleSheet(QStringLiteral(
            "font-size: 15px; font-weight: 600; color: #2D55FA; background: transparent; border: none;"));
        row_layout->addWidget(lbl_val);
        right_layout->addWidget(btn);
        right_layout->addWidget(make_row_divider(right_col));
        return {btn, lbl_title, lbl_val};
    };

    // 1. Nick Name
    auto row_nickname = make_row(tr("Nick Name"), GetProfileNickname(system));

    // 2. Full Name
    auto row_fullname = make_row(tr("Full Name"), GetProfileFullNameText());

    // 3. Region
    auto row_region = make_row(tr("Region"), GetProfileRegionText());

    // 4. Date of Birth
    auto row_dob = make_row(tr("Date of Birth"), GetProfileDobText(system));

    // 5. Gender
    auto row_gender = make_row(tr("Gender"), GetProfileGenderText());

    // 6. Time Format
    auto row_time_format = make_row(tr("Time Format"), GetProfileTimeFormatText());

    // 7. Language
    auto row_language = make_row(tr("Language"), GetProfileLanguageText(system));

    right_layout->addStretch();
    content_layout->addWidget(right_col, 1);
    dialog_layout->addWidget(content_widget, 1);

    // Bottom horizontal separator line
    auto* bottom_line = new QFrame(dialog);
    bottom_line->setFrameShape(QFrame::HLine);
    bottom_line->setFrameShadow(QFrame::Plain);
    bottom_line->setStyleSheet(QStringLiteral(
        "background-color: #D2D2D2; border: none; height: 1px; min-height: 1px; max-height: 1px;"));
    dialog_layout->addWidget(bottom_line);

    // Footer with Back/OK action buttons
    auto* footer_widget = new QWidget(dialog);
    auto* footer_layout = new QHBoxLayout(footer_widget);
    footer_layout->setContentsMargins(36, 10, 36, 12);
    footer_layout->addStretch();

    auto* button_box =
        new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, footer_widget);
    button_box->setObjectName(QStringLiteral("buttonBox"));
    if (auto* cancel_button = button_box->button(QDialogButtonBox::Cancel)) {
        cancel_button->setText(tr("Back"));
        cancel_button->setIcon(MakeActionBadge(QStringLiteral("B"), QColor(0xe0, 0x3c, 0x3c)));
        cancel_button->setIconSize(QSize(22, 22));
    }
    if (auto* ok_button = button_box->button(QDialogButtonBox::Ok)) {
        ok_button->setText(tr("OK"));
        ok_button->setIcon(MakeActionBadge(QStringLiteral("A"), QColor(0x3c, 0x8c, 0xe0)));
        ok_button->setIconSize(QSize(22, 22));
    }
    footer_layout->addWidget(button_box);
    dialog_layout->addWidget(footer_widget);

    auto retranslate_profile_dialog = [this, title_label, sec_title, row_nickname, row_fullname,
                                       row_region, row_dob, row_gender, row_time_format,
                                       row_language, button_box] {
        title_label->setText(tr("User Settings"));
        sec_title->setText(tr("Profile Settings"));

        row_nickname.title_lbl->setText(tr("Nick Name"));
        row_fullname.title_lbl->setText(tr("Full Name"));
        row_region.title_lbl->setText(tr("Region"));
        row_dob.title_lbl->setText(tr("Date of Birth"));
        row_gender.title_lbl->setText(tr("Gender"));
        row_time_format.title_lbl->setText(tr("Time Format"));
        row_language.title_lbl->setText(tr("Language"));

        row_fullname.val_lbl->setText(GetProfileFullNameText());
        row_region.val_lbl->setText(GetProfileRegionText());
        row_dob.val_lbl->setText(GetProfileDobText(system));
        row_gender.val_lbl->setText(GetProfileGenderText());
        row_time_format.val_lbl->setText(GetProfileTimeFormatText());
        row_language.val_lbl->setText(GetProfileLanguageText(system));

        if (auto* cancel_button = button_box->button(QDialogButtonBox::Cancel)) {
            cancel_button->setText(tr("Back"));
        }
        if (auto* ok_button = button_box->button(QDialogButtonBox::Ok)) {
            ok_button->setText(tr("OK"));
        }
    };

    struct ProfileDialogFilter : public QObject {
        std::function<void()> on_retranslate;
        ProfileDialogFilter(std::function<void()> cb, QObject* parent)
            : QObject(parent), on_retranslate(std::move(cb)) {}
        bool eventFilter(QObject* watched, QEvent* event) override {
            if (event->type() == QEvent::LanguageChange) {
                if (on_retranslate) {
                    on_retranslate();
                }
            }
            return QObject::eventFilter(watched, event);
        }
    };
    dialog->installEventFilter(new ProfileDialogFilter(retranslate_profile_dialog, dialog));

    // Connections for functionality with Nintendo Console UI Modals
    connect(avatar_btn, &QPushButton::clicked, this,
            [this, avatar_label] {
                const QString filename = QFileDialog::getOpenFileName(
                    this, tr("Select Profile Icon"), QDir::homePath(),
                    tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
                if (!filename.isEmpty()) {
                    UISettings::values.profile_avatar_path = filename;
                    config->Save();
                    avatar_label->setPixmap(GetProfileAvatarPixmap(QSize(160, 160), 8));
                    if (home_screen) {
                        home_screen->ApplyAppearanceSettings();
                    }
                }
            });

    connect(row_nickname.btn, &QPushButton::clicked, this,
            [this, row_nickname, nickname_display] {
                const QString cur = GetProfileNickname(system);
                auto res = ConsoleUI::ShowTextInputModal(
                    this, tr("Nick Name"),
                    tr("Enter a nickname for your profile (up to 10 characters):"),
                    cur, 10);
                if (res && !res->trimmed().isEmpty()) {
                    const QString trimmed = res->trimmed().left(10);
                    UISettings::values.profile_nickname = trimmed;
                    config->Save();
                    auto cfg = Service::CFG::GetModule(system);
                    if (cfg) {
                        cfg->SetUsername(trimmed.toStdU16String());
                        cfg->UpdateConfigNANDSavegame();
                    }
                    row_nickname.val_lbl->setText(trimmed);
                    nickname_display->setText(trimmed);
                    if (home_screen) {
                        home_screen->ApplyAppearanceSettings();
                    }
                }
            });

    connect(row_fullname.btn, &QPushButton::clicked, this,
            [this, row_fullname] {
                const QString cur = UISettings::values.profile_full_name;
                auto res = ConsoleUI::ShowTextInputModal(
                    this, tr("Full Name"),
                    tr("Enter your full profile name:"),
                    cur, 40);
                if (res) {
                    UISettings::values.profile_full_name = res->trimmed();
                    config->Save();
                    row_fullname.val_lbl->setText(GetProfileFullNameText());
                }
            });

    connect(row_region.btn, &QPushButton::clicked, this,
            [this, row_region] {
                int cur_idx = Settings::values.region_value.GetValue() + 1;
                const auto region_names = GetProfileRegionNames();
                if (cur_idx < 0 || cur_idx >= region_names.size()) {
                    cur_idx = 0;
                }
                int selected = ConsoleUI::ShowChoiceModal(
                    this, tr("Region"),
                    tr("Select emulated console region:"),
                    region_names, cur_idx);
                if (selected >= 0) {
                    int new_val = selected - 1;
                    Settings::values.region_value.SetValue(new_val);
                    UISettings::values.profile_region = new_val;
                    config->Save();
                    row_region.val_lbl->setText(GetProfileRegionText());
                    UpdateBootHomeMenuState();
                }
            });

    connect(row_dob.btn, &QPushButton::clicked, this,
            [this, row_dob] {
                auto cfg = Service::CFG::GetModule(system);
                int cur_m = UISettings::values.profile_birthmonth;
                int cur_d = UISettings::values.profile_birthday;
                if (cur_m < 1 || cur_m > 12 || cur_d < 1 || cur_d > 31) {
                    if (cfg) {
                        auto [m, d] = cfg->GetBirthday();
                        if (m >= 1 && m <= 12) cur_m = m;
                        if (d >= 1 && d <= 31) cur_d = d;
                    }
                }
                if (cur_m < 1 || cur_m > 12) cur_m = 1;
                if (cur_d < 1 || cur_d > 31) cur_d = 1;

                auto res = ConsoleUI::ShowDateModal(
                    this, tr("Date of Birth"),
                    tr("Select your birthday:"), cur_m, cur_d);
                if (res) {
                    auto [picked_m, picked_d] = *res;
                    UISettings::values.profile_birthmonth = picked_m;
                    UISettings::values.profile_birthday = picked_d;
                    config->Save();
                    if (cfg) {
                        cfg->SetBirthday(static_cast<u8>(picked_m), static_cast<u8>(picked_d));
                        cfg->UpdateConfigNANDSavegame();
                    }
                    row_dob.val_lbl->setText(GetProfileDobText(system));
                }
            });

    connect(row_gender.btn, &QPushButton::clicked, this,
            [this, row_gender] {
                int cur = UISettings::values.profile_gender;
                const auto gender_names = GetProfileGenderNames();
                if (cur < 0 || cur >= gender_names.size()) cur = 0;
                int selected = ConsoleUI::ShowChoiceModal(
                    this, tr("Gender"),
                    tr("Select gender preference:"),
                    gender_names, cur);
                if (selected >= 0) {
                    UISettings::values.profile_gender = selected;
                    config->Save();
                    row_gender.val_lbl->setText(GetProfileGenderText());
                }
            });

    connect(row_time_format.btn, &QPushButton::clicked, this,
            [this, row_time_format] {
                int cur = (UISettings::values.profile_time_format_24h == 1) ? 1 : 0;
                const auto time_format_names = GetProfileTimeFormatNames();
                int selected = ConsoleUI::ShowChoiceModal(
                    this, tr("Time Format"),
                    tr("Select clock display format:"),
                    time_format_names, cur);
                if (selected >= 0) {
                    UISettings::values.profile_time_format_24h = (selected == 1 ? 1 : 0);
                    config->Save();
                    row_time_format.val_lbl->setText(GetProfileTimeFormatText());
                    if (home_screen) {
                        home_screen->ApplyAppearanceSettings();
                    }
                }
            });

    connect(row_language.btn, &QPushButton::clicked, this,
            [this, retranslate_profile_dialog] {
                auto cfg = Service::CFG::GetModule(system);
                int cur_idx = cfg ? static_cast<int>(cfg->GetSystemLanguage())
                                  : UISettings::values.profile_language;
                if (cur_idx < 0 || cur_idx >= s_profile_lang_names.size()) cur_idx = 1;
                int selected = ConsoleUI::ShowChoiceModal(
                    this, tr("Language"),
                    tr("Select system language:"),
                    s_profile_lang_names, cur_idx);
                if (selected >= 0) {
                    if (cfg) {
                        cfg->SetSystemLanguage(static_cast<Service::CFG::SystemLanguage>(selected));
                        cfg->UpdateConfigNANDSavegame();
                    }
                    UISettings::values.profile_language = selected;

                    const QString locale = LocaleFromProfileLanguage(selected);

                    // Apply the language change across all areas of the application:
                    // 1. Installs translators for main window, menus, status bar, and dialogs
                    // 2. Retranslates game list and home screen
                    OnLanguageChanged(locale);

                    // Persist new language settings
                    config->Save();

                    // 3. Immediately retranslate the active Profile Settings dialog
                    retranslate_profile_dialog();

                    if (home_screen) {
                        home_screen->ApplyAppearanceSettings();
                    }
                }
            });

    QString qss;
    qss += QStringLiteral("* { font-family: 'Roboto', -apple-system, sans-serif; }");
    qss += QStringLiteral("QDialog#profileDialog { background: transparent; border: none; }");
    qss += QStringLiteral("QDialogButtonBox QPushButton { background: transparent; border: none;"
                          " border-radius: 6px; color: #2C2C2C; padding: 4px 10px; font-weight: 600; font-size: 15px; }");
    qss += QStringLiteral("QDialogButtonBox QPushButton:hover { background-color: rgba(0, 0, 0, 0.06); }");
    qss += QStringLiteral("QDialogButtonBox QPushButton:focus { border: none; }");
    qss += QStringLiteral("QDialogButtonBox { border: none; }");
    dialog->setStyleSheet(qss);

    settings_overlay = new QWidget(ui->centralwidget);
    settings_overlay->setObjectName(QStringLiteral("settingsOverlayBackdrop"));
    settings_overlay->setStyleSheet(
        QStringLiteral("#settingsOverlayBackdrop { background-color: rgba(0, 0, 0, 150); }"));
    settings_overlay->setAttribute(Qt::WA_StyledBackground, true);
    settings_overlay->installEventFilter(this);

    settings_card = new QWidget(settings_overlay);
    settings_card->setObjectName(QStringLiteral("settingsOverlayCard"));
    settings_card->setStyleSheet(
        QStringLiteral("#settingsOverlayCard { background-color: #EBEBEB; border: 1px solid #D0D0D0;"
                       " border-radius: 16px; }"));
    settings_card->setAttribute(Qt::WA_StyledBackground, true);
    auto* card_layout = new QVBoxLayout(settings_card);
    card_layout->setContentsMargins(1, 1, 1, 1);
    card_layout->addWidget(dialog);

    RepositionSettingsOverlay();
    settings_overlay->show();
    settings_overlay->raise();
    dialog->setFocus();

    {
        const QRect target = settings_card->geometry();
        QRect from = target;
        from.setSize(target.size() * 0.96);
        from.moveCenter(target.center() + QPoint(0, 14));
        auto* rise = new QPropertyAnimation(settings_card, "geometry", settings_card);
        rise->setDuration(190);
        rise->setStartValue(from);
        rise->setEndValue(target);
        rise->setEasingCurve(QEasingCurve::OutCubic);
        rise->start(QAbstractAnimation::DeleteWhenStopped);
    }

    const auto close_overlay = [this] {
        if (settings_overlay) {
            settings_overlay->deleteLater();
            settings_overlay = nullptr;
            settings_card = nullptr;
        }
    };
    connect(button_box, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(dialog, &QDialog::accepted, this, close_overlay);
    connect(dialog, &QDialog::rejected, this, close_overlay);
}

void GMainWindow::RepositionSettingsOverlay() {
    if (!settings_overlay || !settings_card) {
        return;
    }
    settings_overlay->setGeometry(ui->centralwidget->rect());
    const QSize overlay_size = settings_overlay->size();
    const QSize card_size(qMin(overlay_size.width() - 80, 900),
                          qMin(overlay_size.height() - 80, 650));
    settings_card->setGeometry((overlay_size.width() - card_size.width()) / 2,
                               (overlay_size.height() - card_size.height()) / 2, card_size.width(),
                               card_size.height());
}

// The in-game Home button floats directly over the running game rather than inside any themed
// container, so it can't inherit the palette and has to be recoloured explicitly whenever the
// theme changes — otherwise the dark-theme chrome stays on screen under the White theme.
void GMainWindow::ApplyOverlayButtonTheme() {
    if (!home_button) {
        return;
    }
    const bool white = UISettings::values.theme == QStringLiteral("white");
    const QColor foreground = white ? QColor(Qt::black) : QColor(Qt::white);

    // The letterbox around the 3DS screens is painted by the renderer (its clear colour), not by
    // Qt, so the theme has to drive it explicitly or the game stays framed in black under the
    // White theme. Note this overrides the Background Color setting on the Graphics page.
    const float bg = white ? 1.0f : 0.0f;
    Settings::values.bg_red = bg;
    Settings::values.bg_green = bg;
    Settings::values.bg_blue = bg;

#ifdef Q_OS_MAC
    const QString home_icon_path = QCoreApplication::applicationDirPath() +
                                   QStringLiteral("/../Resources/home_icons/home.png");
#else
    const QString home_icon_path =
        QCoreApplication::applicationDirPath() + QStringLiteral("/home_icons/home.png");
#endif
    // home.png ships as a white glyph; recolour it by keeping its alpha and replacing the RGB,
    // so the same asset serves both themes without needing a second file.
    QPixmap home_icon(home_icon_path);
    if (!home_icon.isNull()) {
        QPainter tint(&home_icon);
        tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint.fillRect(home_icon.rect(), foreground);
        tint.end();
    }
    home_button->setIcon(QIcon(home_icon));

    if (auto* overlay_btn = dynamic_cast<OverlayHomeButton*>(home_button)) {
        overlay_btn->SetTheme(white);
    }
    if (ingame_status) {
        ingame_status->SetTheme(white);
    }
    if (controllers_popover) {
        controllers_popover->SetTheme(white);
    }
}

void GMainWindow::RepositionHomeButton() {
    if (!home_button) {
        return;
    }
    constexpr int Margin = 16;
    const QRect area = ui->centralwidget->rect();
    home_button->move(area.left() + Margin, area.bottom() - home_button->height() - Margin);
}

void GMainWindow::RepositionInGameStatus() {
    if (!ingame_status) {
        return;
    }
    constexpr int Margin = 16;
    const int x = std::max(0, ui->centralwidget->width() - ingame_status->width() - Margin);
    const int y = Margin;
    ingame_status->move(x, y);
}

void GMainWindow::RepositionControllersPopover() {
    if (!controllers_popover) {
        return;
    }
    constexpr int Margin = 16;
    constexpr int Gap = 8;
    const int popover_w = controllers_popover->width();
    int x = ui->centralwidget->width() - popover_w - Margin;
    int y = Margin;

    if (emulation_running && ingame_status && ingame_status->isVisible()) {
        y = ingame_status->geometry().bottom() + Gap;
        x = ui->centralwidget->width() - popover_w - Margin;
    } else if (home_screen && home_screen->isVisible() && home_screen->GetBluetoothButton()) {
        auto* bt_btn = home_screen->GetBluetoothButton();
        const QPoint btn_pos = bt_btn->mapTo(ui->centralwidget, QPoint(0, 0));
        int target_x = btn_pos.x() + bt_btn->width() - popover_w + 30;
        x = std::clamp(target_x, Margin, ui->centralwidget->width() - popover_w - Margin);
        y = btn_pos.y() + bt_btn->height() + Gap;
    } else if (ingame_status) {
        y = ingame_status->geometry().bottom() + Gap;
    }

    if (x < Margin) {
        x = Margin;
    }
    const int max_y = ui->centralwidget->height() - controllers_popover->height() - Margin;
    if (y > max_y) {
        y = std::max(Margin, max_y);
    }
    controllers_popover->move(x, y);
}

void GMainWindow::ToggleControllersPopover() {
    if (!controllers_popover) {
        controllers_popover = new ControllersPopoverWidget(this, ui->centralwidget);
        const bool white = UISettings::values.theme == QStringLiteral("white");
        controllers_popover->SetTheme(white);
    }
    if (controllers_popover->isVisible()) {
        HideControllersPopover();
    } else {
        controllers_popover->RefreshDevices();
        RepositionControllersPopover();
        controllers_popover->show();
        controllers_popover->raise();
        if (ingame_status) {
            ingame_status->SetPopoverOpen(true);
        }
    }
}

void GMainWindow::HideControllersPopover() {
    if (controllers_popover && controllers_popover->isVisible()) {
        controllers_popover->hide();
    }
    if (ingame_status) {
        ingame_status->SetPopoverOpen(false);
    }
}

ControllersPopoverWidget::ControllersPopoverWidget(GMainWindow* main_win, QWidget* parent)
    : QWidget(parent), main_window(main_win) {
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedWidth(350);

    root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(16, 14, 16, 14);
    root_layout->setSpacing(10);

    // Header
    auto* header_layout = new QHBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);

    title_label = new QLabel(tr("Bluetooth & Controllers"), this);
    QFont tf = title_label->font();
    tf.setPointSize(13);
    tf.setBold(true);
    title_label->setFont(tf);
    header_layout->addWidget(title_label);
    header_layout->addStretch();

    close_btn = new QPushButton(QStringLiteral("✕"), this);
    close_btn->setFixedSize(22, 22);
    close_btn->setCursor(Qt::PointingHandCursor);
    connect(close_btn, &QPushButton::clicked, this, [this] {
        if (main_window) {
            main_window->HideControllersPopover();
        }
    });
    header_layout->addWidget(close_btn);
    root_layout->addLayout(header_layout);

    // Section 1: Devices
    devices_header = new QLabel(tr("DEVICES"), this);
    QFont hf = devices_header->font();
    hf.setPointSize(9);
    hf.setBold(true);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    devices_header->setFont(hf);
    root_layout->addWidget(devices_header);

    devices_container = new QWidget(this);
    devices_layout = new QVBoxLayout(devices_container);
    devices_layout->setContentsMargins(0, 0, 0, 0);
    devices_layout->setSpacing(6);
    root_layout->addWidget(devices_container);

    // Pair & Refresh button row
    auto* dev_btn_row = new QHBoxLayout();
    dev_btn_row->setContentsMargins(0, 0, 0, 0);
    dev_btn_row->setSpacing(8);

    pair_btn = new QPushButton(tr("Pair via Bluetooth..."), this);
    pair_btn->setFixedHeight(26);
    pair_btn->setCursor(Qt::PointingHandCursor);
    connect(pair_btn, &QPushButton::clicked, this, &ControllersPopoverWidget::OpenBluetoothPreferences);
    dev_btn_row->addWidget(pair_btn);

    refresh_btn = new QPushButton(tr("⟳ Refresh"), this);
    refresh_btn->setFixedHeight(26);
    refresh_btn->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn, &QPushButton::clicked, this, &ControllersPopoverWidget::RefreshDevices);
    dev_btn_row->addWidget(refresh_btn);

    root_layout->addLayout(dev_btn_row);

    // Divider
    divider1 = new QFrame(this);
    divider1->setFixedHeight(1);
    root_layout->addWidget(divider1);

    // Section 2: Screen & Window Assignment
    assign_header = new QLabel(tr("SCREEN & WINDOW ASSIGNMENT"), this);
    assign_header->setFont(hf);
    root_layout->addWidget(assign_header);

    // Top Screen Row
    auto* top_row = new QHBoxLayout();
    top_row->setContentsMargins(0, 0, 0, 0);
    top_label = new QLabel(tr("Top Screen (Main):"), this);
    top_combo = new QComboBox(this);
    top_combo->setCursor(Qt::PointingHandCursor);
    connect(top_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ControllersPopoverWidget::OnTopProfileChanged);
    top_row->addWidget(top_label);
    top_row->addWidget(top_combo, 1);
    root_layout->addLayout(top_row);

    // Bottom Screen Row
    auto* bottom_row = new QHBoxLayout();
    bottom_row->setContentsMargins(0, 0, 0, 0);
    bottom_label = new QLabel(tr("Bottom Screen (Sub):"), this);
    bottom_combo = new QComboBox(this);
    bottom_combo->setCursor(Qt::PointingHandCursor);
    bottom_combo->addItem(tr("Touchscreen / Direct Mouse"));
    bottom_combo->addItem(tr("Follow Main Controller"));
    bottom_combo->addItem(tr("Separate Controller (P2)"));
    bottom_row->addWidget(bottom_label);
    bottom_row->addWidget(bottom_combo, 1);
    root_layout->addLayout(bottom_row);

    // Auto Map button
    automap_btn = new QPushButton(tr("⚡ Auto-Map Connected Controller"), this);
    automap_btn->setFixedHeight(28);
    automap_btn->setCursor(Qt::PointingHandCursor);
    connect(automap_btn, &QPushButton::clicked, this,
            &ControllersPopoverWidget::AutoMapPrimaryController);
    root_layout->addWidget(automap_btn);

    automap_status = new QLabel(this);
    automap_status->setAlignment(Qt::AlignCenter);
    automap_status->hide();
    root_layout->addWidget(automap_status);

    // Divider
    divider2 = new QFrame(this);
    divider2->setFixedHeight(1);
    root_layout->addWidget(divider2);

    // Footer
    auto* footer_row = new QHBoxLayout();
    footer_row->setContentsMargins(0, 0, 0, 0);

    bt_settings_link = new QPushButton(tr("Bluetooth Settings..."), this);
    bt_settings_link->setCursor(Qt::PointingHandCursor);
    connect(bt_settings_link, &QPushButton::clicked, this,
            &ControllersPopoverWidget::OpenBluetoothPreferences);
    footer_row->addWidget(bt_settings_link);
    footer_row->addStretch();

    all_settings_link = new QPushButton(tr("All Settings..."), this);
    all_settings_link->setCursor(Qt::PointingHandCursor);
    connect(all_settings_link, &QPushButton::clicked, this,
            &ControllersPopoverWidget::OpenControllerSettings);
    footer_row->addWidget(all_settings_link);
    root_layout->addLayout(footer_row);

    ApplyStyles();
    RefreshDevices();
}

ControllersPopoverWidget::~ControllersPopoverWidget() {
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void ControllersPopoverWidget::SetTheme(bool is_white) {
    if (white == is_white) {
        return;
    }
    white = is_white;
    ApplyStyles();
    RefreshDevices();
    update();
}

void ControllersPopoverWidget::ApplyStyles() {
    const QString fg = white ? QStringLiteral("#1C1C1E") : QStringLiteral("#FFFFFF");
    const QString sub_fg = white ? QStringLiteral("rgba(60, 60, 67, 0.70)")
                                 : QStringLiteral("rgba(255, 255, 255, 0.65)");
    const QString item_bg = white ? QStringLiteral("rgba(0, 0, 0, 0.05)")
                                  : QStringLiteral("rgba(255, 255, 255, 0.08)");
    const QString item_hover = white ? QStringLiteral("rgba(0, 0, 0, 0.10)")
                                     : QStringLiteral("rgba(255, 255, 255, 0.14)");
    const QString border_color = white ? QStringLiteral("rgba(0, 0, 0, 0.12)")
                                       : QStringLiteral("rgba(255, 255, 255, 0.12)");

    title_label->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(fg));
    devices_header->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(sub_fg));
    assign_header->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(sub_fg));
    top_label->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; background: transparent;").arg(fg));
    bottom_label->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; background: transparent;").arg(fg));

    close_btn->setStyleSheet(QStringLiteral(
        "QPushButton { border-radius: 11px; background-color: %1; color: %2; font-size: 11px; font-weight: bold; border: none; }"
        "QPushButton:hover { background-color: %3; color: %4; }")
        .arg(item_bg, sub_fg, item_hover, fg));

    const QString btn_style = QStringLiteral(
        "QPushButton { background-color: %1; color: %2; border-radius: 6px; padding: 0 10px; font-size: 11px; border: none; font-weight: 500; }"
        "QPushButton:hover { background-color: %3; }")
        .arg(item_bg, fg, item_hover);
    pair_btn->setStyleSheet(btn_style);
    refresh_btn->setStyleSheet(btn_style);

    const QString div_style = QStringLiteral("background-color: %1; border: none;").arg(border_color);
    divider1->setStyleSheet(div_style);
    divider2->setStyleSheet(div_style);

    const QString combo_style = QStringLiteral(
        "QComboBox { background-color: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 3px 8px; font-size: 11px; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView { background-color: %4; color: %2; selection-background-color: #007AFF; selection-color: #FFFFFF; border: 1px solid %3; }")
        .arg(item_bg, fg, border_color, white ? QStringLiteral("#FFFFFF") : QStringLiteral("#24262C"));
    top_combo->setStyleSheet(combo_style);
    bottom_combo->setStyleSheet(combo_style);

    automap_btn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #007AFF; color: #FFFFFF; font-weight: 600; font-size: 11px; border-radius: 6px; border: none; }"
        "QPushButton:hover { background-color: #0062CC; }"
        "QPushButton:pressed { background-color: #004FB3; }"));

    bt_settings_link->setStyleSheet(QStringLiteral(
        "QPushButton { color: %1; font-size: 11px; border: none; background: transparent; text-align: left; padding: 2px 0; }"
        "QPushButton:hover { color: %2; }")
        .arg(sub_fg, fg));

    all_settings_link->setStyleSheet(QStringLiteral(
        "QPushButton { color: #007AFF; font-size: 11px; font-weight: 600; border: none; background: transparent; text-align: right; padding: 2px 0; }"
        "QPushButton:hover { color: #0056B3; }"));
}

void ControllersPopoverWidget::RefreshDevices() {
    QLayoutItem* child;
    while ((child = devices_layout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    InputCommon::ReloadInputDevices();

    int num_joysticks = 0;
#ifdef HAVE_SDL2
    num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; ++i) {
        auto* row = new QWidget(devices_container);
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(4, 2, 4, 2);
        row_layout->setSpacing(8);

        auto* icon = new DeviceIconWidget(true, white, row);
        row_layout->addWidget(icon);

        const char* name = SDL_JoystickNameForIndex(i);
        QString dev_name = name ? QString::fromUtf8(name) : tr("Game Controller %1").arg(i + 1);
        auto* name_label = new QLabel(dev_name, row);
        name_label->setStyleSheet(QStringLiteral("color: %1; font-weight: 600; font-size: 12px; background: transparent;").arg(white ? QStringLiteral("#1C1C1E") : QStringLiteral("#FFFFFF")));
        row_layout->addWidget(name_label);
        row_layout->addStretch();

        auto* conn_label = new QLabel(tr("Connected"), row);
        conn_label->setStyleSheet(QStringLiteral("color: #34C759; font-weight: 600; font-size: 11px; background: transparent;"));
        row_layout->addWidget(conn_label);

        devices_layout->addWidget(row);
    }
#endif
    if (num_joysticks == 0) {
        auto* row = new QWidget(devices_container);
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(4, 2, 4, 2);
        row_layout->setSpacing(8);

        auto* icon = new DeviceIconWidget(false, white, row);
        row_layout->addWidget(icon);

        auto* name_label = new QLabel(tr("Built-in Keyboard"), row);
        name_label->setStyleSheet(QStringLiteral("color: %1; font-weight: 600; font-size: 12px; background: transparent;").arg(white ? QStringLiteral("#1C1C1E") : QStringLiteral("#FFFFFF")));
        row_layout->addWidget(name_label);
        row_layout->addStretch();

        auto* conn_label = new QLabel(tr("Active"), row);
        conn_label->setStyleSheet(QStringLiteral("color: #34C759; font-weight: 600; font-size: 11px; background: transparent;"));
        row_layout->addWidget(conn_label);

        devices_layout->addWidget(row);

        auto* hint = new QLabel(tr("No gamepad connected. Pair a controller below."), devices_container);
        hint->setStyleSheet(QStringLiteral("font-size: 11px; color: %1; background: transparent;").arg(white ? QStringLiteral("#8E8E93") : QStringLiteral("#98989D")));
        devices_layout->addWidget(hint);
    }

    suppress_profile_events = true;
    top_combo->clear();
    for (const auto& profile : Settings::values.input_profiles) {
        top_combo->addItem(QString::fromStdString(profile.name));
    }
    if (Settings::values.current_input_profile_index >= 0 &&
        Settings::values.current_input_profile_index < top_combo->count()) {
        top_combo->setCurrentIndex(Settings::values.current_input_profile_index);
    }
    suppress_profile_events = false;

    adjustSize();
    if (main_window) {
        main_window->RepositionControllersPopover();
    }
}

void ControllersPopoverWidget::OpenBluetoothPreferences() {
#ifdef __APPLE__
    QProcess::startDetached(QStringLiteral("open"),
                           {QStringLiteral("x-apple.systempreferences:com.apple.Bluetooth")});
#endif
}

void ControllersPopoverWidget::OpenControllerSettings() {
    if (main_window) {
        main_window->HideControllersPopover();
        main_window->ShowConfigureOverlay(2, true);
    }
}

void ControllersPopoverWidget::OnTopProfileChanged(int index) {
    if (suppress_profile_events || index < 0 ||
        index >= static_cast<int>(Settings::values.input_profiles.size())) {
        return;
    }
    Settings::values.current_input_profile_index = index;
    Settings::LoadProfile(index);
    if (main_window && main_window->GetConfig()) {
        main_window->GetConfig()->Save();
    }
    if (main_window) {
        main_window->GetSystem().ApplySettings();
    }
    InputCommon::ReloadInputDevices();
}

void ControllersPopoverWidget::AutoMapPrimaryController() {
#ifdef HAVE_SDL2
    if (SDL_NumJoysticks() == 0) {
        automap_status->setText(tr("No gamepad connected to map"));
        automap_status->setStyleSheet(QStringLiteral("color: #FF9500; font-size: 11px; background: transparent;"));
        automap_status->show();
        adjustSize();
        if (main_window) {
            main_window->RepositionControllersPopover();
        }
        return;
    }
    SDL_Joystick* js = SDL_JoystickOpen(0);
    if (!js) {
        automap_status->setText(tr("Unable to open controller"));
        automap_status->setStyleSheet(QStringLiteral("color: #FF3B30; font-size: 11px; background: transparent;"));
        automap_status->show();
        adjustSize();
        if (main_window) {
            main_window->RepositionControllersPopover();
        }
        return;
    }
    char guid_str[64] = {0};
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(js), guid_str, sizeof(guid_str));
    Common::ParamPackage params;
    params.Set("engine", "sdl");
    params.Set("port", 0);
    params.Set("guid", guid_str);

    bool mapped = false;
    for (int b = 0; b < Settings::NativeButton::NumButtons; ++b) {
        auto bind = InputCommon::GetControllerButtonBinds(params, b);
        if (bind.Has("engine")) {
            Settings::values.current_input_profile.buttons[b] = bind.Serialize();
            mapped = true;
        }
    }
    for (int a = 0; a < Settings::NativeAnalog::NumAnalogs; ++a) {
        auto bind = InputCommon::GetControllerAnalogBinds(params, a);
        if (bind.Has("engine")) {
            Settings::values.current_input_profile.analogs[a] = bind.Serialize();
            mapped = true;
        }
    }
    SDL_JoystickClose(js);

    if (mapped) {
        Settings::SaveProfile(Settings::values.current_input_profile_index);
        if (main_window && main_window->GetConfig()) {
            main_window->GetConfig()->Save();
        }
        if (main_window) {
            main_window->GetSystem().ApplySettings();
        }
        InputCommon::ReloadInputDevices();
        automap_status->setText(tr("✓ Controller auto-mapped to 3DS controls!"));
        automap_status->setStyleSheet(QStringLiteral("color: #34C759; font-size: 11px; font-weight: bold; background: transparent;"));
        automap_status->show();
    } else {
        automap_status->setText(tr("Auto-mapping unsupported for this controller"));
        automap_status->setStyleSheet(QStringLiteral("color: #FF9500; font-size: 11px; background: transparent;"));
        automap_status->show();
    }
#else
    automap_status->setText(tr("SDL2 not enabled in this build"));
    automap_status->setStyleSheet(QStringLiteral("color: #FF9500; font-size: 11px; background: transparent;"));
    automap_status->show();
#endif
    adjustSize();
    if (main_window) {
        main_window->RepositionControllersPopover();
    }
}

void ControllersPopoverWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor bg = white ? QColor(255, 255, 255, 248) : QColor(28, 30, 36, 245);
    const QColor border = white ? QColor(0, 0, 0, 30) : QColor(255, 255, 255, 30);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setBrush(bg);
    p.setPen(QPen(border, 1.0));
    p.drawRoundedRect(r, 14.0, 14.0);
}

bool ControllersPopoverWidget::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint gpos = me->globalPosition().toPoint();
        const QPoint local_pop = mapFromGlobal(gpos);
        if (!rect().contains(local_pop)) {
            if (main_window && main_window->GetInGameStatus()) {
                auto* status_widget = main_window->GetInGameStatus();
                if (status_widget->isVisible() &&
                    status_widget->rect().contains(status_widget->mapFromGlobal(gpos))) {
                    return false;
                }
            }
            if (main_window && main_window->GetHomeScreen()) {
                auto* hs = main_window->GetHomeScreen();
                if (hs->isVisible() && hs->GetBluetoothButton()) {
                    auto* bt_btn = hs->GetBluetoothButton();
                    if (bt_btn->rect().contains(bt_btn->mapFromGlobal(gpos))) {
                        return false;
                    }
                }
            }
            if (main_window) {
                main_window->HideControllersPopover();
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ControllersPopoverWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (qApp) {
        qApp->installEventFilter(this);
    }
}

void ControllersPopoverWidget::hideEvent(QHideEvent* event) {
    if (qApp) {
        qApp->removeEventFilter(this);
    }
    QWidget::hideEvent(event);
}

void GMainWindow::RepositionScreenDivider() {
    if (!screen_divider || !render_window || !emulation_running) {
        return;
    }
    const auto& layout = render_window->GetFramebufferLayout();
    const qreal dpr = render_window->devicePixelRatioF();
    // FramebufferLayout coordinates are physical pixels; convert to this widget's logical ones.
    const int seam_y = qRound(layout.bottom_screen.top / dpr);
    const QPoint origin = render_window->mapTo(ui->centralwidget, QPoint(0, 0));
    screen_divider->setFixedWidth(render_window->width());
    screen_divider->move(origin.x(), origin.y() + seam_y - screen_divider->height() / 2);
}

void GMainWindow::UpdateScreenSplitFromRatio(double ratio) {
    if (!render_window) {
        return;
    }
    ratio = std::clamp(ratio, 0.15, 0.85);
    const qreal dpr = render_window->devicePixelRatioF();
    const auto width = static_cast<u32>(render_window->width() * dpr);
    const auto height = static_cast<u32>(render_window->height() * dpr);
    const auto top_height = static_cast<u16>(qRound(height * ratio));
    Settings::values.layout_option = Settings::LayoutOption::CustomLayout;
    Settings::values.custom_top_x = 0;
    Settings::values.custom_top_y = 0;
    Settings::values.custom_top_width = static_cast<u16>(width);
    Settings::values.custom_top_height = top_height;
    Settings::values.custom_bottom_x = 0;
    Settings::values.custom_bottom_y = top_height;
    Settings::values.custom_bottom_width = static_cast<u16>(width);
    Settings::values.custom_bottom_height = static_cast<u16>(height - top_height);
    render_window->OnFramebufferSizeChanged();
}

void GMainWindow::OnLoadAmiibo() {
    if (!emu_thread || !emu_thread->IsRunning()) [[unlikely]] {
        return;
    }

    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc == nullptr) {
        return;
    }

    std::scoped_lock lock{system.Kernel().GetHLELock()};
    if (nfc->IsTagActive()) {
        QMessageBox::warning(this, tr("Error opening amiibo data file"),
                             tr("A tag is already in use."));
        return;
    }

    if (!nfc->IsSearchingForAmiibos()) {
        QMessageBox::warning(this, tr("Error opening amiibo data file"),
                             tr("Application is not looking for amiibos."));
        return;
    }

    const QString extensions{QStringLiteral("*.bin")};
    const QString file_filter = tr("Amiibo File") + QStringLiteral(" (%1);;").arg(extensions) +
                                tr("All Files") + QStringLiteral(" (*.*)");
    const QString filename = QFileDialog::getOpenFileName(this, tr("Load Amiibo"), {}, file_filter);

    if (filename.isEmpty()) {
        return;
    }

    LoadAmiibo(filename);
}

void GMainWindow::LoadAmiibo(const QString& filename) {
    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (!nfc) [[unlikely]] {
        return;
    }

    std::scoped_lock lock{system.Kernel().GetHLELock()};
    if (!nfc->LoadAmiibo(filename.toStdString())) {
        QMessageBox::warning(this, tr("Error opening amiibo data file"),
                             tr("Unable to open amiibo file \"%1\" for reading.").arg(filename));
        return;
    }

    ui->action_Remove_Amiibo->setEnabled(true);
}

void GMainWindow::OnRemoveAmiibo() {
    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (!nfc) [[unlikely]] {
        return;
    }

    std::scoped_lock lock{system.Kernel().GetHLELock()};
    nfc->RemoveAmiibo();
    ui->action_Remove_Amiibo->setEnabled(false);
}

void GMainWindow::OnOpenCitraFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::UserDir))));
}

void GMainWindow::OnOpenNANDFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir))));
}

void GMainWindow::OnOpenSDMCFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir))));
}

void GMainWindow::OnToggleFilterBar() {
    game_list->SetFilterVisible(ui->action_Show_Filter_Bar->isChecked());
    if (ui->action_Show_Filter_Bar->isChecked()) {
        game_list->SetFilterFocus();
    } else {
        game_list->ClearFilter();
    }
}

void GMainWindow::OnCreateGraphicsSurfaceViewer() {
    auto graphicsSurfaceViewerWidget =
        new GraphicsSurfaceWidget(system, Pica::g_debug_context, this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsSurfaceViewerWidget);
    // TODO: Maybe graphicsSurfaceViewerWidget->setFloating(true);
    graphicsSurfaceViewerWidget->show();
}

void GMainWindow::OnRecordMovie() {
    MovieRecordDialog dialog(this, system);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    movie_record_on_start = true;
    movie_record_path = dialog.GetPath();
    movie_record_author = dialog.GetAuthor();

    if (emulation_running) { // Restart game
        BootGame(QString(game_path));
    }
    ui->action_Close_Movie->setEnabled(true);
    ui->action_Save_Movie->setEnabled(true);
}

void GMainWindow::OnPlayMovie() {
    MoviePlayDialog dialog(this, game_list, system);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    movie_playback_on_start = true;
    movie_playback_path = dialog.GetMoviePath();
    BootGame(dialog.GetGamePath());

    ui->action_Close_Movie->setEnabled(true);
    ui->action_Save_Movie->setEnabled(false);
}

void GMainWindow::OnCloseMovie() {
    if (movie_record_on_start) {
        QMessageBox::information(this, tr("Record Movie"), tr("Movie recording cancelled."));
        movie_record_on_start = false;
        movie_record_path.clear();
        movie_record_author.clear();
    } else {
        const bool was_running = emu_thread && emu_thread->IsRunning();
        if (was_running) {
            OnPauseGame();
        }

        const bool was_recording = movie.GetPlayMode() == Core::Movie::PlayMode::Recording;
        movie.Shutdown();
        if (was_recording) {
            QMessageBox::information(this, tr("Movie Saved"),
                                     tr("The movie is successfully saved."));
        }

        if (was_running) {
            OnResumeGame(false);
        }
    }

    ui->action_Close_Movie->setEnabled(false);
    ui->action_Save_Movie->setEnabled(false);
}

void GMainWindow::OnSaveMovie() {
    const bool was_running = emu_thread && emu_thread->IsRunning();
    if (was_running) {
        OnPauseGame();
    }

    if (movie.GetPlayMode() == Core::Movie::PlayMode::Recording) {
        movie.SaveMovie();
        QMessageBox::information(this, tr("Movie Saved"), tr("The movie is successfully saved."));
    } else {
        LOG_ERROR(Frontend, "Tried to save movie while movie is not being recorded");
    }

    if (was_running) {
        OnResumeGame(false);
    }
}

void GMainWindow::OnCaptureScreenshot() {
    if (!emu_thread) [[unlikely]] {
        return;
    }

    const bool was_running = emu_thread->IsRunning();

    if (was_running || (QMessageBox::question(this, tr("Application will unpause"),
                                              tr("The application will be unpaused, and the next "
                                                 "frame will be captured. Is this okay?"),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No) == QMessageBox::Yes)) {
        if (was_running) {
            OnPauseGame();
        }
        std::string path = UISettings::values.screenshot_path.GetValue();
        if (!FileUtil::IsDirectory(path)) {
            if (!FileUtil::CreateFullPath(path)) {
                QMessageBox::information(
                    this, tr("Invalid Screenshot Directory"),
                    tr("Cannot create specified screenshot directory. Screenshot "
                       "path is set back to its default value."));
                path = FileUtil::GetUserPath(FileUtil::UserPath::UserDir);
                path.append("screenshots/");
                UISettings::values.screenshot_path = path;
            };
        }

        static QRegularExpression expr(QStringLiteral("[\\/:?\"<>|]"));
        const std::string filename = game_title.remove(expr).toStdString();
        const std::string timestamp = QDateTime::currentDateTime()
                                          .toString(QStringLiteral("dd.MM.yy_hh.mm.ss.z"))
                                          .toStdString();
        path.append(fmt::format("/{}_{}.png", filename, timestamp));
        auto* const screenshot_window =
            secondary_window->HasFocus() ? secondary_window : render_window;
        screenshot_window->CaptureScreenshot(
            UISettings::values.screenshot_resolution_factor.GetValue(),
            QString::fromStdString(path));
        OnResumeGame(false);
    }
}

#ifdef ENABLE_FFMPEG
void GMainWindow::OnDumpVideo() {
    if (ui->action_Dump_Video->isChecked()) {
        OnStartVideoDumping();
    } else {
        OnStopVideoDumping();
    }
}
#endif

void GMainWindow::OnCompressFile() {
    // NOTE: Encrypted files SHOULD NEVER be compressed, otherwise the resulting
    // compressed file will have very poor compression ratios, due to the high
    // entropy caused by encryption. This may cause confusion to the user as they
    // will see the files do not compress well and blame the emulator.
    //
    // This is enforced using the loaders as they already return an error on encryption.

    QStringList filepaths = QFileDialog::getOpenFileNames(
        this, tr("Load 3DS ROM Files"), UISettings::values.roms_path,
        tr("3DS ROM Files") + QStringLiteral(" (*.cia *.cci *.3dsx *.cxi *.3ds);;") +
            tr("All Files") + QStringLiteral(" (*.*)"));

    QString out_path;

    if (filepaths.isEmpty()) {
        return;
    }

    bool single_file = filepaths.size() == 1;
    if (single_file) {
        // If it's a single file, ask the user for the output file.
        auto compress_info = Loader::GetLoaderFileInfo(filepaths[0].toStdString());
        if (!compress_info.is_valid || !compress_info.is_compress_supported) {
            LOG_ERROR(Frontend,
                      "Error compressing file {}, the selected file is not a compatible 3DS ROM "
                      "format or is encrypted or bundled.",
                      filepaths[0].toStdString());
            emit CompressFinished(true, false);
            return;
        }
        if (compress_info.is_compressed) {
            LOG_ERROR(Frontend,
                      "Error compressing file {}, the selected file is already compressed.",
                      filepaths[0].toStdString());
            emit CompressFinished(true, false);
            return;
        }

        QFileInfo fileinfo(filepaths[0]);
        QString final_path = fileinfo.path() + QStringLiteral(DIR_SEP) +
                             fileinfo.completeBaseName() + QStringLiteral(".") +
                             QString::fromStdString(compress_info.recommended_compressed_extension);

        QString out_filter =
            tr("3DS Compressed ROM File") + QStringLiteral(" (*.%1)").arg(QString::fromStdString(
                                                compress_info.recommended_compressed_extension));
        out_path = QFileDialog::getSaveFileName(this, tr("Save 3DS Compressed ROM File"),
                                                final_path, out_filter);
        if (out_path.isEmpty()) {
            return;
        }
    } else {
        // Otherwise, ask the user the directory to output the files.
        out_path = QFileDialog::getExistingDirectory(
            this, tr("Select Output 3DS Compressed ROM Folder"), UISettings::values.roms_path,
            QFileDialog::ShowDirsOnly);
        if (out_path.isEmpty()) {
            return;
        }
    }

    (void)QtConcurrent::run([this, filepaths, out_path] {
        bool single_file = filepaths.size() == 1;
        QString out_filepath;
        bool total_success = true;

        for (const QString& filepath : filepaths) {

            std::string in_path = filepath.toStdString();

            // Identify file type
            auto compress_info = Loader::GetLoaderFileInfo(filepath.toStdString());
            if (!compress_info.is_valid || !compress_info.is_compress_supported) {
                LOG_ERROR(
                    Frontend,
                    "Error decompressing file {}, the selected file is not a compatible 3DS ROM "
                    "format or is encrypted or bundled.",
                    in_path);
                total_success = false;
                continue;
            }
            if (compress_info.is_compressed) {
                LOG_ERROR(Frontend,
                          "Error compressing file {}, the selected file is already compressed.",
                          in_path);
                total_success = false;
                continue;
            }

            if (single_file) {
                out_filepath = out_path;
            } else {
                QFileInfo fileinfo(filepath);
                out_filepath =
                    out_path + QStringLiteral(DIR_SEP) + fileinfo.completeBaseName() +
                    QStringLiteral(".") +
                    QString::fromStdString(compress_info.recommended_compressed_extension);
            }

            std::string out_path = out_filepath.toStdString();

            emit UpdateProgress(0, 0);

            const auto progress = [&](std::size_t written, std::size_t total) {
                emit UpdateProgress(written, total);
            };
            bool success =
                FileUtil::CompressZ3DSFile(in_path, out_path, compress_info.underlying_magic,
                                           compress_info.default_compress_frame_size, progress,
                                           compress_info.default_metadata);
            if (!success) {
                total_success = false;
                FileUtil::Delete(out_path);
            }
        }

        emit CompressFinished(true, total_success);
    });
}

void GMainWindow::OnDecompressFile() {

    QStringList filepaths = QFileDialog::getOpenFileNames(
        this, tr("Load 3DS Compressed ROM Files"), UISettings::values.roms_path,
        tr("3DS Compressed ROM Files") + QStringLiteral(" (*.zcia *zcci *z3dsx *zcxi)") +
            QStringLiteral(";;") + tr("All Files") + QStringLiteral(" (*.*)"));

    QString out_path;

    if (filepaths.isEmpty()) {
        return;
    }

    bool single_file = filepaths.size() == 1;
    if (single_file) {
        // If it's a single file, ask the user for the output file.
        auto compress_info = Loader::GetLoaderFileInfo(filepaths[0].toStdString());
        if (!compress_info.is_valid || !compress_info.is_compress_supported) {
            LOG_ERROR(Frontend,
                      "Error decompressing file {}, the selected file is not a compatible 3DS ROM "
                      "format.",
                      filepaths[0].toStdString());
            emit CompressFinished(false, false);
            return;
        }
        if (!compress_info.is_compressed) {
            LOG_ERROR(Frontend,
                      "Error decompressing file {}, the selected file is already decompressed",
                      filepaths[0].toStdString());
            emit CompressFinished(false, false);
            return;
        }

        QFileInfo fileinfo(filepaths[0]);
        QString final_path = fileinfo.path() + QStringLiteral(DIR_SEP) +
                             fileinfo.completeBaseName() + QStringLiteral(".") +
                             QString::fromStdString(compress_info.recommended_plain_extension);

        QString out_filter = tr("3DS ROM File") +
                             QStringLiteral(" (*.%1)").arg(
                                 QString::fromStdString(compress_info.recommended_plain_extension));
        out_path =
            QFileDialog::getSaveFileName(this, tr("Save 3DS ROM File"), final_path, out_filter);
        if (out_path.isEmpty()) {
            return;
        }
    } else {
        // Otherwise, ask the user the directory to output the files.
        out_path = QFileDialog::getExistingDirectory(this, tr("Select Output 3DS ROM Folder"),
                                                     UISettings::values.roms_path,
                                                     QFileDialog::ShowDirsOnly);
        if (out_path.isEmpty()) {
            return;
        }
    }

    (void)QtConcurrent::run([this, filepaths, out_path] {
        bool single_file = filepaths.size() == 1;
        QString out_filepath;
        bool total_success = true;

        for (const QString& filepath : filepaths) {

            std::string in_path = filepath.toStdString();

            // Identify file type
            auto compress_info = Loader::GetLoaderFileInfo(filepath.toStdString());
            if (!compress_info.is_valid || !compress_info.is_compress_supported) {
                LOG_ERROR(
                    Frontend,
                    "Error decompressing file {}, the selected file is not a compatible 3DS ROM "
                    "format.",
                    in_path);
                total_success = false;
                continue;
            }
            if (!compress_info.is_compressed) {
                LOG_ERROR(Frontend,
                          "Error decompressing file {}, the selected file is already decompressed.",
                          in_path);
                total_success = false;
                continue;
            }

            if (single_file) {
                out_filepath = out_path;
            } else {
                QFileInfo fileinfo(filepath);
                out_filepath = out_path + QStringLiteral(DIR_SEP) + fileinfo.completeBaseName() +
                               QStringLiteral(".") +
                               QString::fromStdString(compress_info.recommended_plain_extension);
            }

            std::string out_path = out_filepath.toStdString();

            emit UpdateProgress(0, 0);

            const auto progress = [&](std::size_t written, std::size_t total) {
                emit UpdateProgress(written, total);
            };

            // TODO(PabloMK7): What should we do with the metadata?
            bool success = FileUtil::DeCompressZ3DSFile(in_path, out_path, progress);
            if (!success) {
                total_success = false;
                FileUtil::Delete(out_path);
            }
        }

        emit CompressFinished(false, total_success);
    });
}

void GMainWindow::OnCreateBundle() {
    QString filepath = QFileDialog::getOpenFileName(
        this, tr("Load Main 3DS ROM File"), UISettings::values.roms_path,
        tr("3DS ROM Files") + QStringLiteral(" (*.cci *.zcci *.cia *.zcia *.cxi *.3ds);;") +
            tr("All Files") + QStringLiteral(" (*.*)"));

    if (filepath.isEmpty()) {
        return;
    }

    std::string in_path = filepath.toStdString();

    auto file_info = Loader::GetLoaderFileInfo(filepath.toStdString());
    if (!file_info.is_valid || !file_info.is_bundle_supported) {
        LOG_ERROR(Frontend,
                  "Error bundling file {}, the selected file is not a compatible 3DS ROM "
                  "format.",
                  in_path);
        emit BundleFinished(true, false);
        return;
    }
    if (file_info.is_bundled) {
        LOG_ERROR(Frontend, "Error bundling file {}, the selected file is already bundled.",
                  in_path);
        emit BundleFinished(true, false);
        return;
    }

    QFileInfo fileinfo(filepath);
    QString final_path = fileinfo.path() + QStringLiteral(DIR_SEP) + fileinfo.completeBaseName() +
                         QStringLiteral(".") +
                         QString::fromStdString(file_info.recommended_bundle_extension);

    QString out_filter = tr("3DS Bundle ROM File") +
                         QStringLiteral(" (*.%1)").arg(
                             QString::fromStdString(file_info.recommended_bundle_extension));

    QString out_path = QFileDialog::getSaveFileName(this, tr("Select Output 3DS Bundled ROM File"),
                                                    final_path, out_filter);
    if (out_path.isEmpty()) {
        return;
    }

    QStringList depends = QFileDialog::getOpenFileNames(
        this, tr("Load CIA File Dependencies"), UISettings::values.roms_path,
        tr("3DS ROM Files") + QStringLiteral(" (*.cia *.zcia);;") + tr("All Files") +
            QStringLiteral(" (*.*)"));

    if (depends.isEmpty()) {
        return;
    }

    std::vector<std::string> depends_vector;

    for (const QString& dep : depends) {

        std::string dep_path = dep.toStdString();

        // Identify file type
        auto dep_file_info = Loader::GetLoaderFileInfo(dep_path);
        if (!dep_file_info.is_valid || !dep_file_info.is_bundle_supported ||
            dep_file_info.is_bundled ||
            dep_file_info.underlying_magic != std::array<u8, 4>({'C', 'I', 'A', '\0'})) {
            LOG_ERROR(Frontend,
                      "Error processing dependency file {}, the selected file is not a compatible "
                      "CIA file.",
                      dep_path);
            emit BundleFinished(true, false);
            return;
        }

        depends_vector.push_back(dep_path);
    }

    emit UpdateProgress(0, 0);

    (void)QtConcurrent::run([this, out_path, in_path, depends_vector] {
        const auto progress = [&](std::size_t written, std::size_t total) {
            emit UpdateProgress(written, total);
        };

        [[maybe_unused]] auto result =
            Loader::CreateBundleFile(out_path.toStdString(), in_path, depends_vector, progress);
        emit BundleFinished(true, result == Loader::ResultStatus::Success);
    });
}

void GMainWindow::OnExtractBundle() {
    // TODO(PabloMK7): Implement before 2127 release, do not translate temporary strings.
    QMessageBox::warning(
        this, tr("Bundle File"),
        QStringLiteral("Bundle extraction is not implemented yet, please use a TAR file browser."));
}

#ifdef ENABLE_FFMPEG
void GMainWindow::OnStartVideoDumping() {
    DumpingDialog dialog(this, system);
    if (dialog.exec() != QDialog::DialogCode::Accepted) {
        ui->action_Dump_Video->setChecked(false);
        return;
    }
    const auto path = dialog.GetFilePath();
    if (emulation_running) {
        StartVideoDumping(path);
    } else {
        video_dumping_on_start = true;
        video_dumping_path = path;
    }
}

void GMainWindow::StartVideoDumping(const QString& path) {
    auto& renderer = system.GPU().Renderer();
    const auto layout{Layout::FrameLayoutFromResolutionScale(renderer.GetResolutionScaleFactor())};

    auto dumper = std::make_shared<VideoDumper::FFmpegBackend>(renderer);
    if (dumper->StartDumping(path.toStdString(), layout)) {
        system.RegisterVideoDumper(dumper);
    } else {
        QMessageBox::critical(
            this, QStringLiteral("Extendo"),
            tr("Could not start video dumping.<br>Please ensure that the video encoder is "
               "configured correctly.<br>Refer to the log for details."));
        ui->action_Dump_Video->setChecked(false);
    }
}

void GMainWindow::OnStopVideoDumping() {
    ui->action_Dump_Video->setChecked(false);

    if (video_dumping_on_start) {
        video_dumping_on_start = false;
        video_dumping_path.clear();
    } else {
        auto dumper = system.GetVideoDumper();
        if (!dumper || !dumper->IsDumping()) {
            return;
        }

        game_paused_for_dumping = emu_thread->IsRunning();
        OnPauseGame();

        auto future = QtConcurrent::run([dumper] { dumper->StopDumping(); });
        auto* future_watcher = new QFutureWatcher<void>(this);
        connect(future_watcher, &QFutureWatcher<void>::finished, this, [this] {
            if (game_shutdown_delayed) {
                game_shutdown_delayed = false;
                ShutdownGame();
            } else if (game_paused_for_dumping) {
                game_paused_for_dumping = false;
                OnResumeGame(false);
            }
        });
        future_watcher->setFuture(future);
    }
}
#endif

void GMainWindow::UpdateStatusBar() {
    if (!emu_thread) [[unlikely]] {
        status_bar_update_timer.stop();
        return;
    }

    // Update movie status
    const u64 current = movie.GetCurrentInputIndex();
    const u64 total = movie.GetTotalInputCount();
    const auto play_mode = movie.GetPlayMode();
    if (play_mode == Core::Movie::PlayMode::Recording) {
        message_label->setText(tr("Recording %1").arg(current));
        message_label_used_for_movie = true;
        ui->action_Save_Movie->setEnabled(true);
    } else if (play_mode == Core::Movie::PlayMode::Playing) {
        message_label->setText(tr("Playing %1 / %2").arg(current).arg(total));
        message_label_used_for_movie = true;
        ui->action_Save_Movie->setEnabled(false);
    } else if (play_mode == Core::Movie::PlayMode::MovieFinished) {
        message_label->setText(tr("Movie Finished"));
        message_label_used_for_movie = true;
        ui->action_Save_Movie->setEnabled(false);
    } else if (message_label_used_for_movie) { // Clear the label if movie was just closed
        message_label->setText(QString{});
        message_label_used_for_movie = false;
        ui->action_Save_Movie->setEnabled(false);
    }

    auto results = system.GetAndResetPerfStats();

    if (show_artic_label) {
        const bool do_mb = results.artic_transmitted >= (1000.0 * 1000.0);
        const double value = do_mb ? (results.artic_transmitted / (1000.0 * 1000.0))
                                   : (results.artic_transmitted / 1000.0);
        static const std::array<std::pair<Core::PerfStats::PerfArticEventBits, QString>, 5>
            perf_events = {
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_SHARED_EXT_DATA,
                               tr("(Accessing SharedExtData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_SYSTEM_SAVE_DATA,
                               tr("(Accessing SystemSaveData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_BOSS_EXT_DATA,
                               tr("(Accessing BossExtData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_EXT_DATA,
                               tr("(Accessing ExtData)")),
                std::make_pair(Core::PerfStats::PerfArticEventBits::ARTIC_SAVE_DATA,
                               tr("(Accessing SaveData)")),
            };

        const QString unit = do_mb ? QStringLiteral("MB/s") : QStringLiteral("KB/s");
        QString event{};
        for (auto p : perf_events) {
            if (results.artic_events.Get(p.first)) {
                event = QString::fromStdString(" ") + p.second;
                break;
            }
        }

        static const std::array label_color = {QStringLiteral(""), QStringLiteral("#eed202"),
                                               QStringLiteral("#ff3333")};

        int style_index;

        if (value > 200.0) {
            style_index = 2;
        } else if (value > 125.0) {
            style_index = 1;
        } else {
            style_index = 0;
        }

        QString style_sheet;
        if (!label_color[style_index].isEmpty()) {
            style_sheet = QStringLiteral("QLabel { color: %0; }").arg(label_color[style_index]);
        }

        artic_traffic_label->setText(
            tr("Artic Traffic: %1 %2%3").arg(value, 0, 'f', 0).arg(unit).arg(event));
        artic_traffic_label->setStyleSheet(style_sheet);
    }

    if (Settings::GetFrameLimit() == 0) {
        emu_speed_label->setText(tr("Speed: %1%").arg(results.emulation_speed * 100.0, 0, 'f', 0));
    } else {
        emu_speed_label->setText(tr("Speed: %1% / %2%")
                                     .arg(results.emulation_speed * 100.0, 0, 'f', 0)
                                     .arg(Settings::GetFrameLimit()));
    }
    game_fps_label->setText(tr("App: %1 FPS").arg(results.game_fps, 0, 'f', 0));
    if (UISettings::values.show_advanced_frametime_info) {
        emu_frametime_label->setText(
            tr("Frame: %1 ms (GPU: [CMD: %2 ms, SWP: %3 ms], IPC: %4 ms, SVC: %5 ms, Rem: %6 ms)")
                .arg(results.time_vblank_interval * 1000.0, 2, 'f', 2)
                .arg(results.time_gpu * 1000.0, 2, 'f', 2)
                .arg(results.time_swap * 1000.0, 2, 'f', 2)
                .arg(results.time_hle_ipc * 1000.0, 2, 'f', 2)
                .arg(results.time_hle_svc * 1000.0, 2, 'f', 2)
                .arg(results.time_remaining * 1000.0, 2, 'f', 2));
    } else {
        emu_frametime_label->setText(
            tr("Frame: %1 ms").arg(results.time_vblank_interval * 1000.0, 2, 'f', 2));
    }

    if (show_artic_label) {
        artic_traffic_label->setVisible(true);
    }
    emu_speed_label->setVisible(true);
    game_fps_label->setVisible(true);
    emu_frametime_label->setVisible(true);
}

void GMainWindow::UpdateBootHomeMenuState() {
    const auto current_region = Settings::values.region_value.GetValue();
    for (u32 region = 0; region < Core::NUM_SYSTEM_TITLE_REGIONS; region++) {
        const auto path = Core::GetHomeMenuNcchPath(region);
        ui->menu_Boot_Home_Menu->actions().at(region)->setEnabled(
            (current_region == Settings::REGION_VALUE_AUTO_SELECT ||
             current_region == static_cast<int>(region)) &&
            !path.empty() && FileUtil::Exists(path));
    }
}

void GMainWindow::HideMouseCursor() {
    if (!emu_thread || !UISettings::values.hide_mouse.GetValue()) {
        mouse_hide_timer.stop();
        ShowMouseCursor();
        return;
    }
    render_window->setCursor(QCursor(Qt::BlankCursor));
    secondary_window->setCursor(QCursor(Qt::BlankCursor));
    if (UISettings::values.single_window_mode.GetValue()) {
        setCursor(QCursor(Qt::BlankCursor));
    }
}

void GMainWindow::ShowMouseCursor() {
    unsetCursor();
    render_window->unsetCursor();
    secondary_window->unsetCursor();
    if (emu_thread && UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }
}

void GMainWindow::OnMute() {
    Settings::values.audio_muted = !Settings::values.audio_muted;
    UpdateVolumeUI();
}

void GMainWindow::OnDecreaseVolume() {
    Settings::values.audio_muted = false;
    const auto current_volume =
        static_cast<s32>(Settings::values.volume.GetValue() * volume_slider->maximum());
    int step = 5;
    if (current_volume <= 30) {
        step = 2;
    }
    if (current_volume <= 6) {
        step = 1;
    }
    const auto value =
        static_cast<float>(std::max(current_volume - step, 0)) / volume_slider->maximum();
    Settings::values.volume.SetValue(value);
    UpdateVolumeUI();
}

void GMainWindow::OnIncreaseVolume() {
    Settings::values.audio_muted = false;
    const auto current_volume =
        static_cast<s32>(Settings::values.volume.GetValue() * volume_slider->maximum());
    int step = 5;
    if (current_volume < 30) {
        step = 2;
    }
    if (current_volume < 6) {
        step = 1;
    }
    const auto value = static_cast<float>(current_volume + step) / volume_slider->maximum();
    Settings::values.volume.SetValue(value);
    UpdateVolumeUI();
}

void GMainWindow::UpdateVolumeUI() {
    const auto volume_value =
        static_cast<int>(Settings::values.volume.GetValue() * volume_slider->maximum());
    volume_slider->setValue(volume_value);
    if (Settings::values.audio_muted) {
        volume_button->setChecked(false);
        volume_button->setText(tr("VOLUME: MUTE"));
    } else {
        volume_button->setChecked(true);
        volume_button->setText(tr("VOLUME: %1%", "Volume percentage (e.g. 50%)").arg(volume_value));
    }
}

void GMainWindow::UpdateAPIIndicator(bool update) {
    static std::array graphics_apis = {QStringLiteral("SOFTWARE"), QStringLiteral("OPENGL"),
                                       QStringLiteral("VULKAN")};

    static std::array graphics_api_colors = {QStringLiteral("#3ae400"), QStringLiteral("#00ccdd"),
                                             QStringLiteral("#91242a")};

    u32 api_index = static_cast<u32>(Settings::values.graphics_api.GetValue());
    if (update) {
        api_index = (api_index + 1) % graphics_apis.size();
        // Skip past any disabled renderers.
#ifndef ENABLE_SOFTWARE_RENDERER
        if (api_index == static_cast<u32>(Settings::GraphicsAPI::Software)) {
            api_index = (api_index + 1) % graphics_apis.size();
        }
#endif
#ifndef ENABLE_OPENGL
        if (api_index == static_cast<u32>(Settings::GraphicsAPI::OpenGL)) {
            api_index = (api_index + 1) % graphics_apis.size();
        }
#endif
#ifndef ENABLE_VULKAN
        if (api_index == static_cast<u32>(Settings::GraphicsAPI::Vulkan)) {
            api_index = (api_index + 1) % graphics_apis.size();
        }
#else
        if (physical_devices.empty()) {
            if (api_index == static_cast<u32>(Settings::GraphicsAPI::Vulkan)) {
                api_index = (api_index + 1) % graphics_apis.size();
            }
        }
#endif
        Settings::values.graphics_api = static_cast<Settings::GraphicsAPI>(api_index);
    }

    const QString style_sheet = QStringLiteral("QPushButton { font-weight: bold; color: %0; }")
                                    .arg(graphics_api_colors[api_index]);

    graphics_api_button->setText(graphics_apis[api_index]);
    graphics_api_button->setStyleSheet(style_sheet);
}

void GMainWindow::UpdateStatusButtons() {
    UpdateAPIIndicator();
    UpdateVolumeUI();
}

void GMainWindow::OnMouseActivity() {
    ShowMouseCursor();
}

void GMainWindow::mouseMoveEvent([[maybe_unused]] QMouseEvent* event) {
    OnMouseActivity();
}

void GMainWindow::mousePressEvent([[maybe_unused]] QMouseEvent* event) {
    OnMouseActivity();
}

void GMainWindow::mouseReleaseEvent([[maybe_unused]] QMouseEvent* event) {
    OnMouseActivity();
}

void GMainWindow::showEvent([[maybe_unused]] QShowEvent* event) {
    game_list->LoadCompatibilityList();
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

void GMainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    RepositionSettingsOverlay();
    RepositionHomeButton();
    if (home_button && emulation_running) {
        home_button->raise();
    }
    RepositionInGameStatus();
    if (ingame_status && emulation_running) {
        ingame_status->raise();
    }
    RepositionControllersPopover();
    if (controllers_popover && controllers_popover->isVisible()) {
        controllers_popover->raise();
    }
    if (custom_screen_split_ratio >= 0.0) {
        // custom_top/bottom_* are absolute pixel positions, not scaled to the window — once
        // the user has dragged the divider at least once, the chosen ratio has to be
        // reapplied at the new window size on every resize, or the screens would stay pinned
        // to their old pixel sizes instead of filling the new window.
        UpdateScreenSplitFromRatio(custom_screen_split_ratio);
    }
    RepositionScreenDivider();
}

bool GMainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == settings_overlay && event->type() == QEvent::MouseButtonPress) {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        if (settings_card && !settings_card->geometry().contains(mouse_event->pos())) {
            // Clicked the dimmed backdrop, not the card itself: treat like Cancel.
            if (auto* dialog = settings_overlay->findChild<QDialog*>()) {
                dialog->reject();
            }
        }
        return true;
    }
    if (watched == screen_divider) {
        switch (event->type()) {
        case QEvent::Enter:
            screen_divider->setStyleSheet(
                QStringLiteral("background-color: rgba(10, 132, 255, 180);"));
            break;
        case QEvent::Leave:
            if (!dragging_screen_divider) {
                screen_divider->setStyleSheet(
                    QStringLiteral("background-color: rgba(10, 132, 255, 0);"));
            }
            break;
        case QEvent::MouseButtonPress:
            dragging_screen_divider = true;
            break;
        case QEvent::MouseButtonRelease:
            dragging_screen_divider = false;
            screen_divider->setStyleSheet(
                QStringLiteral("background-color: rgba(10, 132, 255, 0);"));
            break;
        case QEvent::MouseMove:
            if (dragging_screen_divider) {
                auto* mouse_event = static_cast<QMouseEvent*>(event);
                const QPoint global_pos = screen_divider->mapToGlobal(mouse_event->pos());
                const QPoint local_to_render = render_window->mapFromGlobal(global_pos);
                const double ratio =
                    static_cast<double>(local_to_render.y()) / std::max(1, render_window->height());
                custom_screen_split_ratio = std::clamp(ratio, 0.15, 0.85);
                UpdateScreenSplitFromRatio(custom_screen_split_ratio);
                RepositionScreenDivider();
            }
            break;
        default:
            break;
        }
        return false;
    }
    return QMainWindow::eventFilter(watched, event);
}

bool GMainWindow::ShowExceptionDialog(Core::System::ResultStatus result,
                                      const std::string& details) {
    QDialog dialog(this);
    dialog.setWindowTitle(result == Core::System::ResultStatus::ErrorCoreExceptionRaised
                              ? tr("An exception occurred")
                              : tr("An invalid memory access occurred"));

    dialog.setMinimumSize(600, 500);

    auto* layout = new QVBoxLayout(&dialog);

    auto* label = new QLabel(
        result == Core::System::ResultStatus::ErrorCoreExceptionRaised
            ? tr("An exception occurred while executing the emulated application.")
            : tr("An invalid memory access occurred while executing the emulated application."));

    layout->addWidget(label);

    auto* textEdit = new QPlainTextEdit();
    textEdit->setPlainText(QString::fromStdString(details));
    textEdit->setReadOnly(true);
    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(10);
    textEdit->setFont(monoFont);
    textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(textEdit);

    auto* buttonLayout = new QHBoxLayout();

    auto* ignoreButton = new QPushButton(tr("Ignore for this Session"));

    QObject::connect(ignoreButton, &QPushButton::clicked,
                     []() { Core::SetIgnoreExceptionsForSession(true); });

    buttonLayout->addWidget(ignoreButton);
    buttonLayout->addStretch();

    auto* continueButton = new QPushButton(tr("Continue"));
    QObject::connect(continueButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttonLayout->addWidget(continueButton);

    auto* stopButton = new QPushButton(tr("Stop Emulation"));
    QObject::connect(stopButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonLayout->addWidget(stopButton);

    layout->addLayout(buttonLayout);

    return dialog.exec() == QDialog::Accepted;
}

void GMainWindow::OnCoreError(Core::System::ResultStatus result, std::string details) {
    QString status_message;

    // Handle exception dialogs separately
    if (result == Core::System::ResultStatus::ErrorCoreExceptionRaised) {
        if (ShowExceptionDialog(result, details)) {
            if (emu_thread) {
                ShutdownGame();
                return;
            }
        }

        if (emu_thread) {
            emu_thread->SetRunning(true);
            message_label->setText(status_message);
            message_label_used_for_movie = false;
        }
        return;
    }

    QString title, message;
    QMessageBox::Icon error_severity_icon;
    bool can_continue = true;
    if (result == Core::System::ResultStatus::ErrorSystemFiles) {
        const QString common_message =
            tr("%1 is missing. Please <a "
               "href='https://github.com/azahar-emu/azahar/wiki/Dumping-System-Files'>dump your "
               "system archives</a>.<br/>Continuing emulation may result in crashes and bugs.");

        if (!details.empty()) {
            message = common_message.arg(QString::fromStdString(details));
        } else {
            message = common_message.arg(tr("A system archive"));
        }

        title = tr("System Archive Not Found");
        status_message = tr("System Archive Missing");
        error_severity_icon = QMessageBox::Icon::Critical;
    } else if (result == Core::System::ResultStatus::ErrorSavestate) {
        title = tr("Save/load Error");
        message = QString::fromStdString(details);
        error_severity_icon = QMessageBox::Icon::Warning;
    } else if (result == Core::System::ResultStatus::ErrorArticDisconnected) {
        title = tr("Artic Server");
        message =
            tr(fmt::format("A communication error has occurred. The game will quit.\n{}", details)
                   .c_str());
        error_severity_icon = QMessageBox::Icon::Critical;
        can_continue = false;
    } else if (result == Core::System::ResultStatus::ErrorSavestateBuildMismatch) {
        title = tr("Savestate version mismatch");
        message = tr("Could not load savestate because it was created on a different Azahar "
                     "version:<br/>"
                     "<b>Azahar %1</b>.<br/><br/>Please read our blog entry <a "
                     "href='https://azahar-emu.org/blog/understanding-save-states/'>understanding "
                     "savestates</a> for more information.<br/><br/>To recover your progress, "
                     "downgrade to <b>Azahar %1</b>, load this savestate and use the application's "
                     "built-in save functionality.")
                      .arg(QString::fromStdString(details));
        error_severity_icon = QMessageBox::Icon::Critical;
    } else {
        title = tr("Fatal Error");
        message = tr("A fatal error occurred. "
                     "Check the log for details."
                     "<br/>Continuing emulation may result in crashes and bugs.");
        status_message = tr("Fatal Error encountered");
        error_severity_icon = QMessageBox::Icon::Critical;
    }

    QMessageBox message_box;
    message_box.setWindowTitle(title);
    message_box.setText(message);
    message_box.setIcon(error_severity_icon);
    if (error_severity_icon == QMessageBox::Icon::Critical) {
        if (can_continue) {
            message_box.addButton(tr("Continue"), QMessageBox::RejectRole);
        }
        QPushButton* abort_button =
            message_box.addButton(tr("Quit Application"), QMessageBox::AcceptRole);
        if (result != Core::System::ResultStatus::ShutdownRequested)
            message_box.exec();

        if (!can_continue || result == Core::System::ResultStatus::ShutdownRequested ||
            message_box.clickedButton() == abort_button) {
            if (emu_thread) {
                ShutdownGame();
                return;
            }
        }
    } else {
        // This block should run when the error isn't too big of a deal
        // e.g. when a save state can't be saved or loaded
        message_box.addButton(tr("OK"), QMessageBox::RejectRole);
        message_box.exec();
    }

    // Only show the message if the game is still running.
    if (emu_thread) {
        emu_thread->SetRunning(true);
        message_label->setText(status_message);
        message_label_used_for_movie = false;
    }
}

void GMainWindow::OnMenuAboutCitra() {
    AboutDialog about{this};
    about.exec();
}

bool GMainWindow::ConfirmClose() {
    if (!emu_thread || !UISettings::values.confirm_before_closing) {
        return true;
    }

    const bool was_home_visible = home_button && home_button->isVisible();
    const bool was_status_visible = ingame_status && ingame_status->isVisible();
    HideControllersPopover();
    if (was_home_visible) {
        home_button->hide();
    }
    if (was_status_visible) {
        ingame_status->hide();
    }

    const bool was_running = emu_thread && emu_thread->IsRunning();
    if (was_running) {
        OnPauseGame();
    }

    const auto choice = ConsoleUI::ShowConfirmModal(
        this, QString(),
        tr("Do you want to save your progress before exiting?\nSaving creates an auto-resume state so you can continue right where you left off."),
        tr("Save & Quit"), tr("Quit"), tr("Cancel"));

    if (choice == ConsoleUI::ActionChoice::Primary) {
        SaveAutoResumeState();
        return true;
    } else if (choice == ConsoleUI::ActionChoice::Secondary) {
        const auto resume_path = Core::GetSaveStatePath(game_title_id, 0, Core::AutoResumeSlot);
        FileUtil::Delete(resume_path);
        return true;
    } else {
        if (was_home_visible) {
            home_button->show();
            home_button->raise();
        }
        if (was_status_visible) {
            ingame_status->show();
            ingame_status->raise();
        }
        if (was_running) {
            OnResumeGame(false);
        }
        return false;
    }
}

void GMainWindow::closeEvent(QCloseEvent* event) {
    if (!ConfirmClose()) {
        event->ignore();
        return;
    }

    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    hotkey_registry.SaveHotkeys();

    // Shutdown session if the emu thread is active...
    if (emu_thread) {
        ShutdownGame();
    }

    // Save settings in case they were changed from outside the configuration menu.
    config->Save();

    render_window->close();
    secondary_window->close();
    multiplayer_state->Close();
    InputCommon::Shutdown();
    QWidget::closeEvent(event);
}

static bool IsSingleFileDropEvent(const QMimeData* mime) {
    return mime->hasUrls() && mime->urls().length() == 1;
}

static const std::array<std::string, 13> AcceptedExtensions = {
    "cci",  "cxi",  "bin",   "3dsx", "app",  "elf", "axf",
    "zcci", "zcxi", "z3dsx", "3ds",  "bcci", "bcxi"};

static bool IsCorrectFileExtension(const QMimeData* mime) {
    const QString& filename = mime->urls().at(0).toLocalFile();
    return std::find(AcceptedExtensions.begin(), AcceptedExtensions.end(),
                     QFileInfo(filename).suffix().toStdString()) != AcceptedExtensions.end();
}

static bool IsAcceptableDropEvent(QDropEvent* event) {
    return IsSingleFileDropEvent(event->mimeData()) && IsCorrectFileExtension(event->mimeData());
}

void GMainWindow::AcceptDropEvent(QDropEvent* event) {
    if (IsAcceptableDropEvent(event)) {
        event->setDropAction(Qt::DropAction::LinkAction);
        event->accept();
    }
}

bool GMainWindow::DropAction(QDropEvent* event) {
    if (!IsAcceptableDropEvent(event)) {
        return false;
    }

    const QMimeData* mime_data = event->mimeData();
    const QString& filename = mime_data->urls().at(0).toLocalFile();

    if (emulation_running && QFileInfo(filename).suffix() == QStringLiteral("bin")) {
        // Amiibo
        LoadAmiibo(filename);
    } else {
        // Game
        if (ConfirmChangeGame()) {
            BootGame(filename);
        }
    }
    return true;
}

void GMainWindow::OnFileOpen(const QFileOpenEvent* event) {
    BootGame(event->file());
}

void GMainWindow::dropEvent(QDropEvent* event) {
    DropAction(event);
}

void GMainWindow::dragEnterEvent(QDragEnterEvent* event) {
    AcceptDropEvent(event);
}

void GMainWindow::dragMoveEvent(QDragMoveEvent* event) {
    AcceptDropEvent(event);
}

bool GMainWindow::ConfirmChangeGame() {
    if (!emu_thread) [[unlikely]] {
        return true;
    }

    auto answer = QMessageBox::question(
        this, QStringLiteral("Extendo"),
        tr("The application is still running. Would you like to stop emulation?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer != QMessageBox::No;
}

void GMainWindow::filterBarSetChecked(bool state) {
    ui->action_Show_Filter_Bar->setChecked(state);
    emit(OnToggleFilterBar());
}

inline bool isDarkMode() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Use colorScheme for Qt 6.5 and later
    const auto scheme = QGuiApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark;
#else
    // Fallback for Qt 6.4: Check the window palette
    QPalette palette = QGuiApplication::palette();
    return palette.color(QPalette::Window).lightness() < 128; // Rough check for dark mode
#endif
}

// Fully custom app palette (Fusion, no native colors). White theme = white surfaces + black
// text/accents; every other theme shares the dark set.
static QPalette BuildAppPalette(bool white) {
    QPalette p;
    if (white) {
        p.setColor(QPalette::Window, QColor(0xff, 0xff, 0xff));
        p.setColor(QPalette::WindowText, QColor(0, 0, 0));
        p.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
        p.setColor(QPalette::AlternateBase, QColor(0xf0, 0xf0, 0xf0));
        p.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xff));
        p.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
        p.setColor(QPalette::Text, QColor(0, 0, 0));
        p.setColor(QPalette::Button, QColor(0xed, 0xed, 0xed));
        p.setColor(QPalette::ButtonText, QColor(0, 0, 0));
        p.setColor(QPalette::BrightText, QColor(0xff, 0, 0));
        p.setColor(QPalette::Link, QColor(0, 0, 0));
        p.setColor(QPalette::Highlight, QColor(0, 0, 0));
        p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
        p.setColor(QPalette::PlaceholderText, QColor(0x77, 0x77, 0x77));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(0xa0, 0xa0, 0xa0));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0xa0, 0xa0, 0xa0));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0xa0, 0xa0, 0xa0));
    } else {
        // Taste rule 2: all of these are the one base hue at increasing tints, not separate greys.
        p.setColor(QPalette::Window, QColor(0x16, 0x18, 0x1d));
        p.setColor(QPalette::WindowText, Qt::white);
        p.setColor(QPalette::Base, QColor(0x1b, 0x1d, 0x22));
        p.setColor(QPalette::AlternateBase, QColor(0x22, 0x24, 0x29));
        p.setColor(QPalette::ToolTipBase, QColor(0x22, 0x24, 0x29));
        p.setColor(QPalette::ToolTipText, Qt::white);
        p.setColor(QPalette::Text, Qt::white);
        p.setColor(QPalette::Button, QColor(0x27, 0x29, 0x2f));
        p.setColor(QPalette::ButtonText, Qt::white);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, QColor(0x0a, 0x84, 0xff));
        p.setColor(QPalette::Highlight, QColor(0x0a, 0x84, 0xff));
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::PlaceholderText, QColor(0x9a, 0x9a, 0xa0));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6a, 0x6a, 0x70));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6a, 0x6a, 0x70));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6a, 0x6a, 0x70));
    }
    return p;
}

void GMainWindow::UpdateUITheme() {
    const QString icons_base_path = QStringLiteral(":/icons/");
    // Custom White theme: apply the light palette and skip all the dark theme QSS files — the
    // palette + the panel/home stylesheets (which read the same White flag) carry the look.
    if (UISettings::values.theme == QStringLiteral("white")) {
        QApplication::setPalette(BuildAppPalette(true));
        qApp->setStyleSheet({});
        setStyleSheet({});
        QStringList white_paths(default_theme_paths);
        white_paths.append(icons_base_path + QStringLiteral("default"));
        QIcon::setThemeName(QStringLiteral("default"));
        QIcon::setThemeSearchPaths(white_paths);
        if (home_screen) {
            home_screen->ApplyAppearanceSettings();
        }
        ApplyOverlayButtonTheme();
        return;
    }
    QApplication::setPalette(BuildAppPalette(false));
    QString default_theme;
    if (!isDarkMode()) {
        default_theme = QStringLiteral("default");
    } else {
        default_theme = QStringLiteral("default_with_light_icons");
    }

    const QString default_theme_path = icons_base_path + default_theme;

    const QString& current_theme = UISettings::values.theme;
    const bool is_default_theme = current_theme == QString::fromUtf8(UISettings::themes[0].second);
    QStringList theme_paths(default_theme_paths);

    if (is_default_theme || current_theme.isEmpty()) {
        const QString theme_uri(QStringLiteral(":default/style.qss"));
        QFile f(theme_uri);
        if (f.open(QFile::ReadOnly | QFile::Text)) {
            // Read once into a string: calling readAll() twice leaves the stream at EOF, so the
            // second call returns "" and silently clears the main window's stylesheet.
            const QString stylesheet = QTextStream(&f).readAll();
            qApp->setStyleSheet(stylesheet);
            setStyleSheet(stylesheet);
        } else {
            LOG_ERROR(Frontend,
                      "Unable to open default stylesheet, falling back to empty stylesheet");
            qApp->setStyleSheet({});
            setStyleSheet({});
        }
        theme_paths.append(default_theme_path);
        QIcon::setThemeName(default_theme);
    } else {
        const QString theme_uri(QLatin1Char{':'} + current_theme + QStringLiteral("/style.qss"));
        QFile f(theme_uri);
        if (f.open(QFile::ReadOnly | QFile::Text)) {
            // Single read — see the note in the default-theme branch above.
            const QString stylesheet = QTextStream(&f).readAll();
            qApp->setStyleSheet(stylesheet);
            setStyleSheet(stylesheet);
        } else {
            LOG_ERROR(Frontend, "Unable to set style, stylesheet file not found");
        }

        const QString current_theme_path = icons_base_path + current_theme;
        theme_paths.append({default_theme_path, current_theme_path});
        QIcon::setThemeName(current_theme);
    }

    QIcon::setThemeSearchPaths(theme_paths);
    if (home_screen) {
        home_screen->ApplyAppearanceSettings();
    }
    ApplyOverlayButtonTheme();
}

void GMainWindow::LoadTranslation() {
    bool loaded = false;

    const QString lang_en = QStringLiteral("en");
    const QString languages_dir = QStringLiteral(":/languages/");

    // If language is empty, but profile_language is configured to a non-English language,
    // initialize from profile_language so they stay in sync.
    if (UISettings::values.language.isEmpty() && UISettings::values.profile_language >= 0 &&
        UISettings::values.profile_language != 1) {
        UISettings::values.language = LocaleFromProfileLanguage(UISettings::values.profile_language);
    }

    // Workaround for incorrect Qt system language detection
    // TODO: Allow the "<System>" option to actually be selected rather than overriding the
    //       selected language option? Current behaviour is better than the issue it fixes,
    //       but not ideal.
    if (UISettings::values.language.isEmpty()) {
        QStringList languages;
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        languages = QLocale::system().uiLanguages(QLocale::TagSeparator::Underscore);
#else
        languages = QLocale::system().uiLanguages();
        for (auto& lang : languages)
            lang.replace(u'-', u'_');
#endif
        for (const auto& lang : languages) {
            // If the first language found is English, no need to install any translation
            if (lang == lang_en) {
                UISettings::values.language = lang_en;
                return;
            }
            loaded = citraTranslator.load(lang, languages_dir);
            if (loaded) {
                UISettings::values.language = lang;
                break;
            }
        }
    }

    // If the selected language is English, no need to install any translation
    if (UISettings::values.language == lang_en) {
        return;
    }

    const QString qtbase_prefix = QStringLiteral("qtbase_");
    if (UISettings::values.language.isEmpty() && !loaded) {
        // Use the system's default locale
        qtTranslator.load(qtbase_prefix + QLocale::system().name(), {}, {},
                          QStringLiteral(":/languages/"));
        loaded = citraTranslator.load(QLocale::system(), {}, {}, QStringLiteral(":/languages/"));
    } else {
        // Otherwise load from the specified file
        qtTranslator.load(qtbase_prefix + UISettings::values.language,
                          QStringLiteral(":/languages/"));
        loaded = citraTranslator.load(UISettings::values.language, QStringLiteral(":/languages/"));
    }

    if (loaded) {
        qApp->installTranslator(&qtTranslator);
        qApp->installTranslator(&citraTranslator);
    } else {
        UISettings::values.language = lang_en;
    }
}

void GMainWindow::OnLanguageChanged(const QString& locale) {
    qApp->removeTranslator(&qtTranslator);
    qApp->removeTranslator(&citraTranslator);

    UISettings::values.language = locale;
    LoadTranslation();
    QLocale::setDefault(locale.isEmpty() ? QLocale::system() : QLocale(locale));
    ui->retranslateUi(this);
    RetranslateStatusBar();
    UpdateWindowTitle();
    if (game_list) {
        game_list->RetranslateUI();
    }
    if (home_screen) {
        home_screen->RetranslateUI();
    }

    int p_lang = ProfileLanguageFromLocale(locale);
    if (p_lang != -1) {
        UISettings::values.profile_language = p_lang;
        if (auto cfg = Service::CFG::GetModule(system)) {
            cfg->SetSystemLanguage(static_cast<Service::CFG::SystemLanguage>(p_lang));
            cfg->UpdateConfigNANDSavegame();
        }
    }
}

void GMainWindow::OnConfigurePerGame() {
    u64 title_id{};
    system.GetAppLoader().ReadProgramId(title_id);
    OpenPerGameConfiguration(title_id, game_path);
}

void GMainWindow::OpenPerGameConfiguration(u64 title_id, const QString& file_name) {
    Settings::SetConfiguringGlobal(false);
    ConfigurePerGame dialog(this, title_id, file_name, gl_renderer, physical_devices, system);
    const auto result = dialog.exec();

    if (result != QDialog::Accepted) {
        Settings::RestoreGlobalState(system.IsPoweredOn());
        return;
    } else if (result == QDialog::Accepted) {
        dialog.ApplyConfiguration();
    }

    // Do not cause the global config to write local settings into the config file
    const bool is_powered_on = system.IsPoweredOn();
    Settings::RestoreGlobalState(system.IsPoweredOn());

    if (!is_powered_on) {
        config->Save();
    }

    UpdateStatusButtons();
}

void GMainWindow::OnMoviePlaybackCompleted() {
    OnPauseGame();
    QMessageBox::information(this, tr("Playback Completed"), tr("Movie playback completed."));
}

#ifdef ENABLE_QT_UPDATE_CHECKER
void GMainWindow::OnEmulatorUpdateAvailable() {
    QString version_string = update_future.result();
    if (version_string.isEmpty())
        return;

    QMessageBox update_prompt(this);
    update_prompt.setWindowTitle(tr("Update Available"));
    update_prompt.setIcon(QMessageBox::Information);
    update_prompt.addButton(QMessageBox::Yes);
    update_prompt.addButton(QMessageBox::Ignore);
    update_prompt.setText(tr("Update %1 for Extendo is available.\nWould you like to download it?")
                              .arg(version_string));
    update_prompt.exec();
    if (update_prompt.button(QMessageBox::Yes) == update_prompt.clickedButton()) {
        std::string update_page_url;
        if (ShouldCheckForPrereleaseUpdates()) {
            update_page_url = "https://github.com/azahar-emu/azahar/releases";
        } else {
            update_page_url = "https://azahar-emu.org/pages/download/";
        }
        QDesktopServices::openUrl(QUrl(QString::fromStdString(update_page_url)));
    }
}
#endif

void GMainWindow::OnSwitchDiskResources(VideoCore::LoadCallbackStage stage, std::size_t value,
                                        std::size_t total, const std::string& object) {
    if (stage == VideoCore::LoadCallbackStage::Prepare) {
        loading_shaders_label->setText(QString());
        loading_shaders_label->setVisible(true);
    } else if (stage == VideoCore::LoadCallbackStage::Complete) {
        loading_shaders_label->setVisible(false);
    } else {
        loading_shaders_label->setText(
            loading_screen->GetStageTranslation(stage, value, total, object));
    }
}

void GMainWindow::UpdateWindowTitle() {
    if (game_title.isEmpty()) {
        setWindowTitle(QStringLiteral("Extendo"));
    } else {
        setWindowTitle(QStringLiteral("Extendo | %1").arg(game_title));
        render_window->setWindowTitle(
            QStringLiteral("Extendo | %1 | %2").arg(game_title, tr("Primary Window")));
        secondary_window->setWindowTitle(
            QStringLiteral("Extendo | %1 | %2").arg(game_title, tr("Secondary Window")));
    }
}

void GMainWindow::UpdateUISettings() {
    if (!ui->action_Fullscreen->isChecked()) {
        UISettings::values.geometry = saveGeometry();
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
    }
    if (!secondary_window->isFullScreen()) {
        UISettings::values.secondarywindow_geometry = secondary_window->saveGeometry();
    }
    UISettings::values.state = saveState();
#if MICROPROFILE_ENABLED
    UISettings::values.microprofile_geometry = microProfileDialog->saveGeometry();
    UISettings::values.microprofile_visible = microProfileDialog->isVisible();
#endif
    UISettings::values.single_window_mode = ui->action_Single_Window_Mode->isChecked();
    UISettings::values.fullscreen = ui->action_Fullscreen->isChecked();
    UISettings::values.display_titlebar = ui->action_Display_Dock_Widget_Headers->isChecked();
    UISettings::values.show_filter_bar = ui->action_Show_Filter_Bar->isChecked();
    UISettings::values.show_status_bar = ui->action_Show_Status_Bar->isChecked();
    UISettings::values.first_start = false;
}

void GMainWindow::SyncMenuUISettings() {
    ui->action_Screen_Layout_Default->setChecked(Settings::values.layout_option.GetValue() ==
                                                 Settings::LayoutOption::Default);
    ui->action_Screen_Layout_Single_Screen->setChecked(Settings::values.layout_option.GetValue() ==
                                                       Settings::LayoutOption::SingleScreen);
    ui->action_Screen_Layout_Large_Screen->setChecked(Settings::values.layout_option.GetValue() ==
                                                      Settings::LayoutOption::LargeScreen);
    ui->action_Screen_Layout_Hybrid_Screen->setChecked(Settings::values.layout_option.GetValue() ==
                                                       Settings::LayoutOption::HybridScreen);
    ui->action_Screen_Layout_Side_by_Side->setChecked(Settings::values.layout_option.GetValue() ==
                                                      Settings::LayoutOption::SideScreen);
    ui->action_Screen_Layout_Separate_Windows->setChecked(
        Settings::values.layout_option.GetValue() == Settings::LayoutOption::SeparateWindows);
    ui->action_Screen_Layout_Custom_Layout->setChecked(Settings::values.layout_option.GetValue() ==
                                                       Settings::LayoutOption::CustomLayout);
    ui->action_Screen_Layout_Swap_Screens->setChecked(Settings::values.swap_screen.GetValue());
    ui->action_Screen_Layout_Upright_Screens->setChecked(
        Settings::values.upright_screen.GetValue());

    ui->menu_Small_Screen_Position->setEnabled(Settings::values.layout_option.GetValue() ==
                                               Settings::LayoutOption::LargeScreen);

    ui->action_Small_Screen_TopRight->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::TopRight);
    ui->action_Small_Screen_MiddleRight->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::MiddleRight);
    ui->action_Small_Screen_BottomRight->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::BottomRight);
    ui->action_Small_Screen_TopLeft->setChecked(Settings::values.small_screen_position.GetValue() ==
                                                Settings::SmallScreenPosition::TopLeft);
    ui->action_Small_Screen_MiddleLeft->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::MiddleLeft);
    ui->action_Small_Screen_BottomLeft->setChecked(
        Settings::values.small_screen_position.GetValue() ==
        Settings::SmallScreenPosition::BottomLeft);
    ui->action_Small_Screen_Above->setChecked(Settings::values.small_screen_position.GetValue() ==
                                              Settings::SmallScreenPosition::AboveLarge);
    ui->action_Small_Screen_Below->setChecked(Settings::values.small_screen_position.GetValue() ==
                                              Settings::SmallScreenPosition::BelowLarge);
}

void GMainWindow::RetranslateStatusBar() {
    if (emu_thread)
        UpdateStatusBar();

    emu_speed_label->setToolTip(tr("Current emulation speed. Values higher or lower than 100% "
                                   "indicate emulation is running faster or slower than a 3DS."));
    game_fps_label->setToolTip(tr("How many frames per second the app is currently displaying. "
                                  "This will vary from app to app and scene to scene."));
    emu_frametime_label->setToolTip(
        tr("Time taken to emulate a 3DS frame, not counting framelimiting or v-sync. For "
           "full-speed emulation this should be at most 16.67 ms."));

    multiplayer_state->retranslateUi();
}

#ifdef ENABLE_DISCORD_RPC
void GMainWindow::SetDiscordEnabled([[maybe_unused]] bool state) {
    if (state) {
        discord_rpc = std::make_unique<DiscordRPC::DiscordImpl>(system);
    } else {
        discord_rpc = std::make_unique<DiscordRPC::NullImpl>();
    }
}
#endif

#ifdef __unix__
void GMainWindow::SetGamemodeEnabled(bool state) {
    if (emulation_running) {
        Common::Linux::SetGamemodeState(state);
    }
}
#endif

#ifdef main
#undef main
#endif

static Qt::HighDpiScaleFactorRoundingPolicy GetHighDpiRoundingPolicy() {
#ifdef _WIN32
    // For Windows, we want to avoid scaling artifacts on fractional scaling ratios.
    // This is done by setting the optimal scaling policy for the primary screen.

    // Create a temporary QApplication.
    int temp_argc = 0;
    char** temp_argv = nullptr;
    QApplication temp{temp_argc, temp_argv};

    // Get the current screen geometry.
    const QScreen* primary_screen = QGuiApplication::primaryScreen();
    if (!primary_screen) {
        return Qt::HighDpiScaleFactorRoundingPolicy::PassThrough;
    }

    const QRect screen_rect = primary_screen->geometry();
    const qreal real_ratio = primary_screen->devicePixelRatio();
    const qreal real_width = std::trunc(screen_rect.width() * real_ratio);
    const qreal real_height = std::trunc(screen_rect.height() * real_ratio);

    // Recommended minimum width and height for proper window fit.
    // Any screen with a lower resolution than this will still have a scale of 1.
    constexpr qreal minimum_width = 1350.0;
    constexpr qreal minimum_height = 900.0;

    const qreal width_ratio = std::max(1.0, real_width / minimum_width);
    const qreal height_ratio = std::max(1.0, real_height / minimum_height);

    // Get the lower of the 2 ratios and truncate, this is the maximum integer scale.
    const qreal max_ratio = std::trunc(std::min(width_ratio, height_ratio));

    if (max_ratio > real_ratio) {
        return Qt::HighDpiScaleFactorRoundingPolicy::Round;
    } else {
        return Qt::HighDpiScaleFactorRoundingPolicy::Floor;
    }
#else
    // Other OSes should be better than Windows at fractional scaling.
    return Qt::HighDpiScaleFactorRoundingPolicy::PassThrough;
#endif
}

#ifdef _WIN32
void GMainWindow::UpdateScrollBarStyle() {
    constexpr auto customScrollbarStyleTemplate = (R"(
        QScrollBar:vertical {{
            background: palette(window);
            width: 16px;
            margin: 0px;
            border-left: 1px solid palette({0});
        }}
        QScrollBar::handle:vertical {{
            background: palette({1});
            border: none;
            border-radius: 4.5px;
            margin: 3px;
        }}
        QScrollBar::handle:vertical:hover {{
            background: palette({0});
        }}
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
            border: none;
            height: 0px;
        }}
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {{
            background: none;
        }}
    )");

    const auto darkCustomScrollbarStyle =
        QString::fromUtf8(std::format(customScrollbarStyleTemplate, "midlight", "light"));
    const auto lightCustomScrollbarStyle =
        QString::fromUtf8(std::format(customScrollbarStyleTemplate, "dark", "mid"));

    const bool isDarkMode = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);

    if (isDarkMode) {
        setStyleSheet(darkCustomScrollbarStyle);
    } else {
        setStyleSheet(lightCustomScrollbarStyle);
    }
}

void GMainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ApplicationPaletteChange) {
        UpdateScrollBarStyle();
    }
    QMainWindow::changeEvent(event);
}
#endif

int LaunchQtFrontend(int argc, char* argv[]) {
#ifdef __APPLE__
    // Ensure that the linker doesn't optimize qt_swizzle.mm out of existence.
    QtSwizzle::Dummy();
#endif

#if MICROPROFILE_ENABLED
    MicroProfileOnThreadCreate("Frontend");
    SCOPE_EXIT({ MicroProfileShutdown(); });
#endif

    // Init settings params
    QCoreApplication::setOrganizationName(QStringLiteral("Extendo Developers"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("extendo-emu.org"));
    QCoreApplication::setApplicationName(QStringLiteral("Extendo"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.extendo_emu.Extendo"));

    auto rounding_policy = GetHighDpiRoundingPolicy();
    QApplication::setHighDpiScaleFactorRoundingPolicy(rounding_policy);

#ifdef __APPLE__
    auto bundle_dir = FileUtil::GetBundleDirectory();
    if (bundle_dir) {
        FileUtil::SetCurrentDir(bundle_dir.value() + "..");
    }
#endif

#ifdef ENABLE_OPENGL
    QCoreApplication::setAttribute(Qt::AA_DontCheckOpenGLContextThreadAffinity);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

    QApplication app(argc, argv);

    // App-wide custom font: Extendo UI. Loaded as a loose file from the bundle's
    // Resources (copied there at bundle time), then set as the default QApplication font so
    // every widget inherits it.
    {
#ifdef Q_OS_MAC
        const QString font_path =
            QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/extendo_ui.ttf");
#else
        const QString font_path =
            QCoreApplication::applicationDirPath() + QStringLiteral("/extendo_ui.ttf");
#endif
        const int font_id = QFontDatabase::addApplicationFont(font_path);
        if (font_id != -1) {
            const QStringList families = QFontDatabase::applicationFontFamilies(font_id);
            if (!families.isEmpty()) {
                QFont app_font(families.front());
                app.setFont(app_font);
            }
        }
    }

    // Hard-stop the OS from enforcing its own appearance on menus, dialogs and in-game chrome:
    // Fusion is a Qt-drawn (non-native) style, so macOS can't impose native widgets, and a
    // fixed palette means system Light/Dark mode no longer flips the app's colors. Everything
    // is now driven by the app's own palette + theme QSS, consistently, regardless of macOS.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    // Startup fallback palette (dark); GMainWindow::UpdateUITheme re-applies the correct one for
    // the saved theme (incl. the custom White theme) once config is loaded.
    QApplication::setPalette(BuildAppPalette(false));

    // Required when using .qrc resources from within a static library.
    // See https://doc.qt.io/qt-5/resources.html#using-resources-in-a-library
    Q_INIT_RESOURCE(compatibility_list);
    Q_INIT_RESOURCE(theme_colorful);
    Q_INIT_RESOURCE(theme_colorful_dark);
    Q_INIT_RESOURCE(theme_colorful_midnight_blue);
    Q_INIT_RESOURCE(theme_default);
    Q_INIT_RESOURCE(theme_qdarkstyle);
    Q_INIT_RESOURCE(theme_qdarkstyle_midnight_blue);
#ifdef ENABLE_QT_TRANSLATION
    Q_INIT_RESOURCE(languages);
#endif

    // Qt changes the locale and causes issues in float conversion using std::to_string() when
    // generating shaders
    setlocale(LC_ALL, "C");

    auto& system{Core::System::GetInstance()};

    // Register Qt image interface
    system.RegisterImageInterface(std::make_shared<QtImageInterface>());

    GMainWindow main_window(system);

#ifdef _WIN32
    main_window.UpdateScrollBarStyle();
#endif

    // Register frontend applets
    Frontend::RegisterDefaultApplets(system);

    system.RegisterMiiSelector(std::make_shared<QtMiiSelector>(main_window));
    system.RegisterSoftwareKeyboard(std::make_shared<QtKeyboard>(main_window));

#ifdef __APPLE__
    // Register microphone permission check.
    system.RegisterMicPermissionCheck(&AppleAuthorization::CheckAuthorizationForMicrophone);
#endif

    main_window.show();

    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &main_window,
                     &GMainWindow::OnAppFocusStateChanged);

    // Process any pending events before executing the app (prevents freeze-on–boot on macOS)
    app.processEvents();

    int result = app.exec();
    return result;
}
