// The central help-topic registry (RES-HELP-008, ADR-000215): which help
// topic belongs to which part of the user interface. One table, keyed by a
// widget's class ("AcousticsPanel") or by a panel class and the object name
// of one of its tabs ("EmPanel/patternTab"), so no user-interface code
// carries help file paths; topics are the help's stable ids.
//
// A widget may also name its topic itself with the dynamic property
// "helpTopic" (dialogs do); that wins over the table.
#pragma once

#include <QHash>
#include <QString>

class QWidget;

namespace HelpTopics {

inline constexpr const char* kHome = "home";

// Every registered key and its topic (tests check each topic exists).
const QHash<QString, QString>& registry();
// The most specific topic for a widget: its "helpTopic" property, else a
// tab of its panel, else its panel or dialog class, walking up to the window;
// kHome when nothing matches.
QString topicFor(const QWidget* widget);
// The topic that explains a failure message (solver refusal, Python error, ...).
QString topicForMessage(const QString& message);

}  // namespace HelpTopics
