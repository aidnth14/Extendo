// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <unordered_map>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QString>
#include <QVBoxLayout>
#include "citra_qt/applets/swkbd.h"
#include "citra_qt/console_modal.h"

QtKeyboardValidator::QtKeyboardValidator(QtKeyboard* keyboard_) : keyboard(keyboard_) {}

QtKeyboardValidator::State QtKeyboardValidator::validate(QString& input, int& pos) const {
    if (keyboard->ValidateFilters(input.toStdString()) == Frontend::ValidationError::None) {
        if (input.size() > keyboard->config.max_text_length)
            return State::Invalid;
        return State::Acceptable;
    } else {
        return State::Invalid;
    }
}

QtKeyboardDialog::QtKeyboardDialog(QWidget* parent, QtKeyboard* keyboard_)
    : QDialog(parent), keyboard(keyboard_) {
    using namespace Frontend;
    const auto config = keyboard->config;
    layout = new QVBoxLayout;
    label = new QLabel(QString::fromStdString(config.hint_text));
    line_edit = new QLineEdit;
    line_edit->setValidator(new QtKeyboardValidator(keyboard));
    buttons = new QDialogButtonBox;
    // Initialize buttons
    switch (config.button_config) {
    case ButtonConfig::Triple:
        buttons->addButton(config.button_text[1].empty()
                               ? tr(SWKBD_BUTTON_FORGOT)
                               : QString::fromStdString(config.button_text[1]),
                           QDialogButtonBox::ButtonRole::HelpRole);
        [[fallthrough]];
    case ButtonConfig::Dual:
        buttons->addButton(config.button_text[0].empty()
                               ? tr(SWKBD_BUTTON_CANCEL)
                               : QString::fromStdString(config.button_text[0]),
                           QDialogButtonBox::ButtonRole::RejectRole);
        [[fallthrough]];
    case ButtonConfig::Single:
        buttons->addButton(config.button_text[2].empty()
                               ? tr(SWKBD_BUTTON_OKAY)
                               : QString::fromStdString(config.button_text[2]),
                           QDialogButtonBox::ButtonRole::AcceptRole);
        break;
    case ButtonConfig::None:
        break;
    }
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { Submit(); });
    connect(buttons, &QDialogButtonBox::rejected, this, [this] {
        button = QtKeyboard::cancel_id;
        accept();
    });
    connect(buttons, &QDialogButtonBox::helpRequested, this, [this] {
        button = QtKeyboard::forgot_id;
        accept();
    });
    layout->addWidget(label);
    layout->addWidget(line_edit);
    layout->addWidget(buttons);
    setLayout(layout);
}

void QtKeyboardDialog::Submit() {
    auto error = keyboard->ValidateInput(line_edit->text().toStdString());
    if (error != Frontend::ValidationError::None) {
        HandleValidationError(error);
    } else {
        button = keyboard->ok_id;
        text = line_edit->text();
        accept();
    }
}

QString ValidationErrorMessage(Frontend::ValidationError error, u16 max_text_length) {
    using namespace Frontend;
    switch (error) {
    case ValidationError::FixedLengthRequired:
        return QObject::tr("Text length is not correct (should be %1 characters)")
            .arg(max_text_length);
    case ValidationError::MaxLengthExceeded:
        return QObject::tr("Text is too long (should be no more than %1 characters)")
            .arg(max_text_length);
    case ValidationError::BlankInputNotAllowed:
        return QObject::tr("Blank input is not allowed");
    case ValidationError::EmptyInputNotAllowed:
        return QObject::tr("Empty input is not allowed");
    default:
        return QObject::tr("That entry is not valid");
    }
}

void QtKeyboardDialog::HandleValidationError(Frontend::ValidationError error) {
    QMessageBox::critical(this, tr("Validation error"),
                          ValidationErrorMessage(error, keyboard->config.max_text_length));
}

QtKeyboard::QtKeyboard(QWidget& parent_) : parent(parent_) {}

void QtKeyboard::Execute(const Frontend::KeyboardConfig& config) {
    SoftwareKeyboard::Execute(config);
    if (this->config.button_config != Frontend::ButtonConfig::None) {
        ok_id = static_cast<u8>(this->config.button_config);
    }
    QMetaObject::invokeMethod(this, "OpenInputDialog", Qt::BlockingQueuedConnection);
    Finalize(result_text, result_button);
}

void QtKeyboard::ShowError(const std::string& error) {
    QString message = QString::fromStdString(error);
    QMetaObject::invokeMethod(this, "ShowErrorDialog", Qt::BlockingQueuedConnection,
                              Q_ARG(QString, message));
}

void QtKeyboard::OpenInputDialog() {
    // Single/Dual button prompts (the nickname and name entry games actually use) are routed
    // through the console-styled in-app modal so they match the rest of the UI instead of
    // appearing as a stock OS dialog over the game.
    //
    // Triple-button prompts carry a third "I Forgot" action that the console text modal has no
    // slot for, so those keep the plain dialog rather than silently losing a button.
    const bool console_styled = config.button_config == Frontend::ButtonConfig::Single ||
                                config.button_config == Frontend::ButtonConfig::Dual;

    if (console_styled) {
        const QString hint = QString::fromStdString(config.hint_text);
        const int max_length = static_cast<int>(config.max_text_length);

        // Validation is re-checked after each attempt; a rejected entry re-opens the modal with
        // what was typed still in place rather than discarding it.
        QString entry;
        while (true) {
            const auto input = ConsoleUI::ShowTextInputModal(
                &parent, tr("Enter Text"), hint, entry, max_length > 0 ? max_length : 32);
            if (!input) {
                result_text.clear();
                result_button = cancel_id;
                break;
            }

            entry = *input;
            const auto error = ValidateInput(entry.toStdString());
            if (error != Frontend::ValidationError::None) {
                ConsoleUI::ShowAlertModal(&parent, tr("Invalid Entry"),
                                          ValidationErrorMessage(error, config.max_text_length),
                                          ConsoleUI::AlertType::Warning);
                continue;
            }

            result_text = entry.toStdString();
            result_button = ok_id;
            break;
        }
    } else {
        QtKeyboardDialog dialog(&parent, this);
        dialog.setWindowFlags(dialog.windowFlags() &
                              ~(Qt::WindowCloseButtonHint | Qt::WindowContextHelpButtonHint));
        dialog.setWindowModality(Qt::WindowModal);
        dialog.exec();

        result_text = dialog.text.toStdString();
        result_button = dialog.button;
    }

    LOG_INFO(Frontend, "SWKBD input dialog finished, text={}, button={}", result_text,
             result_button);
}

void QtKeyboard::ShowErrorDialog(QString message) {
    QMessageBox::critical(&parent, tr("Software Keyboard"), message);
}
