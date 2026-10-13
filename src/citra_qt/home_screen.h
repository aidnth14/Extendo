// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

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

/// Nintendo Switch-style horizontal tile shelf shown in place of the tree-view game list.
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
    void OpenScreenshots();
    void PowerOff();
    void BackgroundImageChanged();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void SetupUi();
    void OnTileActivated(QListWidgetItem* item);
    void OnTileContextMenu(const QPoint& pos);
    void OnChooseWallpaper();
    void UpdateClock();
    void UpdateTileListWidth();
    void LoadCustomCovers();
    void SaveCustomCovers();

    QStandardItemModel* source_model;
    QMap<QString, QString> custom_covers; // game full path -> custom cover image path
    QListWidget* tile_list = nullptr;
    QLabel* clock_label = nullptr;
    QLabel* title_label = nullptr;
    QWidget* pill_bar = nullptr;
    QWidget* empty_state_container = nullptr;
    QWidget* populated_container = nullptr;
    QToolButton* large_add_button = nullptr;
    QTimer* clock_timer = nullptr;
    QPixmap background_pixmap;
    QFileSystemWatcher* style_watcher = nullptr;

    static constexpr int AddDirectoryRole = Qt::UserRole + 100;
    static constexpr int FullPathRole = Qt::UserRole + 101;
};
