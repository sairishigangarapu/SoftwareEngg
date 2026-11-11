"""Selenium UI test fixtures (STP section 5: UI automation with Selenium).

Each test session starts the real backend (`uatc serve`) on a free port with
its own temporary allowed root, so the tests drive the real web page against
the real C++ API, exactly as a user would.

Environment variables (all optional):
  UATC_CLI            path to the uatc binary        (default build/uatc_cli)
  UATC_CHROME_BINARY  Chrome/Chromium to drive        (default: Selenium finds one)
  UATC_CHROMEDRIVER   ChromeDriver to use             (default: Selenium Manager)
  UATC_UI_REQUIRED=1  fail instead of skip when no browser can be started
                      (set in CI so a missing browser is never silently green)
"""
import os
import re
import shutil
import socket
import subprocess
import time
from pathlib import Path

import pytest
from selenium import webdriver
from selenium.webdriver.chrome.service import Service
from selenium.webdriver.common.by import By
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.support.ui import WebDriverWait

REPO = Path(__file__).resolve().parents[2]
CLI = Path(os.environ.get("UATC_CLI", REPO / "build" / "uatc_cli"))


def _free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


@pytest.fixture(scope="session")
def server(tmp_path_factory):
    """Starts `uatc serve` and yields (base_url, token, root)."""
    if not CLI.exists():
        pytest.fail(f"{CLI} not found - run `make cli` first")

    root = tmp_path_factory.mktemp("uatc_root")
    port = _free_port()
    proc = subprocess.Popen(
        [str(CLI), "serve", "--port", str(port), "--root", str(root),
         "--web", str(REPO / "web")],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    token = None
    deadline = time.time() + 10
    while time.time() < deadline and token is None:
        line = proc.stdout.readline()
        if not line:
            break
        match = re.search(r"session token:\s+([0-9a-f]{32})", line)
        if match:
            token = match.group(1)
    if token is None:
        proc.kill()
        pytest.fail("uatc serve did not print a session token")

    yield f"http://127.0.0.1:{port}", token, Path(root)

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()


@pytest.fixture(scope="session")
def driver():
    options = webdriver.ChromeOptions()
    options.add_argument("--headless=new")
    options.add_argument("--no-sandbox")
    options.add_argument("--disable-dev-shm-usage")
    options.add_argument("--window-size=1100,900")
    if os.environ.get("UATC_CHROME_BINARY"):
        options.binary_location = os.environ["UATC_CHROME_BINARY"]
    service = (Service(os.environ["UATC_CHROMEDRIVER"])
               if os.environ.get("UATC_CHROMEDRIVER") else Service())
    try:
        drv = webdriver.Chrome(options=options, service=service)
    except Exception as error:  # no browser / driver on this machine
        if os.environ.get("UATC_UI_REQUIRED") == "1":
            raise
        pytest.skip(f"no Chrome/ChromeDriver available: {error}")
    drv.set_page_load_timeout(20)
    yield drv
    drv.quit()


class Ui:
    """Small helpers over the page's stable element IDs (web/README.md)."""

    def __init__(self, driver, base_url, token, root):
        self.d = driver
        self.base = base_url
        self.token = token
        self.root = root

    # ---- navigation
    def open_live(self):
        # A fresh load every time: the token is in the fragment, so a plain
        # get() to the same URL would not reload the page.
        self.d.get("about:blank")
        self.d.get(f"{self.base}/?live#token={self.token}")
        self.wait_visible("screen-home")

    def open_demo(self, fail=False):
        self.d.get("about:blank")
        self.d.get(f"{self.base}/" + ("?fail" if fail else ""))
        self.wait_visible("screen-home")

    # ---- element access
    def el(self, element_id):
        return self.d.find_element(By.ID, element_id)

    def click(self, element_id):
        WebDriverWait(self.d, 10).until(
            EC.element_to_be_clickable((By.ID, element_id))).click()

    def type(self, element_id, text):
        field = self.el(element_id)
        field.clear()
        field.send_keys(text)

    def text(self, element_id):
        return self.el(element_id).text

    def visible(self, element_id):
        return self.el(element_id).is_displayed()

    def wait_visible(self, element_id, timeout=20):
        WebDriverWait(self.d, timeout).until(
            EC.visibility_of_element_located((By.ID, element_id)))

    def wait_screen(self, *names, timeout=60):
        """Waits until one of the named screens is shown; returns its name."""
        def shown(_):
            for name in names:
                if self.el(f"screen-{name}").is_displayed():
                    return name
            return False
        return WebDriverWait(self.d, timeout).until(shown)

    # ---- the file browser dialog
    def browse_open_folder(self, name):
        button = WebDriverWait(self.d, 10).until(EC.element_to_be_clickable(
            (By.CSS_SELECTOR, f'#browser-list button[aria-label="Open folder {name}"]')))
        button.click()
        WebDriverWait(self.d, 10).until(
            lambda _: self.text("browser-path").endswith("/" + name))

    def browse_choose_file(self, name):
        WebDriverWait(self.d, 10).until(EC.element_to_be_clickable(
            (By.CSS_SELECTOR, f'#browser-list button[aria-label="Choose file {name}"]'))).click()


@pytest.fixture
def ui(driver, server):
    base, token, root = server
    return Ui(driver, base, token, root)


@pytest.fixture
def corpus(server):
    """A small project folder inside the allowed root, rebuilt per test."""
    _, _, root = server
    project = root / "project"
    shutil.rmtree(project, ignore_errors=True)
    (project / "src").mkdir(parents=True)
    (project / "README.md").write_text("Test project for the UI suite\n" * 20)
    (project / "data.csv").write_text(
        "".join(f"{i},{i * 3 % 101},{i % 7}\n" for i in range(20000)))
    (project / "src" / "main.cpp").write_text("int main() { return 0; }\n")
    (project / "empty.txt").write_text("")
    return project
