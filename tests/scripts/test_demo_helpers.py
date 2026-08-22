import importlib.util
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


def load_script(name: str):
    path = REPOSITORY_ROOT / "scripts" / name
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def test_demo_alert_compatibility_is_exact():
    helper = load_script("ensure-demo-alert-rule.py")
    assert helper.compatible(dict(helper.RULE))
    incompatible = dict(helper.RULE)
    incompatible["threshold"] = 41.0
    assert not helper.compatible(incompatible)
    disabled = dict(helper.RULE)
    disabled["enabled"] = False
    assert not helper.compatible(disabled)
    assert helper.compatible(disabled, include_enabled=False)


def test_port_parser_reports_only_tcp_listeners():
    helper = load_script("check-host-ports.py")
    table = """sl local_address rem_address st queues
0: 00000000:1F40 00000000:0000 0A ignored
1: 0100007F:22B8 0100007F:ABCD 01 ignored
2: 00000000:0BB8 00000000:0000 0A ignored
"""
    assert helper.parse_listening_ports(table) == {8000, 3000}
