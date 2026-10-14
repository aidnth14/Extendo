// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <QColor>
#include <QMap>
#include <QPixmap>
#include <QWidget>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QStandardItemModel;
class QTimer;
class QToolButton;
class QFileSystemWatcher;
class QBoxLayout;
class QPropertyAnimation;
class QMovie;
class QShowEvent;
class QHideEvent;
class TileHoverDelegate;
class TileShelfWidget;

/// Horizontal console-style tile shelf shown in place of the tree-view game list.
/// Reads from the same QStandardItemModel that GameList populates, so it stays in sync with
/// whatever directories the user has configured without duplicating any ROM-scanning logic.
class HomeScreenWidget : public QWidget {
    Q_OBJECT

public:
    explicit HomeScreenWidget(QStandardItemModel* source_model, QWidget* parent = nullptr);
    ~HomeScreenWidget() override;

public slots:
    /// Rebuilds the tile shelf from the current contents of the source model.
    void Refresh();
    /// Re-reads background image / font settings from UISettings and repaints.
    void ApplyAppearanceSettings();
    /// Re-reads home_screen_style.qss from the user config dir and reapplies it live.
    void ReloadExternalStyle();

signals:
    void GameChosen(const QString& game_path);
    void AddDirectory();
    void OpenSettings();
    void OpenProfile();
    void PowerOff();
    void BackgroundImageChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void SetupUi();
    void OnTileActivated(QListWidgetItem* item);
    void OnTileContextMenu(const QPoint& pos);
    void OnChooseWallpaper();
    void OnBackgroundMovieFrame();
    void UpdateClock();
    /// Re-reads the host battery and repaints the status icon.
    void UpdateBattery();
    void UpdateTileListWidth();
    void UpdateTileIconSizeForWidth(int available_width);
    /// Re-docks the pill bar (bottom horizontal vs left vertical) per home_pill_position.
    void ApplyPillBarPlacement();
    /// Repaints and re-docks the persistent A/Y button legend.
    void UpdateLegendBar();
    /// Rescales the wallpaper source to the widget size and re-derives contrast colours.
    void RebuildScaledBackground();
    void LoadCustomCovers();
    void SaveCustomCovers();
    /// Recomputes home_text_color from the current background's brightness and reapplies it.
    void UpdateTextColorForBackground();
    void ApplyCombinedStylesheet();

    QStandardItemModel* source_model;
    int tile_icon_size = 96; // adjusted live to the window width; see UpdateTileIconSizeForWidth
    QMap<QString, QString> custom_covers; // game full path -> custom cover image path
    TileShelfWidget* tile_list = nullptr;
    TileHoverDelegate* tile_hover_delegate = nullptr;
    QLabel* clock_label = nullptr;
    QLabel* title_label = nullptr;
    QWidget* pill_bar = nullptr;
    QBoxLayout* pill_layout = nullptr;
    QWidget* empty_state_container = nullptr;
    QWidget* populated_container = nullptr;
    QToolButton* large_add_button = nullptr;
    QTimer* clock_timer = nullptr;
    QPixmap background_pixmap; // untouched source
    QPixmap background_scaled; // cached, scaled to the widget once per resize
    QMovie* background_movie = nullptr;
    QMap<QString, QMovie*> cover_movies;
    QFileSystemWatcher* style_watcher = nullptr;
    // Auto-picked (white on a dark background, black on a bright one) so home screen text
    // stays legible regardless of the user's chosen wallpaper.
    QColor home_text_color = Qt::white;
    QString external_style_qss;
    // Tiles activate on a single click, but itemClicked and itemActivated can both fire for a
    // single double-click gesture; this swallows the duplicate so a game is never booted twice.
    bool tile_activation_locked = false;
    /// Drives the eased horizontal scrolling of the tile shelf.
    QPropertyAnimation* scroll_animation = nullptr;
    QLabel* battery_label = nullptr;
    QLabel* battery_percent_label = nullptr;
    QWidget* legend_bar = nullptr;

    static constexpr int AddDirectoryRole = Qt::UserRole + 100;
    static constexpr int FullPathRole = Qt::UserRole + 101;
};
