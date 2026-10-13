"""Compile one explicitly selected Arduino target; no upload or core installation."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
p = argparse.ArgumentParser()
p.add_argument('--fqbn', required=True)
p.add_argument('--example', choices=['CoreSmoke', 'PortableClock'], default='CoreSmoke')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
a.output.mkdir(parents=True, exist_ok=True)
report = {'fqbn': a.fqbn, 'example': a.example, 'status': 'NOT_RUN',
          'hardware_tested': False, 'note': 'Compile only; not certification of board support.'}
cli = shutil.which('arduino-cli')
code = 2
if cli:
    report['cli_version'] = subprocess.run([cli, 'version'], capture_output=True, text=True).stdout.strip()
    report['installed_cores'] = subprocess.run([cli, 'core', 'list', '--format', 'json'], capture_output=True, text=True).stdout
    cmd = [cli, 'compile', '--fqbn', a.fqbn, '--library', str(root),
           '--build-path', str((a.output/'build').resolve()),
           str(root/'examples'/a.example)]
    run = subprocess.run(cmd, capture_output=True, text=True)
    (a.output/'compile.log').write_text(run.stdout+run.stderr)
    report['status'] = 'PASS' if run.returncode == 0 else 'FAIL'
    report['command'] = cmd
    code = run.returncode
else:
    report['reason'] = 'arduino-cli unavailable; no board compilation performed'
(a.output/'result.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
sys.exit(code)
