// Copyright 2026 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <optional>
#include <utility>
#include <QDialog>
#include <QString>
#include <QStringList>

class QWidget;

namespace ConsoleUI {

enum class ActionChoice {
    Primary,
    Secondary,
    Cancel,
};

enum class AlertType {
    Info,
    Warning,
    Error,
};

/**
 * Displays a Switch-styled console modal list of choices conforming to consoleui_taste.md.
 * Features:
 * - Theme-aware styling (light/white card in White theme, dark card in Dark theme).
 * - Rendered as an in-app overlay directly inside parent window (no separate OS dialog).
 * - Selection is scale + luminance inversion (selected row turns solid with inverted text).
 * - Persistent docked [A] OK / [B] Back legend.
 *
 * @param parent Parent widget.
 * @param title Header title.
 * @param subtitle Subtitle or instructions.
 * @param items List of item strings.
 * @param current_index Initial selected index.
 * @return Selected index, or -1 if canceled.
 */
int ShowChoiceModal(QWidget* parent, const QString& title, const QString& subtitle,
                    const QStringList& items, int current_index = 0);

/**
 * Displays a Switch-styled console modal text input dialog conforming to consoleui_taste.md.
 * Features:
 * - Theme-aware in-app overlay.
 * - Frameless card with large rounded input box.
 * - Real-time character counter and hint.
 * - Docked [A] OK / [B] Back legend.
 *
 * @param parent Parent widget.
 * @param title Header title.
 * @param subtitle Instructions / prompt.
 * @param initial_text Initial input string.
 * @param max_length Maximum allowable characters.
 * @return New text if confirmed, std::nullopt if canceled.
 */
std::optional<QString> ShowTextInputModal(QWidget* parent, const QString& title,
                                          const QString& subtitle, const QString& initial_text,
                                          int max_length = 32);

/**
 * Displays a Switch-styled console modal birthday / date picker conforming to consoleui_taste.md.
 * Features:
 * - Theme-aware in-app overlay.
 * - Month and Day stepper cards with large touch-friendly buttons.
 * - Docked [A] OK / [B] Back legend.
 *
 * @param parent Parent widget.
 * @param title Header title.
 * @param subtitle Instructions.
 * @param initial_month Initial month (1..12).
 * @param initial_day Initial day (1..31).
 * @return Pair of (month, day) if confirmed, std::nullopt if canceled.
 */
std::optional<std::pair<int, int>> ShowDateModal(QWidget* parent, const QString& title,
                                                 const QString& subtitle, int initial_month = 1,
                                                 int initial_day = 1);

/**
 * Displays a Switch-styled console confirmation modal with up to 3 actions conforming to consoleui_taste.md.
 * Features:
 * - Theme-aware in-app overlay.
 * - If secondary_text is empty, shows Primary [A] and Cancel [B] side-by-side.
 * - If secondary_text is provided, renders large console action buttons (Primary, Secondary, Cancel).
 * - Docked [A] / [B] legend icons.
 *
 * @return ActionChoice::Primary, ActionChoice::Secondary, or ActionChoice::Cancel.
 */
ActionChoice ShowConfirmModal(QWidget* parent, const QString& title, const QString& message,
                             const QString& primary_text, const QString& secondary_text = {},
                             const QString& cancel_text = {});

/**
 * Displays a Switch-styled console alert/information modal.
 */
void ShowAlertModal(QWidget* parent, const QString& title, const QString& message,
                    AlertType type = AlertType::Info);

/**
 * Helper for Yes/No confirmation prompts.
 */
inline bool ShowQuestionModal(QWidget* parent, const QString& title, const QString& message,
                              const QString& yes_text = {}, const QString& no_text = {}) {
    return ShowConfirmModal(parent, title, message,
                            yes_text.isEmpty() ? QStringLiteral("Yes") : yes_text, {},
                            no_text.isEmpty() ? QStringLiteral("No") : no_text) ==
           ActionChoice::Primary;
}

} // namespace ConsoleUI
