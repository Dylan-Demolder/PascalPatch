"""Compatibility: MeleeMod is now PascalPatch.

``import meleemod.native`` (and ``python -m meleemod.cli``) keep working: every
``meleemod.<name>`` is the same module object as ``pascalpatch.<name>``.
"""
import importlib, importlib.abc, importlib.util, sys

import pascalpatch
from pascalpatch import *  # noqa: F401,F403
__version__ = pascalpatch.__version__


class _Alias(importlib.abc.MetaPathFinder, importlib.abc.Loader):
    def find_spec(self, name, path=None, target=None):
        if name.startswith("meleemod.") and importlib.util.find_spec("pascalpatch" + name[8:]):
            return importlib.util.spec_from_loader(name, self)
        return None

    def create_module(self, spec):
        return importlib.import_module("pascalpatch" + spec.name[8:])

    def exec_module(self, module):
        pass


sys.meta_path.insert(0, _Alias())
