"""A deliberately failing plugin (test data)."""


def register(api):
    raise RuntimeError("this plugin fails on purpose")
