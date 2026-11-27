#include "HelpTopics.h"

#include <QMetaObject>
#include <QTabWidget>
#include <QWidget>

namespace HelpTopics {

const QHash<QString, QString>& registry()
{
    static const QHash<QString, QString> table = {
        // Windows, docks and panels (by class).
        {QStringLiteral("MainWindow"), QStringLiteral("ui.main-window")},
        {QStringLiteral("ViewportWidget"), QStringLiteral("ui.viewport")},
        {QStringLiteral("SceneTreePanel"), QStringLiteral("ui.scene-hierarchy")},
        {QStringLiteral("PropertiesPanel"), QStringLiteral("ui.property-editors")},
        {QStringLiteral("AnalysisPanel"), QStringLiteral("ui.analysis-panels")},
        {QStringLiteral("AcousticsPanel"), QStringLiteral("acoustics.focusing")},
        {QStringLiteral("FieldAnalysisPanel"), QStringLiteral("acoustics.field-analysis")},
        {QStringLiteral("FdtdPanel"), QStringLiteral("acoustics.fdtd")},
        {QStringLiteral("TimeSignalPanel"), QStringLiteral("acoustics.sources")},
        {QStringLiteral("MechanicsPanel"), QStringLiteral("mechanics.modal")},
        {QStringLiteral("EmPanel"), QStringLiteral("em.antennas")},
        {QStringLiteral("OptimizationPanel"), QStringLiteral("opt.studio")},
        {QStringLiteral("ExperimentsPanel"), QStringLiteral("exp.acquisition")},
        {QStringLiteral("PythonConsole"), QStringLiteral("py.console")},
        {QStringLiteral("ScriptEditor"), QStringLiteral("py.editor")},
        {QStringLiteral("DemoStudioPanel"), QStringLiteral("ui.demo-studio")},
        {QStringLiteral("PresenterOverlay"), QStringLiteral("ui.demo-studio")},
        {QStringLiteral("HelpWindow"), QStringLiteral("ui.help-window")},
        // Property editors (by class).
        {QStringLiteral("ArrayEditor"), QStringLiteral("acoustics.arrays")},
        {QStringLiteral("EmitterEditor"), QStringLiteral("acoustics.sources")},
        {QStringLiteral("TargetEditor"), QStringLiteral("concepts.targets")},
        {QStringLiteral("RegionEditor"), QStringLiteral("concepts.regions")},
        {QStringLiteral("ManipulatorEditor"), QStringLiteral("concepts.manipulators")},
        {QStringLiteral("MechanicalEditor"), QStringLiteral("mechanics.structures")},
        {QStringLiteral("EmEditor"), QStringLiteral("em.antennas")},
        // Dialogs (by class).
        {QStringLiteral("MaterialsDialog"), QStringLiteral("concepts.materials")},
        {QStringLiteral("OutputAuthorizationDialog"), QStringLiteral("exp.safety")},
        {QStringLiteral("PluginsDialog"), QStringLiteral("py.plugins")},
        {QStringLiteral("WorkflowsDialog"), QStringLiteral("py.workflows")},
        {QStringLiteral("AboutDialog"), QStringLiteral("gs.introduction")},
        // Tabs (panel class / tab object name).
        {QStringLiteral("AcousticsPanel/mediumTab"), QStringLiteral("acoustics.materials")},
        {QStringLiteral("AcousticsPanel/focusTab"), QStringLiteral("acoustics.focusing")},
        {QStringLiteral("AcousticsPanel/inverseTab"), QStringLiteral("acoustics.optimization")},
        {QStringLiteral("AcousticsPanel/simulationTab"), QStringLiteral("sim.resources")},
        {QStringLiteral("AcousticsPanel/displayTab"), QStringLiteral("ui.visualization")},
        {QStringLiteral("EmPanel/impedanceTab"), QStringLiteral("em.impedance")},
        {QStringLiteral("EmPanel/patternTab"), QStringLiteral("em.patterns")},
        {QStringLiteral("EmPanel/arrayTab"), QStringLiteral("em.arrays")},
        {QStringLiteral("EmPanel/fieldsTab"), QStringLiteral("em.near-far-field")},
        {QStringLiteral("EmPanel/linesTab"), QStringLiteral("em.transmission-lines")},
        {QStringLiteral("MechanicsPanel/modesTab"), QStringLiteral("mechanics.modal")},
        {QStringLiteral("MechanicsPanel/harmonicTab"), QStringLiteral("mechanics.forced-response")},
        {QStringLiteral("MechanicsPanel/transientTab"), QStringLiteral("mechanics.waves")},
        {QStringLiteral("MechanicsPanel/staticTab"), QStringLiteral("mechanics.boundary-conditions")},
        {QStringLiteral("MechanicsPanel/targetingTab"), QStringLiteral("mechanics.optimization")},
        {QStringLiteral("MechanicsPanel/moreTab"), QStringLiteral("mechanics.resonators")},
        {QStringLiteral("OptimizationPanel/problemTab"), QStringLiteral("opt.studio")},
        {QStringLiteral("OptimizationPanel/variablesTab"), QStringLiteral("opt.variables")},
        {QStringLiteral("OptimizationPanel/objectivesTab"), QStringLiteral("opt.objectives")},
        {QStringLiteral("OptimizationPanel/runTab"), QStringLiteral("opt.algorithms")},
        {QStringLiteral("OptimizationPanel/candidatesTab"), QStringLiteral("opt.studio")},
        {QStringLiteral("OptimizationPanel/tradeoffTab"), QStringLiteral("opt.objectives")},
        {QStringLiteral("OptimizationPanel/analysisTab"), QStringLiteral("opt.robust")},
        {QStringLiteral("OptimizationPanel/topologyTab"), QStringLiteral("opt.topology")},
        {QStringLiteral("ExperimentsPanel/devicesTab"), QStringLiteral("exp.discovery")},
        {QStringLiteral("ExperimentsPanel/experimentsTab"), QStringLiteral("exp.acquisition")},
        {QStringLiteral("ExperimentsPanel/calibrationTab"), QStringLiteral("exp.calibration")},
        {QStringLiteral("ExperimentsPanel/runTab"), QStringLiteral("exp.safety")},
        {QStringLiteral("ExperimentsPanel/measurementsTab"), QStringLiteral("exp.measurements")},
        {QStringLiteral("FieldAnalysisPanel/pointsTab"), QStringLiteral("acoustics.field-analysis")},
        {QStringLiteral("FieldAnalysisPanel/comparisonTab"), QStringLiteral("sim.comparison")},
        {QStringLiteral("AnalysisPanel/consoleTab"), QStringLiteral("ui.analysis-panels")},
        {QStringLiteral("AnalysisPanel/diagnosticsTab"), QStringLiteral("trouble.build-runtime")},
        {QStringLiteral("AnalysisPanel/simulationTab"), QStringLiteral("sim.results")},
    };
    return table;
}

namespace {
QString classOf(const QWidget* w)
{
    return QString::fromLatin1(w->metaObject()->className());
}

// The nearest ancestor (or the widget itself) whose class is registered.
const QWidget* panelOf(const QWidget* w)
{
    for (const QWidget* p = w; p; p = p->parentWidget())
        if (registry().contains(classOf(p)))
            return p;
    return nullptr;
}
}  // namespace

QString topicFor(const QWidget* widget)
{
    for (const QWidget* w = widget; w; w = w->parentWidget()) {
        const QVariant own = w->property("helpTopic");
        if (own.isValid() && !own.toString().isEmpty())
            return own.toString();
        // A tab of a panel: the panel's class / the tab's object name.
        if (const QWidget* panel = panelOf(w->parentWidget()); panel && !w->objectName().isEmpty()) {
            const auto it = registry().constFind(classOf(panel) + QLatin1Char('/') + w->objectName());
            if (it != registry().constEnd())
                return it.value();
        }
        // The current tab of a tab widget that is itself the widget (focus on the tab bar).
        if (const auto* tabs = qobject_cast<const QTabWidget*>(w); tabs && tabs->currentWidget())
            if (const QWidget* panel = panelOf(tabs)) {
                const auto it = registry().constFind(classOf(panel) + QLatin1Char('/') + tabs->currentWidget()->objectName());
                if (it != registry().constEnd() && w == widget)
                    return it.value();
            }
        const auto it = registry().constFind(classOf(w));
        if (it != registry().constEnd())
            return it.value();
    }
    return QString::fromLatin1(kHome);
}

QString topicForMessage(const QString& message)
{
    const QString m = message.toLower();
    if (m.contains(QLatin1String("python")) || m.contains(QLatin1String("traceback")) || m.contains(QLatin1String("script")))
        return QStringLiteral("trouble.python");
    if (m.contains(QLatin1String("plugin")))
        return QStringLiteral("trouble.plugins");
    if (m.contains(QLatin1String("memory")) || m.contains(QLatin1String("too large")) || m.contains(QLatin1String("cells")))
        return QStringLiteral("trouble.memory");
    if (m.contains(QLatin1String("mesh")))
        return QStringLiteral("trouble.mesh");
    if (m.contains(QLatin1String("calibrat")))
        return QStringLiteral("trouble.calibration");
    if (m.contains(QLatin1String("device")) || m.contains(QLatin1String("connect")))
        return QStringLiteral("trouble.devices");
    if (m.contains(QLatin1String("acquisition")) || m.contains(QLatin1String("sample")))
        return QStringLiteral("trouble.acquisition");
    if (m.contains(QLatin1String("authoriz")) || m.contains(QLatin1String("output")))
        return QStringLiteral("exp.safety");
    return QStringLiteral("trouble.solver-failures");
}

}  // namespace HelpTopics
