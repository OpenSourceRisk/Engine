"""Tracing helpers for ORE Python example scripts.

This module adds opt-in diagnostics for native shutdown crashes without
changing the scripts' normal output. Tracing is enabled via environment
variables so CI behavior remains unchanged unless explicitly requested.
"""

import atexit
import faulthandler
import gc
import os
import sys
from typing import Any, Dict, Optional


def _env_flag(name: str) -> bool:
    """Return True when the named environment variable is truthy."""
    value = os.environ.get(name, "")
    return value.lower() in {"1", "true", "yes", "on"}


def _short_repr(value: Any) -> str:
    """Return a bounded representation suitable for tracing output."""
    if isinstance(value, (str, bytes)):
        text = repr(value)
    elif isinstance(value, (int, float, bool)) or value is None:
        text = repr(value)
    elif isinstance(value, (list, tuple, set, dict)):
        text = f"{type(value).__name__}(len={len(value)})"
    else:
        text = type(value).__name__

    if len(text) > 80:
        return text[:77] + "..."
    return text


class _CallableProxy:
    """Wrap a callable ORE attribute and trace calls when enabled."""

    def __init__(self, name: str, target: Any, tracer: "ScriptTracer") -> None:
        self._name = name
        self._target = target
        self._tracer = tracer

    def __call__(self, *args: Any, **kwargs: Any) -> Any:
        call_id = self._tracer.next_call_id()
        args_text = ", ".join(_short_repr(arg) for arg in args)
        kwargs_text = ", ".join(
            f"{key}={_short_repr(value)}" for key, value in kwargs.items()
        )
        signature = ", ".join(
            part for part in (args_text, kwargs_text) if part
        )
        self._tracer.checkpoint(
            f"CALL #{call_id} {self._name}({signature})"
        )
        try:
            result = self._target(*args, **kwargs)
        except Exception as exc:
            self._tracer.checkpoint(
                f"RAISE #{call_id} {self._name}:"
                f" {type(exc).__name__}: {exc}"
            )
            raise
        self._tracer.checkpoint(
            f"RETURN #{call_id} {self._name} -> {_short_repr(result)}"
        )
        return result

    def __getattr__(self, name: str) -> Any:
        return getattr(self._target, name)


class _ModuleProxy:
    """Proxy the ORE module so constructor calls can be traced centrally."""

    def __init__(self, module: Any, tracer: "ScriptTracer") -> None:
        self._module = module
        self._tracer = tracer
        self._cache: Dict[str, Any] = {}

    def __getattr__(self, name: str) -> Any:
        if name in self._cache:
            return self._cache[name]

        value = getattr(self._module, name)
        if callable(value) and not name.startswith("__"):
            wrapped = _CallableProxy(name, value, self._tracer)
            self._cache[name] = wrapped
            return wrapped

        return value


class ScriptTracer:
    """Provide opt-in diagnostics for shutdown and constructor tracing."""

    def __init__(
        self,
        ore_module: Any,
        script_path: str,
        *,
        trace_calls_by_default: bool = False,
    ) -> None:
        self._ore_module = ore_module
        self._script_path = script_path
        self._script_name = os.path.basename(script_path)
        self._trace_enabled = _env_flag("ORE_TRACE_SCRIPT")
        self._trace_calls = _env_flag("ORE_TRACE_CALLS")
        if self._trace_enabled and trace_calls_by_default:
            self._trace_calls = True
        self._use_wlog = _env_flag("ORE_TRACE_WITH_WLOG")
        self._python_log: Optional[Any] = None
        self._ore_logger: Optional[Any] = None
        self._call_count = 0

        if self.enabled:
            self._open_python_log()
            self._enable_faulthandler()
            if self._use_wlog:
                self._configure_ore_logger()
            atexit.register(self._on_exit)
            self.checkpoint(
                "trace enabled"
                f" calls={self._trace_calls}"
                f" wlog={self._use_wlog}"
            )

    @property
    def enabled(self) -> bool:
        """Return True when any tracing mode is enabled."""
        return self._trace_enabled or self._trace_calls

    def module(self) -> Any:
        """Return either the raw ORE module or a call-tracing proxy."""
        if not self._trace_calls:
            return self._ore_module
        return _ModuleProxy(self._ore_module, self)

    def checkpoint(self, message: str) -> None:
        """Emit a high-level trace message for the current script."""
        if not self.enabled:
            return

        text = f"[{self._script_name}] {message}"
        self._write_local(text)
        if self._use_wlog:
            try:
                self._ore_module.WLOG(text)
            except Exception as exc:
                self._write_local(
                    f"[{self._script_name}] WLOG failed:"
                    f" {type(exc).__name__}: {exc}"
                )
                self._use_wlog = False

    def collect_garbage(self, label: str) -> None:
        """Force Python GC and trace the collection boundary."""
        if not self.enabled:
            return

        self._write_local(f"[{self._script_name}] gc.collect start: {label}")
        collected = gc.collect()
        self._write_local(
            f"[{self._script_name}] gc.collect done: {label}"
            f" collected={collected}"
        )

    def next_call_id(self) -> int:
        """Return the next stable sequence number for a traced call."""
        self._call_count += 1
        return self._call_count

    def _open_python_log(self) -> None:
        """Open a per-script trace log file for direct Python diagnostics."""
        default_name = (
            os.path.splitext(os.path.basename(self._script_path))[0] +
            ".trace.log"
        )
        file_name = os.environ.get("ORE_TRACE_FILE", default_name)
        self._python_log = open(file_name, "a", encoding="utf-8", buffering=1)

    def _enable_faulthandler(self) -> None:
        """Enable faulthandler so fatal signals still emit Python traces."""
        target = self._python_log if self._python_log is not None else sys.stderr
        try:
            faulthandler.enable(file=target, all_threads=True)
        except Exception as exc:
            self._write_local(
                f"[{self._script_name}] faulthandler enable failed:"
                f" {type(exc).__name__}: {exc}"
            )

    def _configure_ore_logger(self) -> None:
        """Register an ORE file logger when WLOG tracing is requested."""
        try:
            file_name = os.environ.get(
                "ORE_TRACE_ORE_LOG_FILE",
                os.path.splitext(os.path.basename(self._script_path))[0] +
                ".ore.log",
            )
            self._ore_logger = self._ore_module.FileLogger(file_name)
            self._ore_module.Log.instance().registerLogger(self._ore_logger)
            self._ore_module.Log.instance().setMask(255)
            self._ore_module.Log.instance().switchOn()
        except Exception as exc:
            self._write_local(
                f"[{self._script_name}] ORE logger setup failed:"
                f" {type(exc).__name__}: {exc}"
            )
            self._use_wlog = False

    def _write_local(self, text: str) -> None:
        """Write a trace line to stderr and the optional trace file."""
        print(text, file=sys.stderr, flush=True)
        if self._python_log is not None:
            self._python_log.write(text + "\n")
            self._python_log.flush()

    def _on_exit(self) -> None:
        """Trace the interpreter shutdown phase in a stable order."""
        self._write_local(f"[{self._script_name}] atexit start")
        self.collect_garbage("atexit")

        try:
            self._write_local(f"[{self._script_name}] Log.switchOff start")
            self._ore_module.Log.instance().switchOff()
            self._write_local(f"[{self._script_name}] Log.switchOff done")
        except Exception as exc:
            self._write_local(
                f"[{self._script_name}] Log.switchOff failed:"
                f" {type(exc).__name__}: {exc}"
            )

        try:
            self._write_local(
                f"[{self._script_name}] Log.removeAllLoggers start"
            )
            self._ore_module.Log.instance().removeAllLoggers()
            self._write_local(
                f"[{self._script_name}] Log.removeAllLoggers done"
            )
        except Exception as exc:
            self._write_local(
                f"[{self._script_name}] Log.removeAllLoggers failed:"
                f" {type(exc).__name__}: {exc}"
            )

        self._write_local(f"[{self._script_name}] atexit end")