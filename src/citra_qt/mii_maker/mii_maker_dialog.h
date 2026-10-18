// Copyright 2026 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <QColor>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QString>
#include <QWidget>
#include <vector>
#include "citra_qt/mii_maker/mii_manager.h"
#include "citra_qt/mii_maker/mii_render.h"
#include "core/hle/mii.h"

namespace MiiMaker {

// Theme-aware palette, computed once per open from UISettings::values.theme so every
// widget this dialog builds reads the live app theme instead of hardcoded colors.
struct MiiTheme {
    bool is_light{false};

    QColor backdrop;
    QString panel_bg;
    QString panel_border;
    QString text_primary;
    QString text_secondary;
    QString text_faint;

    QString rail_item_bg;
    QString rail_item_hover;
    QString rail_item_text;
    QString rail_item_sel_bg;
    QString rail_item_sel_text;

    QString card_bg;
    QString card_hover;
    QString card_text;
    QString card_sel_bg;
    QString card_sel_text;

    QString swatch_border;
    QString swatch_sel_border;

    QString input_bg;
    QString input_border;
    QString input_text;

    QString accent;
    QString accent_hover;
    QString danger;
    QString danger_hover;
};

MiiTheme ComputeMiiTheme();

class MiiMakerDialog : public QWidget {
    Q_OBJECT

public:
    explicit MiiMakerDialog(QWidget* parent = nullptr, const Mii::MiiData* initial_mii = nullptr);
    ~MiiMakerDialog() override;

    const Mii::MiiData& GetMii() const {
        return mii;
    }

    // Shows the dialog as an in-window overlay (no separate OS window) and blocks until
    // the user accepts or cancels, matching the ConsoleUI::BaseConsoleModal pattern used
    // elsewhere in the app. Returns QDialog::Accepted or QDialog::Rejected.
    int Exec();

signals:
    void MiiSaved(const Mii::MiiData& mii, const QString& avatar_path);

private slots:
    void UpdatePreview();
    void OnRandomizeClicked();
    void OnSaveClicked();
    void OnCancelClicked();
    void SwitchCategory(int index);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void SetupUI();
    void CreateCategoryRail(QWidget* parent_widget);
    QWidget* CreateFacePage();
    QWidget* CreateHairPage();
    QWidget* CreateEyebrowsPage();
    QWidget* CreateEyesPage();
    QWidget* CreateNosePage();
    QWidget* CreateMouthPage();
    QWidget* CreateFacialHairPage();
    QWidget* CreateGlassesPage();
    QWidget* CreateBodyPage();
    QWidget* CreateProfilePage();

    QPushButton* MakeOptionCard(const QString& title, bool is_selected);
    QPushButton* MakeImageOptionCard(const QString& category, const QString& asset_file,
                                     const QString& label, bool is_selected);
    void RestyleOptionCard(QPushButton* btn, const QString& title, bool is_selected);
    void RestyleColorSwatch(QPushButton* btn, const QString& hex, bool is_selected, int radius);
    void Accept();
    void Reject();
    static QWidget* ResolveParent(QWidget* parent);

    Mii::MiiData mii;
    int current_tilt{0};
    MiiTheme theme;

    QWidget* card{nullptr};
    QEventLoop* loop{nullptr};
    int result{0};

    // UI elements
    QLabel* preview_label{nullptr};
    QLabel* name_display{nullptr};
    QLabel* gender_badge{nullptr};
    QLineEdit* name_input{nullptr};
    QSlider* tilt_slider{nullptr};

    std::vector<QPushButton*> cat_buttons;
    QStackedWidget* pages_stack{nullptr};
    std::vector<QPushButton*> shirt_swatches;
};

} // namespace MiiMaker
