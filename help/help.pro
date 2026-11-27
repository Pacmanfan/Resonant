# The in-app help and the shared data of an installation (REQ-CHAT-20261126-010).
# `make` runs tools/help/build_help.py: help/src -> HTML, navigation (index.json)
# and search index (search.json) in this build directory. The development build
# uses --draft (a screenshot not captured yet is a warning, reported in
# build-report.json); the documentation test and packaging build strictly.
# `make install` puts the help, demonstrations, examples, example plugins and
# the Python package under $$PREFIX/share/resonant.
TEMPLATE = aux

isEmpty(PREFIX): PREFIX = /usr/local
HELP_TOOL = $$PWD/../tools/help/build_help.py

helpbuild.target = help-html
helpbuild.commands = python3 $$HELP_TOOL --source $$PWD/src --screenshots $$PWD/screenshots/manifest.json --out $$OUT_PWD --draft --quiet
helpbuild.depends = FORCE
first.depends = helpbuild
QMAKE_EXTRA_TARGETS += helpbuild first

helpfiles.path = $$PREFIX/share/resonant/help
helpfiles.files = $$OUT_PWD/html $$OUT_PWD/index.json $$OUT_PWD/search.json $$OUT_PWD/build-report.json
helpfiles.CONFIG += no_check_exist
demofiles.path = $$PREFIX/share/resonant
demofiles.files = $$PWD/../demos $$PWD/../examples
pluginfiles.path = $$PREFIX/share/resonant/plugins
pluginfiles.files = $$PWD/../plugins/examples
pythonfiles.path = $$PREFIX/share/resonant/python
pythonfiles.files = $$PWD/../python/resonant $$PWD/../python/examples $$PWD/../python/templates
INSTALLS += helpfiles demofiles pluginfiles pythonfiles
