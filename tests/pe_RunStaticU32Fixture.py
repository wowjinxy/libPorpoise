#!/usr/bin/env python3

from __future__ import annotations

import subprocess
import sys


raise SystemExit(subprocess.run([sys.argv[1]], check=False).returncode)
