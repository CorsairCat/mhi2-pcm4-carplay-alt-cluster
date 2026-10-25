#!/usr/bin/env python3
"""One-shot local timer for an offline Codex run; no reboot persistence."""
import argparse
from datetime import datetime, timedelta
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parent.parent
PROMPT = """Continue the PCM4 MU1476 offline display port in this workspace.
Read PCM4_MU1476_DISPLAY_PORT.md, AUDI_PCM4_METHOD_COMPARISON.md and
PCM4_CARPLAY_CLUSTER_RESEARCH.md first. The owner accepts upgrading to MU1476;
MU0809 is not a blocker. Wireless is out of scope. Prioritize resolving NVIDIA
cluster output mapping and visibility, then Porsche Java HMI takeover and stock
restoration. Continue useful implementation and static analysis, recording
reproducible evidence and limitations in PCM4_MU1476_DISPLAY_PORT.md. Run relevant
checks. Never execute vendor firmware binaries, flash or connect to a car, publish
private firmware, or commit/push. Keep derived artifacts under private/. Preserve
existing changes. Make meaningful progress through the offline tasks; finish with
a clear handoff when remaining work needs runtime evidence or unavailable tools.
Do not spawn other agents. Do not change sandbox or approval settings.
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--worker", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.worker:
        state_path = Path(args.worker)
        state = json.loads(state_path.read_text())
        while (remaining := state["start_timestamp"] - time.time()) > 0:
            time.sleep(min(remaining, 30))
        state["status"] = "starting"
        state_path.write_text(json.dumps(state, indent=2) + "\n")
        command = state["command"]
        try:
            with open(state["events"], "w") as events:
                result = subprocess.run(command, input=PROMPT, text=True,
                                        cwd=ROOT, stdout=events, stderr=subprocess.STDOUT)
            state.update(status="completed" if result.returncode == 0 else "failed",
                         exit_code=result.returncode)
        except OSError as error:
            state.update(status="failed", error=str(error))
        state_path.write_text(json.dumps(state, indent=2) + "\n")
        return
    codex = shutil.which("codex")
    if not codex:
        parser.error("codex is not on PATH")
    now = datetime.now(ZoneInfo("America/New_York"))
    target = now.replace(hour=3, minute=0, second=0, microsecond=0)
    if target <= now:
        target += timedelta(days=1)
    run_dir = ROOT / "private" / "scheduled-runs" / target.strftime("%Y%m%dT030000%z")
    command = [codex, "-a", "never", "-c", "model_auto_compact_token_limit=330000",
               "exec", "--sandbox", "workspace-write", "-C", str(ROOT),
               "--json", "--output-last-message", str(run_dir / "final.md"), "-"]
    state = dict(status="scheduled", scheduled_for=target.isoformat(),
                 start_timestamp=target.timestamp(), command=command,
                 events=str(run_dir / "events.jsonl"))
    if args.dry_run:
        print(json.dumps(state, indent=2))
        return
    # A fixed per-start-time directory prevents accidental duplicate schedules.
    run_dir.mkdir(parents=True, exist_ok=False)
    state_path = run_dir / "state.json"
    state_path.write_text(json.dumps(state, indent=2) + "\n")
    with open(run_dir / "timer.log", "a") as log:
        process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()),
                                    "--worker", str(state_path)], cwd=ROOT,
                                   stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                                   start_new_session=True)
    state["timer_pid"] = process.pid
    state_path.write_text(json.dumps(state, indent=2) + "\n")
    print(json.dumps(state, indent=2))


if __name__ == "__main__":
    main()
