// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <QDialog>
#include "core/frontend/applets/mii_selector.h"

class QComboBox;
class QDialogButtonBox;
class QVBoxLayout;
class QtMiiSelector;

class QtMiiSelectorDialog final : public QDialog {
    Q_OBJECT

public:
    QtMiiSelectorDialog(QWidget* parent, QtMiiSelector* mii_selector_);

    int GetSelectedIndex() const {
        return selected_index;
    }

private:
    QDialogButtonBox* buttons{nullptr};
    QComboBox* combobox{nullptr};
    QVBoxLayout* layout{nullptr};
    QtMiiSelector* mii_selector{nullptr};
    u32 return_code = 0;
    int selected_index = 0;
    std::vector<Mii::MiiData> miis;

    friend class QtMiiSelector;
};

class QtMiiSelector final : public QObject, public Frontend::MiiSelector {
    Q_OBJECT

public:
    explicit QtMiiSelector(QWidget& parent);
    void Setup(const Frontend::MiiSelectorConfig& config) override;

private:
    Q_INVOKABLE void OpenDialog();

    QWidget& parent;

    friend class QtMiiSelectorDialog;
};
